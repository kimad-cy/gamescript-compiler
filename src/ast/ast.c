#include "ast.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

GameDecl *g_ast = NULL;

void *xcalloc(size_t n) {
    void *p = calloc(1, n);
    if (!p) { fputs("gsc: out of memory\n", stderr); exit(2); }
    return p;
}

char *xstrdup(const char *s) {
    char *p = strdup(s);
    if (!p) { fputs("gsc: out of memory\n", stderr); exit(2); }
    return p;
}

const char *type_name(Type t) {
    switch (t) {
    case TY_VOID:   return "VOID";
    case TY_INT:    return "INTEGER";
    case TY_FLOAT:  return "FLOAT";
    case TY_STRING: return "STRING";
    case TY_BOOL:   return "BOOLEAN";
    case TY_VEC2:   return "VECTOR2";
    default:        return "<error>";
    }
}

const char *op_name(Op o) {
    switch (o) {
    case OP_ADD: return "+";   case OP_SUB: return "-";
    case OP_MUL: return "*";   case OP_DIV: return "/";   case OP_MOD: return "%";
    case OP_EQ:  return "==";  case OP_NEQ: return "!=";
    case OP_LT:  return "<";   case OP_GT:  return ">";
    case OP_LE:  return "<=";  case OP_GE:  return ">=";
    case OP_AND: return "AND"; case OP_OR:  return "OR";
    case OP_NOT: return "NOT"; case OP_NEG: return "-";
    }
    return "?";
}

const char *dir_name(Dir d) {
    switch (d) {
    case DIR_UP: return "UP";   case DIR_DOWN: return "DOWN";
    case DIR_LEFT: return "LEFT"; default: return "RIGHT";
    }
}

Name *mk_name(char *s, Loc loc) {
    Name *n = xcalloc(sizeof *n);
    n->s = s; n->loc = loc;
    return n;
}

int name_count(const Name *n) {
    int c = 0;
    for (; n; n = n->next) c++;
    return c;
}

Expr *mk_expr(ExprKind k, Loc loc) {
    Expr *e = xcalloc(sizeof *e);
    e->kind = k; e->loc = loc; e->type = TY_ERR;
    return e;
}

Expr *mk_binary(Op op, Expr *l, Expr *r, Loc loc) {
    Expr *e = mk_expr(E_BINARY, loc);
    e->op = op; e->l = l; e->r = r;
    return e;
}

Expr *mk_unary(Op op, Expr *x, Loc loc) {
    Expr *e = mk_expr(E_UNARY, loc);
    e->op = op; e->l = x;
    return e;
}

Stmt *mk_stmt(StmtKind k, Loc loc) {
    Stmt *s = xcalloc(sizeof *s);
    s->kind = k; s->loc = loc;
    return s;
}

Event *mk_event(EventKind k, char *arg, Stmt *body, Loc loc) {
    Event *ev = xcalloc(sizeof *ev);
    ev->kind = k; ev->arg = arg; ev->body = body; ev->loc = loc;
    return ev;
}
