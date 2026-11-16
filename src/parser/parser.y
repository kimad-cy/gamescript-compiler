/* parser.y -- GameScript syntax analyzer (Bison, LALR(1))
 *
 * Input : token stream from yylex()
 * Output: g_ast, the Abstract Syntax Tree of the whole program
 * Errors: syntax errors are reported with line/column and the parser
 *         recovers at the next ';' inside statements so several errors
 *         can be found in a single run.
 *
 * Operator precedence and associativity are declared with %left / %right /
 * %nonassoc below (lowest precedence first). Comparisons are %nonassoc, so
 * `a < b < c` is a syntax error, exactly as in the EBNF.
 */

%code requires {
    #include "ast.h"

    /* Semantic value of tokens and grammar symbols (yylval / $$). */
    typedef union {
        int    ival;     /* INT_LIT                                           */
        double fval;     /* FLOAT_LIT                                         */
        char  *sval;     /* ID, STRING_LIT (malloc'ed, owned by the AST)      */
        Type   type;
        Dir    dir;
        Expr       *expr;
        Stmt       *stmt;
        Name       *name;
        Param      *param;
        FuncDecl   *func;
        Event      *event;
        Prop       *prop;
        EntityDecl *ent;
        Rule       *rule;
        GameDecl   *game;
    } GsValue;
}

%{
#include <stdio.h>
#include <string.h>
#include "ast.h"
#include "errors.h"

int  yylex(void);
void yyerror(const char *msg);

#define LOCOF(l) ((Loc){ (l).first_line, (l).first_column })

static void syntax_error_at(Loc loc, const char *msg) {
    report_error(ERR_SYNTAX, loc.line, loc.col, "%s", msg);
}
%}

%define api.value.type {GsValue}
%locations
%define parse.error custom

/* ---- end of input ---- */
%token END 0 "end of file"

/* ---- literals and identifiers ---- */
%token <ival> INT_LIT    "integer literal"
%token <fval> FLOAT_LIT  "float literal"
%token <sval> STRING_LIT "string literal"
%token <sval> ID         "identifier"

/* ---- structure keywords ---- */
%token KW_GAME "GAME" KW_ENTITY "ENTITY" KW_PROPERTY "PROPERTY"
%token KW_FUNCTION "FUNCTION" KW_RULE "RULE"
/* ---- events ---- */
%token KW_ON "ON" KW_START "START" KW_UPDATE "UPDATE" KW_DEATH "DEATH"
%token KW_COLLISION "COLLISION" KW_WITH "WITH" KW_KEY_PRESS "KEY_PRESS"
/* ---- statements ---- */
%token KW_IF "IF" KW_ELSE "ELSE" KW_WHILE "WHILE" KW_RETURN "RETURN" KW_VAR "VAR"
%token KW_MOVE "MOVE" KW_BY "BY" KW_UP "UP" KW_DOWN "DOWN" KW_LEFT "LEFT" KW_RIGHT "RIGHT"
%token KW_SPAWN "SPAWN" KW_AT "AT" KW_DESTROY "DESTROY" KW_SELF "SELF"
%token KW_PLAY "PLAY" KW_SOUND "SOUND" KW_PRINT "PRINT" KW_WIN "WIN" KW_LOSE "LOSE"
/* ---- types and boolean literals ---- */
%token KW_INTEGER "INTEGER" KW_FLOAT "FLOAT" KW_STRING "STRING" KW_BOOLEAN "BOOLEAN"
%token KW_VECTOR2 "VECTOR2" KW_TRUE "TRUE" KW_FALSE "FALSE"
/* ---- logical operators (words) ---- */
%token KW_AND "AND" KW_OR "OR" KW_NOT "NOT"
/* ---- symbolic operators ---- */
%token PLUS "+" MINUS "-" STAR "*" SLASH "/" PERCENT "%" ASSIGN "="
%token EQ "==" NEQ "!=" LT "<" GT ">" LE "<=" GE ">="
/* ---- delimiters ---- */
%token LPAREN "(" RPAREN ")" LBRACE "{" RBRACE "}" SEMI ";" COLON ":" COMMA "," DOT "."

/* ---- precedence: lowest first ---- */
%left  KW_OR
%left  KW_AND
%precedence KW_NOT
%nonassoc EQ NEQ LT GT LE GE
%left  PLUS MINUS
%left  STAR SLASH PERCENT
%precedence UMINUS

%type <game>  game_items
%type <ent>   entity entity_items
%type <rule>  rule
%type <func>  function
%type <param> param param_list opt_params
%type <type>  type opt_ret
%type <prop>  property
%type <event> event
%type <stmt>  block stmts statement if_stmt vardecl action
%type <expr>  expr primary postfix args arg_list
%type <dir>   dir

%%

program
    : KW_GAME ID LBRACE game_items RBRACE
        { $4->name = $2; $4->loc = LOCOF(@1); g_ast = $4; }
    ;

/* ----- top level: entities, rules and global functions in any order ----- */
game_items
    : %empty                    { $$ = xcalloc(sizeof(GameDecl)); }
    | game_items entity         { LIST_APPEND($1->entities, $2); $$ = $1; }
    | game_items rule           { LIST_APPEND($1->rules, $2);    $$ = $1; }
    | game_items function       { LIST_APPEND($1->funcs, $2);    $$ = $1; }
    ;

entity
    : KW_ENTITY ID LBRACE entity_items RBRACE
        { $4->name = $2; $4->loc = LOCOF(@1); $$ = $4; }
    ;

entity_items
    : %empty                    { $$ = xcalloc(sizeof(EntityDecl)); }
    | entity_items property     { LIST_APPEND($1->props, $2);  $$ = $1; }
    | entity_items function     { $2->owner = $1; LIST_APPEND($1->funcs, $2); $$ = $1; }
    | entity_items event        { LIST_APPEND($1->events, $2); $$ = $1; }
    ;

property
    : KW_PROPERTY ID COLON type SEMI
        { $$ = xcalloc(sizeof(Prop)); $$->name = $2; $$->ty = $4; $$->loc = LOCOF(@2); }
    | KW_PROPERTY ID COLON type ASSIGN expr SEMI
        { $$ = xcalloc(sizeof(Prop)); $$->name = $2; $$->ty = $4; $$->init = $6; $$->loc = LOCOF(@2); }
    ;

type
    : KW_INTEGER  { $$ = TY_INT; }
    | KW_FLOAT    { $$ = TY_FLOAT; }
    | KW_STRING   { $$ = TY_STRING; }
    | KW_BOOLEAN  { $$ = TY_BOOL; }
    | KW_VECTOR2  { $$ = TY_VEC2; }
    ;

/* ----- functions ----- */
function
    : KW_FUNCTION ID LPAREN opt_params RPAREN opt_ret block
        { $$ = xcalloc(sizeof(FuncDecl));
          $$->name = $2; $$->params = $4; $$->ret = $6; $$->body = $7; $$->loc = LOCOF(@2); }
    ;

opt_params
    : %empty        { $$ = NULL; }
    | param_list    { $$ = $1; }
    ;

param_list
    : param                     { $$ = $1; }
    | param_list COMMA param    { LIST_APPEND($1, $3); $$ = $1; }
    ;

param
    : ID COLON type
        { $$ = xcalloc(sizeof(Param)); $$->name = $1; $$->ty = $3; $$->loc = LOCOF(@1); }
    ;

opt_ret
    : %empty        { $$ = TY_VOID; }
    | COLON type    { $$ = $2; }
    ;

/* ----- events (only inside entities) ----- */
event
    : KW_ON KW_START block                       { $$ = mk_event(EV_START,  NULL, $3, LOCOF(@1)); }
    | KW_ON KW_UPDATE block                      { $$ = mk_event(EV_UPDATE, NULL, $3, LOCOF(@1)); }
    | KW_ON KW_DEATH block                       { $$ = mk_event(EV_DEATH,  NULL, $3, LOCOF(@1)); }
    | KW_ON KW_COLLISION KW_WITH ID block        { $$ = mk_event(EV_COLLISION, $4, $5, LOCOF(@1)); }
    | KW_ON KW_KEY_PRESS STRING_LIT block        { $$ = mk_event(EV_KEY, $3, $4, LOCOF(@1)); }
    ;

/* ----- rules ----- */
rule
    : KW_RULE ID block
        { $$ = xcalloc(sizeof(Rule)); $$->name = $2; $$->body = $3; $$->loc = LOCOF(@2); }
    ;

/* ----- statements ----- */
block
    : LBRACE stmts RBRACE   { $$ = $2; }
    ;

stmts
    : %empty                { $$ = NULL; }
    | stmts statement       { $$ = $1; LIST_APPEND($$, $2); }
    ;

statement
    : vardecl
    | if_stmt
    | KW_WHILE expr block
        { $$ = mk_stmt(S_WHILE, LOCOF(@1)); $$->e = $2; $$->a = $3; }
    | KW_RETURN SEMI
        { $$ = mk_stmt(S_RETURN, LOCOF(@1)); }
    | KW_RETURN expr SEMI
        { $$ = mk_stmt(S_RETURN, LOCOF(@1)); $$->e = $2; }
    | action
    | postfix ASSIGN expr SEMI
        { if ($1->kind != E_ACCESS) {
              syntax_error_at($1->loc, "invalid assignment target (expected a variable or property)");
              $$ = NULL;
          } else {
              $$ = mk_stmt(S_ASSIGN, $1->loc); $$->target = $1; $$->e = $3;
          } }
    | postfix SEMI
        { if ($1->kind != E_CALL) {
              syntax_error_at($1->loc, "expression is not a statement (only function calls can stand alone)");
              $$ = NULL;
          } else {
              $$ = mk_stmt(S_CALL, $1->loc); $$->e = $1;
          } }
    | error SEMI
        { yyerrok; $$ = NULL; }
    ;

vardecl
    : KW_VAR ID COLON type SEMI
        { $$ = mk_stmt(S_VAR, LOCOF(@2)); $$->name = $2; $$->ty = $4; }
    | KW_VAR ID COLON type ASSIGN expr SEMI
        { $$ = mk_stmt(S_VAR, LOCOF(@2)); $$->name = $2; $$->ty = $4; $$->e = $6; }
    ;

if_stmt
    : KW_IF expr block
        { $$ = mk_stmt(S_IF, LOCOF(@1)); $$->e = $2; $$->a = $3; }
    | KW_IF expr block KW_ELSE block
        { $$ = mk_stmt(S_IF, LOCOF(@1)); $$->e = $2; $$->a = $3; $$->b = $5; }
    | KW_IF expr block KW_ELSE if_stmt
        { $$ = mk_stmt(S_IF, LOCOF(@1)); $$->e = $2; $$->a = $3; $$->b = $5; }
    ;

action
    : KW_MOVE dir KW_BY expr SEMI
        { $$ = mk_stmt(S_MOVE, LOCOF(@1)); $$->dir = $2; $$->e = $4; }
    | KW_MOVE ID dir KW_BY expr SEMI
        { $$ = mk_stmt(S_MOVE, LOCOF(@1)); $$->name = $2; $$->dir = $3; $$->e = $5; }
    | KW_MOVE KW_SELF dir KW_BY expr SEMI
        { $$ = mk_stmt(S_MOVE, LOCOF(@1)); $$->is_self = 1; $$->dir = $3; $$->e = $5; }
    | KW_SPAWN ID KW_AT expr SEMI
        { $$ = mk_stmt(S_SPAWN, LOCOF(@1)); $$->name = $2; $$->e = $4; }
    | KW_DESTROY ID SEMI
        { $$ = mk_stmt(S_DESTROY, LOCOF(@1)); $$->name = $2; }
    | KW_DESTROY KW_SELF SEMI
        { $$ = mk_stmt(S_DESTROY, LOCOF(@1)); $$->is_self = 1; }
    | KW_PLAY KW_SOUND expr SEMI
        { $$ = mk_stmt(S_SOUND, LOCOF(@1)); $$->e = $3; }
    | KW_PRINT expr SEMI
        { $$ = mk_stmt(S_PRINT, LOCOF(@1)); $$->e = $2; }
    | KW_WIN SEMI   { $$ = mk_stmt(S_WIN,  LOCOF(@1)); }
    | KW_LOSE SEMI  { $$ = mk_stmt(S_LOSE, LOCOF(@1)); }
    ;

dir
    : KW_UP     { $$ = DIR_UP; }
    | KW_DOWN   { $$ = DIR_DOWN; }
    | KW_LEFT   { $$ = DIR_LEFT; }
    | KW_RIGHT  { $$ = DIR_RIGHT; }
    ;

/* ----- expressions (precedence resolved by the declarations above) ----- */
expr
    : expr KW_OR  expr      { $$ = mk_binary(OP_OR,  $1, $3, LOCOF(@2)); }
    | expr KW_AND expr      { $$ = mk_binary(OP_AND, $1, $3, LOCOF(@2)); }
    | KW_NOT expr           { $$ = mk_unary(OP_NOT, $2, LOCOF(@1)); }
    | expr EQ  expr         { $$ = mk_binary(OP_EQ,  $1, $3, LOCOF(@2)); }
    | expr NEQ expr         { $$ = mk_binary(OP_NEQ, $1, $3, LOCOF(@2)); }
    | expr LT  expr         { $$ = mk_binary(OP_LT,  $1, $3, LOCOF(@2)); }
    | expr GT  expr         { $$ = mk_binary(OP_GT,  $1, $3, LOCOF(@2)); }
    | expr LE  expr         { $$ = mk_binary(OP_LE,  $1, $3, LOCOF(@2)); }
    | expr GE  expr         { $$ = mk_binary(OP_GE,  $1, $3, LOCOF(@2)); }
    | expr PLUS  expr       { $$ = mk_binary(OP_ADD, $1, $3, LOCOF(@2)); }
    | expr MINUS expr       { $$ = mk_binary(OP_SUB, $1, $3, LOCOF(@2)); }
    | expr STAR  expr       { $$ = mk_binary(OP_MUL, $1, $3, LOCOF(@2)); }
    | expr SLASH expr       { $$ = mk_binary(OP_DIV, $1, $3, LOCOF(@2)); }
    | expr PERCENT expr     { $$ = mk_binary(OP_MOD, $1, $3, LOCOF(@2)); }
    | MINUS expr %prec UMINUS { $$ = mk_unary(OP_NEG, $2, LOCOF(@1)); }
    | primary
    ;

primary
    : INT_LIT       { $$ = mk_expr(E_INT, LOCOF(@1));    $$->ival = $1; }
    | FLOAT_LIT     { $$ = mk_expr(E_FLOAT, LOCOF(@1));  $$->fval = $1; }
    | STRING_LIT    { $$ = mk_expr(E_STRING, LOCOF(@1)); $$->sval = $1; }
    | KW_TRUE       { $$ = mk_expr(E_BOOL, LOCOF(@1));   $$->bval = 1; }
    | KW_FALSE      { $$ = mk_expr(E_BOOL, LOCOF(@1));   $$->bval = 0; }
    | KW_VECTOR2 LPAREN expr COMMA expr RPAREN
        { $$ = mk_expr(E_VEC2, LOCOF(@1)); $$->l = $3; $$->r = $5; }
    | LPAREN expr RPAREN    { $$ = $2; }
    | postfix
    ;

/* A postfix chain covers  x   Entity.prop   a.b.c   f(x)   Entity.f(x).
 * It is one rule family (instead of separate `access` and `call` rules)
 * so the LALR(1) automaton never has to guess early; the shape of the
 * chain is validated in the actions below. */
postfix
    : ID        { $$ = mk_expr(E_ACCESS, LOCOF(@1)); $$->path = mk_name($1, LOCOF(@1)); }
    | KW_SELF   { $$ = mk_expr(E_ACCESS, LOCOF(@1)); $$->path = mk_name(xstrdup("SELF"), LOCOF(@1)); }
    | postfix DOT ID
        { $$ = $1;
          if ($1->kind != E_ACCESS) {
              syntax_error_at(LOCOF(@2), "cannot access a member of a function call result");
              $$ = mk_expr(E_ACCESS, $1->loc);      /* placeholder: avoids a second, cascading error */
              $$->path = mk_name(xstrdup("?"), $1->loc);
          } else {
              Name *n = mk_name($3, LOCOF(@3));
              LIST_APPEND($1->path, n);
          } }
    | postfix LPAREN args RPAREN
        { int n = ($1->kind == E_ACCESS) ? name_count($1->path) : 0;
          if (n < 1 || n > 2) {
              syntax_error_at($1->loc, "invalid call target (expected f(...) or Entity.f(...))");
              $$ = mk_expr(E_CALL, $1->loc);        /* placeholder: avoids a cascading error */
              $$->fname = xstrdup("?");
          } else {
              $$ = mk_expr(E_CALL, $1->loc);
              if (n == 1) { $$->fname = $1->path->s; }
              else        { $$->path = $1->path; $$->fname = $1->path->next->s;
                            $$->loc = $1->path->next->loc; $$->path->next = NULL; }
              $$->args = $3;
          } }
    ;

args
    : %empty        { $$ = NULL; }
    | arg_list      { $$ = $1; }
    ;

arg_list
    : expr                  { $$ = $1; }
    | arg_list COMMA expr   { LIST_APPEND($1, $3); $$ = $1; }
    ;

%%

/* ---- syntax error messages -------------------------------------------
 * `%define parse.error custom` hands us the parser state when it fails:
 * the unexpected lookahead token and the set of tokens that would have been
 * accepted. We turn that into a readable message, with two hints for the most
 * common beginner mistakes (a forgotten ';' or '{').
 */
static const char *pretty(yysymbol_kind_t k, char *buf, size_t n) {
    const char *raw = yysymbol_name(k);
    static const char *plain[] = { "end of file", "identifier", "integer literal",
                                   "float literal", "string literal", NULL };
    for (int i = 0; plain[i]; i++)
        if (strcmp(raw, plain[i]) == 0) return raw;
    snprintf(buf, n, "'%s'", raw);
    return buf;
}

int yyreport_syntax_error(const yypcontext_t *ctx) {
    char msg[512], b1[64], b2[64];
    yysymbol_kind_t expected[64];
    yysymbol_kind_t la = yypcontext_token(ctx);
    const YYLTYPE *loc = yypcontext_location(ctx);
    int n = yypcontext_expected_tokens(ctx, expected, 64);
    int has_semi = 0, has_lbrace = 0, has_arith = 0;
    for (int i = 0; i < n; i++) {
        if (expected[i] == YYSYMBOL_SEMI)   has_semi = 1;
        if (expected[i] == YYSYMBOL_LBRACE) has_lbrace = 1;
        if (expected[i] == YYSYMBOL_STAR)   has_arith = 1;
    }
    int la_is_cmp = (la == YYSYMBOL_EQ || la == YYSYMBOL_NEQ || la == YYSYMBOL_LT ||
                     la == YYSYMBOL_GT || la == YYSYMBOL_LE  || la == YYSYMBOL_GE);

    if (la_is_cmp && has_arith)       /* %nonassoc: a second comparison right after one */
        snprintf(msg, sizeof msg, "comparison operators cannot be chained; combine two comparisons with AND");
    else if (has_lbrace && la != YYSYMBOL_YYEOF)
        snprintf(msg, sizeof msg, "missing '{' before %s", pretty(la, b1, sizeof b1));
    else if (has_semi && la != YYSYMBOL_YYEOF)
        snprintf(msg, sizeof msg, "missing ';' before %s", pretty(la, b1, sizeof b1));
    else {
        int w = snprintf(msg, sizeof msg, "unexpected %s", pretty(la, b1, sizeof b1));
        if (n >= 1 && n <= 5) {
            w += snprintf(msg + w, sizeof msg - w, ", expecting ");
            for (int i = 0; i < n; i++)
                w += snprintf(msg + w, sizeof msg - w, "%s%s",
                              i == 0 ? "" : (i == n - 1 ? " or " : ", "),
                              pretty(expected[i], b2, sizeof b2));
        }
    }
    report_error(ERR_SYNTAX, loc->first_line, loc->first_column, "%s", msg);
    return 0;
}

/* Only used for non-syntax parser failures (e.g. out of memory). */
void yyerror(const char *msg) {
    report_error(ERR_SYNTAX, yylloc.first_line, yylloc.first_column, "%s", msg);
}
