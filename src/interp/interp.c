/* Logix TAC interpreter
 *   - pre-parses the TAC text into instructions (variables become slots, labels become indexes)
 *   - executes them; `print` writes the program output to stdout
 * Also: arrays (array / a[i] = x / t = a[i]), functions (func / param / call / return / endfunc,
 * with per-call saving of the callee's locals so recursion works) and scanf (scan).
 * Runtime errors: division/modulo by zero, array index out of range, runaway loops / recursion. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>
#include "interp/interp.h"

typedef enum { V_INT, V_FLT, V_BOOL } VK;
typedef struct { VK k; long long i; double f; } Val;

typedef enum { O_NONE, O_VAR, O_CONST, O_STR } OK;
typedef struct { OK k; int slot; Val v; char *s; } Opd;

typedef enum { I_ASSIGN, I_CAST, I_BIN, I_NOT, I_NEG, I_IFFALSE, I_GOTO, I_LABEL, I_PRINT, I_RETURN,
               I_ARRAY, I_LOAD, I_STORE, I_PARAM, I_PARAMARR, I_CALL, I_SCAN, I_FUNC, I_ENDFUNC, I_LEN } IK;
typedef struct { IK k; int dst; Opd a, b; char op[3]; int label; char *spec; int line;
                 int arr;        /* array slot (array ops, paramarr) */
                 int aux;        /* I_ARRAY: size; I_CALL: argument count; I_SCAN: format letter */
                 int fn;         /* I_CALL: resolved function index (-1 = not yet) */
                 char *fname;    /* I_CALL */
                 char kind;      /* I_ARRAY: 'i' / 'f' / 'b' element kind */
               } Ins;

/* arrays: a slot holds a heap block; 'owned' = allocated by an 'array' instruction in this activation
 * (array parameters just alias the caller's block) */
typedef struct { Val *d; int n; int owned; } Arr;

typedef struct { char *name; int entry, np, nl, nla; int *pslot, *pisarr, *lslot, *larr; } Fn;
typedef struct { int ret_pc, dst, fn; Val *sv; char *ss; Arr *sa; } Frame;
typedef struct { Val v; int isarr; Arr arr; } PArg;

#define MAX_STEPS 20000000L
#define MAX_DEPTH 20000
#define MAX_PARGS 64

static char **arr_names; static int narrs, caparrs;
static Arr  *arrs;
static Fn   *fns; static int nfns;
static Frame *frames; static int nframes, capframes;
static PArg  pargs[MAX_PARGS]; static int npend;
static int   cur_fn_def = -1;       /* function whose header was parsed last (parse time only) */

static Ins  *code; static int ncode, capcode;
static char **names; static int nnames, capnames;
static int  *label_pc; static int nlabels;
static Val  *vars; static char *isset;

static void *xrealloc(void *p, size_t n) {
    p = realloc(p, n);
    if (!p) { fprintf(stderr, "out of memory\n"); exit(2); }
    return p;
}

static int slot_of(const char *name) {
    for (int i = 0; i < nnames; i++) if (strcmp(names[i], name) == 0) return i;
    if (nnames == capnames) { capnames = capnames ? capnames * 2 : 64; names = xrealloc(names, capnames * sizeof *names); }
    names[nnames] = strdup(name);
    return nnames++;
}

static int arr_slot_of(const char *name) {
    for (int i = 0; i < narrs; i++) if (strcmp(arr_names[i], name) == 0) return i;
    if (narrs == caparrs) { caparrs = caparrs ? caparrs * 2 : 16; arr_names = xrealloc(arr_names, caparrs * sizeof *arr_names); }
    arr_names[narrs] = strdup(name);
    return narrs++;
}

static Val mk_int(long long i) { Val v; v.k = V_INT; v.i = i; v.f = 0; return v; }

/* parse one operand starting at *p; advances *p past it */
static Opd parse_opd(char **p) {
    Opd o; memset(&o, 0, sizeof o);
    char *s = *p;
    while (*s == ' ') s++;
    if (*s == '"') {                               /* string literal (print only) */
        char *e = s + 1;
        while (*e && *e != '"') { if (*e == '\\' && e[1]) e++; e++; }
        o.k = O_STR; o.s = malloc((size_t)(e - s)); memcpy(o.s, s + 1, (size_t)(e - s - 1)); o.s[e - s - 1] = 0;
        *p = *e ? e + 1 : e;
        return o;
    }
    char *e = s;
    while (*e && *e != ' ' && *e != ',') e++;
    char save = *e; *e = 0;
    if (strcmp(s, "true") == 0 || strcmp(s, "false") == 0) {
        o.k = O_CONST; o.v.k = V_BOOL; o.v.i = (s[0] == 't');
    } else if (isdigit((unsigned char)s[0]) || (s[0] == '-' && (isdigit((unsigned char)s[1]) || s[1] == '.'))) {
        o.k = O_CONST;
        if (strpbrk(s, ".eEn")) { o.v.k = V_FLT; o.v.f = atof(s); }
        else { o.v.k = V_INT; o.v.i = atoll(s); }
    } else {
        o.k = O_VAR; o.slot = slot_of(s);
    }
    *e = save;
    *p = e;
    return o;
}

static Ins *new_ins(IK k, int line) {
    if (ncode == capcode) { capcode = capcode ? capcode * 2 : 256; code = xrealloc(code, capcode * sizeof *code); }
    Ins *in = &code[ncode++];
    memset(in, 0, sizeof *in);
    in->k = k; in->line = line;
    return in;
}

static void set_label(int n, int pc) {
    if (n >= nlabels) {
        int old = nlabels; nlabels = n + 16;
        label_pc = xrealloc(label_pc, nlabels * sizeof *label_pc);
        for (int i = old; i < nlabels; i++) label_pc[i] = -1;
    }
    label_pc[n] = pc;
}

static int parse_label_ref(const char *s) { while (*s == ' ') s++; return (*s == 'L') ? atoi(s + 1) : -1; }

/* keyword at the start of a TAC line (and not just a variable that happens to be named like it) */
static int kw(const char *s, const char *k) {
    size_t n = strlen(k);
    return strncmp(s, k, n) == 0 && s[n] == ' ' && s[n + 1] != '=';
}

/* next word of a "a, b, c" style line: skips blanks and commas, ends at the next blank/comma */
static char *trim_word(char **p) {
    char *s = *p;
    while (*s == ' ' || *s == ',') s++;
    char *e = s;
    while (*e && *e != ' ' && *e != ',') e++;
    if (*e) *e++ = 0;
    *p = e;
    return s;
}

/* func name(p1, p2[]) locals(a, b[], t1) */
static int parse_func_header(char *s, int line) {
    char *lp = strchr(s, '(');
    if (!lp) return 0;
    *lp = 0;
    Ins *in = new_ins(I_FUNC, line); (void)in;
    fns = xrealloc(fns, (size_t)(nfns + 1) * sizeof *fns);
    Fn *f = &fns[nfns]; memset(f, 0, sizeof *f);
    f->name = strdup(s); f->entry = ncode;               /* first instruction after the header */
    char *p = lp + 1;
    char *rp = strchr(p, ')');
    if (!rp) return 0;
    *rp = 0;
    char *loc = strstr(rp + 1, "locals(");
    f->pslot = xrealloc(NULL, 64 * sizeof(int)); f->pisarr = xrealloc(NULL, 64 * sizeof(int));
    while (*p) {
        char *w = trim_word(&p);
        if (!*w) break;
        size_t l = strlen(w);
        int isarr = l > 2 && w[l - 2] == '[' && w[l - 1] == ']';
        if (isarr) w[l - 2] = 0;
        if (f->np >= 64) return 0;
        f->pisarr[f->np] = isarr;
        f->pslot[f->np] = isarr ? arr_slot_of(w) : slot_of(w);
        f->np++;
    }
    f->lslot = xrealloc(NULL, 4096 * sizeof(int)); f->larr = xrealloc(NULL, 1024 * sizeof(int));
    if (loc) {
        p = loc + 7;
        char *e = strrchr(p, ')');
        if (e) *e = 0;
        while (*p) {
            char *w = trim_word(&p);
            if (!*w) break;
            size_t l = strlen(w);
            int isarr = l > 2 && w[l - 2] == '[' && w[l - 1] == ']';
            if (isarr) { w[l - 2] = 0; if (f->nla < 1024) f->larr[f->nla++] = arr_slot_of(w); }
            else if (f->nl < 4096) f->lslot[f->nl++] = slot_of(w);
        }
    }
    nfns++;
    cur_fn_def = nfns - 1;
    return 1;
}

static int parse_line(char *s, int line) {
    while (*s == ' ' || *s == '\t') s++;
    size_t len = strlen(s);
    while (len && (s[len - 1] == '\r' || s[len - 1] == '\n' || s[len - 1] == ' ')) s[--len] = 0;
    if (!*s) return 1;

    if (s[0] == 'L' && isdigit((unsigned char)s[1]) && s[len - 1] == ':') {          /* Ln: */
        Ins *in = new_ins(I_LABEL, line);
        in->label = atoi(s + 1);
        set_label(in->label, ncode - 1);
        return 1;
    }
    if (strncmp(s, "goto ", 5) == 0) { Ins *in = new_ins(I_GOTO, line); in->label = parse_label_ref(s + 5); return 1; }
    if (strncmp(s, "ifFalse ", 8) == 0) {
        char *p = s + 8; Ins *in = new_ins(I_IFFALSE, line);
        in->a = parse_opd(&p);
        while (*p == ' ') p++;
        if (strncmp(p, "goto", 4) != 0) return 0;
        in->label = parse_label_ref(p + 4);
        return 1;
    }
    if (strncmp(s, "print ", 6) == 0) {
        char *p = s + 6; Ins *in = new_ins(I_PRINT, line);
        in->a = parse_opd(&p);
        while (*p == ' ') p++;
        if (*p == ',') { p++; Opd sp = parse_opd(&p); in->spec = sp.s; }
        return 1;
    }
    if (strcmp(s, "return") == 0 || strncmp(s, "return ", 7) == 0) {
        Ins *in = new_ins(I_RETURN, line);
        if (s[6]) { char *p = s + 7; in->a = parse_opd(&p); }
        return 1;
    }
    if (strcmp(s, "endfunc") == 0) { new_ins(I_ENDFUNC, line); return 1; }
    if (kw(s, "func")) return parse_func_header(s + 5, line);
    if (kw(s, "param"))    { char *p = s + 6; Ins *in = new_ins(I_PARAM, line); in->a = parse_opd(&p); return 1; }
    if (kw(s, "paramarr")) { char *p = s + 9; Ins *in = new_ins(I_PARAMARR, line); in->arr = arr_slot_of(trim_word(&p)); return 1; }
    if (kw(s, "array")) {                                   /* array NAME SIZE [float|bool] */
        char *p = s + 6; Ins *in = new_ins(I_ARRAY, line);
        in->arr = arr_slot_of(trim_word(&p));
        in->aux = atoi(trim_word(&p));
        char *k = trim_word(&p);
        in->kind = *k == 'f' ? 'f' : *k == 'b' ? 'b' : 'i';
        return 1;
    }
    if (kw(s, "scan")) {                                    /* scan DST, d|f|c|C */
        char *p = s + 5; Ins *in = new_ins(I_SCAN, line);
        in->dst = slot_of(trim_word(&p));
        char *k = trim_word(&p);
        in->aux = *k;
        return 1;
    }
    if (kw(s, "call")) {                                    /* call NAME, N   (result discarded) */
        char *p = s + 5; Ins *in = new_ins(I_CALL, line);
        in->fname = strdup(trim_word(&p)); in->aux = atoi(trim_word(&p)); in->fn = -1; in->dst = -1;
        return 1;
    }

    /* dst = ... */
    char *eq = strstr(s, " = ");
    if (!eq) return 0;
    *eq = 0;
    char *p = eq + 3;
    while (*p == ' ') p++;
    Ins *in;
    {   /* a[idx] = value */
        char *lb = strchr(s, '[');
        if (lb) {
            char *rb = strrchr(s, ']');
            if (!rb || rb < lb) return 0;
            *rb = 0; *lb = 0;
            in = new_ins(I_STORE, line);
            in->arr = arr_slot_of(s);
            char *ip = lb + 1; in->a = parse_opd(&ip);
            in->b = parse_opd(&p);
            return 1;
        }
    }
    int dst = slot_of(s);
    if (kw(p, "call")) {                                    /* t = call NAME, N */
        char *q = p + 5; in = new_ins(I_CALL, line);
        in->fname = strdup(trim_word(&q)); in->aux = atoi(trim_word(&q)); in->fn = -1; in->dst = dst;
        return 1;
    }
    {   /* t = a[idx] */
        char *lb = strchr(p, '['), *sp = strchr(p, ' ');
        if (lb && (!sp || lb < sp)) {
            char *rb = strchr(lb, ']');
            if (!rb) return 0;
            *rb = 0; *lb = 0;
            in = new_ins(I_LOAD, line);
            in->arr = arr_slot_of(p);
            char *ip = lb + 1; in->a = parse_opd(&ip);
            in->dst = dst;
            return 1;
        }
    }
    if (strncmp(p, "len ", 4) == 0) {                      /* t = len a   (Java arr.length) */
        char *q = p + 4;
        in = new_ins(I_LEN, line); in->arr = arr_slot_of(trim_word(&q));
        in->dst = dst;
        return 1;
    }
    if (strncmp(p, "(int) ", 6) == 0) { in = new_ins(I_CAST, line); p += 6; in->a = parse_opd(&p); in->aux = 'i'; }
    else if (strncmp(p, "(float) ", 8) == 0) { in = new_ins(I_CAST, line); p += 8; in->a = parse_opd(&p); }
    else if (strncmp(p, "! ", 2) == 0)  { in = new_ins(I_NOT, line);  p += 2; in->a = parse_opd(&p); }
    else if (strncmp(p, "- ", 2) == 0)  { in = new_ins(I_NEG, line);  p += 2; in->a = parse_opd(&p); }
    else {
        Opd a = parse_opd(&p);
        while (*p == ' ') p++;
        if (!*p) { in = new_ins(I_ASSIGN, line); in->a = a; }
        else {
            in = new_ins(I_BIN, line); in->a = a;
            int n = 0;
            while (*p && *p != ' ' && n < 2) in->op[n++] = *p++;
            in->op[n] = 0;
            in->b = parse_opd(&p);
        }
    }
    in->dst = dst;
    return 1;
}

/* ---- values ---------------------------------------------------------- */

static int is_float(Val v) { return v.k == V_FLT; }
static double as_f(Val v) { return v.k == V_FLT ? v.f : (double)v.i; }
static long long as_i(Val v) { return v.k == V_FLT ? (long long)v.f : v.i; }
static int truthy(Val v) { return v.k == V_FLT ? v.f != 0.0 : v.i != 0; }
static Val wrap32(long long x) { return mk_int((int32_t)(uint32_t)(unsigned long long)x); }

static Val eval(const Opd *o) {
    if (o->k == O_CONST) return o->v;
    if (o->k == O_VAR)   return isset[o->slot] ? vars[o->slot] : mk_int(0);
    return mk_int(0);
}

static void put_string(const char *s) {
    for (; *s; s++) {
        if (*s == '\\' && s[1]) {
            s++;
            switch (*s) {
            case 'n': putchar('\n'); break;  case 't': putchar('\t'); break;
            case 'r': putchar('\r'); break;  case '0': break;
            case 'a': putchar('\a'); break;  case '\\': putchar('\\'); break;
            case '"': putchar('"'); break;   case '\'': putchar('\''); break;
            default:  putchar(*s);
            }
        } else putchar(*s);
    }
}

/* Java's Double.toString: 3.5, 10.0, 0.3333333333333333, 1.0E7 */
static void print_java_double(double d) {
    if (isnan(d)) { fputs("NaN", stdout); return; }
    if (isinf(d)) { fputs(d < 0 ? "-Infinity" : "Infinity", stdout); return; }
    char buf[64];
    double ad = fabs(d);
    if (d == 0 || (ad >= 1e-3 && ad < 1e7)) {
        for (int k = 1; k <= 17; k++) {
            snprintf(buf, sizeof buf, "%.*f", k, d);
            if (strtod(buf, NULL) == d) break;
        }
        fputs(buf, stdout);
        return;
    }
    int p;
    for (p = 0; p <= 17; p++) { snprintf(buf, sizeof buf, "%.*e", p, d); if (strtod(buf, NULL) == d) break; }
    char *e = strchr(buf, 'e');
    int ex = atoi(e + 1);
    *e = 0;
    printf("%s%sE%d", buf, strchr(buf, '.') ? "" : ".0", ex);
}

static void print_val(Val v, const char *spec) {
    if (spec && strcmp(spec, "%J") == 0) {          /* Java println of a double */
        if (v.k == V_FLT) print_java_double(v.f); else printf("%lld", v.i);
        return;
    }
    if (!spec) {
        if (v.k == V_FLT) printf("%g", v.f);
        else if (v.k == V_BOOL) fputs(v.i ? "true" : "false", stdout);
        else printf("%lld", v.i);
        return;
    }
    char fmt[64]; size_t n = 0;
    size_t sl = strlen(spec);
    char conv = spec[sl - 1];
    for (size_t i = 0; i + 1 < sl && n < 40; i++)
        if (spec[i] != 'l' && spec[i] != 'h' && spec[i] != 'z') fmt[n++] = spec[i];
    if (strchr("diux", conv)) { fmt[n++] = 'l'; fmt[n++] = 'l'; }
    fmt[n++] = conv; fmt[n] = 0;
    switch (conv) {
    case 'd': case 'i': printf(fmt, as_i(v)); break;
    case 'u': case 'x': printf(fmt, (unsigned long long)(uint32_t)as_i(v)); break;
    case 'c': printf(fmt, (int)as_i(v)); break;
    case 'f': case 'F': case 'e': case 'E': case 'g': case 'G': printf(fmt, as_f(v)); break;
    default:  print_val(v, NULL);
    }
}

/* sqrt pow fabs floor ceil (float); abs max min (int when every argument is int) */
static int call_builtin(const char *f, PArg *args, int n, Val *res) {
    double x = n > 0 ? as_f(args[0].v) : 0, y = n > 1 ? as_f(args[1].v) : 0;
    int all_int = 1;
    for (int i = 0; i < n; i++) if (args[i].v.k == V_FLT) all_int = 0;
    Val r = mk_int(0);
    if (!strcmp(f, "sqrt") || !strcmp(f, "pow") || !strcmp(f, "fabs") || !strcmp(f, "floor") || !strcmp(f, "ceil")) {
        double z = !strcmp(f, "sqrt") ? sqrt(x) : !strcmp(f, "pow") ? pow(x, y) : !strcmp(f, "fabs") ? fabs(x)
                 : !strcmp(f, "floor") ? floor(x) : ceil(x);
        r.k = V_FLT; r.f = z;
    } else if (!strcmp(f, "abs") || !strcmp(f, "max") || !strcmp(f, "min")) {
        double z = !strcmp(f, "abs") ? fabs(x) : !strcmp(f, "max") ? (x > y ? x : y) : (x < y ? x : y);
        if (all_int) r = wrap32((long long)z); else { r.k = V_FLT; r.f = z; }
    } else return 0;
    *res = r;
    return 1;
}

static int rt_error(const Ins *in, const char *msg) {
    fflush(stdout);
    fprintf(stderr, "Runtime error: %s\n", msg);
    (void)in;
    return 1;
}

static int execute(void) {
    long steps = 0;
    for (int pc = 0; pc < ncode; pc++) {
        if (++steps > MAX_STEPS) return rt_error(NULL, "execution limit exceeded (possible infinite loop)");
        Ins *in = &code[pc];
        switch (in->k) {
        case I_LABEL: break;
        case I_GOTO:
            if (in->label < 0 || in->label >= nlabels || label_pc[in->label] < 0) return rt_error(in, "bad jump target");
            pc = label_pc[in->label]; break;
        case I_IFFALSE:
            if (!truthy(eval(&in->a))) {
                if (in->label < 0 || in->label >= nlabels || label_pc[in->label] < 0) return rt_error(in, "bad jump target");
                pc = label_pc[in->label];
            }
            break;
        case I_PRINT:
            if (in->a.k == O_STR) put_string(in->a.s);
            else {
                print_val(eval(&in->a), in->spec);
                if (!in->spec) putchar('\n');      /* plain `print x;` is one line; printf has its own \n */
            }
            break;
        case I_FUNC: fflush(stdout); return 0;               /* ran into a function body: main is over */
        case I_RETURN:
        case I_ENDFUNC: {
            Val rv = (in->k == I_RETURN && in->a.k != O_NONE) ? eval(&in->a) : mk_int(0);
            if (nframes == 0) { fflush(stdout); return 0; }    /* return from main */
            Frame fr = frames[--nframes];
            Fn *f = &fns[fr.fn];
            for (int i = 0; i < f->nla; i++) if (arrs[f->larr[i]].owned) free(arrs[f->larr[i]].d);
            for (int i = 0; i < f->nl; i++)  { vars[f->lslot[i]] = fr.sv[i]; isset[f->lslot[i]] = fr.ss[i]; }
            for (int i = 0; i < f->nla; i++) arrs[f->larr[i]] = fr.sa[i];
            free(fr.sv); free(fr.ss); free(fr.sa);
            if (fr.dst >= 0) { vars[fr.dst] = rv; isset[fr.dst] = 1; }
            pc = fr.ret_pc;
            break;
        }
        case I_PARAM:
            if (npend >= MAX_PARGS) return rt_error(in, "too many arguments");
            pargs[npend].isarr = 0; pargs[npend].v = eval(&in->a); npend++;
            break;
        case I_PARAMARR:
            if (npend >= MAX_PARGS) return rt_error(in, "too many arguments");
            pargs[npend].isarr = 1; pargs[npend].arr = arrs[in->arr]; npend++;
            break;
        case I_CALL: {
            if (in->fn < 0) {
                for (int i = 0; i < nfns; i++) if (strcmp(fns[i].name, in->fname) == 0) in->fn = i;
                if (in->fn < 0) {                              /* built-in math function? */
                    Val r;
                    if (!call_builtin(in->fname, pargs, npend, &r)) { npend = 0; return rt_error(in, "call to an undefined function"); }
                    npend = 0;
                    if (in->dst >= 0) { vars[in->dst] = r; isset[in->dst] = 1; }
                    break;
                }
            }
            Fn *f = &fns[in->fn];
            if (npend != f->np) { npend = 0; return rt_error(in, "wrong number of arguments in call"); }
            if (nframes >= MAX_DEPTH) return rt_error(in, "stack overflow (infinite recursion?)");
            if (nframes == capframes) { capframes = capframes ? capframes * 2 : 64; frames = xrealloc(frames, capframes * sizeof *frames); }
            Frame *fr = &frames[nframes++];
            fr->ret_pc = pc; fr->dst = in->dst; fr->fn = in->fn;
            fr->sv = xrealloc(NULL, (f->nl ? f->nl : 1) * sizeof(Val));
            fr->ss = xrealloc(NULL, f->nl ? f->nl : 1);
            fr->sa = xrealloc(NULL, (f->nla ? f->nla : 1) * sizeof(Arr));
            for (int i = 0; i < f->nl; i++)  { fr->sv[i] = vars[f->lslot[i]]; fr->ss[i] = isset[f->lslot[i]]; isset[f->lslot[i]] = 0; }
            for (int i = 0; i < f->nla; i++) { fr->sa[i] = arrs[f->larr[i]]; memset(&arrs[f->larr[i]], 0, sizeof(Arr)); }
            for (int i = 0; i < f->np; i++) {
                if (f->pisarr[i]) { arrs[f->pslot[i]] = pargs[i].arr; arrs[f->pslot[i]].owned = 0; }
                else { vars[f->pslot[i]] = pargs[i].v; isset[f->pslot[i]] = 1; }
            }
            npend = 0;
            pc = f->entry - 1;                                 /* loop increment lands on the entry */
            break;
        }
        case I_ARRAY: {
            Arr *A = &arrs[in->arr];
            if (in->aux <= 0 || in->aux > 10000000) return rt_error(in, "bad array size");
            if (A->owned) free(A->d);
            A->d = calloc((size_t)in->aux, sizeof(Val));
            if (!A->d) return rt_error(in, "out of memory");
            A->n = in->aux; A->owned = 1;
            for (int i = 0; i < A->n; i++) {
                if (in->kind == 'f') { A->d[i].k = V_FLT; A->d[i].f = 0; }
                else if (in->kind == 'b') A->d[i].k = V_BOOL;
            }
            break;
        }
        case I_LOAD: case I_STORE: {
            Arr *A = &arrs[in->arr];
            long long ix = as_i(eval(&in->a));
            if (!A->d || ix < 0 || ix >= A->n) {
                char msg[160];
                snprintf(msg, sizeof msg, "array index out of range: %s[%lld] (size %d)", arr_names[in->arr], ix, A->d ? A->n : 0);
                return rt_error(in, msg);
            }
            if (in->k == I_LOAD) { vars[in->dst] = A->d[ix]; isset[in->dst] = 1; }
            else A->d[ix] = eval(&in->b);
            break;
        }
        case I_SCAN: {
            fflush(stdout);
            Val r = mk_int(0); int ok = 0;
            if (in->aux == 'f') { double x; if (scanf("%lf", &x) == 1) { r.k = V_FLT; r.f = x; ok = 1; } }
            else if (in->aux == 'c') { int ch = getchar(); if (ch != EOF) { r = mk_int(ch); ok = 1; } }
            else if (in->aux == 'C') { int ch; while ((ch = getchar()) == ' ' || ch == '\n' || ch == '\t' || ch == '\r') {}
                                       if (ch != EOF) { r = mk_int(ch); ok = 1; } }
            else { long long x; if (scanf("%lld", &x) == 1) { r = wrap32(x); ok = 1; } }
            if (ok) { vars[in->dst] = r; isset[in->dst] = 1; }   /* on end of input the variable keeps its old value */
            break;
        }
        case I_ASSIGN: vars[in->dst] = eval(&in->a); isset[in->dst] = 1; break;
        case I_LEN: { Val r = mk_int(arrs[in->arr].n); vars[in->dst] = r; isset[in->dst] = 1; break; }
        case I_CAST: if (in->aux == 'i') { Val a = eval(&in->a); vars[in->dst] = wrap32(as_i(a)); isset[in->dst] = 1; break; }
        { Val a = eval(&in->a); Val r; r.k = V_FLT; r.f = as_f(a); r.i = 0; vars[in->dst] = r; isset[in->dst] = 1; break; }
        case I_NOT: { Val r = mk_int(!truthy(eval(&in->a))); r.k = V_BOOL; vars[in->dst] = r; isset[in->dst] = 1; break; }
        case I_NEG: {
            Val a = eval(&in->a), r;
            if (is_float(a)) { r.k = V_FLT; r.f = -a.f; r.i = 0; } else r = wrap32(-a.i);
            vars[in->dst] = r; isset[in->dst] = 1; break;
        }
        case I_BIN: {
            Val a = eval(&in->a), b = eval(&in->b), r;
            const char *op = in->op;
            int fl = is_float(a) || is_float(b);
            r = mk_int(0);
            if (!strcmp(op, "&&")) { r = mk_int(truthy(a) && truthy(b)); r.k = V_BOOL; }
            else if (!strcmp(op, "||")) { r = mk_int(truthy(a) || truthy(b)); r.k = V_BOOL; }
            else if (!strcmp(op, "<") || !strcmp(op, ">") || !strcmp(op, "<=") || !strcmp(op, ">=") ||
                     !strcmp(op, "==") || !strcmp(op, "!=")) {
                int c;
                if (fl) {
                    double x = as_f(a), y = as_f(b);
                    c = !strcmp(op, "<") ? x < y : !strcmp(op, ">") ? x > y : !strcmp(op, "<=") ? x <= y :
                        !strcmp(op, ">=") ? x >= y : !strcmp(op, "==") ? x == y : x != y;
                } else {
                    long long x = a.i, y = b.i;
                    c = !strcmp(op, "<") ? x < y : !strcmp(op, ">") ? x > y : !strcmp(op, "<=") ? x <= y :
                        !strcmp(op, ">=") ? x >= y : !strcmp(op, "==") ? x == y : x != y;
                }
                r = mk_int(c); r.k = V_BOOL;
            } else if (fl) {
                double x = as_f(a), y = as_f(b), z = 0;
                switch (op[0]) { case '+': z = x + y; break; case '-': z = x - y; break;
                                 case '*': z = x * y; break; case '/': z = x / y; break; }
                r.k = V_FLT; r.f = z; r.i = 0;
            } else {
                long long x = a.i, y = b.i, z = 0;
                switch (op[0]) {
                case '+': z = x + y; break;
                case '-': z = x - y; break;
                case '*': z = x * y; break;
                case '/': if (y == 0) return rt_error(in, "division by zero"); z = x / y; break;
                case '%': if (y == 0) return rt_error(in, "modulo by zero");   z = x % y; break;
                }
                r = wrap32(z);
            }
            vars[in->dst] = r; isset[in->dst] = 1;
            break;
        }
        }
    }
    fflush(stdout);
    return 0;
}

int interp_run(FILE *tac) {
    char line[4096];
    int ln = 0;
    while (fgets(line, sizeof line, tac)) {
        ln++;
        if (!parse_line(line, ln)) { fprintf(stderr, "Runtime error: cannot understand TAC line %d\n", ln); return 1; }
    }
    vars  = calloc(nnames ? nnames : 1, sizeof *vars);
    isset = calloc(nnames ? nnames : 1, 1);
    arrs  = calloc(narrs ? narrs : 1, sizeof *arrs);
    if (!vars || !isset || !arrs) { fprintf(stderr, "out of memory\n"); return 2; }
    return execute();
}