#ifndef GS_AST_H
#define GS_AST_H

#include <stddef.h>

/* ------------------------------------------------------------------ *
 * Abstract Syntax Tree for GameScript.
 *
 * The parser builds it; semantic analysis annotates it in place
 * (Expr.type, name-resolution fields); code generation reads it.
 * That annotated tree is the compiler's internal representation (IR).
 * ------------------------------------------------------------------ */

typedef struct { int line, col; } Loc;

typedef enum { TY_VOID, TY_INT, TY_FLOAT, TY_STRING, TY_BOOL, TY_VEC2, TY_ERR } Type;
const char *type_name(Type t);

typedef enum {
    OP_ADD, OP_SUB, OP_MUL, OP_DIV, OP_MOD,
    OP_EQ, OP_NEQ, OP_LT, OP_GT, OP_LE, OP_GE,
    OP_AND, OP_OR, OP_NOT, OP_NEG
} Op;
const char *op_name(Op o);

typedef enum { DIR_UP, DIR_DOWN, DIR_LEFT, DIR_RIGHT } Dir;
const char *dir_name(Dir d);

/* A dotted path like  Player.position.x  is a linked list of Names. */
typedef struct Name {
    char *s;
    Loc loc;
    struct Name *next;
} Name;

struct FuncDecl;
struct EntityDecl;
struct Scope;

/* ----------------------------- expressions ---------------------------- */

typedef enum {
    E_INT, E_FLOAT, E_STRING, E_BOOL, E_VEC2,
    E_ACCESS,   /* variable / property / Entity.property / .x .y */
    E_CALL,
    E_UNARY, E_BINARY
} ExprKind;

/* Filled in by semantic analysis: what does an access path start from? */
typedef enum { B_NONE, B_LOCAL, B_SELF_PROP, B_ENTITY_PROP } AccessBase;
typedef enum { C_NONE, C_GLOBAL, C_SELF_METHOD, C_ENTITY_METHOD } CallKind;

typedef struct Expr {
    ExprKind kind;
    Loc loc;
    Type type;                 /* set by semantic analysis */

    int ival; double fval; char *sval; int bval;   /* literals            */
    Op op; struct Expr *l, *r;                     /* unary(l) binary vec2 */

    Name *path;                /* E_ACCESS: n0.n1.n2 ; E_CALL: optional qualifier */
    char *fname;               /* E_CALL: function name                        */
    struct Expr *args;         /* E_CALL: argument list (linked by ->next)     */

    /* --- semantic annotations (access) --- */
    AccessBase base;
    char *base_entity;         /* B_ENTITY_PROP: which entity                  */
    Name *prop;                /* the property name inside the path            */
    Name *comp;                /* .x / .y component, or NULL                   */
    /* --- semantic annotations (call) --- */
    CallKind ckind;
    struct FuncDecl *callee;
    char *call_entity;

    struct Expr *next;         /* argument lists */
} Expr;

/* ------------------------------ statements ---------------------------- */

typedef enum {
    S_VAR, S_ASSIGN, S_IF, S_WHILE, S_RETURN,
    S_MOVE, S_SPAWN, S_DESTROY, S_SOUND, S_PRINT, S_WIN, S_LOSE, S_CALL
} StmtKind;

typedef struct Stmt {
    StmtKind kind;
    Loc loc;
    char *name;        /* VAR: variable name; MOVE/SPAWN/DESTROY: entity name (NULL = SELF) */
    int is_self;       /* MOVE/DESTROY: SELF written explicitly */
    Type ty;           /* VAR: declared type */
    Dir dir;           /* MOVE */
    Expr *target;      /* ASSIGN: left-hand side */
    Expr *e;           /* VAR init, ASSIGN value, IF/WHILE cond, RETURN value, MOVE amount,
                          SPAWN position, SOUND file, PRINT value, CALL expression */
    struct Stmt *a, *b;/* IF: then / else ; WHILE: body (a) */
    struct Stmt *next;
} Stmt;

/* ------------------------------ declarations -------------------------- */

typedef struct Param {
    char *name; Type ty; Loc loc;
    struct Param *next;
} Param;

typedef struct FuncDecl {
    char *name; Param *params; Type ret; Stmt *body; Loc loc;
    struct EntityDecl *owner;          /* NULL for global functions */
    struct FuncDecl *next;
} FuncDecl;

typedef enum { EV_START, EV_UPDATE, EV_DEATH, EV_COLLISION, EV_KEY } EventKind;

typedef struct Event {
    EventKind kind;
    char *arg;                         /* collision target entity, or key name */
    Stmt *body; Loc loc;
    struct Event *next;
} Event;

typedef struct Prop {
    char *name; Type ty; Expr *init; Loc loc;
    struct Prop *next;
} Prop;

typedef struct EntityDecl {
    char *name; Prop *props; FuncDecl *funcs; Event *events; Loc loc;
    struct Scope *scope;               /* entity scope, built by semantic analysis */
    struct EntityDecl *next;
} EntityDecl;

typedef struct Rule {
    char *name; Stmt *body; Loc loc;
    struct Rule *next;
} Rule;

typedef struct GameDecl {
    char *name; EntityDecl *entities; FuncDecl *funcs; Rule *rules; Loc loc;
} GameDecl;

/* ------------------------------ helpers ------------------------------- */

void *xcalloc(size_t n);
char *xstrdup(const char *s);

/* Append `node` at the end of the singly-linked list whose head is `head`. */
#define LIST_APPEND(head, node) do {                         \
        __typeof__(head) *gs_pp = &(head);                   \
        while (*gs_pp) gs_pp = &(*gs_pp)->next;              \
        *gs_pp = (node);                                     \
    } while (0)

Name *mk_name(char *s, Loc loc);
Expr *mk_expr(ExprKind k, Loc loc);
Expr *mk_binary(Op op, Expr *l, Expr *r, Loc loc);
Expr *mk_unary(Op op, Expr *x, Loc loc);
Stmt *mk_stmt(StmtKind k, Loc loc);
Event *mk_event(EventKind k, char *arg, Stmt *body, Loc loc);

int name_count(const Name *n);

extern GameDecl *g_ast;                /* result of parsing */

void ast_print(const GameDecl *g);     /* --ast: indented dump to stdout */

#endif
