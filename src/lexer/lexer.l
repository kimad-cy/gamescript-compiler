/* lexer.l -- GameScript lexical analyzer (Flex)
 *
 * Input : source characters (yyin)
 * Output: one token per yylex() call; semantic value in yylval,
 *         source position in yylloc (1-based line/column).
 * Errors: reported through report_error(ERR_LEXICAL, ...). The lexer never
 *         stops at the first error: it reports, skips the bad lexeme and
 *         keeps scanning, so the user sees every lexical error in one run.
 */

%{
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "errors.h"
#include "parser.tab.h"   /* token codes, GsValue, YYLTYPE */

static void update_location(void);
static char *unescape(const char *body, int len);
static void lex_error(int line, int col, const char *fmt, ...);

/* Flex calls this macro before every rule action: it keeps yylloc exact. */
#define YY_USER_ACTION update_location();

#define INT32_MAX_VAL 2147483647LL

static int cur_line = 1, cur_col = 1;   /* position of the NEXT character */
static int comment_line, comment_col;   /* where the open comment started */
%}

%option noyywrap nounput noinput

/* exclusive start condition: inside a block comment only comment rules apply */
%x BLOCK_COMMENT

DIGIT     [0-9]
ID_START  [A-Za-z_]
ID_CHAR   [A-Za-z0-9_]

%%

 /* ---------- whitespace and comments ---------- */
[ \t\r\n]+                  { /* skipped; update_location counted the newlines */ }
"//".*                      { /* line comment */ }

"/*"                        { comment_line = yylloc.first_line;
                              comment_col  = yylloc.first_column;
                              BEGIN(BLOCK_COMMENT); }
<BLOCK_COMMENT>"*/"         { BEGIN(INITIAL); }
<BLOCK_COMMENT>[^*]+        { /* comment text (may contain newlines) */ }
<BLOCK_COMMENT>"*"          { /* a lone star */ }
<BLOCK_COMMENT><<EOF>>      { lex_error(comment_line, comment_col,
                                        "unterminated block comment");
                              BEGIN(INITIAL);
                              yyterminate(); }

 /* ---------- keywords (must come before the identifier rule) ----------
  * Flex picks the LONGEST match, and on a tie the FIRST rule. So "GAME"
  * is a keyword, but "GAMEOVER" is longer and becomes an identifier. */
"GAME"                      { return KW_GAME; }
"ENTITY"                    { return KW_ENTITY; }
"PROPERTY"                  { return KW_PROPERTY; }
"FUNCTION"                  { return KW_FUNCTION; }
"RULE"                      { return KW_RULE; }
"ON"                        { return KW_ON; }
"START"                     { return KW_START; }
"UPDATE"                    { return KW_UPDATE; }
"DEATH"                     { return KW_DEATH; }
"COLLISION"                 { return KW_COLLISION; }
"WITH"                      { return KW_WITH; }
"KEY_PRESS"                 { return KW_KEY_PRESS; }
"IF"                        { return KW_IF; }
"ELSE"                      { return KW_ELSE; }
"WHILE"                     { return KW_WHILE; }
"RETURN"                    { return KW_RETURN; }
"VAR"                       { return KW_VAR; }
"MOVE"                      { return KW_MOVE; }
"BY"                        { return KW_BY; }
"UP"                        { return KW_UP; }
"DOWN"                      { return KW_DOWN; }
"LEFT"                      { return KW_LEFT; }
"RIGHT"                     { return KW_RIGHT; }
"SPAWN"                     { return KW_SPAWN; }
"AT"                        { return KW_AT; }
"DESTROY"                   { return KW_DESTROY; }
"SELF"                      { return KW_SELF; }
"PLAY"                      { return KW_PLAY; }
"SOUND"                     { return KW_SOUND; }
"PRINT"                     { return KW_PRINT; }
"WIN"                       { return KW_WIN; }
"LOSE"                      { return KW_LOSE; }
"INTEGER"                   { return KW_INTEGER; }
"FLOAT"                     { return KW_FLOAT; }
"STRING"                    { return KW_STRING; }
"BOOLEAN"                   { return KW_BOOLEAN; }
"VECTOR2"                   { return KW_VECTOR2; }
"TRUE"                      { return KW_TRUE; }
"FALSE"                     { return KW_FALSE; }
"AND"                       { return KW_AND; }
"OR"                        { return KW_OR; }
"NOT"                       { return KW_NOT; }

 /* ---------- identifiers ---------- */
{ID_START}{ID_CHAR}*        { yylval.sval = strdup(yytext); return ID; }

 /* ---------- numbers ---------- */
{DIGIT}+"."{DIGIT}+         { yylval.fval = atof(yytext); return FLOAT_LIT; }
{DIGIT}+"."                 { lex_error(yylloc.first_line, yylloc.first_column,
                                "malformed float literal '%s' (digits are required after the point)", yytext); }
"."{DIGIT}+                 { lex_error(yylloc.first_line, yylloc.first_column,
                                "malformed float literal '%s' (digits are required before the point)", yytext); }
{DIGIT}+{ID_START}{ID_CHAR}* { lex_error(yylloc.first_line, yylloc.first_column,
                                "invalid token '%s': identifiers cannot start with a digit", yytext); }
{DIGIT}+                    { long long v = strtoll(yytext, NULL, 10);
                              if (yyleng > 10 || v > INT32_MAX_VAL) {
                                  lex_error(yylloc.first_line, yylloc.first_column,
                                      "integer literal '%s' does not fit in 32 bits", yytext);
                              } else {
                                  yylval.ival = (int)v;
                                  return INT_LIT;
                              } }

 /* ---------- strings ---------- */
 /* valid: any chars except quote, backslash, newline; escapes \" \\ \n */
\"([^\"\\\n]|\\[\"\\n])*\"  { yylval.sval = unescape(yytext + 1, yyleng - 2);
                              return STRING_LIT; }
 /* terminated, but contains an unknown escape such as \q */
\"([^\"\\\n]|\\[^\n])*\"    { for (int i = 1; i < yyleng - 1; i++) {
                                  if (yytext[i] == '\\') {
                                      lex_error(yylloc.first_line, yylloc.first_column + i,
                                          "invalid escape sequence '\\%c' (allowed: \\\" \\\\ \\n)", yytext[i + 1]);
                                      break;
                                  }
                              } }
 /* no closing quote before end of line / file */
\"([^\"\\\n]|\\[^\n])*\\?   { lex_error(yylloc.first_line, yylloc.first_column,
                                "unterminated string literal"); }

 /* ---------- operators ---------- */
"=="                        { return EQ; }
"!="                        { return NEQ; }
"<="                        { return LE; }
">="                        { return GE; }
"="                         { return ASSIGN; }
"<"                         { return LT; }
">"                         { return GT; }
"+"                         { return PLUS; }
"-"                         { return MINUS; }
"*"                         { return STAR; }
"/"                         { return SLASH; }
"%"                         { return PERCENT; }

 /* common mistakes from C-like languages get a helpful message */
"&&"                        { lex_error(yylloc.first_line, yylloc.first_column,
                                "unknown operator '&&' (use AND)"); }
"||"                        { lex_error(yylloc.first_line, yylloc.first_column,
                                "unknown operator '||' (use OR)"); }
"!"                         { lex_error(yylloc.first_line, yylloc.first_column,
                                "unexpected '!' (use NOT, or '!=' for inequality)"); }

 /* ---------- delimiters ---------- */
"("                         { return LPAREN; }
")"                         { return RPAREN; }
"{"                         { return LBRACE; }
"}"                         { return RBRACE; }
";"                         { return SEMI; }
":"                         { return COLON; }
","                         { return COMMA; }
"."                         { return DOT; }

 /* ---------- anything else is an invalid character ---------- */
[\x80-\xFF]+                { lex_error(yylloc.first_line, yylloc.first_column,
                                "non-ASCII character(s) are not allowed outside strings and comments"); }
.                           { if (yytext[0] >= 32 && yytext[0] < 127)
                                  lex_error(yylloc.first_line, yylloc.first_column,
                                      "invalid character '%c'", yytext[0]);
                              else
                                  lex_error(yylloc.first_line, yylloc.first_column,
                                      "invalid control character (0x%02X)", (unsigned char)yytext[0]); }

%%

/* Advance (cur_line, cur_col) over the lexeme just matched and record its span.
 * Columns count bytes; a tab counts as one column. */
static void update_location(void) {
    yylloc.first_line   = cur_line;
    yylloc.first_column = cur_col;
    for (int i = 0; i < yyleng; i++) {
        if (yytext[i] == '\n') { cur_line++; cur_col = 1; }
        else                   { cur_col++; }
    }
    yylloc.last_line   = cur_line;
    yylloc.last_column = cur_col;          /* exclusive end */
}

/* Turn the raw body of a string literal (between the quotes) into its value. */
static char *unescape(const char *body, int len) {
    char *out = malloc((size_t)len + 1), *w = out;
    for (int i = 0; i < len; i++) {
        if (body[i] == '\\') {
            i++;
            *w++ = (body[i] == 'n') ? '\n' : body[i];   /* \" and \\ map to themselves */
        } else {
            *w++ = body[i];
        }
    }
    *w = '\0';
    return out;
}

#include <stdarg.h>
static void lex_error(int line, int col, const char *fmt, ...) {
    char buf[256];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof buf, fmt, ap);
    va_end(ap);
    report_error(ERR_LEXICAL, line, col, "%s", buf);
}
