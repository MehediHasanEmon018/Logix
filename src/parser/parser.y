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
%}

%define parse.error verbose
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

%type <node> program stmt_list stmt block decl_stmt declarators declarator simple lvalue
%type <node> func_def params_opt param_list param call args_opt scan_targets scan_target
%type <node> case_list case_clause
%type <node> for_init for_cond for_step print_args expr_list expr
%type <type> type

%%

program     : stmt_list                 { ast_root = mk_program($1); $$ = ast_root; }
            ;

stmt_list   : /* empty */               { $$ = NULL; }
            | stmt_list stmt            { $$ = list_append($1, $2); }
            ;

stmt        : decl_stmt ';'             { $$ = $1; }
            | simple ';'                { $$ = $1; }
            | call ';'                  { $$ = mk_expr_stmt($1, @1.first_line); }
            | PRINT expr ';'            { $$ = mk_print($2, @1.first_line); }
            | PRINTF '(' print_args ')' ';'
                                        { $$ = $3; }
            | SCANF '(' STRING_LIT ',' scan_targets ')' ';'
                                        { $$ = mk_scanf($3, $5, @1.first_line); }
            | RETURN ';'                { $$ = mk_return(NULL, @1.first_line); }
            | RETURN expr ';'           { $$ = mk_return($2, @1.first_line); }
            | BREAK ';'                 { $$ = mk_jump(N_BREAK, @1.first_line); }
            | CONTINUE ';'              { $$ = mk_jump(N_CONTINUE, @1.first_line); }
            | IF '(' expr ')' stmt %prec LOWER_THAN_ELSE
                                        { $$ = mk_if($3, $5, NULL, @1.first_line); }
            | IF '(' expr ')' stmt ELSE stmt
                                        { $$ = mk_if($3, $5, $7, @1.first_line); }
            | WHILE '(' expr ')' stmt   { $$ = mk_while($3, $5, @1.first_line); }
            | DO stmt WHILE '(' expr ')' ';'
                                        { $$ = mk_dowhile($2, $5, @1.first_line); }
            | FOR '(' for_init ';' for_cond ';' for_step ')' stmt
                                        { /* for(i;c;s) b  ==>  { i; while (c) { b } step s } */
                                          $$ = mk_block(list_append($3, mk_for_loop($5, $9, $7, @1.first_line)),
                                                        @1.first_line); }
            | SWITCH '(' expr ')' '{' case_list '}'
                                        { $$ = mk_switch($3, $6, @1.first_line); }
            | func_def                  { $$ = $1; }
            | block                     { $$ = $1; }
            | ';'                       { $$ = NULL; }
            | error ';'                 { yyerrok; $$ = NULL; }
            ;

case_list   : /* empty */               { $$ = NULL; }
            | case_list case_clause     { $$ = list_append($1, $2); }
            ;

case_clause : CASE expr ':' stmt_list   { $$ = mk_case($2, $4, @1.first_line); }
            | DEFAULT ':' stmt_list     { $$ = mk_case(NULL, $3, @1.first_line); }
            ;

/* int add(int a, int b) { ... }     int add(int, int);     void show(int a[], int n) { ... } */
func_def    : type ID '(' params_opt ')' block   { $$ = mk_func($1, $2, $4, $6, @2.first_line); }
            | VOID ID '(' params_opt ')' block   { $$ = mk_func(T_VOID, $2, $4, $6, @2.first_line); }
            | type ID '(' params_opt ')' ';'     { $$ = mk_func($1, $2, $4, NULL, @2.first_line); }
            | VOID ID '(' params_opt ')' ';'     { $$ = mk_func(T_VOID, $2, $4, NULL, @2.first_line); }
            ;

params_opt  : /* empty */               { $$ = NULL; }
            | VOID                      { $$ = NULL; }
            | param_list                { $$ = $1; }
            ;

param_list  : param                     { $$ = $1; }
            | param_list ',' param      { $$ = list_append($1, $3); }
            ;

param       : type ID                   { $$ = mk_param($1, $2, 0, @2.first_line); }
            | type ID '[' ']'           { $$ = mk_param($1, $2, 1, @2.first_line); }
            | type ID '[' INT_LIT ']'   { $$ = mk_param($1, $2, 1, @2.first_line); }
            | type                      { $$ = mk_param($1, NULL, 0, @1.first_line); }
            ;

call        : ID '(' args_opt ')'       { $$ = mk_call($1, $3, @1.first_line); }
            ;

args_opt    : /* empty */               { $$ = NULL; }
            | expr_list                 { $$ = $1; }
            ;

block       : '{' stmt_list '}'         { $$ = mk_block($2, @1.first_line); }
            | '{' stmt_list error '}'   { yyerrok; $$ = mk_block($2, @1.first_line); }
            ;

/* int a;   int a = 5;   int a, b = 2, c;   int arr[5];   int arr[] = {1, 2, 3}; */
decl_stmt   : type declarators          { for (Node *n = $2; n; n = n->next) if (n->kind == N_DECL) n->type = $1;
                                          $$ = $2; }
            ;

declarators : declarator                { $$ = $1; }
            | declarators ',' declarator{ $$ = list_append($1, $3); }
            ;

declarator  : ID                        { $$ = mk_decl(T_NONE, $1, @1.first_line); }
            | ID '=' expr               { $$ = mk_decl_init(T_NONE, $1, $3, @1.first_line); }
            | ID '[' expr ']'           { $$ = mk_array_decl($1, $3, NULL, @1.first_line); }
            | ID '[' expr ']' '=' '{' expr_list '}'
                                        { $$ = mk_array_decl($1, $3, $7, @1.first_line); }
            | ID '[' expr ']' '=' '{' expr_list ',' '}'
                                        { $$ = mk_array_decl($1, $3, $7, @1.first_line); }
            | ID '[' ']' '=' '{' expr_list '}'
                                        { $$ = mk_array_decl($1, NULL, $6, @1.first_line); }
            | ID '[' ']' '=' '{' expr_list ',' '}'
                                        { $$ = mk_array_decl($1, NULL, $6, @1.first_line); }
            ;

lvalue      : ID                        { $$ = mk_id($1, @1.first_line); }
            | ID '[' expr ']'           { $$ = mk_index($1, $3, @1.first_line); }
            ;

/* assignment-like statements (also usable in a for header) */
simple      : lvalue '=' expr           { $$ = mk_assign_lv($1, $3, @1.first_line); }
            | lvalue ADD_ASSIGN expr    { $$ = mk_assign_lv($1, mk_binop("+", $1, $3, @2.first_line), @1.first_line); }
            | lvalue SUB_ASSIGN expr    { $$ = mk_assign_lv($1, mk_binop("-", $1, $3, @2.first_line), @1.first_line); }
            | lvalue MUL_ASSIGN expr    { $$ = mk_assign_lv($1, mk_binop("*", $1, $3, @2.first_line), @1.first_line); }
            | lvalue DIV_ASSIGN expr    { $$ = mk_assign_lv($1, mk_binop("/", $1, $3, @2.first_line), @1.first_line); }
            | lvalue MOD_ASSIGN expr    { $$ = mk_assign_lv($1, mk_binop("%", $1, $3, @2.first_line), @1.first_line); }
            | lvalue INC                { $$ = mk_assign_lv($1, mk_binop("+", $1, mk_int(1, @1.first_line), @2.first_line), @1.first_line); }
            | lvalue DEC                { $$ = mk_assign_lv($1, mk_binop("-", $1, mk_int(1, @1.first_line), @2.first_line), @1.first_line); }
            | INC lvalue                { $$ = mk_assign_lv($2, mk_binop("+", $2, mk_int(1, @1.first_line), @1.first_line), @2.first_line); }
            | DEC lvalue                { $$ = mk_assign_lv($2, mk_binop("-", $2, mk_int(1, @1.first_line), @1.first_line), @2.first_line); }
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
            ;

/* scanf("%d %d", &a, &arr[i]) */
scan_targets: scan_target                   { $$ = $1; }
            | scan_targets ',' scan_target  { $$ = list_append($1, $3); }
            ;

scan_target : '&' ID                    { $$ = mk_id($2, @2.first_line); }
            | '&' ID '[' expr ']'       { $$ = mk_index($2, $4, @2.first_line); }
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
            | '(' type ')' expr %prec UMINUS
                                        { $$ = mk_unop($2 == T_FLOAT ? "(float)" : $2 == T_INT ? "(int)" : "(bool)", $4, @1.first_line); }
            | call                      { $$ = $1; }
            | ID '[' expr ']'           { $$ = mk_index($1, $3, @1.first_line); }
            | ID                        { $$ = mk_id($1, @1.first_line); }
            | INT_LIT                   { $$ = mk_int($1, @1.first_line); }
            | FLOAT_LIT                 { $$ = mk_float($1, @1.first_line); }
            | TRUE_LIT                  { $$ = mk_bool(1, @1.first_line); }
            | FALSE_LIT                 { $$ = mk_bool(0, @1.first_line); }
            ;

%%

/* Which line to blame? */
static int error_line(void) {
    int cur_line = yylloc.first_line;
    if (yychar == 0) return prev_tok_line ? prev_tok_line : cur_line;
    int ends_stmt = prev_tok == ID || prev_tok == INT_LIT || prev_tok == FLOAT_LIT ||
                    prev_tok == TRUE_LIT || prev_tok == FALSE_LIT || prev_tok == ')' ||
                    prev_tok == ']' || prev_tok == INC || prev_tok == DEC;
    if (ends_stmt && cur_line > prev_tok_line) return prev_tok_line;
    return cur_line;
}

void yyerror(const char *msg) {
    fprintf(stderr, "Syntax error at line %d: %s\n", error_line(), msg);
    syntax_errors++;
}