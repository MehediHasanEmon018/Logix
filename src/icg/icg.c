/* Logix Three Address Code generator
 *
 * Strategy: a post-order walk over the AST.
 *   - gen_expr() emits the instructions for an expression and returns the
 *     "place" (variable name, literal, or fresh temporary tN) holding its value.
 *   - Control flow is lowered to labels (Ln:), "ifFalse c goto Ln" and "goto Ln".
 *   - Variables shadowed in nested scopes get unique TAC names (x, x_1, x_2 ...)
 *     so that the flat TAC never confuses an inner x with the outer x.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include "icg/icg.h"

static FILE *out;
static int   temp_count, label_count;

/* ---- functions ---------------------------------------------------------
 * Each function body is generated into its own text buffer (so its header can list the
 * locals) and printed after the main program:
 *     func name(p1, p2[]) locals(p1, p2[], x, t3)
 *     ...
 *     endfunc
 * "locals" = everything that must be saved/restored per call (recursion). */
typedef struct Fn {
    const Node *node;                 /* the N_FUNC node */
    char  *buf; size_t len, cap;      /* generated body */
    char **locals; int nlocals;       /* tac names, arrays end with [] */
    char **params; int nparams;
    struct Fn *next;
} Fn;
static Fn *fn_list, *fn_tail;         /* definitions generated so far, in source order */
static Fn *curfn;                     /* function being generated (NULL = main / top level) */
static const Node *root_node;         /* for finding callee signatures */

static void add_str(char ***arr, int *n, const char *s) {
    *arr = realloc(*arr, (size_t)(*n + 1) * sizeof **arr);
    if (!*arr) { fprintf(stderr, "out of memory\n"); exit(2); }
    (*arr)[(*n)++] = strdup(s);
}

static void put(const char *text) {   /* write to the current destination */
    if (!curfn) { fputs(text, out); return; }
    size_t l = strlen(text);
    if (curfn->len + l + 1 > curfn->cap) {
        curfn->cap = (curfn->len + l + 1) * 2;
        curfn->buf = realloc(curfn->buf, curfn->cap);
        if (!curfn->buf) { fprintf(stderr, "out of memory\n"); exit(2); }
    }
    memcpy(curfn->buf + curfn->len, text, l + 1);
    curfn->len += l;
}

/* loops: where 'break' and 'continue' jump to */
#define MAX_LOOPS 64
static int brk_lab[MAX_LOOPS], cont_lab[MAX_LOOPS], loop_sp;

/* ---- name management (scope-aware) ---------------------------------- */

typedef struct Var  { char *name; char *tac; struct Var *next; } Var;
typedef struct Used { char *name; int count; struct Used *next; } Used;
typedef struct Taken { char *tac; struct Taken *next; } Taken;
static Taken *taken;   /* every TAC name already handed out */

static Var  *vars;   /* variables visible right now, innermost first */
static Used *used;   /* how many times each source name has been declared */

static char *dup_str(const char *s) {
    char *p = strdup(s);
    if (!p) { fprintf(stderr, "out of memory\n"); exit(2); }
    return p;
}

/* "t12" / "L3" would collide with our own temporaries and labels */
static int looks_generated(const char *s) {
    if ((s[0] != 't' && s[0] != 'L') || !s[1]) return 0;
    for (const char *p = s + 1; *p; p++)
        if (!isdigit((unsigned char)*p)) return 0;
    return 1;
}

static int is_taken(const char *s) {
    for (Taken *t = taken; t; t = t->next) if (strcmp(t->tac, s) == 0) return 1;
    return 0;
}

static void declare_ex(const char *name, int is_array) {
    Used *u = used;
    while (u && strcmp(u->name, name) != 0) u = u->next;
    if (!u) {
        u = calloc(1, sizeof(Used));
        if (!u) { fprintf(stderr, "out of memory\n"); exit(2); }
        u->name = dup_str(name);
        u->next = used;
        used = u;
    }
    char buf[256];
    /* first choice: the plain name; otherwise name_k with the first k that nobody has used yet */
    if (u->count == 0 && !looks_generated(name) && !is_taken(name)) snprintf(buf, sizeof buf, "%s", name);
    else {
        do { snprintf(buf, sizeof buf, "%s_%d", name, u->count); u->count++; } while (is_taken(buf));
    }
    if (u->count == 0) u->count = 1;
    Taken *tk = calloc(1, sizeof(Taken));
    if (!tk) { fprintf(stderr, "out of memory\n"); exit(2); }
    tk->tac = dup_str(buf); tk->next = taken; taken = tk;

    Var *v = calloc(1, sizeof(Var));
    if (!v) { fprintf(stderr, "out of memory\n"); exit(2); }
    v->name = dup_str(name);
    v->tac  = dup_str(buf);
    v->next = vars;
    vars = v;
    if (curfn) {                                   /* per-call storage: saved and restored around calls */
        char loc[300];
        snprintf(loc, sizeof loc, "%s%s", buf, is_array ? "[]" : "");
        add_str(&curfn->locals, &curfn->nlocals, loc);
    }
}
static void declare(const char *name) { declare_ex(name, 0); }

static const char *lookup(const char *name) {
    for (Var *v = vars; v; v = v->next)
        if (strcmp(v->name, name) == 0) return v->tac;
    return name;   /* unreachable after a clean semantic pass */
}

/* ---- output helpers --------------------------------------------------- */

static void emit(const char *fmt, ...) {
    char line[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(line, sizeof line - 6, fmt, ap);
    va_end(ap);
    put("    ");
    put(line);
    put("\n");
}

static void emit_label(int n) { char b[32]; snprintf(b, sizeof b, "L%d:\n", n); put(b); }

static char *new_temp(void) {
    char buf[32];
    snprintf(buf, sizeof buf, "t%d", ++temp_count);
    if (curfn) add_str(&curfn->locals, &curfn->nlocals, buf);   /* temporaries of a function are per-call too */
    return dup_str(buf);
}

static int new_label(void) { return ++label_count; }

/* 'a' converted to float if needed; returns the place to use */
static char *to_float(char *place) {
    char *t = new_temp();
    emit("%s = (float) %s", t, place);
    return t;
}

/* ---- expressions ------------------------------------------------------ */

static char *gen_expr(const Node *n);
static char *gen_call(const Node *n, int want_value);

static char *gen_expr(const Node *n) {
    char buf[64];
    switch (n->kind) {
    case N_INDEX: {
        char *idx = gen_expr(n->a);
        char *t = new_temp();
        emit("%s = %s[%s]", t, lookup(n->name), idx);
        return t;
    }
    case N_CALL:
        return gen_call(n, 1);

    case N_INT:
        snprintf(buf, sizeof buf, "%d", n->ival);
        return dup_str(buf);
    case N_FLOAT:
        snprintf(buf, sizeof buf, "%.15g", n->fval);                 /* shortest form that is still exact enough ... */
        if (strtod(buf, NULL) != n->fval) snprintf(buf, sizeof buf, "%.17g", n->fval);   /* ... otherwise full precision */
        if (!strpbrk(buf, ".eEn")) strcat(buf, ".0");   /* keep 2.0 looking like a float */
        return dup_str(buf);
    case N_BOOL:
        return dup_str(n->ival ? "true" : "false");
    case N_STR: {                                    /* printf text: kept as written, escapes are decoded when it is printed */
        char *q = malloc(strlen(n->name) + 3);
        if (!q) { fprintf(stderr, "out of memory\n"); exit(2); }
        sprintf(q, "\"%s\"", n->name);
        return q;
    }
    case N_ID:
        return dup_str(lookup(n->name));

    case N_TERNARY: {                                /* c ? x : y */
        char *c = gen_expr(n->a);
        char *res = new_temp();
        int lelse = new_label(), lend = new_label();
        emit("ifFalse %s goto L%d", c, lelse);
        char *x = gen_expr(n->b);
        if (n->type == T_FLOAT && n->b->type == T_INT) x = to_float(x);
        emit("%s = %s", res, x);
        emit("goto L%d", lend);
        emit_label(lelse);
        char *y = gen_expr(n->c);
        if (n->type == T_FLOAT && n->c->type == T_INT) y = to_float(y);
        emit("%s = %s", res, y);
        emit_label(lend);
        return res;
    }
    case N_LENGTH: {                                 /* Java: arr.length */
        char *t = new_temp();
        emit("%s = len %s", t, lookup(n->name));
        return t;
    }
    case N_UNOP: {
        char *a = gen_expr(n->a);
        if (n->op[0] == '(') {                       /* Java casts */
            if (n->type == n->a->type) return a;
            if (n->type == T_FLOAT) return to_float(a);
            char *t = new_temp();
            emit("%s = (int) %s", t, a);
            return t;
        }
        char *t = new_temp();
        emit("%s = %s %s", t, n->op, a);
        return t;
    }
    case N_BINOP: {
        /* && and || short-circuit: the right operand is only evaluated when
         * the left one does not already decide the result */
        if (strcmp(n->op, "&&") == 0 || strcmp(n->op, "||") == 0) {
            char *l = gen_expr(n->a);
            char *res = new_temp();
            int lend = new_label();
            emit("%s = %s", res, l);
            if (n->op[0] == '&') {
                emit("ifFalse %s goto L%d", res, lend);     /* left false -> result false */
            } else {
                int lright = new_label();                   /* only ifFalse exists in our TAC */
                emit("ifFalse %s goto L%d", res, lright);
                emit("goto L%d", lend);                     /* left true -> result true */
                emit_label(lright);
            }
            char *r = gen_expr(n->b);
            emit("%s = %s", res, r);
            emit_label(lend);
            return res;
        }
        char *l = gen_expr(n->a);
        char *r = gen_expr(n->b);
        /* mixed int/float arithmetic or comparison: widen the int side */
        if (n->a->type == T_INT && n->b->type == T_FLOAT) l = to_float(l);
        else if (n->a->type == T_FLOAT && n->b->type == T_INT) r = to_float(r);
        char *t = new_temp();
        emit("%s = %s %s %s", t, l, n->op, r);
        return t;
    }
    default:
        return dup_str("?");
    }
}

static const Node *find_func_node(const char *name) {
    const Node *proto = NULL;
    for (const Node *n = root_node ? root_node->a : NULL; n; n = n->next)
        if (n->kind == N_FUNC && strcmp(n->name, name) == 0) { if (n->b) return n; proto = n; }
    return proto;
}

/* f(a, b): arguments are evaluated first, then pushed with 'param', then 'call' */
static char *gen_call(const Node *n, int want_value) {
    const Node *f = find_func_node(n->name);
    const Node *p = f ? f->a : NULL;
    int cnt = 0;
    char *places[64]; int arrs[64];
    for (const Node *a = n->a; a && cnt < 64; a = a->next, cnt++, p = p ? p->next : NULL) {
        if (p && p->is_array) { places[cnt] = dup_str(lookup(a->name)); arrs[cnt] = 1; continue; }
        char *v = gen_expr(a);
        if (p && p->type == T_FLOAT && a->type == T_INT) v = to_float(v);
        places[cnt] = v; arrs[cnt] = 0;
    }
    for (int i = 0; i < cnt; i++) emit(arrs[i] ? "paramarr %s" : "param %s", places[i]);
    if (want_value) {
        char *t = new_temp();
        emit("%s = call %s, %d", t, n->name, cnt);
        return t;
    }
    emit("call %s, %d", n->name, cnt);
    return NULL;
}

/* does this statement contain a 'continue' that belongs to the loop around it? */
static int has_continue(const Node *n) {
    for (; n; n = n->next) {
        switch (n->kind) {
        case N_CONTINUE: return 1;
        case N_IF:       if (has_continue(n->b) || has_continue(n->c)) return 1; break;
        case N_BLOCK:    if (has_continue(n->a)) return 1; break;
        case N_SWITCH:   for (const Node *c = n->b; c; c = c->next) if (has_continue(c->b)) return 1; break;
        default: break;                  /* nested loops own their continues */
        }
    }
    return 0;
}

/* ---- statements ------------------------------------------------------- */

static void gen_stmts(const Node *n);
static void gen_function(const Node *n);

/* body of if/while: a block opens its own scope; a lone statement gets one too */
static void gen_body(const Node *n) {
    if (!n) return;
    Var *mark = vars;
    gen_stmts(n);
    vars = mark;
}

static void gen_stmts(const Node *n) {
    for (; n; n = n->next) {
        switch (n->kind) {
        case N_DECL:
            if (n->is_array) {
                declare_ex(n->name, 1);
                const char *arr = lookup(n->name);
                emit("array %s %d%s", arr, n->size, n->type == T_FLOAT ? " float" : n->type == T_BOOL ? " bool" : "");
                int i = 0;
                for (const Node *e = n->b; e; e = e->next, i++) {
                    char *v = gen_expr(e);
                    if (n->type == T_FLOAT && e->type == T_INT) v = to_float(v);
                    emit("%s[%d] = %s", arr, i, v);
                }
            } else if (n->a) {
                /* evaluate the initializer first: "int x = x + 1;" in an inner
                 * scope must read the OUTER x, then declare the new one */
                char *v = gen_expr(n->a);
                if (n->type == T_FLOAT && n->a->type == T_INT) v = to_float(v);
                declare(n->name);
                emit("%s = %s", lookup(n->name), v);
            } else {
                declare(n->name);      /* no instruction: just bookkeeping */
            }
            break;

        case N_ASSIGN: {
            char *v = gen_expr(n->a);
            if (n->type == T_FLOAT && n->a->type == T_INT) v = to_float(v);
            if (n->b) {
                char *idx = gen_expr(n->b);
                emit("%s[%s] = %s", lookup(n->name), idx, v);
            } else {
                emit("%s = %s", lookup(n->name), v);
            }
            break;
        }
        case N_SCAN: {
            char spec = n->ival == 'c' || n->ival == 'C' ? (char)n->ival : (n->type == T_FLOAT ? 'f' : 'd');
            if (n->b) {
                char *t = new_temp();
                emit("scan %s, %c", t, spec);
                char *idx = gen_expr(n->b);
                emit("%s[%s] = %s", lookup(n->name), idx, t);
            } else {
                emit("scan %s, %c", lookup(n->name), spec);
            }
            break;
        }
        case N_CALL:
            gen_call(n, 0);
            break;
        case N_RETURN:
            if (n->a) {
                char *v = gen_expr(n->a);
                if (curfn && curfn->node->type == T_FLOAT && n->a->type == T_INT) v = to_float(v);
                emit("return %s", v);
            } else {
                emit("return");
            }
            break;
        case N_BREAK:
            if (loop_sp) emit("goto L%d", brk_lab[loop_sp - 1]);
            break;
        case N_CONTINUE:
            if (loop_sp) emit("goto L%d", cont_lab[loop_sp - 1]);
            break;
        case N_FUNC:
            gen_function(n);
            break;
        case N_PRINT: {
            char *v = gen_expr(n->a);
            if (n->name && strcmp(n->name, "%j") == 0)            /* Java println: format chosen by type */
                emit("print %s, \"%s\"", v, n->a->type == T_FLOAT ? "%J" : n->a->type == T_BOOL ? "%s" : "%d");
            else if (n->name && strncmp(n->name, "%k", 2) == 0)  /* C++ cout: float format, %d for int / bool */
                emit("print %s, \"%s\"", v, n->a->type == T_FLOAT ? n->name + 2 : "%d");
            else if (n->name) emit("print %s, \"%s\"", v, n->name);     /* printf conversion such as %.2f */
            else         emit("print %s", v);
            break;
        }
        case N_IF: {
            char *c = gen_expr(n->a);
            if (!n->c) {
                int lend = new_label();
                emit("ifFalse %s goto L%d", c, lend);
                gen_body(n->b);
                emit_label(lend);
            } else {
                int lelse = new_label(), lend = new_label();
                emit("ifFalse %s goto L%d", c, lelse);
                gen_body(n->b);
                emit("goto L%d", lend);
                emit_label(lelse);
                gen_body(n->c);
                emit_label(lend);
            }
            break;
        }
        case N_WHILE: {
            int lstart = new_label(), lend = new_label();
            /* 'continue' goes to the loop test, or to the for-step when there is one */
            int lcont = lstart, need_cont = n->c && has_continue(n->b);
            if (need_cont) lcont = new_label();
            emit_label(lstart);
            char *c = gen_expr(n->a);
            emit("ifFalse %s goto L%d", c, lend);
            if (loop_sp < MAX_LOOPS) { brk_lab[loop_sp] = lend; cont_lab[loop_sp] = lcont; loop_sp++; }
            gen_body(n->b);
            if (loop_sp) loop_sp--;
            if (need_cont) emit_label(lcont);
            if (n->c) { Var *mark = vars; gen_stmts(n->c); vars = mark; }
            emit("goto L%d", lstart);
            emit_label(lend);
            break;
        }
        case N_SWITCH: {
            /* test each case in order, jump to its body; bodies follow each other (C fall-through) */
            char *v = gen_expr(n->a);
            if (n->a->kind != N_ID && n->a->kind != N_INT) { char *t = new_temp(); emit("%s = %s", t, v); v = t; }
            int ncase = 0;
            for (const Node *c = n->b; c; c = c->next) ncase++;
            int *lab = calloc((size_t)ncase + 1, sizeof *lab);
            if (!lab) { fprintf(stderr, "out of memory\n"); exit(2); }
            int k = 0, ldef = 0, lend;
            for (const Node *c = n->b; c; c = c->next, k++) lab[k] = new_label();
            lend = new_label();
            k = 0;
            for (const Node *c = n->b; c; c = c->next, k++) {
                if (!c->a) { ldef = lab[k]; continue; }
                char *t = new_temp();
                emit("%s = %s != %d", t, v, c->a->ival);
                emit("ifFalse %s goto L%d", t, lab[k]);       /* not (v != c)  ==  v == c */
            }
            emit("goto L%d", ldef ? ldef : lend);
            /* break leaves the switch; continue still belongs to the loop around it */
            int outer_cont = loop_sp ? cont_lab[loop_sp - 1] : lend;
            if (loop_sp < MAX_LOOPS) { brk_lab[loop_sp] = lend; cont_lab[loop_sp] = outer_cont; loop_sp++; }
            Var *mark = vars;
            k = 0;
            for (const Node *c = n->b; c; c = c->next, k++) { emit_label(lab[k]); gen_stmts(c->b); }
            vars = mark;
            if (loop_sp) loop_sp--;
            emit_label(lend);
            free(lab);
            break;
        }
        case N_DOWHILE: {
            int lstart = new_label(), lend = new_label();
            int lcont = lstart, need_cont = has_continue(n->b);
            if (need_cont) lcont = new_label();
            emit_label(lstart);
            if (loop_sp < MAX_LOOPS) { brk_lab[loop_sp] = lend; cont_lab[loop_sp] = lcont; loop_sp++; }
            gen_body(n->b);
            if (loop_sp) loop_sp--;
            if (need_cont) emit_label(lcont);
            char *c = gen_expr(n->a);
            emit("ifFalse %s goto L%d", c, lend);
            emit("goto L%d", lstart);
            emit_label(lend);
            break;
        }
        case N_BLOCK: {
            Var *mark = vars;
            gen_stmts(n->a);
            vars = mark;
            break;
        }
        default:
            break;
        }
    }
}

/* function definition: generated into its own buffer, printed after the main program */
static void gen_function(const Node *n) {
    if (!n->b || curfn) return;                       /* prototype (or illegal nesting, rejected by semantic) */
    Fn *f = calloc(1, sizeof *f);
    if (!f) { fprintf(stderr, "out of memory\n"); exit(2); }
    f->node = n;
    if (fn_tail) fn_tail->next = f; else fn_list = f;
    fn_tail = f;

    Var *mark = vars;
    int saved_sp = loop_sp;
    curfn = f; loop_sp = 0;
    for (const Node *p = n->a; p; p = p->next) {
        declare_ex(p->name, p->is_array);
        char pn[300];
        snprintf(pn, sizeof pn, "%s%s", lookup(p->name), p->is_array ? "[]" : "");
        add_str(&f->params, &f->nparams, pn);
    }
    gen_stmts(n->b->a);
    curfn = NULL; loop_sp = saved_sp;
    vars = mark;
}

static void print_functions(void) {
    if (!fn_list) return;
    emit("return");                                   /* end of the main program: functions follow */
    for (Fn *f = fn_list; f; f = f->next) {
        fprintf(out, "func %s(", f->node->name);
        for (int i = 0; i < f->nparams; i++) fprintf(out, "%s%s", i ? ", " : "", f->params[i]);
        fprintf(out, ") locals(");
        for (int i = 0; i < f->nlocals; i++) fprintf(out, "%s%s", i ? ", " : "", f->locals[i]);
        fprintf(out, ")\n");
        if (f->buf) fputs(f->buf, out);
        fprintf(out, "endfunc\n");
    }
}

void tac_generate(const Node *root, FILE *f) {
    out = f;
    temp_count = label_count = 0;
    vars = NULL;
    used = NULL;
    taken = NULL;
    fn_list = fn_tail = NULL; curfn = NULL; loop_sp = 0;
    root_node = root;
    if (root) gen_stmts(root->a);
    print_functions();
}
