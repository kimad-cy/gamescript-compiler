#ifndef GS_SEMANTIC_H
#define GS_SEMANTIC_H

#include "ast.h"

/* Check and annotate the AST. Returns the number of semantic errors. */
int semantic_analyze(GameDecl *g);

#endif
