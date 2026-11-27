/* Logix parser (Bison) - grammar from Section 5 of the project manual. */
%code requires {
#include "ast/ast.h"
}

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast/ast.h"

int  yylex(void);
void yyerror(const char *msg);
extern int yylineno;
int   syntax_errors = 0;
extern int prev_tok_line, prev_tok;   /* from lexer.l: line + kind of the token before the lookahead */
extern int yychar;
Node *ast_root = NULL;
extern int java_mode;                 /* lexer.l: compiling a Java program */
extern int java_is_char_var(const char *name);

/* ---- Java helpers ---------------------------------------------------- */
static int is_str_expr(const Node *e) {          /* does this + chain build a String? */
    if (!e) return 0;
    if (e->kind == N_STR) return 1;
    return e->kind == N_BINOP && strcmp(e->op, "+") == 0 && (is_str_expr(e->a) || is_str_expr(e->b));
}
/* System.out.print(ln)("a = " + a + ", b = " + b)  ->  a chain of N_PRINT, one per piece.
 * Like Java, numbers before the first string are added, everything after it is concatenated. */
static Node *jprint_parts(Node *e, Node *out, int l) {
    if (e->kind == N_BINOP && strcmp(e->op, "+") == 0 && is_str_expr(e)) {
        out = jprint_parts(e->a, out, l);
        return jprint_parts(e->b, out, l);
    }
    Node *pr = mk_print(e, l);
    if (e->kind != N_STR) {
        int is_char = (e->kind == N_INT && e->op && strcmp(e->op, "char") == 0) ||
                      ((e->kind == N_ID || e->kind == N_INDEX) && java_is_char_var(e->name));
        pr->name = strdup(is_char ? "%c" : "%j");   /* %j: chosen by type in the ICG (int / double / boolean) */
    }
    return list_append(out, pr);
}
static Node *mk_jprint(Node *e, int newline, int l) {
    Node *out = jprint_parts(e, NULL, l);
    if (newline) out = list_append(out, mk_print(mk_str(strdup("\\n"), l), l));
    return out;
}
/* Java printf: %n is a newline */
static char *java_fmt(char *f) {
    char *o = malloc(strlen(f) * 2 + 1), *q = o;
    for (char *p = f; *p; p++) {
        if (p[0] == '%' && p[1] == 'n') { *q++ = '\\'; *q++ = 'n'; p++; }
        else if (p[0] == '%' && p[1] == '%') { *q++ = '%'; *q++ = '%'; p++; }
        else *q++ = *p;
    }
    *q = 0;
    return o;
}
/* int x = sc.nextInt();  /  x = sc.nextInt();  /  a[i] = sc.nextInt(); */
static Node *mk_jread(Node *lv, int spec, int l) {
    char fmt[4] = { '%', spec == 'C' ? 'c' : (char)spec, 0, 0 };
    if (spec == 'C') { fmt[0] = ' '; fmt[1] = '%'; fmt[2] = 'c'; }
    lv->next = NULL;
    return mk_scanf(strdup(fmt), lv, l);
}
/* new int[5] : the size must be a constant (arrays have a fixed size in Logix) */
static int const_size(Node *e, int l) {
    if (e->kind == N_INT) return e->ival;
    fprintf(stderr, "Syntax error at line %d: array size must be a number, e.g. new int[10]\n", l);
    syntax_errors++;
    return 1;
}
/* for (int x : arr) body  ==>  for (int _i = 0; _i < arr.length; _i++) { int x = arr[_i]; body } */
static Node *mk_foreach(Type t, char *var, char *arr, Node *body, int l) {
    static int cnt = 0;
    char iname[32]; snprintf(iname, sizeof iname, "_i%d", ++cnt);
    Node *init = mk_decl_init(T_INT, strdup(iname), mk_int(0, l), l);
    init->type = T_INT;
    Node *len = mk_id(strdup(arr), l); len->kind = N_LENGTH;
    Node *cond = mk_binop("<", mk_id(strdup(iname), l), len, l);
    Node *step = mk_assign(strdup(iname), mk_binop("+", mk_id(strdup(iname), l), mk_int(1, l), l), l);
    Node *x = mk_decl_init(t, var, mk_index(strdup(arr), mk_id(strdup(iname), l), l), l);
    x->type = t;
    return mk_for(init, cond, step, mk_block(list_append(x, body), l), l);
}
/* ---- C++ helpers: cout << a << "text" << endl;   cin >> a >> b[i]; ---- */
static int cpp_fixed = 0, cpp_prec = -1;      /* state of << fixed << setprecision(n) */
static Node *cpp_out(Node *e, int l) {
    Node *pr = mk_print(e, l);
    int is_char = (e->kind == N_INT && e->op && strcmp(e->op, "char") == 0) ||
                  ((e->kind == N_ID || e->kind == N_INDEX) && java_is_char_var(e->name));
    if (is_char) { pr->name = strdup("%c"); return pr; }
    char spec[32];                       /* %k<float format>: the ICG uses it for doubles, %d otherwise */
    if (cpp_fixed)        snprintf(spec, sizeof spec, "%%k%%.%df", cpp_prec < 0 ? 6 : cpp_prec);
    else if (cpp_prec > 0) snprintf(spec, sizeof spec, "%%k%%.%dg", cpp_prec);
    else                  snprintf(spec, sizeof spec, "%%k%%g");
    pr->name = strdup(spec);
    return pr;
}
static Node *cpp_in(Node *lv, int l) {
    int is_char = java_is_char_var(lv->name);
    lv->next = NULL;
    Node *n = mk_scanf(strdup(is_char ? " %c" : "%d"), lv, l);
    if (!is_char) n->ival = 'a';         /* read whatever the variable's type is */
    return n;
}

/* Java fields may come after main(): declarations go first so main can use them */
static Node *java_hoist(Node *stmts) {
    Node *decls = NULL, *rest = NULL;
    while (stmts) {
        Node *n = stmts; stmts = stmts->next; n->next = NULL;
        if (n->kind == N_DECL) decls = list_append(decls, n); else rest = list_append(rest, n);
    }
    return list_append(decls, rest);
}
%}

%error-verbose
%locations

%union {
    int    ival;
    double fval;
    char  *sval;
    Type   type;
    Node  *node;
}

%token INT FLOAT BOOL IF ELSE WHILE PRINT TRUE_LIT FALSE_LIT
%token VOID RETURN FOR PRINTF SCANF DO BREAK CONTINUE SWITCH CASE DEFAULT
%token INC DEC ADD_ASSIGN SUB_ASSIGN MUL_ASSIGN DIV_ASSIGN MOD_ASSIGN
%token <sval> STRING_LIT
%token <sval> ID
%token <ival> INT_LIT
%token <fval> FLOAT_LIT
%token AND OR EQ NEQ LE GE
/* Java-only tokens (produced by lexer.l only in Java mode) */
%token JPRINTLN JPRINT JNEW
/* C++-only tokens */
%token COUT CIN ENDL FIXED SETPREC SHL SHR
%token <sval> JSTRING JLENGTH
%token <ival> JREAD JCAST JCHARLIT

%right '?' ':'
%left OR
%left AND
%left EQ NEQ
%left '<' '>' LE GE
%left '+' '-'
%left '*' '/' '%'
%right '!' UMINUS
%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%type <node> program stmt_list stmt block decl_stmt declarators declarator simple
%type <node> func_def for_init for_cond for_step print_args expr_list expr
%type <node> lvalue call params param_list param scan_list
%type <node> case_list case_clause out_list out_item in_list
%type <type> type

%%

program     : stmt_list                 { ast_root = mk_program(java_mode == 1 ? java_hoist($1) : $1); $$ = ast_root; }
            ;

stmt_list   : /* empty */               { $$ = NULL; }
            | stmt_list stmt            { $$ = list_append($1, $2); }
            ;

stmt        : decl_stmt ';'             { $$ = $1; }
            | simple ';'                { $$ = $1; }
            | PRINT expr ';'            { $$ = mk_print($2, @1.first_line); }
            | PRINTF '(' print_args ')' ';'
                                        { $$ = $3; }
            | SCANF '(' STRING_LIT ',' scan_list ')' ';'
                                        { $$ = mk_scanf($3, $5, @1.first_line); }
            | call ';'                  { $$ = $1; }
            | JPRINTLN '(' ')' ';'      { $$ = mk_print(mk_str(strdup("\\n"), @1.first_line), @1.first_line); }
            | JPRINTLN '(' expr ')' ';' { $$ = mk_jprint($3, 1, @1.first_line); }
            | JPRINT '(' expr ')' ';'   { $$ = mk_jprint($3, 0, @1.first_line); }
            | FOR '(' type ID ':' ID ')' stmt
                                        { $$ = mk_foreach($3, $4, $6, $8, @1.first_line); }
            | BREAK ';'                 { $$ = mk_break(@1.first_line); }
            | CONTINUE ';'              { $$ = mk_continue(@1.first_line); }
            | DO stmt WHILE '(' expr ')' ';'
                                        { $$ = mk_dowhile($5, $2, @1.first_line); }
            | RETURN ';'                { $$ = mk_return(NULL, @1.first_line); }
            | RETURN expr ';'           { $$ = mk_return($2, @1.first_line); }
            | IF '(' expr ')' stmt %prec LOWER_THAN_ELSE
                                        { $$ = mk_if($3, $5, NULL, @1.first_line); }
            | IF '(' expr ')' stmt ELSE stmt
                                        { $$ = mk_if($3, $5, $7, @1.first_line); }
            | WHILE '(' expr ')' stmt   { $$ = mk_while($3, $5, @1.first_line); }
            | FOR '(' for_init ';' for_cond ';' for_step ')' stmt
                                        { /* for(i;c;s) b  ==>  { i; while (c) b  [step s] }  (see mk_for) */
                                          $$ = mk_for($3, $5, $7, $9, @1.first_line); }
            | COUT out_list ';'         { $$ = $2; }
            | CIN in_list ';'           { $$ = $2; }
            | SWITCH '(' expr ')' '{' case_list '}'
                                        { $$ = mk_switch($3, $6, @1.first_line); }
            | func_def                  { $$ = $1; }
            | block                     { $$ = $1; }
            | ';'                       { $$ = NULL; }
            | error ';'                 { yyerrok; $$ = NULL; }
            ;

/* C++:  cout << "x = " << x << endl; */
out_list    : SHL out_item              { $$ = $2; }
            | out_list SHL out_item     { $$ = list_append($1, $3); }
            ;

out_item    : STRING_LIT                { $$ = mk_print(mk_str($1, @1.first_line), @1.first_line); }
            | ENDL                      { $$ = mk_print(mk_str(strdup("\\n"), @1.first_line), @1.first_line); }
            | FIXED                     { cpp_fixed = 1; $$ = NULL; }
            | SETPREC '(' INT_LIT ')'   { cpp_prec = $3; $$ = NULL; }
            | expr                      { $$ = cpp_out($1, @1.first_line); }
            ;

/* C++:  cin >> a >> arr[i]; */
in_list     : SHR lvalue                { $$ = cpp_in($2, @2.first_line); }
            | in_list SHR lvalue        { $$ = list_append($1, cpp_in($3, @3.first_line)); }
            ;

case_list   : /* empty */               { $$ = NULL; }
            | case_list case_clause     { $$ = list_append($1, $2); }
            ;

case_clause : CASE expr ':' stmt_list   { $$ = mk_case($2, $4, @1.first_line); }
            | DEFAULT ':' stmt_list     { $$ = mk_case(NULL, $3, @1.first_line); }
            ;

/* int main() { ... }  -- C-style entry point, compiled as a plain block.
 * int f(int a, float b[]) { ... }  -- ordinary function; int f(int a);  -- prototype */
func_def    : type ID '(' params ')' block      { $$ = mk_func($1, $2, $4, $6, @1.first_line); }
            | VOID ID '(' params ')' block      { $$ = mk_func(T_VOID, $2, $4, $6, @1.first_line); }
            | type ID '(' params ')' ';'        { $$ = mk_func($1, $2, $4, NULL, @1.first_line); if (!strcmp($2, "main")) $$ = NULL; }
            | VOID ID '(' params ')' ';'        { $$ = mk_func(T_VOID, $2, $4, NULL, @1.first_line); if (!strcmp($2, "main")) $$ = NULL; }
            ;

params      : /* empty */                       { $$ = NULL; }
            | VOID                              { $$ = NULL; }
            | param_list                        { $$ = $1; }
            ;

param_list  : param                             { $$ = $1; }
            | param_list ',' param              { $$ = list_append($1, $3); }
            ;

param       : type ID                           { $$ = mk_param($1, $2, 0, @2.first_line); }
            | type ID '[' ']'                   { $$ = mk_param($1, $2, 1, @2.first_line); }
            | type '[' ']' ID                   { $$ = mk_param($1, $4, 1, @4.first_line); }     /* Java: int[] a */
            ;

block       : '{' stmt_list '}'         { $$ = mk_block($2, @1.first_line); }
            | '{' stmt_list error '}'   { yyerrok; $$ = mk_block($2, @1.first_line); }
            ;

/* int a;   int a = 5;   int a, b = 2, c;   */
decl_stmt   : type declarators          { for (Node *n = $2; n; n = n->next) if (n->kind == N_DECL) n->type = $1;
                                          $$ = $2; }
            /* Java arrays:  int[] a = new int[5];   int[] a = {1, 2, 3}; */
            | type '[' ']' ID '=' JNEW type '[' expr ']'
                                        { $$ = mk_decl_array($1, $4, const_size($9, @9.first_line), NULL, @4.first_line); }
            | type '[' ']' ID '=' '{' expr_list '}'
                                        { $$ = mk_decl_array($1, $4, 0, $7, @4.first_line); }
            ;

declarators : declarator                { $$ = $1; }
            | declarators ',' declarator{ $$ = list_append($1, $3); }
            ;

declarator  : ID                        { $$ = mk_decl(T_NONE, $1, @1.first_line); }
            | ID '=' expr               { $$ = mk_decl_init(T_NONE, $1, $3, @1.first_line); }
            | ID '[' INT_LIT ']'        { $$ = mk_decl_array(T_NONE, $1, $3, NULL, @1.first_line); }
            | ID '[' INT_LIT ']' '=' '{' expr_list '}'
                                        { $$ = mk_decl_array(T_NONE, $1, $3, $7, @1.first_line); }
            | ID '[' ']' '=' '{' expr_list '}'
                                        { $$ = mk_decl_array(T_NONE, $1, 0, $6, @1.first_line); }
            | ID '[' ']' '=' JNEW type '[' expr ']'      /* Java: int a[] = new int[5]; */
                                        { $$ = mk_decl_array(T_NONE, $1, const_size($8, @8.first_line), NULL, @1.first_line); }
            | ID '=' JREAD              { /* Java: int n = sc.nextInt();  ->  int n; scan n */
                                          Node *d = mk_decl(T_NONE, $1, @1.first_line);
                                          $$ = list_append(d, mk_jread(mk_id(strdup($1), @1.first_line), $3, @1.first_line)); }
            ;

/* a variable or an array element: usable as an expression, an assignment target or a scanf argument */
lvalue      : ID                        { $$ = mk_id($1, @1.first_line); }
            | ID '[' expr ']'           { $$ = mk_index($1, $3, @1.first_line); }
            ;

call        : ID '(' ')'                { $$ = mk_call($1, NULL, @1.first_line); }
            | ID '(' expr_list ')'      { $$ = mk_call($1, $3, @1.first_line); }
            ;

scan_list   : '&' lvalue                { $$ = $2; }
            | scan_list ',' '&' lvalue  { $$ = list_append($1, $4); }
            ;

/* assignment-like statements (also usable in a for header) */
simple      : lvalue '=' expr           { $$ = mk_assign_lv($1, $3, @1.first_line); }
            | lvalue '=' JREAD          { $$ = mk_jread($1, $3, @1.first_line); }     /* Java: x = sc.nextInt(); */
            | lvalue ADD_ASSIGN expr    { $$ = mk_assign_lv($1, mk_binop("+", mk_lv_read($1), $3, @2.first_line), @1.first_line); }
            | lvalue SUB_ASSIGN expr    { $$ = mk_assign_lv($1, mk_binop("-", mk_lv_read($1), $3, @2.first_line), @1.first_line); }
            | lvalue MUL_ASSIGN expr    { $$ = mk_assign_lv($1, mk_binop("*", mk_lv_read($1), $3, @2.first_line), @1.first_line); }
            | lvalue DIV_ASSIGN expr    { $$ = mk_assign_lv($1, mk_binop("/", mk_lv_read($1), $3, @2.first_line), @1.first_line); }
            | lvalue MOD_ASSIGN expr    { $$ = mk_assign_lv($1, mk_binop("%", mk_lv_read($1), $3, @2.first_line), @1.first_line); }
            | lvalue INC                { $$ = mk_assign_lv($1, mk_binop("+", mk_lv_read($1), mk_int(1, @1.first_line), @2.first_line), @1.first_line); }
            | lvalue DEC                { $$ = mk_assign_lv($1, mk_binop("-", mk_lv_read($1), mk_int(1, @1.first_line), @2.first_line), @1.first_line); }
            | INC lvalue                { $$ = mk_assign_lv($2, mk_binop("+", mk_lv_read($2), mk_int(1, @1.first_line), @1.first_line), @2.first_line); }
            | DEC lvalue                { $$ = mk_assign_lv($2, mk_binop("-", mk_lv_read($2), mk_int(1, @1.first_line), @1.first_line), @2.first_line); }
            ;

for_init    : /* empty */               { $$ = NULL; }
            | decl_stmt                 { $$ = $1; }
            | simple                    { $$ = $1; }
            ;

for_cond    : /* empty */               { $$ = mk_bool(1, yylineno); }
            | expr                      { $$ = $1; }
            ;

for_step    : /* empty */               { $$ = NULL; }
            | simple                    { $$ = $1; }
            ;

/* printf("fmt", e1, e2, ...) */
print_args  : STRING_LIT                { $$ = mk_printf($1, NULL, @1.first_line); }
            | STRING_LIT ',' expr_list  { $$ = mk_printf($1, $3, @1.first_line); }
            | JSTRING                   { $$ = mk_printf(java_fmt($1), NULL, @1.first_line); }     /* System.out.printf */
            | JSTRING ',' expr_list     { $$ = mk_printf(java_fmt($1), $3, @1.first_line); }
            ;

expr_list   : expr                      { $$ = $1; }
            | expr_list ',' expr        { $$ = list_append($1, $3); }
            ;

type        : INT                       { $$ = T_INT; }
            | FLOAT                     { $$ = T_FLOAT; }
            | BOOL                      { $$ = T_BOOL; }
            ;

expr        : expr '?' expr ':' expr    { $$ = mk_ternary($1, $3, $5, @2.first_line); }
            | expr OR expr              { $$ = mk_binop("||", $1, $3, @2.first_line); }
            | expr AND expr             { $$ = mk_binop("&&", $1, $3, @2.first_line); }
            | expr EQ expr              { $$ = mk_binop("==", $1, $3, @2.first_line); }
            | expr NEQ expr             { $$ = mk_binop("!=", $1, $3, @2.first_line); }
            | expr '<' expr             { $$ = mk_binop("<",  $1, $3, @2.first_line); }
            | expr '>' expr             { $$ = mk_binop(">",  $1, $3, @2.first_line); }
            | expr LE expr              { $$ = mk_binop("<=", $1, $3, @2.first_line); }
            | expr GE expr              { $$ = mk_binop(">=", $1, $3, @2.first_line); }
            | expr '+' expr             { $$ = mk_binop("+",  $1, $3, @2.first_line); }
            | expr '-' expr             { $$ = mk_binop("-",  $1, $3, @2.first_line); }
            | expr '*' expr             { $$ = mk_binop("*",  $1, $3, @2.first_line); }
            | expr '/' expr             { $$ = mk_binop("/",  $1, $3, @2.first_line); }
            | expr '%' expr             { $$ = mk_binop("%",  $1, $3, @2.first_line); }
            | '!' expr                  { $$ = mk_unop("!", $2, @1.first_line); }
            | '-' expr %prec UMINUS     { if ($2->kind == N_INT)        { $2->ival = -$2->ival; $$ = $2; }
                                          else if ($2->kind == N_FLOAT) { $2->fval = -$2->fval; $$ = $2; }
                                          else $$ = mk_unop("-", $2, @1.first_line); }
            | '(' expr ')'              { $$ = $2; }
            | lvalue                    { $$ = $1; }
            | call                      { $$ = $1; }
            | INT_LIT                   { $$ = mk_int($1, @1.first_line); }
            | FLOAT_LIT                 { $$ = mk_float($1, @1.first_line); }
            | TRUE_LIT                  { $$ = mk_bool(1, @1.first_line); }
            | FALSE_LIT                 { $$ = mk_bool(0, @1.first_line); }
            /* Java-only expressions */
            | JSTRING                   { $$ = mk_str($1, @1.first_line); }       /* only valid inside println */
            | JCHARLIT                  { $$ = mk_int($1, @1.first_line); $$->op = "char"; }
            | JLENGTH                   { $$ = mk_id($1, @1.first_line); $$->kind = N_LENGTH; }   /* arr.length */
            | JCAST expr %prec UMINUS   { $$ = mk_unop($1 == T_FLOAT ? "(float)" : "(int)", $2, @1.first_line); }
            ;

%%

/* Which line to blame? */
static int error_line(void) {
    int cur_line = yylloc.first_line;
    if (yychar == 0) return prev_tok_line ? prev_tok_line : cur_line;
    int ends_stmt = prev_tok == ID || prev_tok == INT_LIT || prev_tok == FLOAT_LIT ||
                    prev_tok == TRUE_LIT || prev_tok == FALSE_LIT || prev_tok == ')' || prev_tok == ']';
    if (ends_stmt && cur_line > prev_tok_line) return prev_tok_line;
    return cur_line;
}

void yyerror(const char *msg) {
    fprintf(stderr, "Syntax error at line %d: %s\n", error_line(), msg);
    syntax_errors++;
}