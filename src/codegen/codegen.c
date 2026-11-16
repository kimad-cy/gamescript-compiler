/* codegen.c -- GameScript -> Python code generation
 *
 * Input : the type-checked, name-resolved AST
 * Output: a Python module built on runtime/gs_runtime.py
 *
 * Mapping:
 *   ENTITY X        -> class E_X(Entity)   (properties p_*, methods f_*, events on_*)
 *   global FUNCTION -> def f_name(...)
 *   RULE R          -> def rule_R()  registered with G.add_rule
 *   locals          -> v_name     (the prefixes avoid clashes with Python keywords)
 *
 * Because semantic analysis already resolved every name, this phase never
 * has to look anything up: it just prints what the annotations say.
 */

#include "codegen.h"
#include <stdarg.h>
#include <stdlib.h>
#include <string.h>

static FILE *out;
static int indent;
static const EntityDecl *cur_ent;     /* entity whose code is being emitted  */
static const char *cur_collision;     /* ON COLLISION WITH <this>: `other`   */
static Type cur_ret = TY_VOID;        /* return type of the function being emitted */

/* ------------------------------ output ------------------------------- */

static void line(const char *fmt, ...) {
    for (int i = 0; i < indent; i++) fputs("    ", out);
    va_list ap;
    va_start(ap, fmt);
    vfprintf(out, fmt, ap);
    va_end(ap);
    fputc('\n', out);
}

static void put_pystr(const char *s) {
    fputc('"', out);
    for (; *s; s++) {
        switch (*s) {
        case '"':  fputs("\\\"", out); break;
        case '\\': fputs("\\\\", out); break;
        case '\n': fputs("\\n", out);  break;
        default:
            if ((unsigned char)*s < 32) fprintf(out, "\\x%02x", (unsigned char)*s);
            else fputc(*s, out);
        }
    }
    fputc('"', out);
}

static void put_float(double d) {
    char buf[40];
    for (int prec = 15; prec <= 17; prec++) {      /* shortest text that round-trips */
        snprintf(buf, sizeof buf, "%.*g", prec, d);
        if (strtod(buf, NULL) == d) break;
    }
    fputs(buf, out);
    if (!strpbrk(buf, ".eEn")) fputs(".0", out);   /* keep it a Python float */
}

static const char *default_value(Type t) {
    switch (t) {
    case TY_INT:    return "0";
    case TY_FLOAT:  return "0.0";
    case TY_STRING: return "\"\"";
    case TY_BOOL:   return "False";
    case TY_VEC2:   return "Vec2(0, 0)";
    default:        return "None";
    }
}

/* ---------------------------- expressions ---------------------------- */

static void expr(const Expr *e);

/* INTEGER stored into a FLOAT slot must become a Python float (so it prints as 5.0). */
static void expr_as(const Expr *e, Type target) {
    if (target == TY_FLOAT && e->type == TY_INT) { fputs("float(", out); expr(e); fputc(')', out); }
    else expr(e);
}

/* How to reach entity `name` from the code being emitted. */
static void entity_ref(const char *name) {
    if (cur_collision && strcmp(name, cur_collision) == 0) fputs("other", out);
    else if (cur_ent && strcmp(name, cur_ent->name) == 0)  fputs("self", out);
    else fprintf(out, "G.ref(\"%s\")", name);
}

static void prop_ref(const Name *prop) {
    if (strcmp(prop->s, "position") == 0) fputs("position", out);
    else fprintf(out, "p_%s", prop->s);
}

static void access(const Expr *e) {
    switch (e->base) {
    case B_LOCAL:       fprintf(out, "v_%s", e->path->s); break;
    case B_SELF_PROP:   fputs("self.", out); prop_ref(e->prop); break;
    case B_ENTITY_PROP: entity_ref(e->base_entity); fputc('.', out); prop_ref(e->prop); break;
    default:            fputs("None", out); break;
    }
    if (e->comp) fprintf(out, ".%s", e->comp->s);
}

static void call(const Expr *e) {
    switch (e->ckind) {
    case C_GLOBAL:        fprintf(out, "f_%s(", e->fname); break;
    case C_SELF_METHOD:   fprintf(out, "self.f_%s(", e->fname); break;
    case C_ENTITY_METHOD: entity_ref(e->call_entity); fprintf(out, ".f_%s(", e->fname); break;
    default:              fprintf(out, "None("); break;
    }
    const Param *p = e->callee ? e->callee->params : NULL;
    for (const Expr *a = e->args; a; a = a->next) {
        expr_as(a, p ? p->ty : a->type);
        if (a->next) fputs(", ", out);
        if (p) p = p->next;
    }
    fputc(')', out);
}

static const char *py_op(Op op) {
    switch (op) {
    case OP_AND: return "and";
    case OP_OR:  return "or";
    default:     return op_name(op);       /* + - * / % == != < > <= >= */
    }
}

static void expr(const Expr *e) {
    switch (e->kind) {
    case E_INT:    fprintf(out, "%d", e->ival); break;
    case E_FLOAT:  put_float(e->fval); break;
    case E_STRING: put_pystr(e->sval); break;
    case E_BOOL:   fputs(e->bval ? "True" : "False", out); break;
    case E_VEC2:   fputs("Vec2(", out); expr(e->l); fputs(", ", out); expr(e->r); fputc(')', out); break;
    case E_ACCESS: access(e); break;
    case E_CALL:   call(e); break;
    case E_UNARY:
        if (e->op == OP_NOT) { fputs("(not ", out); expr(e->l); fputc(')', out); }
        else                 { fputs("(-", out);    expr(e->l); fputc(')', out); }
        break;
    case E_BINARY:
        fputc('(', out); expr(e->l);
        fprintf(out, " %s ", py_op(e->op));
        expr(e->r); fputc(')', out);
        break;
    }
}

/* ----------------------------- statements ---------------------------- */

static void stmts(const Stmt *s);

static void block(const Stmt *s) { indent++; stmts(s); indent--; }

/* the object a MOVE / DESTROY acts on, as a Python expression */
static void target_ref(const Stmt *s) {
    if (s->name) entity_ref(s->name); else fputs("self", out);
}

static void stmt(const Stmt *s) {
    switch (s->kind) {
    case S_VAR:
        for (int i = 0; i < indent; i++) fputs("    ", out);
        fprintf(out, "v_%s = ", s->name);
        if (s->e) expr_as(s->e, s->ty); else fputs(default_value(s->ty), out);
        fputc('\n', out);
        break;
    case S_ASSIGN:
        for (int i = 0; i < indent; i++) fputs("    ", out);
        access(s->target); fputs(" = ", out);
        expr_as(s->e, s->target->type);
        fputc('\n', out);
        break;
    case S_IF:
        for (int i = 0; i < indent; i++) fputs("    ", out);
        fputs("if ", out); expr(s->e); fputs(":\n", out);
        block(s->a);
        if (s->b) { line("else:"); block(s->b); }
        break;
    case S_WHILE:
        for (int i = 0; i < indent; i++) fputs("    ", out);
        fputs("while ", out); expr(s->e); fputs(":\n", out);
        block(s->a);
        break;
    case S_RETURN:
        for (int i = 0; i < indent; i++) fputs("    ", out);
        if (s->e) {
            fputs("return ", out);
            expr_as(s->e, cur_ret);        /* INTEGER -> FLOAT widening */
            fputc('\n', out);
        } else fputs("return\n", out);
        break;
    case S_MOVE:
        for (int i = 0; i < indent; i++) fputs("    ", out);
        target_ref(s); fprintf(out, ".move(\"%s\", ", dir_name(s->dir));
        expr(s->e); fputs(")\n", out);
        break;
    case S_SPAWN:
        for (int i = 0; i < indent; i++) fputs("    ", out);
        fprintf(out, "G.spawn(E_%s, ", s->name); expr(s->e); fputs(")\n", out);
        break;
    case S_DESTROY:
        for (int i = 0; i < indent; i++) fputs("    ", out);
        fputs("G.destroy(", out); target_ref(s); fputs(")\n", out);
        break;
    case S_SOUND:
        for (int i = 0; i < indent; i++) fputs("    ", out);
        fputs("G.play_sound(", out); expr(s->e); fputs(")\n", out);
        break;
    case S_PRINT:
        for (int i = 0; i < indent; i++) fputs("    ", out);
        fputs("G.print(", out); expr(s->e); fputs(")\n", out);
        break;
    case S_WIN:  line("G.win()");  break;
    case S_LOSE: line("G.lose()"); break;
    case S_CALL:
        for (int i = 0; i < indent; i++) fputs("    ", out);
        expr(s->e); fputc('\n', out);
        break;
    }
}

static void stmts(const Stmt *s) {
    if (!s) { line("pass"); return; }
    for (; s; s = s->next) stmt(s);
}

/* ---------------------------- declarations --------------------------- */

static void params(const FuncDecl *f, int with_self) {
    if (with_self) fputs("self", out);
    for (const Param *p = f->params; p; p = p->next)
        fprintf(out, "%sv_%s", (with_self || p != f->params) ? ", " : "", p->name);
}

static void function(const FuncDecl *f, int is_method) {
    for (int i = 0; i < indent; i++) fputs("    ", out);
    fprintf(out, "def f_%s(", f->name);
    params(f, is_method);
    fputs("):\n", out);
    cur_ret = f->ret;
    block(f->body);
    cur_ret = TY_VOID;
    fputc('\n', out);
}

/* Python name of an event handler method. */
static void handler_name(const Event *ev, char *buf, size_t n) {
    switch (ev->kind) {
    case EV_START:     snprintf(buf, n, "on_start"); break;
    case EV_UPDATE:    snprintf(buf, n, "on_update"); break;
    case EV_DEATH:     snprintf(buf, n, "on_death"); break;
    case EV_COLLISION: snprintf(buf, n, "on_collision_%s", ev->arg); break;
    case EV_KEY:       snprintf(buf, n, "on_key_%s", ev->arg); break;
    }
}

static void entity(const EntityDecl *ed) {
    cur_ent = ed;
    line("class E_%s(Entity):", ed->name);
    indent++;
    line("NAME = \"%s\"", ed->name);

    for (int i = 0; i < indent; i++) fputs("    ", out);
    fputs("PROPS = [\"position\"", out);
    for (const Prop *p = ed->props; p; p = p->next) fprintf(out, ", \"%s\"", p->name);
    fputs("]\n", out);

    for (int i = 0; i < indent; i++) fputs("    ", out);
    fputs("COLLISIONS = {", out);
    int first = 1;
    for (const Event *ev = ed->events; ev; ev = ev->next)
        if (ev->kind == EV_COLLISION) {
            fprintf(out, "%s\"%s\": \"on_collision_%s\"", first ? "" : ", ", ev->arg, ev->arg);
            first = 0;
        }
    fputs("}\n", out);

    for (int i = 0; i < indent; i++) fputs("    ", out);
    fputs("KEYS = {", out);
    first = 1;
    for (const Event *ev = ed->events; ev; ev = ev->next)
        if (ev->kind == EV_KEY) {
            fprintf(out, "%s\"%s\": \"on_key_%s\"", first ? "" : ", ", ev->arg, ev->arg);
            first = 0;
        }
    fputs("}\n\n", out);

    line("def __init__(self, game, pos=None):");
    indent++;
    line("Entity.__init__(self, game, pos)");
    for (const Prop *p = ed->props; p; p = p->next) {
        for (int i = 0; i < indent; i++) fputs("    ", out);
        fprintf(out, "self.p_%s = ", p->name);
        if (p->init) expr_as(p->init, p->ty); else fputs(default_value(p->ty), out);
        fputc('\n', out);
    }
    indent--;
    fputc('\n', out);

    for (const FuncDecl *f = ed->funcs; f; f = f->next) function(f, 1);

    for (const Event *ev = ed->events; ev; ev = ev->next) {
        char name[128];
        handler_name(ev, name, sizeof name);
        cur_collision = (ev->kind == EV_COLLISION) ? ev->arg : NULL;
        if (ev->kind == EV_COLLISION) line("def %s(self, other):", name);
        else                          line("def %s(self):", name);
        block(ev->body);
        fputc('\n', out);
        cur_collision = NULL;
    }
    indent--;
    cur_ent = NULL;
}

void codegen_python(const GameDecl *g, FILE *o, const char *runtime_dir) {
    out = o; indent = 0; cur_ent = NULL; cur_collision = NULL;

    fprintf(out, "# Generated by gsc from GAME %s -- do not edit.\n", g->name);
    fputs("import os, sys\n", out);
    fputs("sys.path[:0] = [os.path.dirname(os.path.abspath(__file__))", out);
    if (runtime_dir) { fputs(", ", out); put_pystr(runtime_dir); }
    fputs("]\n", out);
    fputs("from gs_runtime import *\n\n", out);
    fprintf(out, "G = Game(\"%s\")\n\n", g->name);

    for (const FuncDecl *f = g->funcs; f; f = f->next) function(f, 0);
    for (const EntityDecl *e = g->entities; e; e = e->next) { entity(e); fputc('\n', out); }

    for (const Rule *r = g->rules; r; r = r->next) {
        line("def rule_%s():", r->name);
        block(r->body);
        fputc('\n', out);
    }

    for (const EntityDecl *e = g->entities; e; e = e->next) line("G.register(E_%s)", e->name);
    for (const Rule *r = g->rules; r; r = r->next) line("G.add_rule(\"%s\", rule_%s)", r->name, r->name);
    fputs("\nG.init_defaults()\n\n", out);
    fputs("if __name__ == \"__main__\":\n    main(G)\n", out);
}
