#ifndef GS_TOKENS_H
#define GS_TOKENS_H
/* Name of a token code (for --tokens dumps and tests). */
const char *token_name(int t);

/* Print the token stream of the already-opened yyin; returns lexical error count. */
int dump_tokens(void);
#endif
