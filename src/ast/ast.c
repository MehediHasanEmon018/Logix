/* Logix AST - construction and printing */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ast/ast.h"

static Node *new_node(NodeKind k, int line) {
    Node *n = calloc(1, sizeof(Node));
    if (!n) { fprintf(stderr, "out of memory\n"); exit(2); }
    n->kind = k; n->line = line; n->type = T_NONE;
    return n;
}

Node *mk_program(Node *s)  { Node *n = new_node(N_PROGRAM, 1); n->a = s; return n; }
Node *mk_block(Node *s, int l) { Node *n = new_node(N_BLOCK, l); n->a = s; return n; }
Node *mk_decl(Type t, char *name, int l) { Node *n = new_node(N_DECL, l); n->type = t; n->name = name; return n; }
Node *mk_decl_init(Type t, char *name, Node *init, int l) { Node *n = mk_decl(t, name, l); n->a = init; return n; }
Node *mk_assign(char *name, Node *e, int l) { Node *n = new_node(N_ASSIGN, l); n->name = name; n->a = e; return n; }
Node *mk_assign_lv(Node *lv, Node *e, int l) { Node *n = mk_assign(lv->name, e, l); n->b = lv->a; return n; }
Node *mk_lv_read(const Node *lv) {
    return lv->kind == N_INDEX ? mk_index(strdup(lv->name), lv->a, lv->line) : mk_id(strdup(lv->name), lv->line);
}
Node *mk_index(char *name, Node *idx, int l) { Node *n = new_node(N_INDEX, l); n->name = name; n->a = idx; return n; }
Node *mk_decl_array(Type t, char *name, int size, Node *init, int l) {
    Node *n = mk_decl(t, name, l); n->is_array = 1; n->size = size; n->b = init; return n;
}
Node *mk_func(Type ret, char *name, Node *params, Node *body, int l) {
    if (strcmp(name, "main") == 0) return body;      /* main is compiled inline, as before */
    Node *n = new_node(N_FUNC, l); n->type = ret; n->name = name; n->a = params; n->b = body; return n;
}
Node *mk_param(Type t, char *name, int is_array, int l) {
    Node *n = new_node(N_PARAM, l); n->type = t; n->name = name; n->is_array = is_array; return n;
}
Node *mk_call(char *name, Node *args, int l) { Node *n = new_node(N_CALL, l); n->name = name; n->a = args; return n; }
Node *mk_dowhile(Node *c, Node *b, int l) { Node *n = new_node(N_DOWHILE, l); n->a = c; n->b = b; return n; }
Node *mk_break(int l)    { return new_node(N_BREAK, l); }
Node *mk_continue(int l) { return new_node(N_CONTINUE, l); }
Node *mk_for(Node *init, Node *cond, Node *step, Node *body, int l) {
    Node *w = mk_while(cond, body, l);
    w->c = step;                       /* kept apart from the body so that 'continue' still runs the step */
    return mk_block(list_append(init, w), l);
}

/* scanf("%d %f", &a, &b[i])  ->  one N_SCAN per argument; ival = d (int) / f (float) / c (char) /
 * C (char, skips leading whitespace) / ? (unsupported conversion) / 0 (no conversion left for this argument) */
Node *mk_scanf(char *fmt, Node *lvs, int l) {
    Node *out = NULL;
    size_t len = strlen(fmt), i = 0;
    while (lvs) {
        Node *lv = lvs; lvs = lvs->next; lv->next = NULL;
        int spec = 0, skipws = 0;
        while (i < len && !spec) {
            if (fmt[i] != '%') { if (fmt[i] == ' ' || fmt[i] == '\\') skipws = 1; i++; continue; }
            i++;
            if (i < len && fmt[i] == '%') { i++; continue; }
            while (i < len && strchr("0123456789*lhz", fmt[i])) i++;
            if (i >= len) break;
            switch (fmt[i]) {
            case 'd': case 'i': case 'u':  spec = 'd'; break;
            case 'f': case 'e': case 'g':  spec = 'f'; break;
            case 'c':                      spec = skipws ? 'C' : 'c'; break;
            default:                       spec = '?';
            }
            i++;
        }
        Node *n = new_node(N_SCAN, lv->line);
        n->name = lv->name; n->b = lv->a; n->ival = spec;
        out = list_append(out, n);
    }
    return out;
}
Node *mk_switch(Node *e, Node *c, int l) { Node *n = new_node(N_SWITCH, l); n->a = e; n->b = c; return n; }
Node *mk_case(Node *v, Node *s, int l)   { Node *n = new_node(N_CASE, l); n->a = v; n->b = s; return n; }
Node *mk_ternary(Node *c, Node *x, Node *y, int l) { Node *n = new_node(N_TERNARY, l); n->a = c; n->b = x; n->c = y; return n; }
Node *mk_print(Node *e, int l) { Node *n = new_node(N_PRINT, l); n->a = e; return n; }
Node *mk_if(Node *c, Node *t, Node *e, int l) { Node *n = new_node(N_IF, l); n->a = c; n->b = t; n->c = e; return n; }
Node *mk_while(Node *c, Node *b, int l) { Node *n = new_node(N_WHILE, l); n->a = c; n->b = b; return n; }
Node *mk_binop(const char *op, Node *a, Node *b, int l) { Node *n = new_node(N_BINOP, l); n->op = op; n->a = a; n->b = b; return n; }
Node *mk_unop(const char *op, Node *e, int l) { Node *n = new_node(N_UNOP, l); n->op = op; n->a = e; return n; }
Node *mk_id(char *name, int l) { Node *n = new_node(N_ID, l); n->name = name; return n; }
Node *mk_int(int v, int l) { Node *n = new_node(N_INT, l); n->ival = v; return n; }
Node *mk_float(double v, int l) { Node *n = new_node(N_FLOAT, l); n->fval = v; return n; }
Node *mk_bool(int v, int l) { Node *n = new_node(N_BOOL, l); n->ival = v; return n; }
Node *mk_str(char *t, int l) { Node *n = new_node(N_STR, l); n->name = t; return n; }
Node *mk_return(Node *e, int l) { Node *n = new_node(N_RETURN, l); n->a = e; return n; }

/* printf("a %d b\n", x)  ->  print "a "; print x; print " b\n"; */
Node *mk_printf(char *fmt, Node *args, int l) {
    Node *out = NULL;
    size_t len = strlen(fmt), bi = 0;
    char *buf = malloc(len + 1);
    if (!buf) { fprintf(stderr, "out of memory\n"); exit(2); }
    for (size_t i = 0; i < len; i++) {
        if (fmt[i] == '%' && i + 1 < len && fmt[i + 1] == '%') { buf[bi++] = '%'; i++; continue; }
        if (fmt[i] == '%' && args) {
            size_t j = i + 1;
            while (j < len && strchr("0123456789.-+ #lhz", fmt[j])) j++;
            if (j < len && strchr("dioufFeEgGcsx", fmt[j])) {
                if (bi) { buf[bi] = 0; out = list_append(out, mk_print(mk_str(strdup(buf), l), l)); bi = 0; }
                Node *a = args; args = args->next; a->next = NULL;
                Node *pr = mk_print(a, l);
                pr->name = malloc(j - i + 2);
                memcpy(pr->name, fmt + i, j - i + 1); pr->name[j - i + 1] = 0;
                out = list_append(out, pr);
                i = j;
                continue;
            }
        }
        buf[bi++] = fmt[i];
    }
    if (bi) { buf[bi] = 0; out = list_append(out, mk_print(mk_str(strdup(buf), l), l)); }
    while (args) {
        Node *a = args; args = args->next; a->next = NULL;
        out = list_append(out, mk_print(a, l));
    }
    free(buf);
    return out;
}

Node *list_append(Node *list, Node *item) {
    if (!item) return list;
    if (!list) return item;
    Node *p = list;
    while (p->next) p = p->next;
    p->next = item;
    return list;
}

const char *type_name(Type t) {
    switch (t) { case T_INT: return "int"; case T_FLOAT: return "float"; case T_BOOL: return "bool"; case T_VOID: return "void"; default: return "none"; }
}

static void indent(int d) { for (int i = 0; i < d; i++) printf("  "); }

static void print_one(const Node *n, int d) {
    indent(d);
    switch (n->kind) {
    case N_PROGRAM: printf("Program\n"); ast_print(n->a, d + 1); break;
    case N_BLOCK:   printf("Block\n");   ast_print(n->a, d + 1); break;
    case N_SWITCH:  printf("Switch (line %d)\n", n->line); print_one(n->a, d + 1); ast_print(n->b, d + 1); break;
    case N_CASE:    if (n->a) { printf("Case\n"); print_one(n->a, d + 1); } else printf("Default\n");
                    ast_print(n->b, d + 1); break;
    case N_TERNARY: printf("Ternary\n"); print_one(n->a, d + 1); print_one(n->b, d + 1); print_one(n->c, d + 1); break;
    case N_LENGTH:  printf("Length %s\n", n->name); break;
    case N_DECL:    if (n->is_array) printf("Decl %s %s[%d] (line %d)\n", type_name(n->type), n->name, n->size, n->line);
                    else printf("Decl %s %s (line %d)\n", type_name(n->type), n->name, n->line);
                    if (n->a) { indent(d + 1); printf("Init:\n"); print_one(n->a, d + 2); }
                    if (n->b) { indent(d + 1); printf("Init list:\n"); ast_print(n->b, d + 2); }
                    break;
    case N_ASSIGN:  printf("Assign %s%s (line %d)\n", n->name, n->b ? "[]" : "", n->line);
                    if (n->b) { indent(d + 1); printf("Index:\n"); print_one(n->b, d + 2); }
                    print_one(n->a, d + 1); break;
    case N_PRINT:   printf("Print%s%s (line %d)\n", n->name ? " " : "", n->name ? n->name : "", n->line); print_one(n->a, d + 1); break;
    case N_IF:
        printf("If (line %d)\n", n->line);
        indent(d + 1); printf("Cond:\n");  print_one(n->a, d + 2);
        indent(d + 1); printf("Then:\n");  ast_print(n->b, d + 2);
        if (n->c) { indent(d + 1); printf("Else:\n"); ast_print(n->c, d + 2); }
        break;
    case N_WHILE:
        printf("While (line %d)\n", n->line);
        indent(d + 1); printf("Cond:\n"); print_one(n->a, d + 2);
        indent(d + 1); printf("Body:\n"); ast_print(n->b, d + 2);
        if (n->c) { indent(d + 1); printf("Step:\n"); ast_print(n->c, d + 2); }
        break;
    case N_DOWHILE:
        printf("DoWhile (line %d)\n", n->line);
        indent(d + 1); printf("Body:\n"); ast_print(n->b, d + 2);
        indent(d + 1); printf("Cond:\n"); print_one(n->a, d + 2);
        break;
    case N_BREAK:    printf("Break (line %d)\n", n->line); break;
    case N_CONTINUE: printf("Continue (line %d)\n", n->line); break;
    case N_FUNC:
        printf("%s %s %s (line %d)\n", n->b ? "Function" : "Prototype", type_name(n->type), n->name, n->line);
        if (n->a) { indent(d + 1); printf("Params:\n"); ast_print(n->a, d + 2); }
        if (n->b) { indent(d + 1); printf("Body:\n"); ast_print(n->b->a, d + 2); }
        break;
    case N_PARAM: printf("Param %s %s%s\n", type_name(n->type), n->name, n->is_array ? "[]" : ""); break;
    case N_CALL:  printf("Call %s (line %d)\n", n->name, n->line); ast_print(n->a, d + 1); break;
    case N_INDEX: printf("Index %s\n", n->name); print_one(n->a, d + 1); break;
    case N_SCAN:  printf("Scan %s%s (format %c, line %d)\n", n->name, n->b ? "[]" : "", n->ival ? n->ival : '-', n->line);
                  if (n->b) print_one(n->b, d + 1);
                  break;
    case N_BINOP: printf("BinOp '%s'\n", n->op); print_one(n->a, d + 1); print_one(n->b, d + 1); break;
    case N_UNOP:  printf("UnOp '%s'\n", n->op);  print_one(n->a, d + 1); break;
    case N_ID:    printf("Id %s\n", n->name); break;
    case N_INT:   printf("Int %d\n", n->ival); break;
    case N_FLOAT: printf("Float %g\n", n->fval); break;
    case N_BOOL:  printf("Bool %s\n", n->ival ? "true" : "false"); break;
    case N_STR:   printf("String \"%s\"\n", n->name); break;
    case N_RETURN: printf("Return (line %d)\n", n->line); if (n->a) print_one(n->a, d + 1); break;
    }
}

void ast_print(const Node *n, int depth) {
    for (; n; n = n->next) print_one(n, depth);
}