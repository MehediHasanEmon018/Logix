/* Logix driver: lexer -> parser -> AST -> semantic -> TAC */
#include <stdio.h>
#include <string.h>
#include "ast/ast.h"
#include "semantic/semantic.h"
#include "symtab/symtab.h"
#include "icg/icg.h"
#include "interp/interp.h"

extern FILE *yyin;
extern int yyparse(void);
extern int lexical_errors;
extern int syntax_errors;
extern Node *ast_root;
extern void lexer_java_mode(void);
extern void lexer_cpp_mode(void);

/* C++ source?  --cpp, a .cpp/.cc file, or cout / cin / <iostream> in the text */
static int looks_like_cpp(const char *path) {
    size_t n = strlen(path);
    if ((n > 4 && strcmp(path + n - 4, ".cpp") == 0) || (n > 3 && strcmp(path + n - 3, ".cc") == 0)) return 1;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    static char buf[1 << 16];
    size_t len = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[len] = 0;
    return strstr(buf, "<iostream>") || strstr(buf, "using namespace std") || strstr(buf, "std::") ||
           strstr(buf, "cout <<") || strstr(buf, "cout<<") || strstr(buf, "cin >>") || strstr(buf, "cin>>");
}

/* Java source?  --java, a .java file name, or (for .mc files from the web editor)
 * tell-tale Java text such as "System.out." / "public static void main" */
static int looks_like_java(const char *path) {
    size_t n = strlen(path);
    if (n > 5 && strcmp(path + n - 5, ".java") == 0) return 1;
    FILE *f = fopen(path, "r");
    if (!f) return 0;
    static char buf[1 << 16];
    size_t len = fread(buf, 1, sizeof buf - 1, f);
    fclose(f);
    buf[len] = 0;
    return strstr(buf, "System.out.") || strstr(buf, "public static void main") ||
           strstr(buf, "import java.") || strstr(buf, "static void main(String");
}

int main(int argc, char **argv) {
    int show_ast = 0, show_symtab = 0, run = 0, java = 0, cpp = 0;
    const char *path = NULL;

    for (int i = 1; i < argc; i++) {
        if (strcmp(argv[i], "--ast") == 0) show_ast = 1;
        else if (strcmp(argv[i], "--symtab") == 0) show_symtab = 1;
        else if (strcmp(argv[i], "--run") == 0) run = 1;
        else if (strcmp(argv[i], "--java") == 0) java = 1;
        else if (strcmp(argv[i], "--cpp") == 0) cpp = 1;
        else path = argv[i];
    }
    if (!path) {
        fprintf(stderr, "Usage: %s <source.mc|source.java> [--ast] [--symtab] [--run] [--java] [--cpp]\n", argv[0]);
        return 2;
    }
    yyin = fopen(path, "r");
    if (!yyin) {
        perror(path);
        return 2;
    }
    if (java || looks_like_java(path)) lexer_java_mode();
    else if (cpp || looks_like_cpp(path)) lexer_cpp_mode();
    yyparse();
    fclose(yyin);

    if (lexical_errors || syntax_errors) {
        fprintf(stderr, "Compilation failed: %d lexical error(s), %d syntax error(s)\n",
                lexical_errors, syntax_errors);
        return 1;
    }
    if (show_ast) ast_print(ast_root, 0);

    int sem_errors = semantic_check(ast_root);
    if (sem_errors) {
        fprintf(stderr, "Compilation failed: %d semantic error(s)\n", sem_errors);
        return 1;
    }
    if (show_symtab) st_dump();

    /* --run: generate the TAC, execute it and print the program's output (like an online C compiler) */
    if (run) {
        FILE *tmp = tmpfile();
        if (!tmp) { perror("tmpfile"); return 2; }
        tac_generate(ast_root, tmp);
        rewind(tmp);
        int rc = interp_run(tmp);
        fclose(tmp);
        return rc ? 3 : 0;
    }

    /* Final phase: Three Address Code goes to stdout (redirect with > file.tac) */
    tac_generate(ast_root, stdout);
    fprintf(stderr, "Compilation successful: TAC generated.\n");
    return 0;
}