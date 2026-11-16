/* semantic.c -- GameScript semantic analysis
 *
 * Input : the AST from the parser
 * Output: the same AST, annotated (Expr.type, resolved names), plus
 *         semantic errors/warnings reported with line and column.
 *
 * Two passes over the program:
 *   pass 1  declare every entity, global function, property and method
 *           (so forward references and recursion work);
 *   pass 2  check bodies: types, scopes, calls, events and rules.
 */

#include "semantic.h"
#include "symtab.h"
#include "errors.h"
#include <string.h>

#define SERR(loc, ...) report_error(ERR_SEMANTIC, (loc).line, (loc).col, __VA_ARGS__)

typedef struct {
    Scope      *scope;
    EntityDecl *ent;     /* enclosing entity, or NULL (global function / rule) */
    FuncDecl   *func;    /* enclosing function, or NULL                        */
    Rule       *rule;    /* enclosing rule, or NULL                            */
    Event      *event;   /* enclosing event handler, or NULL                   */
} Ctx;

static GameDecl *G;
static Scope *global_scope;

static const char *VALID_KEYS[] = {
    "SPACE", "UP", "DOWN", "LEFT", "RIGHT", "ENTER", "ESCAPE",
    "A","B","C","D","E","F","G","H","I","J","K","L","M",
    "N","O","P","Q","R","S","T","U","V","W","X","Y","Z",
    "0","1","2","3","4","5","6","7","8","9", NULL
};

/* ------------------------------ helpers ------------------------------ */

static int is_num(Type t)  { return t == TY_INT || t == TY_FLOAT; }

/* Can a value of type `src` be stored in a slot of type `dst`?
 * Same type, or INTEGER widened to FLOAT. TY_ERR is accepted silently so
 * one mistake does not produce a cascade of follow-up errors. */
static int assignable(Type dst, Type src) {
    if (dst == TY_ERR || src == TY_ERR) return 1;
    return dst == src || (dst == TY_FLOAT && src == TY_INT);
}

static EntityDecl *find_entity(const char *name) {
    for (EntityDecl *e = G->entities; e; e = e->next)
        if (strcmp(e->name, name) == 0) return e;
    return NULL;
}

static FuncDecl *find_method(EntityDecl *ed, const char *name) {
    Symbol *s = scope_lookup_here(ed->scope, name);
    return (s && s->kind == SYM_FUNC) ? (FuncDecl *)s->decl : NULL;
}

static int is_const_zero(const Expr *e) {
    if (e->kind == E_INT)   return e->ival == 0;
    if (e->kind == E_FLOAT) return e->fval == 0.0;
    if (e->kind == E_UNARY && e->op == OP_NEG) return is_const_zero(e->l);
    return 0;
}

/* Constant expression: literals combined with operators (no names, no calls). */
static int is_const_expr(const Expr *e) {
    switch (e->kind) {
    case E_INT: case E_FLOAT: case E_STRING: case E_BOOL: return 1;
    case E_VEC2:   return is_const_expr(e->l) && is_const_expr(e->r);
    case E_UNARY:  return is_const_expr(e->l);
    case E_BINARY: return is_const_expr(e->l) && is_const_expr(e->r);
    default:       return 0;
    }
}

static int stmt_returns(const Stmt *s);
static int always_returns(const Stmt *list) {
    for (; list; list = list->next)
        if (stmt_returns(list)) return 1;
    return 0;
}
static int stmt_returns(const Stmt *s) {
    if (s->kind == S_RETURN) return 1;
    if (s->kind == S_IF) return s->b && always_returns(s->a) && always_returns(s->b);
    return 0;
}

static void warn_unused(Scope *sc) {
    for (Symbol *y = sc->syms; y; y = y->next)
        if (y->kind == SYM_VAR && !y->used)
            report_warning(y->loc.line, y->loc.col,
                           "variable '%s' is declared but never used", y->name);
}

/* ---------------------------- expressions ---------------------------- */

static Type check_expr(Ctx *c, Expr *e);

/* Like check_expr, but a value is required (a VOID call is an error). */
static Type check_value(Ctx *c, Expr *e) {
    Type t = check_expr(c, e);
    if (t == TY_VOID) {
        SERR(e->loc, "function '%s' returns no value, so it cannot be used in an expression",
             e->fname);
        e->type = TY_ERR;
        return TY_ERR;
    }
    return t;
}

/* Resolve  x | prop | SELF.prop | Entity.prop | <any>.x/.y  and return its type. */
static Type resolve_access(Ctx *c, Expr *e, int is_write) {
    Name *n0 = e->path, *rest = NULL;
    Type t = TY_ERR;
    e->base = B_NONE; e->prop = NULL; e->comp = NULL; e->base_entity = NULL;

    if (strcmp(n0->s, "SELF") == 0) {
        if (!c->ent) { SERR(n0->loc, "SELF can only be used inside an entity"); return TY_ERR; }
        if (!n0->next) { SERR(n0->loc, "SELF is not a value; use SELF.property"); return TY_ERR; }
        Symbol *p = scope_lookup_here(c->ent->scope, n0->next->s);
        if (!p || p->kind != SYM_PROP) {
            SERR(n0->next->loc, "entity '%s' has no property '%s'", c->ent->name, n0->next->s);
            return TY_ERR;
        }
        e->base = B_SELF_PROP; e->prop = n0->next; t = p->ty; rest = n0->next->next;
    } else {
        Symbol *sym = scope_lookup(c->scope, n0->s);
        if (!sym) { SERR(n0->loc, "undeclared identifier '%s'", n0->s); return TY_ERR; }
        switch (sym->kind) {
        case SYM_VAR: case SYM_PARAM:
            e->base = B_LOCAL; t = sym->ty; rest = n0->next;
            if (!is_write) sym->used = 1;
            break;
        case SYM_PROP:
            e->base = B_SELF_PROP; e->prop = n0; t = sym->ty; rest = n0->next;
            break;
        case SYM_FUNC:
            SERR(n0->loc, "'%s' is a function; call it with parentheses", n0->s);
            return TY_ERR;
        case SYM_ENTITY: {
            EntityDecl *ed = (EntityDecl *)sym->decl;
            if (!n0->next) {
                SERR(n0->loc, "entity '%s' is not a value; use %s.property", n0->s, n0->s);
                return TY_ERR;
            }
            Symbol *p = scope_lookup_here(ed->scope, n0->next->s);
            if (!p || p->kind != SYM_PROP) {
                SERR(n0->next->loc, "entity '%s' has no property '%s'", ed->name, n0->next->s);
                return TY_ERR;
            }
            e->base = B_ENTITY_PROP; e->base_entity = ed->name;
            e->prop = n0->next; t = p->ty; rest = n0->next->next;
            break; }
        }
    }

    if (rest) {                                   /* vector component .x / .y */
        if (t != TY_VEC2) {
            SERR(rest->loc, "cannot access member '%s' of type %s", rest->s, type_name(t));
            return TY_ERR;
        }
        if (strcmp(rest->s, "x") != 0 && strcmp(rest->s, "y") != 0) {
            SERR(rest->loc, "VECTOR2 has no member '%s' (members are x and y)", rest->s);
            return TY_ERR;
        }
        if (rest->next) { SERR(rest->next->loc, "too many member accesses"); return TY_ERR; }
        if (is_write) {
            SERR(rest->loc, "cannot assign to vector component '%s'; assign a whole VECTOR2", rest->s);
            return TY_ERR;
        }
        e->comp = rest; t = TY_FLOAT;
    }
    return t;
}

static Type check_call(Ctx *c, Expr *e) {
    FuncDecl *fd = NULL;
    e->ckind = C_NONE; e->callee = NULL; e->call_entity = NULL;

    if (e->path) {                                          /* qualified call */
        Name *q = e->path;
        if (strcmp(q->s, "SELF") == 0) {
            if (!c->ent) SERR(q->loc, "SELF can only be used inside an entity");
            else {
                fd = find_method(c->ent, e->fname);
                e->ckind = C_SELF_METHOD;
                if (!fd) SERR(e->loc, "entity '%s' has no function '%s'", c->ent->name, e->fname);
            }
        } else {
            EntityDecl *ed = find_entity(q->s);
            if (!ed) SERR(q->loc, "unknown entity '%s'", q->s);
            else {
                fd = find_method(ed, e->fname);
                e->ckind = C_ENTITY_METHOD; e->call_entity = ed->name;
                if (!fd) SERR(e->loc, "entity '%s' has no function '%s'", ed->name, e->fname);
            }
        }
    } else {                                                /* plain call     */
        Symbol *s = scope_lookup(c->scope, e->fname);
        if (!s) SERR(e->loc, "undeclared function '%s'", e->fname);
        else if (s->kind != SYM_FUNC) SERR(e->loc, "'%s' is not a function", e->fname);
        else {
            fd = (FuncDecl *)s->decl;
            e->ckind = fd->owner ? C_SELF_METHOD : C_GLOBAL;
        }
    }
    e->callee = fd;

    /* arguments are always checked, even when the callee is unknown */
    int nargs = 0;
    for (Expr *a = e->args; a; a = a->next) { check_value(c, a); nargs++; }

    if (fd) {
        int nparams = 0;
        for (Param *p = fd->params; p; p = p->next) nparams++;
        if (nargs != nparams) {
            SERR(e->loc, "function '%s' expects %d argument(s) but got %d",
                 fd->name, nparams, nargs);
        } else {
            int i = 1;
            Param *p = fd->params;
            for (Expr *a = e->args; a; a = a->next, p = p->next, i++)
                if (!assignable(p->ty, a->type))
                    SERR(a->loc, "argument %d of '%s': expected %s but got %s",
                         i, fd->name, type_name(p->ty), type_name(a->type));
        }
        return fd->ret;
    }
    return TY_ERR;
}

static Type check_binary(Ctx *c, Expr *e) {
    Type a = check_value(c, e->l), b = check_value(c, e->r);
    Op op = e->op;
    int cmp_or_logic = (op >= OP_EQ && op <= OP_OR);
    if (a == TY_ERR || b == TY_ERR) return cmp_or_logic ? TY_BOOL : TY_ERR;

    #define BAD() do { SERR(e->loc, "operator '%s' cannot be applied to %s and %s", \
                            op_name(op), type_name(a), type_name(b)); \
                       return cmp_or_logic ? TY_BOOL : TY_ERR; } while (0)

    switch (op) {
    case OP_ADD:
        if (is_num(a) && is_num(b)) return (a == TY_INT && b == TY_INT) ? TY_INT : TY_FLOAT;
        if (a == TY_STRING && b == TY_STRING) return TY_STRING;
        if (a == TY_VEC2 && b == TY_VEC2) return TY_VEC2;
        BAD();
    case OP_SUB:
        if (is_num(a) && is_num(b)) return (a == TY_INT && b == TY_INT) ? TY_INT : TY_FLOAT;
        if (a == TY_VEC2 && b == TY_VEC2) return TY_VEC2;
        BAD();
    case OP_MUL:
        if (is_num(a) && is_num(b)) return (a == TY_INT && b == TY_INT) ? TY_INT : TY_FLOAT;
        if (a == TY_VEC2 && is_num(b)) return TY_VEC2;
        BAD();
    case OP_DIV:
        if (!(is_num(a) && is_num(b))) BAD();
        if (is_const_zero(e->r)) SERR(e->loc, "division by zero (the divisor is the constant 0)");
        return TY_FLOAT;
    case OP_MOD:
        if (!(a == TY_INT && b == TY_INT)) BAD();
        if (is_const_zero(e->r)) SERR(e->loc, "modulo by zero (the divisor is the constant 0)");
        return TY_INT;
    case OP_EQ: case OP_NEQ:
        if (a == b || (is_num(a) && is_num(b))) return TY_BOOL;
        BAD();
    case OP_LT: case OP_GT: case OP_LE: case OP_GE:
        if (is_num(a) && is_num(b)) return TY_BOOL;
        BAD();
    case OP_AND: case OP_OR:
        if (a == TY_BOOL && b == TY_BOOL) return TY_BOOL;
        BAD();
    default:
        return TY_ERR;
    }
    #undef BAD
}

static Type check_expr(Ctx *c, Expr *e) {
    Type t = TY_ERR;
    switch (e->kind) {
    case E_INT:    t = TY_INT; break;
    case E_FLOAT:  t = TY_FLOAT; break;
    case E_STRING: t = TY_STRING; break;
    case E_BOOL:   t = TY_BOOL; break;
    case E_VEC2: {
        Type a = check_value(c, e->l), b = check_value(c, e->r);
        if ((a != TY_ERR && !is_num(a)) || (b != TY_ERR && !is_num(b)))
            SERR(e->loc, "VECTOR2 components must be numbers (got %s and %s)",
                 type_name(a), type_name(b));
        t = TY_VEC2;
        break; }
    case E_ACCESS: t = resolve_access(c, e, 0); break;
    case E_CALL:   t = check_call(c, e); break;
    case E_UNARY: {
        Type a = check_value(c, e->l);
        if (e->op == OP_NEG) {
            if (a == TY_ERR) t = TY_ERR;
            else if (is_num(a)) t = a;
            else SERR(e->loc, "operator '-' cannot be applied to %s", type_name(a));
        } else { /* OP_NOT */
            t = TY_BOOL;
            if (a != TY_ERR && a != TY_BOOL)
                SERR(e->loc, "operator 'NOT' cannot be applied to %s", type_name(a));
        }
        break; }
    case E_BINARY: t = check_binary(c, e); break;
    }
    e->type = t;
    return t;
}

/* ----------------------------- statements ---------------------------- */

static void check_stmts(Ctx *c, Stmt *s);

static void check_block(Ctx *c, Stmt *list) {
    Scope *saved = c->scope;
    c->scope = scope_new(saved);
    check_stmts(c, list);
    warn_unused(c->scope);
    c->scope = saved;
}

static void require_bool(Ctx *c, Expr *cond, const char *what) {
    Type t = check_value(c, cond);
    if (t != TY_ERR && t != TY_BOOL)
        SERR(cond->loc, "%s must be BOOLEAN but is %s", what, type_name(t));
}

static void forbid_in_rule(Ctx *c, Stmt *s, const char *what) {
    if (c->rule)
        SERR(s->loc, "%s is not allowed inside RULE '%s' (rules only observe the game)",
             what, c->rule->name);
}

/* Resolve the entity a MOVE / DESTROY acts on. NULL (plus an error) if invalid. */
static void check_target(Ctx *c, Stmt *s, const char *what) {
    if (s->name) {
        if (!find_entity(s->name)) SERR(s->loc, "unknown entity '%s' in %s", s->name, what);
    } else if (!c->ent) {
        if (s->is_self) SERR(s->loc, "SELF can only be used inside an entity");
        else SERR(s->loc, "%s has no target: name an entity (not inside an entity here)", what);
    }
}

static void check_stmt(Ctx *c, Stmt *s) {
    switch (s->kind) {
    case S_VAR: {
        Type t = s->e ? check_value(c, s->e) : s->ty;
        if (s->e && !assignable(s->ty, t))
            SERR(s->e->loc, "cannot initialize variable '%s' of type %s with %s",
                 s->name, type_name(s->ty), type_name(t));
        if (!scope_declare(c->scope, s->name, SYM_VAR, s->ty, s->loc, NULL)) {
            Symbol *prev = scope_lookup_here(c->scope, s->name);
            SERR(s->loc, "'%s' is already declared in this scope (line %d)", s->name, prev->loc.line);
        }
        break; }
    case S_ASSIGN: {
        forbid_in_rule(c, s, "assignment");
        Type lt = resolve_access(c, s->target, 1);
        s->target->type = lt;
        Type rt = check_value(c, s->e);
        if (!assignable(lt, rt))
            SERR(s->e->loc, "cannot assign %s to '%s' of type %s",
                 type_name(rt), s->target->path->s, type_name(lt));
        break; }
    case S_IF:
        require_bool(c, s->e, "IF condition");
        check_block(c, s->a);
        if (s->b) check_block(c, s->b);
        break;
    case S_WHILE:
        require_bool(c, s->e, "WHILE condition");
        check_block(c, s->a);
        break;
    case S_RETURN:
        if (c->func) {
            Type rt = c->func->ret;
            if (rt == TY_VOID) {
                if (s->e) { check_value(c, s->e);
                    SERR(s->loc, "function '%s' returns no value, but RETURN has an expression",
                         c->func->name); }
            } else if (!s->e) {
                SERR(s->loc, "function '%s' must return a value of type %s",
                     c->func->name, type_name(rt));
            } else {
                Type t = check_value(c, s->e);
                if (!assignable(rt, t))
                    SERR(s->e->loc, "cannot return %s from function '%s' declared to return %s",
                         type_name(t), c->func->name, type_name(rt));
            }
        } else if (s->e) {
            check_value(c, s->e);
            SERR(s->loc, "RETURN with a value is only allowed inside a function");
        }
        break;
    case S_MOVE: {
        forbid_in_rule(c, s, "MOVE");
        check_target(c, s, "MOVE");
        Type t = check_value(c, s->e);
        if (t != TY_ERR && !is_num(t))
            SERR(s->e->loc, "MOVE amount must be a number but is %s", type_name(t));
        break; }
    case S_SPAWN: {
        forbid_in_rule(c, s, "SPAWN");
        if (!find_entity(s->name)) SERR(s->loc, "unknown entity '%s' in SPAWN", s->name);
        Type t = check_value(c, s->e);
        if (t != TY_ERR && t != TY_VEC2)
            SERR(s->e->loc, "SPAWN position must be VECTOR2 but is %s", type_name(t));
        break; }
    case S_DESTROY:
        forbid_in_rule(c, s, "DESTROY");
        check_target(c, s, "DESTROY");
        if (!s->name && c->event && c->event->kind == EV_DEATH)
            SERR(s->loc, "DESTROY SELF inside ON DEATH would recurse forever");
        break;
    case S_SOUND: {
        forbid_in_rule(c, s, "PLAY SOUND");
        Type t = check_value(c, s->e);
        if (t != TY_ERR && t != TY_STRING)
            SERR(s->e->loc, "PLAY SOUND needs a STRING file name but got %s", type_name(t));
        break; }
    case S_PRINT:
        check_value(c, s->e);
        break;
    case S_WIN: case S_LOSE:
        if (!c->rule)
            SERR(s->loc, "%s can only be used inside a RULE", s->kind == S_WIN ? "WIN" : "LOSE");
        break;
    case S_CALL:
        check_expr(c, s->e);
        break;
    }
}

static void check_stmts(Ctx *c, Stmt *s) {
    int warned = 0, returned = 0;
    for (; s; s = s->next) {
        if (returned && !warned) {
            report_warning(s->loc.line, s->loc.col, "unreachable code after RETURN");
            warned = 1;
        }
        check_stmt(c, s);
        if (stmt_returns(s)) returned = 1;
    }
}

/* ----------------------------- declarations -------------------------- */

static void check_function(Ctx *outer, FuncDecl *f) {
    Ctx c = *outer;
    c.func = f;
    c.scope = scope_new(outer->scope);
    for (Param *p = f->params; p; p = p->next)
        if (!scope_declare(c.scope, p->name, SYM_PARAM, p->ty, p->loc, NULL))
            SERR(p->loc, "duplicate parameter '%s' in function '%s'", p->name, f->name);
    check_stmts(&c, f->body);
    warn_unused(c.scope);
    if (f->ret != TY_VOID && !always_returns(f->body))
        SERR(f->loc, "function '%s' may reach its end without returning a value of type %s",
             f->name, type_name(f->ret));
}

static int valid_key(const char *k) {
    for (int i = 0; VALID_KEYS[i]; i++)
        if (strcmp(VALID_KEYS[i], k) == 0) return 1;
    return 0;
}

static void check_events(EntityDecl *ed) {
    for (Event *ev = ed->events; ev; ev = ev->next) {
        for (Event *prev = ed->events; prev != ev; prev = prev->next) {
            if (prev->kind != ev->kind) continue;
            if ((ev->kind == EV_COLLISION || ev->kind == EV_KEY) && strcmp(prev->arg, ev->arg) != 0)
                continue;
            switch (ev->kind) {
            case EV_START:  SERR(ev->loc, "duplicate ON START handler in entity '%s'", ed->name); break;
            case EV_UPDATE: SERR(ev->loc, "duplicate ON UPDATE handler in entity '%s'", ed->name); break;
            case EV_DEATH:  SERR(ev->loc, "duplicate ON DEATH handler in entity '%s'", ed->name); break;
            case EV_COLLISION:
                SERR(ev->loc, "duplicate ON COLLISION WITH %s handler in entity '%s'", ev->arg, ed->name); break;
            case EV_KEY:
                SERR(ev->loc, "duplicate ON KEY_PRESS \"%s\" handler in entity '%s'", ev->arg, ed->name); break;
            }
            break;
        }
        if (ev->kind == EV_COLLISION && !find_entity(ev->arg))
            SERR(ev->loc, "unknown entity '%s' in ON COLLISION WITH", ev->arg);
        if (ev->kind == EV_KEY && !valid_key(ev->arg))
            SERR(ev->loc, "invalid key name \"%s\" (use SPACE, UP, DOWN, LEFT, RIGHT, ENTER, ESCAPE, A-Z or 0-9)",
                 ev->arg);

        Ctx c = { scope_new(ed->scope), ed, NULL, NULL, ev };
        check_stmts(&c, ev->body);
        warn_unused(c.scope);
    }
}

/* Pass 1: put every name of the program into the symbol tables. */
static void declare_all(void) {
    global_scope = scope_new(NULL);

    for (EntityDecl *ed = G->entities; ed; ed = ed->next) {
        ed->scope = scope_new(global_scope);
        if (!scope_declare(global_scope, ed->name, SYM_ENTITY, TY_ERR, ed->loc, ed)) {
            Symbol *p = scope_lookup_here(global_scope, ed->name);
            SERR(ed->loc, "entity '%s' conflicts with the %s declared at line %d",
                 ed->name, p->kind == SYM_ENTITY ? "entity" : "name", p->loc.line);
        }
    }
    for (FuncDecl *f = G->funcs; f; f = f->next) {
        if (!scope_declare(global_scope, f->name, SYM_FUNC, f->ret, f->loc, f)) {
            Symbol *p = scope_lookup_here(global_scope, f->name);
            SERR(f->loc, "function '%s' conflicts with the %s declared at line %d",
                 f->name, p->kind == SYM_ENTITY ? "entity" : "function", p->loc.line);
        }
    }
    for (EntityDecl *ed = G->entities; ed; ed = ed->next) {
        Symbol *pos = scope_declare(ed->scope, "position", SYM_PROP, TY_VEC2, ed->loc, NULL);
        if (pos) pos->builtin = 1;
        for (Prop *p = ed->props; p; p = p->next) {
            if (!scope_declare(ed->scope, p->name, SYM_PROP, p->ty, p->loc, NULL)) {
                Symbol *prev = scope_lookup_here(ed->scope, p->name);
                if (prev->builtin)
                    SERR(p->loc, "'position' is a built-in property of every entity and cannot be redeclared");
                else
                    SERR(p->loc, "'%s' is already declared in entity '%s' (line %d)",
                         p->name, ed->name, prev->loc.line);
            }
        }
        for (FuncDecl *f = ed->funcs; f; f = f->next) {
            f->owner = ed;
            if (!scope_declare(ed->scope, f->name, SYM_FUNC, f->ret, f->loc, f)) {
                Symbol *prev = scope_lookup_here(ed->scope, f->name);
                SERR(f->loc, "'%s' is already declared in entity '%s' (line %d)",
                     f->name, ed->name, prev->loc.line);
            }
        }
    }
}

int semantic_analyze(GameDecl *g) {
    G = g;
    errors_batch_begin();
    declare_all();

    /* Pass 2 -- entities */
    for (EntityDecl *ed = g->entities; ed; ed = ed->next) {
        Ctx pc = { global_scope, ed, NULL, NULL, NULL };
        for (Prop *p = ed->props; p; p = p->next) {
            if (!p->init) continue;
            if (!is_const_expr(p->init)) {
                SERR(p->init->loc, "initializer of property '%s' must be a constant expression", p->name);
                continue;
            }
            Type t = check_value(&pc, p->init);
            if (!assignable(p->ty, t))
                SERR(p->init->loc, "cannot initialize property '%s' of type %s with %s",
                     p->name, type_name(p->ty), type_name(t));
        }
        Ctx ec = { ed->scope, ed, NULL, NULL, NULL };
        for (FuncDecl *f = ed->funcs; f; f = f->next) check_function(&ec, f);
        check_events(ed);
        if (!ed->events)
            report_warning(ed->loc.line, ed->loc.col,
                           "entity '%s' has no event handlers and will never do anything", ed->name);
    }

    /* Pass 2 -- global functions */
    Ctx gc = { global_scope, NULL, NULL, NULL, NULL };
    for (FuncDecl *f = g->funcs; f; f = f->next) check_function(&gc, f);

    /* Pass 2 -- rules */
    for (Rule *r = g->rules; r; r = r->next) {
        for (Rule *p = g->rules; p != r; p = p->next)
            if (strcmp(p->name, r->name) == 0) {
                SERR(r->loc, "rule '%s' is already declared (line %d)", r->name, p->loc.line);
                break;
            }
        Ctx rc = { scope_new(global_scope), NULL, NULL, r, NULL };
        check_stmts(&rc, r->body);
        warn_unused(rc.scope);
    }
    errors_batch_flush();
    return errors_count(ERR_SEMANTIC);
}
