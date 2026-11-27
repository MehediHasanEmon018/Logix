/* Logix semantic analyzer
 * Detects: undeclared variable / function, redeclaration, scope violation,
 *          type mismatch, invalid assignment, invalid expression,
 *          wrong argument count / type in calls, bad return, bad array use,
 *          break / continue outside a loop, scanf format problems. */
#include <stdio.h>
#include <stdlib.h>
#include <stdarg.h>
#include <string.h>
#include "semantic/semantic.h"
#include "symtab/symtab.h"

static int errors = 0;

static void err(int line, const char *fmt, ...) {
    va_list ap;
    fprintf(stderr, "Semantic error at line %d: ", line);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    errors++;
}

static int is_num(Type t) { return t == T_INT || t == T_FLOAT; }
static int is(const char *a, const char *b) { return strcmp(a, b) == 0; }
/* a value of type 'from' may be stored in something of type 'to' (int widens to float) */
static int assignable(Type to, Type from) { return to == from || (to == T_FLOAT && from == T_INT); }

/* ---- function table (functions can be called before their definition) ---- */

typedef struct Func {
    char *name; Type ret; int line;
    int nparams; Type *pt; int *pa;
    int defined;
    struct Func *next;
} Func;
static Func *funcs;

static Func *find_func(const char *name) {
    for (Func *f = funcs; f; f = f->next) if (is(f->name, name)) return f;
    return NULL;
}

static int count_list(const Node *n) { int c = 0; for (; n; n = n->next) c++; return c; }

static void register_func(Node *n) {
    Func *old = find_func(n->name);
    int np = count_list(n->a);
    if (old) {
        if (n->b && old->defined) err(n->line, "redefinition of function '%s' (first defined at line %d)", n->name, old->line);
        else if (old->nparams != np || old->ret != n->type) err(n->line, "conflicting declaration of function '%s'", n->name);
        if (n->b) { old->defined = 1; old->line = n->line; }
        return;
    }
    Func *f = calloc(1, sizeof *f);
    if (!f) { fprintf(stderr, "out of memory\n"); exit(2); }
    f->name = n->name; f->ret = n->type; f->line = n->line; f->nparams = np; f->defined = n->b != NULL;
    f->pt = calloc(np ? np : 1, sizeof(Type)); f->pa = calloc(np ? np : 1, sizeof(int));
    int i = 0;
    char sig[96]; snprintf(sig, sizeof sig, "%s(", type_name(n->type));
    for (Node *p = n->a; p; p = p->next, i++) {
        f->pt[i] = p->type; f->pa[i] = p->is_array;
        strncat(sig, i ? "," : "", sizeof sig - strlen(sig) - 1);
        strncat(sig, type_name(p->type), sizeof sig - strlen(sig) - 1);
        if (p->is_array) strncat(sig, "[]", sizeof sig - strlen(sig) - 1);
    }
    strncat(sig, ")", sizeof sig - strlen(sig) - 1);
    f->next = funcs; funcs = f;
    Symbol *s = st_insert(n->name, n->type, n->line);   /* global scope: functions show up in --symtab */
    s->is_func = 1; s->sig = strdup(sig);
}

/* undeclared variable vs. scope violation */
static void report_unknown(const char *name, int line) {
    Symbol *dead = st_find_inactive(name);
    if (dead)
        err(line, "scope violation: '%s' (declared at line %d) is not visible here", name, dead->line);
    else
        err(line, "undeclared variable '%s'", name);
}

static Type check_expr(Node *n);

/* name must be a declared variable; returns NULL after reporting */
static Symbol *lookup_var(const char *name, int line) {
    Symbol *s = st_lookup(name);
    if (!s) { report_unknown(name, line); return NULL; }
    if (s->is_func) { err(line, "'%s' is a function, not a variable", name); return NULL; }
    return s;
}

static void check_index(Node *idx) {
    Type it = check_expr(idx);
    if (it != T_NONE && it != T_INT)
        err(idx->line, "array index must be int, got '%s'", type_name(it));
}

/* built-in math functions (<math.h> / <stdlib.h>, Java Math.*): user functions with the same name win */
typedef struct { const char *name; int nargs; int ret; } Builtin;     /* ret: 'f' float, 'a' like the arguments */
static const Builtin builtins[] = {
    { "sqrt", 1, 'f' }, { "pow", 2, 'f' }, { "fabs", 1, 'f' }, { "floor", 1, 'f' }, { "ceil", 1, 'f' },
    { "abs", 1, 'a' }, { "max", 2, 'a' }, { "min", 2, 'a' },
};
static Type check_builtin(Node *n, const Builtin *b, int given) {
    if (given != b->nargs) {
        err(n->line, "function '%s' expects %d argument(s), got %d", n->name, b->nargs, given);
        return n->type = T_NONE;
    }
    Type res = T_INT; int bad = 0;
    for (Node *a = n->a; a; a = a->next) {
        Type t = check_expr(a);
        if (t == T_NONE) { bad = 1; continue; }
        if (!is_num(t)) { err(a->line, "type mismatch: '%s' needs a number, got '%s'", n->name, type_name(t)); bad = 1; }
        if (t == T_FLOAT) res = T_FLOAT;
    }
    if (bad) return n->type = T_NONE;
    return n->type = (b->ret == 'f') ? T_FLOAT : res;
}

/* a call used as a value (want_value) or as a statement */
static Type check_call(Node *n, int want_value) {
    Func *f = find_func(n->name);
    int given = count_list(n->a);
    if (!f)
        for (size_t i = 0; i < sizeof builtins / sizeof builtins[0]; i++)
            if (is(builtins[i].name, n->name)) return check_builtin(n, &builtins[i], given);
    if (!f) {
        err(n->line, "undeclared function '%s'", n->name);
        for (Node *a = n->a; a; a = a->next) if (!(a->kind == N_ID && st_lookup(a->name) && st_lookup(a->name)->is_array)) check_expr(a);
        return T_NONE;
    }
    if (given != f->nparams)
        err(n->line, "function '%s' expects %d argument(s), got %d", n->name, f->nparams, given);
    int i = 0;
    for (Node *a = n->a; a; a = a->next, i++) {
        int want_arr = (i < f->nparams) && f->pa[i];
        if (a->kind == N_ID) {
            Symbol *s = st_lookup(a->name);
            if (s && s->is_array) {
                a->type = s->type;
                if (i < f->nparams && !want_arr)
                    err(a->line, "argument %d of '%s' is the array '%s' but a single value is expected", i + 1, n->name, a->name);
                else if (want_arr && s->type != f->pt[i])
                    err(a->line, "type mismatch: argument %d of '%s' expects '%s[]', got '%s[]'", i + 1, n->name, type_name(f->pt[i]), type_name(s->type));
                continue;
            }
        }
        Type at = check_expr(a);
        if (i >= f->nparams || at == T_NONE) continue;
        if (want_arr)
            err(a->line, "argument %d of '%s' must be an array of '%s'", i + 1, n->name, type_name(f->pt[i]));
        else if (!assignable(f->pt[i], at))
            err(a->line, "type mismatch: argument %d of '%s' expects '%s', got '%s'", i + 1, n->name, type_name(f->pt[i]), type_name(at));
    }
    if (want_value && f->ret == T_VOID) {
        err(n->line, "function '%s' returns void and cannot be used as a value", n->name);
        return n->type = T_NONE;
    }
    return n->type = f->ret;
}

static Type check_expr(Node *n) {
    switch (n->kind) {
    case N_INT:   return n->type = T_INT;
    case N_FLOAT: return n->type = T_FLOAT;
    case N_BOOL:  return n->type = T_BOOL;
    case N_STR:                                 /* only legal inside print (N_PRINT skips this check) */
        err(n->line, "a string can only be printed here (String variables / operations are not supported)");
        return n->type = T_NONE;

    case N_TERNARY: {                           /* c ? x : y */
        Type c = check_expr(n->a), x = check_expr(n->b), y = check_expr(n->c);
        if (c != T_NONE && c != T_BOOL) err(n->line, "condition of '?:' must be bool, got '%s'", type_name(c));
        if (x == T_NONE || y == T_NONE) return n->type = T_NONE;
        if (is_num(x) && is_num(y)) return n->type = (x == T_FLOAT || y == T_FLOAT) ? T_FLOAT : T_INT;
        if (x == y && x != T_VOID) return n->type = x;
        err(n->line, "type mismatch: '?:' branches are '%s' and '%s'", type_name(x), type_name(y));
        return n->type = T_NONE;
    }
    case N_LENGTH: {                            /* Java: arr.length */
        Symbol *s = lookup_var(n->name, n->line);
        if (!s) return n->type = T_NONE;
        if (!s->is_array) { err(n->line, "'%s' is not an array, it has no .length", n->name); return n->type = T_NONE; }
        return n->type = T_INT;
    }

    case N_ID: {
        Symbol *s = lookup_var(n->name, n->line);
        if (!s) return n->type = T_NONE;
        if (s->is_array) {
            err(n->line, "array '%s' cannot be used as a value (index it, or pass it to a function)", n->name);
            return n->type = T_NONE;
        }
        return n->type = s->type;
    }

    case N_INDEX: {
        Symbol *s = lookup_var(n->name, n->line);
        check_index(n->a);
        if (!s) return n->type = T_NONE;
        if (!s->is_array) { err(n->line, "'%s' is not an array", n->name); return n->type = T_NONE; }
        return n->type = s->type;
    }

    case N_CALL:
        return check_call(n, 1);

    case N_UNOP: {
        Type t = check_expr(n->a);
        if (t == T_NONE) return n->type = T_NONE;      /* already reported */
        if (n->op[0] == '(') {                          /* Java cast: (int) x, (double) x */
            if (!is_num(t)) {
                err(n->line, "invalid expression: cannot cast '%s' to %s", type_name(t), n->op);
                return n->type = T_NONE;
            }
            return n->type = is(n->op, "(float)") ? T_FLOAT : T_INT;
        }
        if (is(n->op, "-")) {                           /* unary minus */
            if (!is_num(t)) {
                err(n->line, "invalid expression: operator '-' needs a numeric operand, got '%s'", type_name(t));
                return n->type = T_NONE;
            }
            return n->type = t;
        }
        if (t != T_BOOL) {
            err(n->line, "invalid expression: operator '%s' needs a bool operand, got '%s'", n->op, type_name(t));
            return n->type = T_NONE;
        }
        return n->type = T_BOOL;
    }

    case N_BINOP: {
        Type l = check_expr(n->a), r = check_expr(n->b);
        if (l == T_NONE || r == T_NONE) return n->type = T_NONE;   /* avoid cascades */
        const char *op = n->op;

        if (is(op, "+") || is(op, "-") || is(op, "*") || is(op, "/")) {
            if (!is_num(l) || !is_num(r)) {
                err(n->line, "invalid expression: operator '%s' needs numeric operands, got '%s' and '%s'",
                    op, type_name(l), type_name(r));
                return n->type = T_NONE;
            }
            return n->type = (l == T_FLOAT || r == T_FLOAT) ? T_FLOAT : T_INT;
        }
        if (is(op, "%")) {
            if (l != T_INT || r != T_INT) {
                err(n->line, "invalid expression: operator '%%' needs int operands, got '%s' and '%s'",
                    type_name(l), type_name(r));
                return n->type = T_NONE;
            }
            return n->type = T_INT;
        }
        if (is(op, "<") || is(op, ">") || is(op, "<=") || is(op, ">=")) {
            if (!is_num(l) || !is_num(r)) {
                err(n->line, "invalid expression: operator '%s' needs numeric operands, got '%s' and '%s'",
                    op, type_name(l), type_name(r));
                return n->type = T_NONE;
            }
            return n->type = T_BOOL;
        }
        if (is(op, "==") || is(op, "!=")) {
            if ((is_num(l) && is_num(r)) || (l == T_BOOL && r == T_BOOL))
                return n->type = T_BOOL;
            err(n->line, "type mismatch: cannot compare '%s' with '%s'", type_name(l), type_name(r));
            return n->type = T_NONE;
        }
        /* && || */
        if (l != T_BOOL || r != T_BOOL) {
            err(n->line, "invalid expression: operator '%s' needs bool operands, got '%s' and '%s'",
                op, type_name(l), type_name(r));
            return n->type = T_NONE;
        }
        return n->type = T_BOOL;
    }
    default:
        return T_NONE;
    }
}

static void check_stmts(Node *n);

static int switch_depth = 0;   /* > 0 inside switch: break is legal */
static int loop_depth = 0;     /* > 0 inside while / for / do-while: break and continue are legal */
static int nest = 0;           /* > 0 inside a block or function body: nested function definitions are illegal */
static Type cur_ret = T_NONE;  /* return type of the function being checked (T_NONE: top level / main) */

/* body of if/while: a lone statement gets its own scope; a block opens one itself */
static void check_body(Node *n) {
    if (!n) return;
    if (n->kind == N_BLOCK) { check_stmts(n); return; }
    st_enter();
    nest++;
    check_stmts(n);
    nest--;
    st_exit();
}

static void check_condition(Node *cond, const char *what) {
    Type t = check_expr(cond);
    if (t != T_NONE && t != T_BOOL)
        err(cond->line, "condition of '%s' must be bool, got '%s'", what, type_name(t));
}

static void check_decl(Node *n) {
    if (n->is_array) {
        int count = count_list(n->b);
        if (n->size == 0 && n->b) n->size = count;                /* int a[] = {1, 2, 3}; */
        if (n->size <= 0) err(n->line, "array '%s' must have a positive size", n->name);
        else if (count > n->size) err(n->line, "too many initializers for array '%s' (size %d, got %d)", n->name, n->size, count);
        for (Node *e = n->b; e; e = e->next) {
            Type et = check_expr(e);
            if (et != T_NONE && !assignable(n->type, et))
                err(e->line, "type mismatch: cannot initialize '%s' array '%s' with '%s'", type_name(n->type), n->name, type_name(et));
        }
        Symbol *old = st_lookup_current(n->name);
        if (old) err(n->line, "redeclaration of '%s' (first declared at line %d)", n->name, old->line);
        else { Symbol *s = st_insert(n->name, n->type, n->line); s->is_array = 1; s->size = n->size; }
        return;
    }
    /* initializer is checked BEFORE the name is inserted, so
     * "int x = x;" correctly reports x as undeclared */
    Type it = n->a ? check_expr(n->a) : T_NONE;
    Symbol *old = st_lookup_current(n->name);
    if (old)
        err(n->line, "redeclaration of '%s' (first declared at line %d)", n->name, old->line);
    else
        st_insert(n->name, n->type, n->line);
    if (n->a && it != T_NONE && !assignable(n->type, it))
        err(n->line, "type mismatch: cannot initialize '%s' variable '%s' with '%s'",
            type_name(n->type), n->name, type_name(it));
}

static void check_scan(Node *n) {
    Symbol *s = lookup_var(n->name, n->line);
    if (n->b) check_index(n->b);
    if (!s) return;
    if (n->b && !s->is_array) { err(n->line, "'%s' is not an array", n->name); return; }
    if (!n->b && s->is_array) { err(n->line, "scanf needs an element of array '%s', e.g. &%s[i]", n->name, n->name); return; }
    n->type = s->type;
    if (n->ival == 0)   { err(n->line, "scanf: no format specifier for argument '%s'", n->name); return; }
    if (n->ival == '?') { err(n->line, "scanf: unsupported format for '%s' (use %%d, %%f or %%c)", n->name); return; }
    if (s->type == T_BOOL) { err(n->line, "scanf cannot read into bool variable '%s'", n->name); return; }
    if (n->ival == 'd' && s->type != T_INT)
        err(n->line, "scanf: format '%%d' does not match '%s' variable '%s' (use %%f)", type_name(s->type), n->name);
    else if (n->ival == 'f' && s->type != T_FLOAT)
        err(n->line, "scanf: format '%%f' does not match '%s' variable '%s' (use %%d)", type_name(s->type), n->name);
    else if ((n->ival == 'c' || n->ival == 'C') && s->type != T_INT)
        err(n->line, "scanf: format '%%c' needs an int/char variable, '%s' is '%s'", n->name, type_name(s->type));
}

static void check_func(Node *f) {
    if (!f->b) return;                                    /* prototype: nothing to check */
    Type saved_ret = cur_ret; int saved_loop = loop_depth;
    int saved_switch = switch_depth;
    cur_ret = f->type; loop_depth = 0; switch_depth = 0;
    st_enter(); nest++;
    for (Node *p = f->a; p; p = p->next) {
        if (st_lookup_current(p->name))
            err(p->line, "duplicate parameter name '%s' in function '%s'", p->name, f->name);
        else { Symbol *s = st_insert(p->name, p->type, p->line); s->is_array = p->is_array; }
    }
    check_stmts(f->b->a);                                 /* body shares the parameter scope, like C */
    nest--; st_exit();
    cur_ret = saved_ret; loop_depth = saved_loop; switch_depth = saved_switch;
}

static void check_stmts(Node *n) {
    for (; n; n = n->next) {
        switch (n->kind) {
        case N_DECL:
            check_decl(n);
            break;
        case N_ASSIGN: {
            Symbol *s = lookup_var(n->name, n->line);
            if (n->b) check_index(n->b);
            if (s && n->b && !s->is_array) { err(n->line, "'%s' is not an array", n->name); s = NULL; }
            else if (s && !n->b && s->is_array) { err(n->line, "cannot assign to the whole array '%s' (use %s[i] = ...)", n->name, n->name); s = NULL; }
            Type rt = check_expr(n->a);
            if (s && rt != T_NONE) {
                n->type = s->type;
                if (!assignable(s->type, rt))
                    err(n->line, "invalid assignment: cannot assign '%s' to '%s' %s '%s'",
                        type_name(rt), type_name(s->type), n->b ? "array element" : "variable", n->name);
            }
            break;
        }
        case N_SCAN:
            check_scan(n);
            break;
        case N_PRINT:
            if (n->a->kind != N_STR) check_expr(n->a);
            break;
        case N_CALL:
            check_call(n, 0);
            break;
        case N_RETURN:
            if (cur_ret == T_VOID) {
                if (n->a) { check_expr(n->a); err(n->line, "a void function cannot return a value"); }
            } else if (cur_ret != T_NONE) {
                if (!n->a) err(n->line, "this function must return a '%s' value", type_name(cur_ret));
                else {
                    Type rt = check_expr(n->a);
                    if (rt != T_NONE && !assignable(cur_ret, rt))
                        err(n->line, "type mismatch: cannot return '%s' from a function returning '%s'", type_name(rt), type_name(cur_ret));
                }
            } else if (n->a) check_expr(n->a);
            break;
        case N_IF:
            check_condition(n->a, "if");
            check_body(n->b);
            check_body(n->c);
            break;
        case N_WHILE:
            check_condition(n->a, "while");
            loop_depth++;
            check_body(n->b);
            loop_depth--;
            if (n->c) check_stmts(n->c);                    /* for-loop step */
            break;
        case N_DOWHILE:
            loop_depth++;
            check_body(n->b);
            loop_depth--;
            check_condition(n->a, "do-while");
            break;
        case N_BREAK:
            if (!loop_depth && !switch_depth) err(n->line, "'break' outside of a loop or switch");
            break;
        case N_CONTINUE:
            if (!loop_depth) err(n->line, "'continue' outside of a loop");
            break;
        case N_SWITCH: {
            Type t = check_expr(n->a);
            if (t != T_NONE && t != T_INT) err(n->line, "switch value must be int (or char), got '%s'", type_name(t));
            int defaults = 0;
            switch_depth++;
            st_enter();                                 /* all cases share one scope, like C */
            for (Node *c = n->b; c; c = c->next) {
                if (!c->a) { if (++defaults > 1) err(c->line, "more than one 'default' in a switch"); }
                else if (c->a->kind != N_INT) { check_expr(c->a); err(c->line, "case label must be an integer or char constant"); }
                else {
                    c->a->type = T_INT;
                    for (Node *d = n->b; d != c; d = d->next)
                        if (d->a && d->a->kind == N_INT && d->a->ival == c->a->ival)
                            err(c->line, "duplicate case value %d", c->a->ival);
                }
                check_stmts(c->b);
            }
            st_exit();
            switch_depth--;
            break;
        }
        case N_BLOCK:
            st_enter();
            nest++;
            check_stmts(n->a);
            nest--;
            st_exit();
            break;
        case N_FUNC:
            if (nest) err(n->line, "function '%s' cannot be defined inside another block", n->name);
            else check_func(n);
            break;
        default:
            break;
        }
    }
}

int semantic_check(Node *root) {
    errors = 0; funcs = NULL; loop_depth = 0; switch_depth = 0; nest = 0; cur_ret = T_NONE;
    st_init();
    if (root) {
        /* pass 1: register every top-level function so calls may come before the definition */
        for (Node *n = root->a; n; n = n->next) if (n->kind == N_FUNC) register_func(n);
        for (Func *f = funcs; f; f = f->next)
            if (!f->defined) err(f->line, "function '%s' is declared but never defined", f->name);
        /* pass 2: statements and function bodies in source order */
        check_stmts(root->a);
    }
    return errors;
}
