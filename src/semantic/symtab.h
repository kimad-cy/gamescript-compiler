#ifndef GS_SYMTAB_H
#define GS_SYMTAB_H

#include "ast.h"

/* Symbol table: a chain of scopes. Lookup walks from the innermost scope
 * outwards, so inner declarations shadow outer ones.
 *
 *   global scope   : entities, global functions
 *     entity scope : built-in `position`, properties, methods
 *       event / function scope : parameters, local variables
 *         block scopes (IF / WHILE bodies) : local variables
 */

typedef enum { SYM_VAR, SYM_PARAM, SYM_PROP, SYM_FUNC, SYM_ENTITY } SymKind;

typedef struct Symbol {
    char *name;
    SymKind kind;
    Type ty;              /* variable/property type, function return type */
    Loc loc;
    int used;             /* read at least once (for unused-variable warnings) */
    int builtin;          /* the implicit `position` property */
    void *decl;           /* FuncDecl* or EntityDecl* */
    struct Symbol *next;
} Symbol;

typedef struct Scope {
    Symbol *syms;
    struct Scope *parent;
} Scope;

Scope  *scope_new(Scope *parent);
Symbol *scope_lookup_here(Scope *s, const char *name);   /* this scope only   */
Symbol *scope_lookup(Scope *s, const char *name);        /* whole chain       */
/* Declare a symbol; returns NULL if the name already exists in THIS scope. */
Symbol *scope_declare(Scope *s, const char *name, SymKind kind, Type ty, Loc loc, void *decl);

#endif
