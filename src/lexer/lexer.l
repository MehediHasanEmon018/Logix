/* Logix lexer (Flex) - Section 4.1 / 5 of the project manual */
%option noyywrap nounput noinput yylineno

%{
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <limits.h>
#include "parser.tab.h"

int lexical_errors = 0;

/* line + kind of the last two tokens returned; yyerror() uses them to blame
 * the right line for a missing ';' (see parser.y) */
int cur_tok_line = 1, prev_tok_line = 1, prev_tok = 0;
static int cur_tok = 0;
static int tok(int t) {
    prev_tok_line = cur_tok_line; prev_tok = cur_tok;
    cur_tok_line  = yylloc.first_line; cur_tok = t;
    return t;
}

/* give every token its line number for the parser (@n.first_line) */
#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno;
%}

DIGIT   [0-9]
ID      [A-Za-z_][A-Za-z0-9_]*

%x BLOCK_COMMENT

%%

"int"       { return tok(INT); }
"float"     { return tok(FLOAT); }
"bool"      { return tok(BOOL); }
"if"        { return tok(IF); }
"else"      { return tok(ELSE); }
"while"     { return tok(WHILE); }
"print"     { return tok(PRINT); }
"true"      { return tok(TRUE_LIT); }
"false"     { return tok(FALSE_LIT); }

{DIGIT}+"."{DIGIT}+   { yylval.fval = atof(yytext); return tok(FLOAT_LIT); }
{DIGIT}+              {
        /* strtol + range check instead of atoi: atoi silently wraps on overflow */
        errno = 0;
        char *end;
        long v = strtol(yytext, &end, 10);
        if (errno == ERANGE || v > INT_MAX) {
            fprintf(stderr, "Lexical error at line %d: integer literal '%s' out of range\n",
                    yylineno, yytext);
            lexical_errors++;
            v = 0;   /* keep parsing so later errors are still reported */
        }
        yylval.ival = (int)v;
        return tok(INT_LIT);
    }
{ID}                  { yylval.sval = strdup(yytext); return tok(ID); }

"&&"  { return tok(AND); }
"||"  { return tok(OR); }
"=="  { return tok(EQ); }
"!="  { return tok(NEQ); }
"<="  { return tok(LE); }
">="  { return tok(GE); }
[<>=!+\-*/%]  { return tok(yytext[0]); }
[{}();]       { return tok(yytext[0]); }

"//".*                  { /* single-line comment: discarded */ }
"/*"                    { BEGIN(BLOCK_COMMENT); }
<BLOCK_COMMENT>"*/"     { BEGIN(INITIAL); }
<BLOCK_COMMENT>.|\n     { /* discarded */ }
<BLOCK_COMMENT><<EOF>>  { fprintf(stderr, "Lexical error at line %d: unterminated block comment\n", yylineno);
                          lexical_errors++; BEGIN(INITIAL); }

[ \t\r\n]+  { /* whitespace: discarded */ }

.  { fprintf(stderr, "Lexical error at line %d: invalid token '%s'\n", yylineno, yytext);
     lexical_errors++; }

%%
