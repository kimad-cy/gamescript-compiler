/* main_lex.c -- standalone driver to test the lexer in isolation.
 *
 * Usage: gsc-lex file.gs   (prints the token stream; errors go to stderr)
 * yylval / yylloc are normally defined by Bison's generated parser; this
 * driver is linked WITHOUT the parser, so it defines them itself. */

#include <stdio.h>
#include "errors.h"
#include "parser.tab.h"
#include "tokens.h"

GsValue yylval;
YYLTYPE yylloc;

extern FILE *yyin;

int main(int argc, char **argv) {
    if (argc != 2) { fprintf(stderr, "usage: %s file.gs\n", argv[0]); return 2; }
    if (errors_init(argv[1]) != 0 || !(yyin = fopen(argv[1], "r"))) {
        fprintf(stderr, "cannot open '%s'\n", argv[1]);
        return 2;
    }
    int errs = dump_tokens();
    fclose(yyin);
    errors_free();
    return errs ? 1 : 0;
}
