#include "ast.h"
#include <stdio.h>
#include <string.h>

static void pad(int n) { for (int i = 0; i < n; i++) fputs("  ", stdout); }

static void print_path(const Name *n) {
    for (; n; n = n->next) printf("%s%s", n->s, n->next ? "." : "");
}

static void print_expr(const Expr *e, int d) {
    pad(d);
    switch (e->kind) {
    case E_INT:    printf("Int %d\n", e->ival); break;
    case E_FLOAT:  printf("Float %g\n", e->fval); break;
    case E_STRING: printf("String \"%s\"\n", e->sval); break;
    case E_BOOL:   printf("Bool %s\n", e->bval ? "TRUE" : "FALSE"); break;
    case E_VEC2:
        printf("Vector2\n"); print_expr(e->l, d + 1); print_expr(e->r, d + 1); break;
    case E_ACCESS: printf("Access "); print_path(e->path); putchar('\n'); break;
    case E_CALL:
        printf("Call ");
        if (e->path) printf("%s.", e->path->s);
        printf("%s\n", e->fname);
        for (const Expr *a = e->args; a; a = a->next) print_expr(a, d + 1);
        break;
    case E_UNARY:
        printf("Unary %s\n", op_name(e->op)); print_expr(e->l, d + 1); break;
    case E_BINARY:
        printf("Binary %s\n", op_name(e->op));
        print_expr(e->l, d + 1); print_expr(e->r, d + 1); break;
    }
}

static void print_stmts(const Stmt *s, int d);

static void print_stmt(const Stmt *s, int d) {
    pad(d);
    switch (s->kind) {
    case S_VAR:
        printf("Var %s : %s\n", s->name, type_name(s->ty));
        if (s->e) print_expr(s->e, d + 1);
        break;
    case S_ASSIGN:
        printf("Assign\n"); print_expr(s->target, d + 1); print_expr(s->e, d + 1); break;
    case S_IF:
        printf("If\n"); print_expr(s->e, d + 1);
        pad(d + 1); printf("Then\n"); print_stmts(s->a, d + 2);
        if (s->b) { pad(d + 1); printf("Else\n"); print_stmts(s->b, d + 2); }
        break;
    case S_WHILE:
        printf("While\n"); print_expr(s->e, d + 1); print_stmts(s->a, d + 1); break;
    case S_RETURN:
        printf("Return\n"); if (s->e) print_expr(s->e, d + 1); break;
    case S_MOVE:
        printf("Move %s %s\n", s->name ? s->name : "SELF", dir_name(s->dir));
        print_expr(s->e, d + 1); break;
    case S_SPAWN:
        printf("Spawn %s\n", s->name); print_expr(s->e, d + 1); break;
    case S_DESTROY:
        printf("Destroy %s\n", s->name ? s->name : "SELF"); break;
    case S_SOUND: printf("PlaySound\n"); print_expr(s->e, d + 1); break;
    case S_PRINT: printf("Print\n");     print_expr(s->e, d + 1); break;
    case S_WIN:   printf("Win\n"); break;
    case S_LOSE:  printf("Lose\n"); break;
    case S_CALL:  printf("CallStmt\n"); print_expr(s->e, d + 1); break;
    }
}

static void print_stmts(const Stmt *s, int d) {
    for (; s; s = s->next) print_stmt(s, d);
}

static void print_func(const FuncDecl *f, int d) {
    pad(d);
    printf("Function %s(", f->name);
    for (const Param *p = f->params; p; p = p->next)
        printf("%s: %s%s", p->name, type_name(p->ty), p->next ? ", " : "");
    printf(") : %s\n", type_name(f->ret));
    print_stmts(f->body, d + 1);
}

void ast_print(const GameDecl *g) {
    printf("Game %s\n", g->name);
    for (const EntityDecl *e = g->entities; e; e = e->next) {
        pad(1); printf("Entity %s\n", e->name);
        for (const Prop *p = e->props; p; p = p->next) {
            pad(2); printf("Property %s : %s\n", p->name, type_name(p->ty));
            if (p->init) print_expr(p->init, 3);
        }
        for (const FuncDecl *f = e->funcs; f; f = f->next) print_func(f, 2);
        for (const Event *ev = e->events; ev; ev = ev->next) {
            pad(2);
            switch (ev->kind) {
            case EV_START:     printf("On Start\n"); break;
            case EV_UPDATE:    printf("On Update\n"); break;
            case EV_DEATH:     printf("On Death\n"); break;
            case EV_COLLISION: printf("On Collision With %s\n", ev->arg); break;
            case EV_KEY:       printf("On KeyPress \"%s\"\n", ev->arg); break;
            }
            print_stmts(ev->body, 3);
        }
    }
    for (const FuncDecl *f = g->funcs; f; f = f->next) print_func(f, 1);
    for (const Rule *r = g->rules; r; r = r->next) {
        pad(1); printf("Rule %s\n", r->name);
        print_stmts(r->body, 2);
    }
}
