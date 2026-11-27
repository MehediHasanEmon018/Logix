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

/* #define NAME value   (object-like macros whose value is a number or a char literal) */
typedef struct Macro { char *name; int tok; int ival; double fval; struct Macro *next; } Macro;
static Macro *macros = NULL;
static void add_define(const char *line) {
    const char *p = strstr(line, "define") + 6;
    while (*p == ' ' || *p == '\t') p++;
    const char *ns = p;
    while (*p == '_' || (*p >= 'a' && *p <= 'z') || (*p >= 'A' && *p <= 'Z') || (*p >= '0' && *p <= '9')) p++;
    if (p == ns || *p == '(') return;                   /* function-like macros: not supported */
    size_t nl = (size_t)(p - ns);
    char *name = malloc(nl + 1); memcpy(name, ns, nl); name[nl] = 0;
    while (*p == ' ' || *p == '\t' || *p == '(') p++;
    int neg = 0;
    if (*p == '-') { neg = 1; p++; }
    Macro m = { 0 };
    if (*p == '\'') { m.tok = INT_LIT; m.ival = char_value(p + 1); }
    else if (*p >= '0' && *p <= '9') {
        char *end; double d = strtod(p, &end); int fl = 0;
        for (const char *q = p; q < end; q++) if (*q == '.' || *q == 'e' || *q == 'E') fl = 1;
        if (fl) { m.tok = FLOAT_LIT; m.fval = neg ? -d : d; } else { m.tok = INT_LIT; m.ival = (int)(neg ? -d : d); }
    } else if (!strncmp(p, "true", 4))  m.tok = TRUE_LIT;
    else if (!strncmp(p, "false", 5))   m.tok = FALSE_LIT;
    else { free(name); return; }
    Macro *nm = malloc(sizeof *nm); *nm = m; nm->name = name; nm->next = macros; macros = nm;
}
static Macro *find_macro(const char *name) {
    for (Macro *m = macros; m; m = m->next) if (!strcmp(m->name, name)) return m;
    return NULL;
}
/* an identifier, unless it is a #define'd constant */
static int ident(void) {
    Macro *m = find_macro(yytext);
    if (m) {
        if (m->tok == INT_LIT) yylval.ival = m->ival;
        if (m->tok == FLOAT_LIT) yylval.fval = m->fval;
        return tok(m->tok);
    }
    yylval.sval = strdup(yytext); return tok(ID);
}

/* ---------------- Java mode ----------------------------------------------
 * The same compiler also accepts a basic Java program (class Main + static
 * methods, System.out.println, Scanner, int[] arrays ...).  Java-only rules are
 * active only in the JAVA start condition, which main.c switches on for .java
 * files / --java, so C programs are lexed exactly as before. */
int java_mode = 0;
static int brace_depth = 0;
static int class_stack[32], class_sp = 0;        /* brace depth of each open "class X {" */

/* names declared as char (Java prints them as characters, not numbers) */
typedef struct CharVar { char *name; struct CharVar *next; } CharVar;
static CharVar *char_vars = NULL;
static int char_decl = 0;                        /* inside "char a, b ..." */
int java_is_char_var(const char *name) {
    for (CharVar *c = char_vars; c; c = c->next) if (strcmp(c->name, name) == 0) return 1;
    return 0;
}
static void add_char_var(const char *name) {
    if (java_is_char_var(name)) return;
    CharVar *c = malloc(sizeof *c);
    if (!c) return;
    c->name = strdup(name); c->next = char_vars; char_vars = c;
}
/* "sc.nextInt" -> nothing needed but the kind of value read */
static char *str_body(const char *t, int len) { char *q = malloc(len - 1); memcpy(q, t + 1, len - 2); q[len - 2] = 0; return q; }
/* "arr.length" -> "arr" */
static char *name_before_dot(const char *t) {
    size_t n = 0; while (t[n] && t[n] != '.' && t[n] != ' ' && t[n] != '\t') n++;
    char *q = malloc(n + 1); memcpy(q, t, n); q[n] = 0; return q;
}

/* give every token its line number for the parser (@n.first_line) */
#define YY_USER_ACTION yylloc.first_line = yylloc.last_line = yylineno;
%}

DIGIT   [0-9]
ID      [A-Za-z_][A-Za-z0-9_]*

WS      [ \t\r\n]

%x BLOCK_COMMENT
%s JAVA
%s CPP

%%

 /* ======== C++-only rules (cout / cin; active only in the CPP start condition) ======== */
<CPP>"using"{WS}+"namespace"{WS}+"std"{WS}*";"   { /* using namespace std; : ignored */ }
<CPP>"std"{WS}*"::"                       { /* std::cout -> cout */ }
<CPP>"cout"                               { return tok(COUT); }
<CPP>"cin"                                { return tok(CIN); }
<CPP>"endl"                               { return tok(ENDL); }
<CPP>"fixed"                              { return tok(FIXED); }
<CPP>"setprecision"                       { return tok(SETPREC); }
<CPP>"<<"                                 { return tok(SHL); }
<CPP>">>"                                 { return tok(SHR); }
<CPP>"string"                             {
        fprintf(stderr, "Lexical error at line %d: string variables are not supported (text can only be printed, e.g. cout << \"Hi \" << x)\n", yylineno);
        lexical_errors++;
    }
<CPP>("int"|"long"|"short")               { char_decl = 0; return tok(INT); }
<CPP>("float"|"double")                   { char_decl = 0; return tok(FLOAT); }
<CPP>"bool"                               { char_decl = 0; return tok(BOOL); }
<CPP>"char"                               { char_decl = 1; return tok(INT); }
<CPP>'([^'\\\n]|\\.)'                 { yylval.ival = char_value(yytext + 1); return tok(JCHARLIT); }
<CPP>{DIGIT}+"."{DIGIT}+[fF]              { yylval.fval = atof(yytext); return tok(FLOAT_LIT); }
<CPP>[;){]                                { char_decl = 0; return tok(yytext[0]); }

 /* ======== Java-only rules (C programs never reach these) ======== */
<JAVA>"import"[^\n]*                     { /* import java.util.Scanner; : ignored */ }
<JAVA>"package"[^\n]*                    { /* ignored */ }
<JAVA>("public"|"private"|"protected"|"static"|"final"|"abstract"|"synchronized")  { /* modifiers: ignored */ }
<JAVA>"throws"{WS}+{ID}({WS}*","{WS}*{ID})*  { /* ignored */ }
<JAVA>"class"{WS}+{ID}({WS}+"extends"{WS}+{ID})?{WS}*"{"  {
        /* class Main { ... } : the class wrapper is dropped, its members are compiled as top-level code */
        if (class_sp < 32) class_stack[class_sp++] = brace_depth;
        brace_depth++;
    }
<JAVA>"String"{WS}*"["{WS}*"]"{WS}*{ID}   { /* main(String[] args) : ignored */ }
<JAVA>"String"{WS}+{ID}{WS}*"["{WS}*"]"   { /* main(String args[]) : ignored */ }
<JAVA>"String"{WS}*"..."{WS}*{ID}         { /* main(String... args) : ignored */ }
<JAVA>"String"                            {
        fprintf(stderr, "Lexical error at line %d: String variables are not supported (text can only be printed, e.g. System.out.println(\"Hi \" + x))\n", yylineno);
        lexical_errors++;
    }
<JAVA>"Scanner"{WS}+{ID}{WS}*"="{WS}*"new"{WS}+"Scanner"{WS}*"("{WS}*"System"{WS}*"."{WS}*"in"{WS}*")"  {
        /* Scanner sc = new Scanner(System.in);  : input is always stdin */
    }
<JAVA>{ID}{WS}*"."{WS}*"close"{WS}*"("{WS}*")"   { /* sc.close(); : ignored */ }
<JAVA>{ID}{WS}*"."{WS}*("nextInt"|"nextLong"|"nextShort"|"nextByte"){WS}*"("{WS}*")"  { yylval.ival = 'd'; return tok(JREAD); }
<JAVA>{ID}{WS}*"."{WS}*("nextDouble"|"nextFloat"){WS}*"("{WS}*")"                    { yylval.ival = 'f'; return tok(JREAD); }
<JAVA>{ID}{WS}*"."{WS}*"next"{WS}*"("{WS}*")"{WS}*"."{WS}*"charAt"{WS}*"("{WS}*"0"{WS}*")"  { yylval.ival = 'C'; return tok(JREAD); }
<JAVA>{ID}{WS}*"."{WS}*"length"           { yylval.sval = name_before_dot(yytext); return tok(JLENGTH); }
<JAVA>"System"{WS}*"."{WS}*"out"{WS}*"."{WS}*"println"   { return tok(JPRINTLN); }
<JAVA>"System"{WS}*"."{WS}*"out"{WS}*"."{WS}*"print"     { return tok(JPRINT); }
<JAVA>"System"{WS}*"."{WS}*"out"{WS}*"."{WS}*("printf"|"format")  { return tok(PRINTF); }
<JAVA>"new"                               { return tok(JNEW); }
<JAVA>"Math"{WS}*"."{WS}*"PI"             { yylval.fval = 3.141592653589793; return tok(FLOAT_LIT); }
<JAVA>"Math"{WS}*"."{WS}*                 { /* Math.sqrt(x) -> built-in sqrt(x) */ }
<JAVA>"boolean"                           { char_decl = 0; return tok(BOOL); }
<JAVA>("int"|"long"|"short"|"byte")       { char_decl = 0; return tok(INT); }
<JAVA>("float"|"double")                  { char_decl = 0; return tok(FLOAT); }
<JAVA>"char"                              { char_decl = 1; return tok(INT); }
<JAVA>"("{WS}*("int"|"long"|"short"|"byte"|"char"){WS}*")"   { yylval.ival = T_INT;   return tok(JCAST); }
<JAVA>"("{WS}*("double"|"float"){WS}*")"                     { yylval.ival = T_FLOAT; return tok(JCAST); }
<JAVA>{DIGIT}+"."{DIGIT}+[fFdD]           { yylval.fval = atof(yytext); return tok(FLOAT_LIT); }
<JAVA>{DIGIT}+[dDfF]                      { yylval.fval = atof(yytext); return tok(FLOAT_LIT); }
<JAVA>{DIGIT}+[lL]                        { yylval.ival = (int)strtol(yytext, NULL, 10); return tok(INT_LIT); }
<JAVA>\"([^\"\\\n]|\\.)*\"              { yylval.sval = str_body(yytext, yyleng); return tok(JSTRING); }
<JAVA>'([^'\\\n]|\\.)'                { yylval.ival = char_value(yytext + 1); return tok(JCHARLIT); }
<JAVA>":"                                 { return tok(':'); }
<JAVA>"{"                                 { char_decl = 0; brace_depth++; return tok('{'); }
<JAVA>"}"                                 {
        brace_depth--;
        if (class_sp && class_stack[class_sp - 1] == brace_depth) class_sp--;   /* end of the class: dropped */
        else return tok('}');
    }
<JAVA>";"                                 { char_decl = 0; return tok(';'); }
<JAVA>")"                                 { char_decl = 0; return tok(')'); }
 /* ======== end of Java-only rules ======== */

"long"{WS}+"double"                               { return tok(FLOAT); }
("long"|"short")({WS}+"long")?({WS}+"int")?       { return tok(INT); }   /* long, long long, short int ... */
("const"|"signed"|"unsigned"|"static"|"register") { /* qualifiers: ignored */ }
"("{WS}*("int"|"char"|"long"|"short"|"unsigned"){WS}*")"  { yylval.ival = T_INT;   return tok(JCAST); }  /* cast (int) x */
"("{WS}*("float"|"double"){WS}*")"                        { yylval.ival = T_FLOAT; return tok(JCAST); }  /* cast (float) x */
"switch"    { return tok(SWITCH); }
"case"      { return tok(CASE); }
"default"   { return tok(DEFAULT); }
"int"       { return tok(INT); }
"char"      { return tok(INT); }     /* C compatibility: char is stored as an int (use %c to print it) */
"float"     { return tok(FLOAT); }
"double"    { return tok(FLOAT); }   /* C compatibility: double is treated as float */
"bool"      { return tok(BOOL); }
"void"      { return tok(VOID); }
"return"    { return tok(RETURN); }
"for"       { return tok(FOR); }
"printf"    { return tok(PRINTF); }
"scanf"     { return tok(SCANF); }
"do"        { return tok(DO); }
"break"     { return tok(BREAK); }
"continue"  { return tok(CONTINUE); }
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
 /* Java: after the keywords, so "for"/"void" stay keywords; tracks char variables */
<JAVA,CPP>{ID}                            {
        if (char_decl && (cur_tok == INT || cur_tok == ',' || cur_tok == ']')) add_char_var(yytext);
        return ident();
    }
{ID}                  { return ident(); }

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
[{}();,\[\]&?:] { return tok(yytext[0]); }

"//".*                  { /* single-line comment: discarded */ }
"/*"                    { BEGIN(BLOCK_COMMENT); }
<BLOCK_COMMENT>"*/"     { BEGIN(java_mode == 1 ? JAVA : java_mode == 2 ? CPP : INITIAL); }
<BLOCK_COMMENT>.|\n     { /* discarded */ }
<BLOCK_COMMENT><<EOF>>  { fprintf(stderr, "Lexical error at line %d: unterminated block comment\n", yylineno);
                          lexical_errors++; BEGIN(java_mode == 1 ? JAVA : java_mode == 2 ? CPP : INITIAL); }

[ \t\r\n]+  { /* whitespace: discarded */ }

.  { fprintf(stderr, "Lexical error at line %d: invalid token '%s'\n", yylineno, yytext);
     lexical_errors++; }

%%

/* called by main.c before parsing a Java program */
void lexer_java_mode(void) { java_mode = 1; BEGIN(JAVA); }
/* called by main.c before parsing a C++ program */
void lexer_cpp_mode(void) { java_mode = 2; BEGIN(CPP); }
