/* Logix AST - node definitions (manual 4.3) */
#ifndef LOGIX_AST_H
#define LOGIX_AST_H

typedef enum { T_INT, T_FLOAT, T_BOOL, T_NONE, T_VOID } Type;

typedef enum {
    N_PROGRAM, N_BLOCK, N_DECL, N_ASSIGN, N_PRINT, N_IF, N_WHILE,
    N_BINOP, N_UNOP, N_ID, N_INT, N_FLOAT, N_BOOL, N_STR, N_RETURN,
    /* C extensions */
    N_FUNC,       /* function definition / prototype                         */
    N_PARAM,      /* one function parameter                                  */
    N_CALL,       /* f(args)                                                 */
    N_INDEX,      /* a[i]  (read)                                            */
    N_SCAN,       /* scanf("...", &x, &a[i])                                 */
    N_EXPR_STMT,  /* f(x);  (call used as a statement)                       */
    N_BREAK, N_CONTINUE,
    N_DOWHILE,    /* do body while (cond);                                   */
    N_SWITCH, N_CASE,
    N_TERNARY     /* c ? x : y                                               */
} NodeKind;

typedef struct Node {
    NodeKind     kind;
    int          line;
    const char  *op;      /* N_BINOP / N_UNOP: "+", "&&", "!" ...  N_ID/N_INDEX inside scanf: format spec */
    char        *name;    /* N_DECL / N_ASSIGN / N_ID / N_INDEX / N_CALL / N_FUNC / N_PARAM */
    Type         type;    /* N_DECL: declared type; N_FUNC: return type; later: expression type */
    int          ival;    /* N_INT, N_BOOL (0/1) */
    double       fval;    /* N_FLOAT */
    int          is_array;/* N_DECL / N_PARAM: declared with [] */
    struct Node *a, *b, *c;   /* children (see mk_* below for the meaning per kind) */
    struct Node *ref;         /* N_CALL: the N_FUNC definition it calls (set by the semantic pass) */
    struct Node *next;        /* next statement in a statement list / next item in an argument list */
} Node;

Node *mk_program(Node *stmts);
Node *mk_block(Node *stmts, int line);
Node *mk_decl(Type t, char *name, int line);
Node *mk_decl_init(Type t, char *name, Node *init, int line);   /* int x = expr; (init in ->a) */
Node *mk_array_decl(char *name, Node *size, Node *init_list, int line); /* int a[size] = {..}; size in ->b, list in ->c */
Node *mk_assign(char *name, Node *expr, int line);
Node *mk_assign_lv(Node *lvalue, Node *expr, int line);         /* lvalue is N_ID or N_INDEX */
Node *mk_print(Node *expr, int line);
Node *mk_if(Node *cond, Node *then_s, Node *else_s, int line);
Node *mk_while(Node *cond, Node *body, int line);
Node *mk_for_loop(Node *cond, Node *body, Node *step, int line);/* while with a step (->c) run before each re-test */
Node *mk_dowhile(Node *body, Node *cond, int line);
Node *mk_binop(const char *op, Node *l, Node *r, int line);
Node *mk_unop(const char *op, Node *e, int line);
Node *mk_ternary(Node *c, Node *x, Node *y, int line);
Node *mk_id(char *name, int line);
Node *mk_index(char *name, Node *index, int line);
Node *mk_int(int v, int line);
Node *mk_float(double v, int line);
Node *mk_bool(int v, int line);
Node *mk_str(char *text, int line);              /* string literal (printf text) */
Node *mk_return(Node *expr, int line);           /* expr may be NULL */
Node *mk_printf(char *fmt, Node *args, int line);/* expands to a chain of N_PRINT */
Node *mk_scanf(char *fmt, Node *targets, int line);
Node *mk_func(Type ret, char *name, Node *params, Node *body, int line); /* body NULL = prototype */
Node *mk_param(Type t, char *name, int is_array, int line);
Node *mk_call(char *name, Node *args, int line);
Node *mk_expr_stmt(Node *e, int line);
Node *mk_jump(NodeKind k, int line);             /* N_BREAK / N_CONTINUE */
Node *mk_switch(Node *e, Node *cases, int line);
Node *mk_case(Node *value, Node *stmts, int line);/* value NULL = default */

Node *list_append(Node *list, Node *item);   /* safe when item == NULL */
const char *type_name(Type t);
void  ast_print(const Node *n, int depth);   /* prints a statement chain */

#endif
