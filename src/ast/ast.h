/* Logix AST - node definitions (manual 4.3) */
#ifndef LOGIX_AST_H
#define LOGIX_AST_H

typedef enum { T_INT, T_FLOAT, T_BOOL, T_VOID, T_NONE } Type;

typedef enum {
    N_PROGRAM, N_BLOCK, N_DECL, N_ASSIGN, N_PRINT, N_IF, N_WHILE,
    N_BINOP, N_UNOP, N_ID, N_INT, N_FLOAT, N_BOOL, N_STR, N_RETURN,
    N_FUNC, N_PARAM, N_CALL, N_INDEX, N_DOWHILE, N_BREAK, N_CONTINUE, N_SCAN,
    N_LENGTH,     /* Java: arr.length (name = array) */
    N_SWITCH,     /* a = value, b = chain of N_CASE */
    N_CASE,       /* a = case value (NULL = default), b = statements */
    N_TERNARY     /* a ? b : c */
} NodeKind;

typedef struct Node {
    NodeKind     kind;
    int          line;
    const char  *op;      /* N_BINOP / N_UNOP: "+", "&&", "!" ... */
    char        *name;    /* N_DECL / N_ASSIGN / N_ID */
    Type         type;    /* N_DECL: declared type; later: expression type */
    int          ival;    /* N_INT, N_BOOL (0/1); N_SCAN: format letter d/f/c/C */
    int          is_array;/* N_DECL / N_PARAM: 1 if declared as an array */
    int          size;    /* N_DECL array: element count */
    double       fval;    /* N_FLOAT */
    struct Node *a, *b, *c;   /* children: cond/lhs, then/rhs, else
                               * N_WHILE: a=cond b=body c=for-step (or NULL)
                               * N_ASSIGN/N_SCAN: b = array index (NULL for plain variables)
                               * N_DECL array: b = initializer list
                               * N_FUNC: a=params b=body (NULL = prototype); N_CALL: a=args; N_INDEX: a=index */
    struct Node *next;        /* next statement in a statement list */
} Node;

Node *mk_program(Node *stmts);
Node *mk_block(Node *stmts, int line);
Node *mk_decl(Type t, char *name, int line);
Node *mk_decl_init(Type t, char *name, Node *init, int line);   /* int x = expr; (init in ->a) */
Node *mk_assign(char *name, Node *expr, int line);
Node *mk_print(Node *expr, int line);
Node *mk_if(Node *cond, Node *then_s, Node *else_s, int line);
Node *mk_while(Node *cond, Node *body, int line);
Node *mk_binop(const char *op, Node *l, Node *r, int line);
Node *mk_unop(const char *op, Node *e, int line);
Node *mk_id(char *name, int line);
Node *mk_int(int v, int line);
Node *mk_float(double v, int line);
Node *mk_bool(int v, int line);
Node *mk_str(char *text, int line);              /* string literal (printf text) */
Node *mk_return(Node *expr, int line);           /* expr may be NULL */
Node *mk_printf(char *fmt, Node *args, int line);/* expands to a chain of N_PRINT */

Node *mk_func(Type ret, char *name, Node *params, Node *body, int line);   /* "main" is returned as a plain block */
Node *mk_param(Type t, char *name, int is_array, int line);
Node *mk_call(char *name, Node *args, int line);
Node *mk_index(char *name, Node *idx, int line);                           /* a[i] (read) */
Node *mk_assign_lv(Node *lv, Node *expr, int line);                        /* lv is an N_ID or N_INDEX */
Node *mk_lv_read(const Node *lv);                                          /* copy of an lvalue used as a value */
Node *mk_decl_array(Type t, char *name, int size, Node *init, int line);   /* int a[5]; int a[] = {1,2}; */
Node *mk_dowhile(Node *cond, Node *body, int line);
Node *mk_for(Node *init, Node *cond, Node *step, Node *body, int line);    /* { init; while(cond) body, step } */
Node *mk_break(int line);
Node *mk_continue(int line);
Node *mk_scanf(char *fmt, Node *lvalues, int line);                        /* one N_SCAN per argument */

Node *mk_switch(Node *e, Node *cases, int line);
Node *mk_case(Node *value, Node *stmts, int line);
Node *mk_ternary(Node *c, Node *x, Node *y, int line);

Node *list_append(Node *list, Node *item);   /* safe when item == NULL */
const char *type_name(Type t);
void  ast_print(const Node *n, int depth);   /* prints a statement chain */

#endif