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

/* value of a C character literal body: a  or  \n  \t ... */
static int char_value(const char *s) {
    if (s[0] != '\\') return (unsigned char)s[0];
    switch (s[1]) {
    case 'n': return '\n';  case 't': return '\t';  case 'r': return '\r';
    case '0': return 0;     default:  return (unsigned char)s[1];
    }
}

/* line + kind of the last two tokens returned; yyerror() uses them to blame
 * the right line for a missing ';' (see parser.y) */
int cur_tok_line = 1, prev_tok_line = 1, prev_tok = 0;
static int cur_tok = 0;
static int tok(int t) {
    prev_tok_line = cur_tok_line; prev_tok = cur_tok;
    cur_tok_line  = yylloc.first_line; cur_tok = t;
    return t;
}

/* #define NAME literal   (object-like macros whose value is a number or a char) */
typedef struct Macro { char *name; int tok; int ival; double fval; struct Macro *next; } Macro;
static Macro *macros = NULL;

static void add_define(const char *line) {
    const char *p = strstr(line, "define") + 6;
    while (*p == ' ' || *p == '\t') p++;
    const char *ns = p;
    while (*p == '_' || (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9')) p++;
    if (p == ns || *p == '(') return;                    /* function-like macros are not supported */
    char name[128]; size_t nl = (size_t)(p - ns); if (nl >= sizeof name) return;
    memcpy(name, ns, nl); name[nl] = 0;
    while (*p == ' ' || *p == '\t' || *p == '(') p++;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    Macro m = { 0 };
    if (*p == '\'') {
        if (p[1] == '\\') m.ival = char_value(p + 1); else m.ival = (unsigned char)p[1];
        m.tok = INT_LIT;
    } else if (*p >= '0' && *p <= '9') {
        char *end;
        double d = strtod(p, &end);
        int is_float = 0;
        for (const char *q = p; q < end; q++) if (*q == '.' || *q == 'e' || *q == 'E') is_float = 1;
        if (is_float) { m.tok = FLOAT_LIT; m.fval = neg ? -d : d; }
        else          { m.tok = INT_LIT;   m.ival = (int)(neg ? -d : d); }
    } else if (strncmp(p, "true", 4) == 0)  { m.tok = TRUE_LIT; }
    else if (strncmp(p, "false", 5) == 0)   { m.tok = FALSE_LIT; }
    else return;
    Macro *nm = malloc(sizeof *nm);
    *nm = m; nm->name = strdup(name); nm->next = macros; macros = nm;
}

static Macro *find_macro(const char *name) {
    for (Macro *m = macros; m; m = m->next) if (strcmp(m->name, name) == 0) return m;
    return NULL;
}

/* give every token its line number for the parser (@n.first_line) */
#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno;
%}

DIGIT   [0-9]
ID      [A-Za-z_][A-Za-z0-9_]*

%x BLOCK_COMMENT

%%

"long"[ \t]+"double"                              { return tok(FLOAT); }
("long"|"short")([ \t]+"long")?([ \t]+"int")?     { return tok(INT); }  /* long, long long, short int ... */
"const"|"signed"|"unsigned"|"static"|"register"   { /* qualifiers: ignored */ }
"int"       { return tok(INT); }
"char"      { return tok(INT); }     /* C compatibility: char is stored as an int (use %c to print it) */
"float"     { return tok(FLOAT); }
"double"    { return tok(FLOAT); }   /* C compatibility: double is treated as float */
"bool"      { return tok(BOOL); }
"void"      { return tok(VOID); }
"return"    { return tok(RETURN); }
"for"       { return tok(FOR); }
"printf"    { return tok(PRINTF); }
"if"        { return tok(IF); }
"else"      { return tok(ELSE); }
"while"     { return tok(WHILE); }
"print"     { return tok(PRINT); }
"scanf"     { return tok(SCANF); }
"do"        { return tok(DO); }
"break"     { return tok(BREAK); }
"continue"  { return tok(CONTINUE); }
"switch"    { return tok(SWITCH); }
"case"      { return tok(CASE); }
"default"   { return tok(DEFAULT); }
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
{ID}                  {
        Macro *m = find_macro(yytext);
        if (m) {                                   /* #define'd constant */
            if (m->tok == INT_LIT)   yylval.ival = m->ival;
            if (m->tok == FLOAT_LIT) yylval.fval = m->fval;
            return tok(m->tok);
        }
        yylval.sval = strdup(yytext); return tok(ID);
    }

\"([^\"\\\n]|\\.)*\"   { char *q = malloc(yyleng); memcpy(q, yytext + 1, yyleng - 2); q[yyleng - 2] = 0;
                          yylval.sval = q; return tok(STRING_LIT); }
\"([^\"\\\n]|\\.)*     { fprintf(stderr, "Lexical error at line %d: unterminated string literal\n", yylineno);
                          lexical_errors++; }

'([^'\\\n]|\\.)'       { yylval.ival = char_value(yytext + 1); return tok(INT_LIT); }

"#"[ \t]*"include"[^\n]*   { /* C preprocessor line: ignored */ }
"#"[ \t]*"define"[^\n]*    { add_define(yytext); }

"++"  { return tok(INC); }
"--"  { return tok(DEC); }
"+="  { return tok(ADD_ASSIGN); }
"-="  { return tok(SUB_ASSIGN); }
"*="  { return tok(MUL_ASSIGN); }
"/="  { return tok(DIV_ASSIGN); }
"%="  { return tok(MOD_ASSIGN); }
"&&"  { return tok(AND); }
"||"  { return tok(OR); }
"=="  { return tok(EQ); }
"!="  { return tok(NEQ); }
"<="  { return tok(LE); }
">="  { return tok(GE); }
[<>=!+\-*/%]  { return tok(yytext[0]); }
[{}();,\[\]&?:]  { return tok(yytext[0]); }

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