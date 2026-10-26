/* Logix semantic analyzer (manual 4.5) */
#ifndef LOGIX_SEMANTIC_H
#define LOGIX_SEMANTIC_H

#include "ast/ast.h"

/* Walks the AST, fills Node.type on expressions, prints
 * "Semantic error at line N: ..." for each problem.
 * Returns the number of semantic errors found. */
int semantic_check(Node *root);

#endif
