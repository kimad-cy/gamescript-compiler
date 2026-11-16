#ifndef GS_CODEGEN_H
#define GS_CODEGEN_H

#include <stdio.h>
#include "ast.h"

/* Translate a checked AST to a Python module that uses runtime/gs_runtime.py.
 * `runtime_dir` is added to sys.path by the generated file (may be NULL). */
void codegen_python(const GameDecl *g, FILE *out, const char *runtime_dir);

#endif
