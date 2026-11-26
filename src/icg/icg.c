/* Logix Three Address Code generator
 *
 * Strategy: a post-order walk over the AST.
 *   - gen_expr() emits the instructions for an expression and returns the
 *     "place" (variable name, literal, or fresh temporary tN) holding its value.
 *   - Control flow is lowered to labels (Ln:), "ifFalse c goto Ln" and "goto Ln".
 *   - Variables shadowed in nested scopes get unique TAC names (x, x_1, x_2 ...)
 *     so that the flat TAC never confuses an inner x with the outer x.
 *
 * C extensions (TAC forms):
 *     array a, 10, int          declare an array (size may be a variable)
 *     t1 = a[i]    a[i] = t1    element read / write
 *     x = read int|float|char   scanf
 *     print x, "%d"             printf conversion;  print "text"  prints text
 *     func f, p1, p2 ... endfunc   function body (params listed after the name)
 *     param x / t1 = call f, 2 / call f, 2 / return x
 *   Order: top-level code, then main's body (inlined), then the other functions.
 */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include <ctype.h>
#include "icg/icg.h"

static FILE *out;
static int   temp_count, label_count;
static const Node *cur_func;   /* function being generated (NULL: top level / main) */
static int   has_funcs;        /* program defines functions besides main */
static int   in_func_body;     /* inside a non-main function */

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

static void declare(const char *name) {
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
}

static const char *lookup(const char *name) {
    for (Var *v = vars; v; v = v->next)
        if (strcmp(v->name, name) == 0) return v->tac;
    return name;   /* unreachable after a clean semantic pass */
}

/* ---- output helpers --------------------------------------------------- */

static void emit(const char *fmt, ...) {
    va_list ap;
    fputs("    ", out);
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
    fputc('\n', out);
}

static void emit_label(int n) { fprintf(out, "L%d:\n", n); }

static char *new_temp(void) {
    char buf[32];
    snprintf(buf, sizeof buf, "t%d", ++temp_count);
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

static char *convert(char *v, Type to, Type from) {
    if (to == T_FLOAT && from == T_INT) return to_float(v);
    return v;
}

static char *quote(const char *text) {
    size_t n = strlen(text);
    char *q = malloc(n + 3);
    if (!q) { fprintf(stderr, "out of memory\n"); exit(2); }
    q[0] = '"'; memcpy(q + 1, text, n); q[n + 1] = '"'; q[n + 2] = 0;
    return q;
}

/* builtins take float arguments except abs */
static Type builtin_arg(const char *name) { return strcmp(name, "abs") == 0 ? T_INT : T_FLOAT; }

/* emits param/call; returns the temp holding the result (or NULL when want_value == 0) */
static char *gen_call(const Node *n, int want_value) {
    int nargs = 0;
    const Node *p = n->ref ? n->ref->a : NULL;
    char *places[64];
    for (const Node *a = n->a; a; a = a->next) {
        char *v;
        if (p && p->is_array) v = dup_str(lookup(a->name));          /* arrays are passed by reference */
        else {
            v = gen_expr(a);
            v = convert(v, p ? p->type : builtin_arg(n->name), a->type);
        }
        if (nargs < 64) places[nargs] = v;
        nargs++;
        if (p) p = p->next;
    }
    for (int i = 0; i < nargs && i < 64; i++) emit("param %s", places[i]);
    if (want_value && n->type != T_VOID) {
        char *t = new_temp();
        emit("%s = call %s, %d", t, n->name, nargs);
        return t;
    }
    emit("call %s, %d", n->name, nargs);
    return NULL;
}

static char *gen_expr(const Node *n) {
    char buf[64];
    switch (n->kind) {
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
    case N_STR:
        return quote(n->name);
    case N_ID:
        return dup_str(lookup(n->name));

    case N_INDEX: {
        char *i = gen_expr(n->a);
        char *t = new_temp();
        emit("%s = %s[%s]", t, lookup(n->name), i);
        return t;
    }
    case N_CALL: {
        char *t = gen_call(n, 1);
        return t ? t : dup_str("0");
    }
    case N_TERNARY: {
        char *c = gen_expr(n->a);
        char *res = new_temp();
        int lelse = new_label(), lend = new_label();
        emit("ifFalse %s goto L%d", c, lelse);
        char *x = convert(gen_expr(n->b), n->type, n->b->type);
        emit("%s = %s", res, x);
        emit("goto L%d", lend);
        emit_label(lelse);
        char *y = convert(gen_expr(n->c), n->type, n->c->type);
        emit("%s = %s", res, y);
        emit_label(lend);
        return res;
    }

    case N_UNOP: {
        char *a = gen_expr(n->a);
        if (n->op[0] == '(') {                         /* casts */
            if (n->type == n->a->type) return a;       /* (int) on an int: nothing to do */
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

/* ---- statements ------------------------------------------------------- */

/* break / continue targets of the innermost loop or switch */
typedef struct Jump { int brk; int cont; int is_loop; struct Jump *up; } Jump;
static Jump *jumps;

static void gen_stmts(const Node *n);

/* body of if/while: a block opens its own scope; a lone statement gets one too */
static void gen_body(const Node *n) {
    if (!n) return;
    Var *mark = vars;
    gen_stmts(n);
    vars = mark;
}

static const char *zero_of(Type t) { return t == T_FLOAT ? "0.0" : t == T_BOOL ? "false" : "0"; }

static void gen_decl(const Node *n) {
    if (n->is_array) {
        char *size;
        if (n->b) size = gen_expr(n->b);
        else {
            int count = 0;
            for (const Node *e = n->c; e; e = e->next) count++;
            char buf[32]; snprintf(buf, sizeof buf, "%d", count);
            size = dup_str(buf);
        }
        declare(n->name);
        emit("array %s, %s, %s", lookup(n->name), size, type_name(n->type));
        int k = 0;
        for (const Node *e = n->c; e; e = e->next, k++) {
            char *v = convert(gen_expr(e), n->type, e->type);
            emit("%s[%d] = %s", lookup(n->name), k, v);
        }
        return;
    }
    if (n->a) {
        /* evaluate the initializer first: "int x = x + 1;" in an inner
         * scope must read the OUTER x, then declare the new one */
        char *v = gen_expr(n->a);
        if (n->type == T_FLOAT && n->a->type == T_INT) v = to_float(v);
        declare(n->name);
        emit("%s = %s", lookup(n->name), v);
    } else {
        declare(n->name);      /* no instruction: just bookkeeping ... */
        /* ... except that, when functions exist, variables outside them are
         * given a value so the interpreter knows they are global (C zeroes globals) */
        if (has_funcs && !in_func_body) emit("%s = %s", lookup(n->name), zero_of(n->type));
    }
}

static void gen_stmts(const Node *n) {
    for (; n; n = n->next) {
        switch (n->kind) {
        case N_DECL:
            gen_decl(n);
            break;

        case N_ASSIGN: {
            if (n->b) {                               /* a[i] = v */
                char *i = gen_expr(n->b);
                char *v = gen_expr(n->a);
                if (n->type == T_FLOAT && n->a->type == T_INT) v = to_float(v);
                emit("%s[%s] = %s", lookup(n->name), i, v);
                break;
            }
            char *v = gen_expr(n->a);
            if (n->type == T_FLOAT && n->a->type == T_INT) v = to_float(v);
            emit("%s = %s", lookup(n->name), v);
            break;
        }
        case N_PRINT: {
            char *v = gen_expr(n->a);
            if (n->name && n->a->kind != N_STR) {     /* printf conversion, e.g. "%.2f" */
                char *spec = quote(n->name);
                emit("print %s, %s", v, spec);
            } else emit("print %s", v);
            break;
        }
        case N_SCAN:
            for (const Node *t = n->a; t; t = t->next) {
                const char *kind = (t->type == T_FLOAT) ? "float" : (t->op && t->op[0] == 'c') ? "char" : "int";
                if (t->kind == N_INDEX) {
                    char *i = gen_expr(t->a);
                    char *tmp = new_temp();
                    emit("%s = read %s", tmp, kind);
                    emit("%s[%s] = %s", lookup(t->name), i, tmp);
                } else emit("%s = read %s", lookup(t->name), kind);
            }
            break;
        case N_EXPR_STMT:
            if (n->a->kind == N_CALL) gen_call(n->a, 0);
            else gen_expr(n->a);
            break;
        case N_RETURN:
            if (in_func_body) {
                if (n->a && cur_func->type != T_VOID) {
                    char *v = convert(gen_expr(n->a), cur_func->type, n->a->type);
                    emit("return %s", v);
                } else emit("return");
            } else {
                if (n->a) gen_expr(n->a);             /* main / top level: stop the program */
                emit("return");
            }
            break;
        case N_BREAK:
            if (jumps) { if (!jumps->brk) jumps->brk = new_label(); emit("goto L%d", jumps->brk); }
            break;
        case N_CONTINUE: {
            Jump *j = jumps;
            while (j && !j->is_loop) j = j->up;
            if (j) { if (!j->cont) j->cont = new_label(); emit("goto L%d", j->cont); }
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
        case N_WHILE: {                               /* while, and for (step in ->c) */
            int lstart = new_label(), lend = new_label();
            Jump j = { lend, 0, 1, jumps };
            jumps = &j;
            emit_label(lstart);
            char *c = gen_expr(n->a);
            emit("ifFalse %s goto L%d", c, lend);
            gen_body(n->b);
            if (j.cont) emit_label(j.cont);           /* continue jumps here: run the step, re-test */
            if (n->c) gen_stmts(n->c);
            emit("goto L%d", lstart);
            emit_label(lend);
            jumps = j.up;
            break;
        }
        case N_DOWHILE: {
            int lstart = new_label();
            Jump j = { 0, 0, 1, jumps };
            jumps = &j;
            emit_label(lstart);
            gen_body(n->b);
            if (j.cont) emit_label(j.cont);
            char *c = gen_expr(n->a);
            if (!j.brk) j.brk = new_label();
            emit("ifFalse %s goto L%d", c, j.brk);
            emit("goto L%d", lstart);
            emit_label(j.brk);
            jumps = j.up;
            break;
        }
        case N_SWITCH: {
            char *v = gen_expr(n->a);
            if (n->a->kind != N_ID && n->a->kind != N_INT) {   /* keep the value stable */
                char *t = new_temp(); emit("%s = %s", t, v); v = t;
            }
            int ncase = 0;
            for (const Node *c = n->b; c; c = c->next) ncase++;
            int *lab = calloc((size_t)ncase + 1, sizeof *lab);
            if (!lab) { fprintf(stderr, "out of memory\n"); exit(2); }
            int k = 0, ldefault = 0;
            for (const Node *c = n->b; c; c = c->next, k++) lab[k] = new_label();
            Jump j = { new_label(), 0, 0, jumps };
            jumps = &j;
            k = 0;
            for (const Node *c = n->b; c; c = c->next, k++) {
                if (!c->a) { ldefault = lab[k]; continue; }
                char *cv = gen_expr(c->a);
                char *t = new_temp();
                emit("%s = %s != %s", t, v, cv);          /* ifFalse (v != c) == "if v == c" */
                emit("ifFalse %s goto L%d", t, lab[k]);
            }
            emit("goto L%d", ldefault ? ldefault : j.brk);
            Var *mark = vars;
            k = 0;
            for (const Node *c = n->b; c; c = c->next, k++) {   /* bodies in order: fall-through like C */
                emit_label(lab[k]);
                gen_stmts(c->b);
            }
            vars = mark;
            emit_label(j.brk);
            jumps = j.up;
            free(lab);
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

static void gen_function(const Node *f) {
    Var *mark = vars;
    cur_func = f;
    in_func_body = 1;
    char line[1024];
    size_t len = (size_t)snprintf(line, sizeof line, "func %s", f->name);
    for (const Node *p = f->a; p; p = p->next) {
        declare(p->name);
        if (len < sizeof line) len += (size_t)snprintf(line + len, sizeof line - len, ", %s", lookup(p->name));
    }
    fprintf(out, "%s\n", line);
    gen_stmts(f->b->a);
    fprintf(out, "endfunc\n");
    in_func_body = 0;
    cur_func = NULL;
    vars = mark;
}

void tac_generate(const Node *root, FILE *f) {
    out = f;
    temp_count = label_count = 0;
    vars = NULL;
    used = NULL;
    taken = NULL;
    jumps = NULL;
    cur_func = NULL;
    in_func_body = 0;
    has_funcs = 0;
    if (!root) return;

    const Node *main_fn = NULL;
    for (const Node *n = root->a; n; n = n->next)
        if (n->kind == N_FUNC && n->b) {
            if (strcmp(n->name, "main") == 0) main_fn = n;
            else has_funcs = 1;
        }

    /* 1. top-level statements (globals, Logix-style scripts) in source order */
    gen_stmts(root->a);                    /* (function definitions are skipped here) */
    /* 2. main's body runs next, inlined (a "return" in it stops the program) */
    if (main_fn) {
        cur_func = main_fn;
        Var *mark = vars;
        gen_stmts(main_fn->b->a);
        vars = mark;
        cur_func = NULL;
    }
    /* 3. the other functions; the interpreter skips over them unless called */
    for (const Node *n = root->a; n; n = n->next)
        if (n->kind == N_FUNC && n->b && n != main_fn) gen_function(n);
}
