#include "symtab.h"
#include <string.h>

Scope *scope_new(Scope *parent) {
    Scope *s = xcalloc(sizeof *s);
    s->parent = parent;
    return s;
}

Symbol *scope_lookup_here(Scope *s, const char *name) {
    for (Symbol *y = s->syms; y; y = y->next)
        if (strcmp(y->name, name) == 0) return y;
    return NULL;
}

Symbol *scope_lookup(Scope *s, const char *name) {
    for (; s; s = s->parent) {
        Symbol *y = scope_lookup_here(s, name);
        if (y) return y;
    }
    return NULL;
}

Symbol *scope_declare(Scope *s, const char *name, SymKind kind, Type ty, Loc loc, void *decl) {
    if (scope_lookup_here(s, name)) return NULL;
    Symbol *y = xcalloc(sizeof *y);
    y->name = xstrdup(name);
    y->kind = kind; y->ty = ty; y->loc = loc; y->decl = decl;
    LIST_APPEND(s->syms, y);
    return y;
}
