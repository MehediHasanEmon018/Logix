/* Logix TAC interpreter
 *   - pre-parses the TAC text into instructions (variables become slots, labels become indexes)
 *   - executes them; `print` writes the program output to stdout, `read` takes input from stdin
 *   - functions: a call saves the callee's local slots, so recursion works; arrays are
 *     heap objects passed by reference (like C)
 * Runtime errors: division/modulo by zero, array index out of bounds, too deep recursion,
 *                 runaway loops (step limit). */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <stdint.h>
#include <math.h>
#include "interp/interp.h"

typedef enum { V_INT, V_FLT, V_BOOL, V_ARR } VK;
struct Arr;
typedef struct { VK k; long long i; double f; struct Arr *a; } Val;
typedef struct Arr { long long n; VK elem; Val *v; } Arr;

typedef enum { O_NONE, O_VAR, O_CONST, O_STR } OK;
typedef struct { OK k; int slot; Val v; char *s; } Opd;

typedef enum { I_ASSIGN, I_CAST, I_BIN, I_NOT, I_NEG, I_IFFALSE, I_GOTO, I_LABEL, I_PRINT, I_RETURN,
               I_ARRAY, I_LOAD, I_STORE, I_READ, I_PARAM, I_CALL, I_FUNC, I_ENDFUNC } IK;
typedef struct { IK k; int dst; Opd a, b; char op[3]; int label; char *spec; int line;
                 VK elem; char *fname; int func; int nargs; } Ins;

typedef struct { char *name; int start, end; int nparams; int *params; int *locals; int nlocals; } Func;
typedef struct { int ret_pc; int dst; int func; Val *saved; char *saved_set; } Frame;

#define MAX_STEPS 200000000L
#define MAX_DEPTH 100000

static Ins  *code; static int ncode, capcode;
static char **names; static int *owner; static int nnames, capnames;
static int  *label_pc; static int nlabels;
static Val  *vars; static char *isset;
static Func *funcs; static int nfuncs, capfuncs;
static int   parse_func = -1;           /* function whose body is being parsed */

static void *xrealloc(void *p, size_t n) {
    p = realloc(p, n ? n : 1);
    if (!p) { fprintf(stderr, "out of memory\n"); exit(2); }
    return p;
}

static int slot_of(const char *name) {
    for (int i = 0; i < nnames; i++) if (strcmp(names[i], name) == 0) return i;
    if (nnames == capnames) {
        capnames = capnames ? capnames * 2 : 64;
        names = xrealloc(names, capnames * sizeof *names);
        owner = xrealloc(owner, capnames * sizeof *owner);
    }
    names[nnames] = strdup(name);
    owner[nnames] = parse_func;          /* first seen inside a function body -> its local */
    return nnames++;
}

static Val mk_int(long long i) { Val v; memset(&v, 0, sizeof v); v.k = V_INT; v.i = i; return v; }
static Val mk_flt(double f)    { Val v; memset(&v, 0, sizeof v); v.k = V_FLT; v.f = f; return v; }
static Val mk_bool(int b)      { Val v = mk_int(b != 0); v.k = V_BOOL; return v; }

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
    while (*e && *e != ' ' && *e != ',' && *e != '[' && *e != ']') e++;
    char save = *e; *e = 0;
    if (strcmp(s, "true") == 0 || strcmp(s, "false") == 0) {
        o.k = O_CONST; o.v = mk_bool(s[0] == 't');
    } else if (isdigit((unsigned char)s[0]) || (s[0] == '-' && (isdigit((unsigned char)s[1]) || s[1] == '.'))) {
        o.k = O_CONST;
        if (strpbrk(s, ".eEn")) o.v = mk_flt(atof(s));
        else o.v = mk_int(atoll(s));
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
    in->k = k; in->line = line; in->dst = -1; in->func = -1;
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

static VK kind_of(const char *s) {
    while (*s == ' ' || *s == ',') s++;
    if (strncmp(s, "float", 5) == 0) return V_FLT;
    if (strncmp(s, "bool", 4) == 0)  return V_BOOL;
    return V_INT;
}

/* "f, 2" -> name + count */
static void parse_call(Ins *in, char *p) {
    while (*p == ' ') p++;
    char *e = p;
    while (*e && *e != ',' && *e != ' ') e++;
    in->fname = strndup(p, (size_t)(e - p));
    while (*e == ',' || *e == ' ') e++;
    in->nargs = atoi(e);
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
    if (strncmp(s, "func ", 5) == 0) {                     /* func f, p1, p2 */
        if (nfuncs == capfuncs) { capfuncs = capfuncs ? capfuncs * 2 : 16; funcs = xrealloc(funcs, capfuncs * sizeof *funcs); }
        Func *f = &funcs[nfuncs];
        memset(f, 0, sizeof *f);
        Ins *in = new_ins(I_FUNC, line);
        in->func = nfuncs;
        f->start = ncode - 1;
        parse_func = nfuncs++;
        char *p = s + 5;
        while (*p == ' ') p++;
        char *e = p;
        while (*e && *e != ',' && *e != ' ') e++;
        f->name = strndup(p, (size_t)(e - p));
        p = e;
        while (*p) {
            while (*p == ',' || *p == ' ') p++;
            if (!*p) break;
            Opd o = parse_opd(&p);
            if (o.k != O_VAR) return 0;
            f->params = xrealloc(f->params, (size_t)(f->nparams + 1) * sizeof *f->params);
            f->params[f->nparams++] = o.slot;
        }
        return 1;
    }
    if (strcmp(s, "endfunc") == 0) {
        if (parse_func < 0) return 0;
        Ins *in = new_ins(I_ENDFUNC, line);
        in->func = parse_func;
        funcs[parse_func].end = ncode - 1;
        parse_func = -1;
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
    if (strncmp(s, "param ", 6) == 0) { char *p = s + 6; Ins *in = new_ins(I_PARAM, line); in->a = parse_opd(&p); return 1; }
    if (strncmp(s, "call ", 5) == 0)  { Ins *in = new_ins(I_CALL, line); parse_call(in, s + 5); return 1; }
    if (strncmp(s, "array ", 6) == 0) {                     /* array a, size, type */
        char *p = s + 6; Ins *in = new_ins(I_ARRAY, line);
        Opd a = parse_opd(&p);
        if (a.k != O_VAR) return 0;
        in->dst = a.slot;
        while (*p == ',' || *p == ' ') p++;
        in->a = parse_opd(&p);
        in->elem = kind_of(p);
        return 1;
    }
    if (strcmp(s, "return") == 0 || strncmp(s, "return ", 7) == 0) {
        Ins *in = new_ins(I_RETURN, line);
        if (s[6]) { char *p = s + 7; in->a = parse_opd(&p); }
        return 1;
    }
    /* dst = ...   or   a[i] = v */
    char *eq = strstr(s, " = ");
    if (!eq) return 0;
    *eq = 0;
    char *p = eq + 3;
    while (*p == ' ') p++;
    char *br = strchr(s, '[');
    if (br) {                                              /* a[i] = v */
        Ins *in = new_ins(I_STORE, line);
        *br = 0;
        in->dst = slot_of(s);
        char *ip = br + 1;
        in->b = parse_opd(&ip);
        in->a = parse_opd(&p);
        return 1;
    }
    int dst = slot_of(s);
    Ins *in;
    if (strncmp(p, "(float) ", 8) == 0) { in = new_ins(I_CAST, line); p += 8; in->a = parse_opd(&p); }
    else if (strncmp(p, "(int) ", 6) == 0) { in = new_ins(I_CAST, line); p += 6; in->a = parse_opd(&p); in->elem = V_INT; }
    else if (strncmp(p, "! ", 2) == 0)  { in = new_ins(I_NOT, line);  p += 2; in->a = parse_opd(&p); }
    else if (strncmp(p, "- ", 2) == 0)  { in = new_ins(I_NEG, line);  p += 2; in->a = parse_opd(&p); }
    else if (strncmp(p, "call ", 5) == 0) { in = new_ins(I_CALL, line); parse_call(in, p + 5); }
    else if (strncmp(p, "read ", 5) == 0) {
        in = new_ins(I_READ, line);
        in->elem = kind_of(p + 5);
        in->op[0] = (strncmp(p + 5, "char", 4) == 0) ? 'c' : 0;
    }
    else {
        Opd a = parse_opd(&p);
        if (*p == '[') {                                   /* t = a[i] */
            p++;
            in = new_ins(I_LOAD, line); in->a = a; in->b = parse_opd(&p);
            in->dst = dst;
            return 1;
        }
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

static void set_var(int slot, Val v) { vars[slot] = v; isset[slot] = 1; }

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

static void print_val(Val v, const char *spec) {
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

static int rt_error(const Ins *in, const char *msg) {
    fflush(stdout);
    fprintf(stderr, "Runtime error: %s\n", msg);
    (void)in;
    return 1;
}

static Val convert_to(Val v, VK k) {
    if (k == V_FLT && v.k != V_FLT) return mk_flt(as_f(v));
    if (k == V_INT && v.k == V_FLT) return wrap32((long long)v.f);
    return v;
}

static Val *pstack; static int ptop, pcap;
static Frame *frames; static int depth, fcap;

static int do_return(int *pc, Val v) {
    Frame *fr = &frames[--depth];
    Func *f = &funcs[fr->func];
    for (int k = 0; k < f->nlocals; k++) { vars[f->locals[k]] = fr->saved[k]; isset[f->locals[k]] = fr->saved_set[k]; }
    free(fr->saved); free(fr->saved_set);
    *pc = fr->ret_pc;
    if (fr->dst >= 0) set_var(fr->dst, v);
    return 0;
}

static int call_builtin(Ins *in, Val *args, Val *res) {
    const char *f = in->fname;
    double x = in->nargs > 0 ? as_f(args[0]) : 0, y = in->nargs > 1 ? as_f(args[1]) : 0;
    if (!strcmp(f, "sqrt"))  { *res = mk_flt(sqrt(x)); return 1; }
    if (!strcmp(f, "pow"))   { *res = mk_flt(pow(x, y)); return 1; }
    if (!strcmp(f, "fabs"))  { *res = mk_flt(fabs(x)); return 1; }
    if (!strcmp(f, "floor")) { *res = mk_flt(floor(x)); return 1; }
    if (!strcmp(f, "ceil"))  { *res = mk_flt(ceil(x)); return 1; }
    if (!strcmp(f, "abs"))   { long long v = as_i(args[0]); *res = wrap32(v < 0 ? -v : v); return 1; }
    return 0;
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
        case I_FUNC:                               /* reached by falling through: skip the body */
            pc = funcs[in->func].end;
            break;
        case I_ENDFUNC:                            /* fell off the end of a function */
            if (!depth) break;
            do_return(&pc, mk_int(0));
            break;
        case I_RETURN:
            if (!depth) { fflush(stdout); return 0; }          /* return from main: program ends */
            do_return(&pc, in->a.k == O_NONE ? mk_int(0) : eval(&in->a));
            break;
        case I_PARAM:
            if (ptop == pcap) { pcap = pcap ? pcap * 2 : 64; pstack = xrealloc(pstack, pcap * sizeof *pstack); }
            pstack[ptop++] = eval(&in->a);
            break;
        case I_CALL: {
            if (in->nargs > ptop) return rt_error(in, "bad call (missing parameters)");
            Val *args = pstack + (ptop - in->nargs);
            ptop -= in->nargs;
            if (in->func < 0) {                    /* built-in */
                Val r;
                if (!call_builtin(in, args, &r)) {
                    char msg[160]; snprintf(msg, sizeof msg, "unknown function '%s'", in->fname);
                    return rt_error(in, msg);
                }
                if (in->dst >= 0) set_var(in->dst, r);
                break;
            }
            Func *f = &funcs[in->func];
            if (depth >= MAX_DEPTH) return rt_error(in, "stack overflow (recursion too deep)");
            if (depth == fcap) { fcap = fcap ? fcap * 2 : 64; frames = xrealloc(frames, fcap * sizeof *frames); }
            Val argv[64]; int na = in->nargs < 64 ? in->nargs : 64;
            memcpy(argv, args, (size_t)na * sizeof *argv);
            Frame *fr = &frames[depth++];
            fr->ret_pc = pc; fr->dst = in->dst; fr->func = in->func;
            fr->saved = xrealloc(NULL, (size_t)f->nlocals * sizeof *fr->saved);
            fr->saved_set = xrealloc(NULL, (size_t)f->nlocals);
            for (int k = 0; k < f->nlocals; k++) {
                fr->saved[k] = vars[f->locals[k]]; fr->saved_set[k] = isset[f->locals[k]];
                isset[f->locals[k]] = 0;
            }
            for (int k = 0; k < f->nparams && k < na; k++) set_var(f->params[k], argv[k]);
            pc = f->start;                         /* loop's pc++ enters the body */
            break;
        }
        case I_ARRAY: {
            long long n = as_i(eval(&in->a));
            if (n <= 0 || n > 50000000) return rt_error(in, "invalid array size");
            Arr *a = malloc(sizeof *a);
            if (!a) return rt_error(in, "out of memory");
            a->n = n; a->elem = in->elem;
            a->v = calloc((size_t)n, sizeof *a->v);
            if (!a->v) return rt_error(in, "out of memory");
            Val z = in->elem == V_FLT ? mk_flt(0) : in->elem == V_BOOL ? mk_bool(0) : mk_int(0);
            for (long long k = 0; k < n; k++) a->v[k] = z;
            Val v; memset(&v, 0, sizeof v); v.k = V_ARR; v.a = a;
            set_var(in->dst, v);
            break;
        }
        case I_LOAD: case I_STORE: {
            int slot = in->k == I_LOAD ? in->a.slot : in->dst;
            Val av = isset[slot] ? vars[slot] : mk_int(0);
            if (av.k != V_ARR) return rt_error(in, "array used before it was created");
            long long idx = as_i(eval(&in->b));
            if (idx < 0 || idx >= av.a->n) {
                char msg[160];
                snprintf(msg, sizeof msg, "array index %lld out of bounds (size %lld)", idx, av.a->n);
                return rt_error(in, msg);
            }
            if (in->k == I_LOAD) set_var(in->dst, av.a->v[idx]);
            else av.a->v[idx] = convert_to(eval(&in->a), av.a->elem);
            break;
        }
        case I_READ: {
            fflush(stdout);
            Val r;
            if (in->op[0] == 'c') { char c = 0; r = mk_int(scanf(" %c", &c) == 1 ? (unsigned char)c : 0); }
            else if (in->elem == V_FLT) { double d = 0; if (scanf("%lf", &d) != 1) d = 0; r = mk_flt(d); }
            else { long long x = 0; if (scanf("%lld", &x) != 1) x = 0; r = wrap32(x); }
            set_var(in->dst, r);
            break;
        }
        case I_ASSIGN: set_var(in->dst, eval(&in->a)); break;
        case I_CAST:
            if (in->elem == V_INT) set_var(in->dst, wrap32((long long)as_f(eval(&in->a))));
            else set_var(in->dst, mk_flt(as_f(eval(&in->a))));
            break;
        case I_NOT: set_var(in->dst, mk_bool(!truthy(eval(&in->a)))); break;
        case I_NEG: {
            Val a = eval(&in->a);
            set_var(in->dst, is_float(a) ? mk_flt(-a.f) : wrap32(-a.i));
            break;
        }
        case I_BIN: {
            Val a = eval(&in->a), b = eval(&in->b), r;
            const char *op = in->op;
            int fl = is_float(a) || is_float(b);
            r = mk_int(0);
            if (!strcmp(op, "&&")) r = mk_bool(truthy(a) && truthy(b));
            else if (!strcmp(op, "||")) r = mk_bool(truthy(a) || truthy(b));
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
                r = mk_bool(c);
            } else if (fl) {
                double x = as_f(a), y = as_f(b), z = 0;
                switch (op[0]) { case '+': z = x + y; break; case '-': z = x - y; break;
                                 case '*': z = x * y; break; case '/': z = x / y; break; }
                r = mk_flt(z);
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
            set_var(in->dst, r);
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
    /* resolve calls; collect each function's local slots */
    for (int i = 0; i < ncode; i++)
        if (code[i].k == I_CALL)
            for (int f = 0; f < nfuncs; f++)
                if (strcmp(funcs[f].name, code[i].fname) == 0) { code[i].func = f; break; }
    for (int s = 0; s < nnames; s++)
        if (owner[s] >= 0) {
            Func *f = &funcs[owner[s]];
            f->locals = xrealloc(f->locals, (size_t)(f->nlocals + 1) * sizeof *f->locals);
            f->locals[f->nlocals++] = s;
        }
    vars  = calloc(nnames ? nnames : 1, sizeof *vars);
    isset = calloc(nnames ? nnames : 1, 1);
    if (!vars || !isset) { fprintf(stderr, "out of memory\n"); return 2; }
    return execute();
}
