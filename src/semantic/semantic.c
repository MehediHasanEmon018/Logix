/* Logix semantic analyzer
 * Detects: undeclared variable, redeclaration, scope violation,
 *          type mismatch, invalid assignment, invalid expression,
 *          and for the C extensions: wrong function calls (unknown function,
 *          argument count/type), bad returns, array misuse, break/continue
 *          outside a loop. */
#include <stdio.h>
#include <stdarg.h>
#include <string.h>
#include "semantic/semantic.h"
#include "symtab/symtab.h"

static int errors = 0;
static Node *cur_func = NULL;      /* function whose body is being checked (NULL at top level) */
static int loop_depth = 0, switch_depth = 0, block_depth = 0;

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
/* may a value of type 'from' be stored into 'to'?  (int -> float widening is allowed) */
static int assignable(Type to, Type from) { return to == from || (to == T_FLOAT && from == T_INT); }

/* built-in math functions (from <math.h> / <stdlib.h>) */
typedef struct { const char *name; Type ret; int nargs; Type arg; } Builtin;
static const Builtin builtins[] = {
    { "sqrt",  T_FLOAT, 1, T_FLOAT }, { "pow",  T_FLOAT, 2, T_FLOAT },
    { "fabs",  T_FLOAT, 1, T_FLOAT }, { "floor", T_FLOAT, 1, T_FLOAT },
    { "ceil",  T_FLOAT, 1, T_FLOAT }, { "abs",  T_INT,   1, T_INT   },
};
static const Builtin *find_builtin(const char *name) {
    for (size_t i = 0; i < sizeof builtins / sizeof builtins[0]; i++)
        if (is(builtins[i].name, name)) return &builtins[i];
    return NULL;
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

/* f(a, b, ...) */
static Type check_call(Node *n) {
    Symbol *s = st_lookup(n->name);
    int nargs = 0;
    for (Node *a = n->a; a; a = a->next) nargs++;

    if (!s || !s->is_func) {
        const Builtin *b = s ? NULL : find_builtin(n->name);
        if (!b) {
            if (s) err(n->line, "'%s' is not a function", n->name);
            else   err(n->line, "undeclared function '%s'", n->name);
            for (Node *a = n->a; a; a = a->next) check_expr(a);
            return n->type = T_NONE;
        }
        if (nargs != b->nargs) {
            err(n->line, "function '%s' expects %d argument(s), got %d", n->name, b->nargs, nargs);
            return n->type = T_NONE;
        }
        for (Node *a = n->a; a; a = a->next) {
            Type t = check_expr(a);
            if (t != T_NONE && !assignable(b->arg, t))
                err(a->line, "type mismatch: argument of '%s' must be '%s', got '%s'",
                    n->name, type_name(b->arg), type_name(t));
        }
        return n->type = b->ret;
    }

    Node *f = s->func;
    if (is(n->name, "main")) { err(n->line, "calling 'main' is not supported"); return n->type = T_NONE; }
    int nparams = 0;
    for (Node *p = f->a; p; p = p->next) nparams++;
    if (nargs != nparams) {
        err(n->line, "function '%s' expects %d argument(s), got %d", n->name, nparams, nargs);
        for (Node *a = n->a; a; a = a->next) check_expr(a);
        return n->type = T_NONE;
    }
    Node *p = f->a;
    int idx = 1;
    for (Node *a = n->a; a; a = a->next, p = p->next, idx++) {
        if (p->is_array) {
            Symbol *as = (a->kind == N_ID) ? st_lookup(a->name) : NULL;
            if (!as || !as->is_array) {
                if (a->kind == N_ID && !as) report_unknown(a->name, a->line);
                else err(a->line, "argument %d of '%s' must be an array", idx, n->name);
                continue;
            }
            a->type = as->type;
            if (as->type != p->type)
                err(a->line, "type mismatch: argument %d of '%s' must be a '%s' array, got a '%s' array",
                    idx, n->name, type_name(p->type), type_name(as->type));
            continue;
        }
        Type t = check_expr(a);
        if (t != T_NONE && !assignable(p->type, t))
            err(a->line, "type mismatch: argument %d of '%s' must be '%s', got '%s'",
                idx, n->name, type_name(p->type), type_name(t));
    }
    n->ref = f;
    return n->type = f->type;
}

static Type check_expr(Node *n) {
    switch (n->kind) {
    case N_INT:   return n->type = T_INT;
    case N_FLOAT: return n->type = T_FLOAT;
    case N_BOOL:  return n->type = T_BOOL;
    case N_STR:   return n->type = T_NONE;     /* only legal inside print */

    case N_ID: {
        Symbol *s = st_lookup(n->name);
        if (!s) { report_unknown(n->name, n->line); return n->type = T_NONE; }
        if (s->is_func)  { err(n->line, "function '%s' used as a variable", n->name); return n->type = T_NONE; }
        if (s->is_array) { err(n->line, "array '%s' used without an index", n->name); return n->type = T_NONE; }
        return n->type = s->type;
    }

    case N_INDEX: {
        Symbol *s = st_lookup(n->name);
        Type it = check_expr(n->a);
        if (!s) { report_unknown(n->name, n->line); return n->type = T_NONE; }
        if (!s->is_array) { err(n->line, "'%s' is not an array", n->name); return n->type = T_NONE; }
        if (it != T_NONE && it != T_INT)
            err(n->line, "array index must be 'int', got '%s'", type_name(it));
        return n->type = s->type;
    }

    case N_CALL:
        return check_call(n);

    case N_TERNARY: {
        Type c = check_expr(n->a), x = check_expr(n->b), y = check_expr(n->c);
        if (c != T_NONE && c != T_BOOL)
            err(n->line, "condition of '?:' must be bool, got '%s'", type_name(c));
        if (x == T_NONE || y == T_NONE) return n->type = T_NONE;
        if (is_num(x) && is_num(y)) return n->type = (x == T_FLOAT || y == T_FLOAT) ? T_FLOAT : T_INT;
        if (x == y && x != T_VOID) return n->type = x;
        err(n->line, "type mismatch: '?:' branches have types '%s' and '%s'", type_name(x), type_name(y));
        return n->type = T_NONE;
    }

    case N_UNOP: {
        Type t = check_expr(n->a);
        if (t == T_NONE) return n->type = T_NONE;      /* already reported */
        if (n->op[0] == '(') {                          /* cast: (int) x, (float) x */
            if (is(n->op, "(bool)") || !is_num(t)) {
                err(n->line, "invalid expression: cannot cast '%s' to '%s'", type_name(t), n->op);
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

/* body of if/while: a lone statement gets its own scope; a block opens one itself */
static void check_body(Node *n) {
    if (!n) return;
    if (n->kind == N_BLOCK) { check_stmts(n); return; }
    st_enter();
    check_stmts(n);
    st_exit();
}

static void check_loop_body(Node *n) {
    loop_depth++;
    check_body(n);
    loop_depth--;
}

static void check_condition(Node *cond, const char *what) {
    Type t = check_expr(cond);
    if (t != T_NONE && t != T_BOOL)
        err(cond->line, "condition of '%s' must be bool, got '%s'", what, type_name(t));
}

static void check_decl(Node *n) {
    if (n->is_array) {
        int count = 0;
        if (n->b) {
            Type st = check_expr(n->b);
            if (st != T_NONE && st != T_INT)
                err(n->line, "size of array '%s' must be 'int', got '%s'", n->name, type_name(st));
            else if (n->b->kind == N_INT && n->b->ival <= 0)
                err(n->line, "size of array '%s' must be positive", n->name);
        }
        for (Node *e = n->c; e; e = e->next) {
            count++;
            Type et = check_expr(e);
            if (et != T_NONE && !assignable(n->type, et))
                err(e->line, "type mismatch: cannot initialize element of '%s' array '%s' with '%s'",
                    type_name(n->type), n->name, type_name(et));
        }
        if (n->b && n->b->kind == N_INT && count > n->b->ival)
            err(n->line, "too many initializers for array '%s' (size %d, got %d)", n->name, n->b->ival, count);
    }
    /* initializer is checked BEFORE the name is inserted, so
     * "int x = x;" correctly reports x as undeclared */
    Type it = (!n->is_array && n->a) ? check_expr(n->a) : T_NONE;
    Symbol *old = st_lookup_current(n->name);
    if (old)
        err(n->line, "redeclaration of '%s' (first declared at line %d)", n->name, old->line);
    else {
        Symbol *s = st_insert(n->name, n->type, n->line);
        s->is_array = n->is_array;
    }
    if (!n->is_array && n->a && it != T_NONE && !assignable(n->type, it))
        err(n->line, "type mismatch: cannot initialize '%s' variable '%s' with '%s'",
            type_name(n->type), n->name, type_name(it));
}

static void check_assign(Node *n) {
    Symbol *s = st_lookup(n->name);
    if (!s) report_unknown(n->name, n->line);
    else if (s->is_func) { err(n->line, "cannot assign to function '%s'", n->name); s = NULL; }
    else if (n->b && !s->is_array) { err(n->line, "'%s' is not an array", n->name); s = NULL; }
    else if (!n->b && s->is_array) { err(n->line, "invalid assignment: cannot assign to array '%s' (use an index)", n->name); s = NULL; }
    if (n->b) {
        Type it = check_expr(n->b);
        if (it != T_NONE && it != T_INT) err(n->line, "array index must be 'int', got '%s'", type_name(it));
    }
    Type rt = check_expr(n->a);
    if (s && rt != T_NONE) {
        n->type = s->type;
        if (!assignable(s->type, rt))
            err(n->line, "invalid assignment: cannot assign '%s' to '%s' variable '%s'",
                type_name(rt), type_name(s->type), n->name);
    }
}

static void check_scan(Node *n) {
    for (Node *t = n->a; t; t = t->next) {
        Type ty = check_expr(t);
        if (ty == T_NONE) continue;
        if (ty == T_BOOL) err(t->line, "scanf cannot read into bool variable '%s'", t->name);
        else if (ty == T_FLOAT && t->op && t->op[0] != 'f')
            err(t->line, "type mismatch: scanf reads a whole number into float variable '%s' (use %%f)", t->name);
        else if (ty == T_INT && t->op && t->op[0] == 'f')
            err(t->line, "type mismatch: scanf reads a float into int variable '%s' (use %%d)", t->name);
    }
}

static void check_return(Node *n) {
    Type t = n->a ? check_expr(n->a) : T_VOID;
    if (!cur_func) return;                      /* top-level "return": just stops the program */
    if (cur_func->type == T_VOID) {
        if (n->a) err(n->line, "void function '%s' cannot return a value", cur_func->name);
        return;
    }
    if (!n->a) return;                          /* "return;" in a non-void function: allowed (like C) */
    if (t != T_NONE && !assignable(cur_func->type, t))
        err(n->line, "type mismatch: function '%s' returns '%s', got '%s'",
            cur_func->name, type_name(cur_func->type), type_name(t));
}

static void check_func(Node *n) {
    if (cur_func || block_depth > 0) {
        err(n->line, "function '%s' must be defined at the top level", n->name);
        return;
    }
    if (!n->b) return;                          /* prototype: registered by the pre-pass */
    cur_func = n;
    int saved_loop = loop_depth, saved_switch = switch_depth;
    loop_depth = switch_depth = 0;
    st_enter();                                 /* parameters + body share one scope (as in C) */
    for (Node *p = n->a; p; p = p->next) {
        if (!p->name) { err(n->line, "parameter of '%s' needs a name", n->name); continue; }
        if (st_lookup_current(p->name)) { err(p->line, "duplicate parameter '%s'", p->name); continue; }
        Symbol *s = st_insert(p->name, p->type, p->line);
        s->is_array = p->is_array;
    }
    block_depth++;
    check_stmts(n->b->a);
    block_depth--;
    st_exit();
    loop_depth = saved_loop; switch_depth = saved_switch;
    cur_func = NULL;
}

static void check_stmts(Node *n) {
    for (; n; n = n->next) {
        switch (n->kind) {
        case N_DECL:   check_decl(n);   break;
        case N_ASSIGN: check_assign(n); break;
        case N_PRINT:
            if (n->a->kind == N_ID) {   /* printing an array / function name directly */
                Symbol *s = st_lookup(n->a->name);
                if (s && (s->is_array || s->is_func)) { check_expr(n->a); break; }
            }
            if (check_expr(n->a) == T_VOID) err(n->line, "cannot print the result of a void function");
            break;
        case N_SCAN:      check_scan(n);  break;
        case N_EXPR_STMT: check_expr(n->a); break;
        case N_RETURN:    check_return(n); break;
        case N_FUNC:      check_func(n);  break;
        case N_BREAK:
            if (!loop_depth && !switch_depth) err(n->line, "'break' outside a loop or switch");
            break;
        case N_CONTINUE:
            if (!loop_depth) err(n->line, "'continue' outside a loop");
            break;
        case N_IF:
            check_condition(n->a, "if");
            check_body(n->b);
            check_body(n->c);
            break;
        case N_WHILE:
            check_condition(n->a, n->c ? "for" : "while");
            check_loop_body(n->b);
            if (n->c) check_stmts(n->c);    /* for-step: runs in the for-header scope */
            break;
        case N_DOWHILE:
            check_loop_body(n->b);
            check_condition(n->a, "do-while");
            break;
        case N_SWITCH: {
            Type t = check_expr(n->a);
            if (t != T_NONE && t != T_INT)
                err(n->line, "switch expression must be 'int', got '%s'", type_name(t));
            int defaults = 0;
            switch_depth++;
            st_enter();
            for (Node *c = n->b; c; c = c->next) {
                if (!c->a) { if (++defaults > 1) err(c->line, "multiple 'default' labels in one switch"); }
                else {
                    Type ct = check_expr(c->a);
                    if (c->a->kind != N_INT)
                        err(c->line, "case label must be an integer constant");
                    else if (ct != T_INT) err(c->line, "case label must be 'int'");
                    for (Node *d = n->b; d != c; d = d->next)
                        if (d->a && d->a->kind == N_INT && c->a->kind == N_INT && d->a->ival == c->a->ival)
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
            block_depth++;
            check_stmts(n->a);
            block_depth--;
            st_exit();
            break;
        default:
            break;
        }
    }
}

static int same_signature(Node *a, Node *b) {
    if (a->type != b->type) return 0;
    Node *p = a->a, *q = b->a;
    for (; p && q; p = p->next, q = q->next)
        if (p->type != q->type || p->is_array != q->is_array) return 0;
    return !p && !q;
}

/* pre-pass: every top-level function is known before any body is checked,
 * so functions may call each other (and themselves) regardless of order */
static void register_functions(Node *n) {
    for (; n; n = n->next) {
        if (n->kind != N_FUNC) continue;
        Symbol *s = st_lookup_current(n->name);
        if (s && !s->is_func) { err(n->line, "'%s' redeclared as a function", n->name); continue; }
        if (s) {
            if (!same_signature(s->func, n))
                err(n->line, "conflicting declaration of function '%s' (first declared at line %d)", n->name, s->line);
            else if (s->func->b && n->b)
                err(n->line, "redefinition of function '%s' (first defined at line %d)", n->name, s->func->line);
            else if (n->b) s->func = n;
            continue;
        }
        if (n->b == NULL && is(n->name, "main")) continue;
        s = st_insert(n->name, n->type, n->line);
        s->is_func = 1;
        s->func = n;
    }
}

/* a function that is called but only has a prototype */
static void check_defined(Node *n) {
    for (; n; n = n->next) {
        if (n->kind == N_FUNC && !n->b) {
            Symbol *s = st_lookup(n->name);
            if (s && s->is_func && s->func == n && s->func->b == NULL)
                err(n->line, "function '%s' is declared but never defined", n->name);
        }
    }
}

int semantic_check(Node *root) {
    errors = 0;
    cur_func = NULL;
    loop_depth = switch_depth = block_depth = 0;
    st_init();
    if (root) {
        register_functions(root->a);
        check_stmts(root->a);
        check_defined(root->a);
    }
    return errors;
}
