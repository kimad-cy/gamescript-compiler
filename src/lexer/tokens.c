#include "tokens.h"
#include "parser.tab.h"

#define CASE(t) case t: return #t;

const char *token_name(int t) {
    switch (t) {
    CASE(INT_LIT) CASE(FLOAT_LIT) CASE(STRING_LIT) CASE(ID)
    CASE(KW_GAME) CASE(KW_ENTITY) CASE(KW_PROPERTY) CASE(KW_FUNCTION) CASE(KW_RULE)
    CASE(KW_ON) CASE(KW_START) CASE(KW_UPDATE) CASE(KW_DEATH) CASE(KW_COLLISION)
    CASE(KW_WITH) CASE(KW_KEY_PRESS)
    CASE(KW_IF) CASE(KW_ELSE) CASE(KW_WHILE) CASE(KW_RETURN) CASE(KW_VAR)
    CASE(KW_MOVE) CASE(KW_BY) CASE(KW_UP) CASE(KW_DOWN) CASE(KW_LEFT) CASE(KW_RIGHT)
    CASE(KW_SPAWN) CASE(KW_AT) CASE(KW_DESTROY) CASE(KW_SELF) CASE(KW_PLAY)
    CASE(KW_SOUND) CASE(KW_PRINT) CASE(KW_WIN) CASE(KW_LOSE)
    CASE(KW_INTEGER) CASE(KW_FLOAT) CASE(KW_STRING) CASE(KW_BOOLEAN) CASE(KW_VECTOR2)
    CASE(KW_TRUE) CASE(KW_FALSE) CASE(KW_AND) CASE(KW_OR) CASE(KW_NOT)
    CASE(PLUS) CASE(MINUS) CASE(STAR) CASE(SLASH) CASE(PERCENT) CASE(ASSIGN)
    CASE(EQ) CASE(NEQ) CASE(LT) CASE(GT) CASE(LE) CASE(GE)
    CASE(LPAREN) CASE(RPAREN) CASE(LBRACE) CASE(RBRACE) CASE(SEMI) CASE(COLON)
    CASE(COMMA) CASE(DOT)
    default: return "UNKNOWN";
    }
}

#include <stdio.h>
#include <stdlib.h>
#include "errors.h"

int yylex(void);
extern YYLTYPE yylloc;
extern GsValue yylval;

/* One line per token:  line:col  NAME  value */
int dump_tokens(void) {
    int tok, count = 0;
    while ((tok = yylex()) != 0) {
        count++;
        printf("%3d:%-3d %-12s", yylloc.first_line, yylloc.first_column, token_name(tok));
        switch (tok) {
        case INT_LIT:    printf(" %d", yylval.ival); break;
        case FLOAT_LIT:  printf(" %g", yylval.fval); break;
        case ID:         printf(" %s", yylval.sval);  free(yylval.sval); break;
        case STRING_LIT: printf(" \"%s\"", yylval.sval); free(yylval.sval); break;
        default: break;
        }
        putchar('\n');
    }
    printf("-- %d token(s), %d lexical error(s)\n", count, errors_count(ERR_LEXICAL));
    return errors_count(ERR_LEXICAL);
}
