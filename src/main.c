/* main.c -- gsc, the GameScript compiler driver
 *
 *   source (.gs) --lexer--> tokens --parser--> AST --semantic--> typed AST
 *                                                       --codegen--> Python
 *
 * Each phase runs only if the previous one found no errors, because later
 * phases cannot trust a broken tree. Within a phase, all errors are reported.
 */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "errors.h"
#include "ast.h"
#include "parser.tab.h"
#include "tokens.h"
#include "semantic.h"
#include "codegen.h"

/* Absolute path of runtime/, baked in by the Makefile so generated programs can import it. */
#ifdef RUNTIME_DIR
static const char *runtime_dir = RUNTIME_DIR;
#else
static const char *runtime_dir = NULL;
#endif

extern FILE *yyin;
int yyparse(void);

static void usage(const char *prog) {
    fprintf(stderr,
        "GameScript compiler\n"
        "usage: %s [options] file.gs\n"
        "  -o FILE     write the generated Python to FILE (default: <file>.py)\n"
        "  --check     analyze only; do not generate code\n"
        "  --tokens    print the token stream (lexical analysis only)\n"
        "  --ast       print the abstract syntax tree (after parsing)\n"
        "  -h          this help\n", prog);
}

static void summary_failed(void) {
    fprintf(stderr, "compilation failed: %d lexical, %d syntax, %d semantic error(s)",
            errors_count(ERR_LEXICAL), errors_count(ERR_SYNTAX), errors_count(ERR_SEMANTIC));
    if (warnings_count()) fprintf(stderr, ", %d warning(s)", warnings_count());
    fputc('\n', stderr);
}

int main(int argc, char **argv) {
    const char *input = NULL, *output = NULL;
    int opt_check = 0, opt_tokens = 0, opt_ast = 0;

    for (int i = 1; i < argc; i++) {
        if (!strcmp(argv[i], "-o") && i + 1 < argc) output = argv[++i];
        else if (!strcmp(argv[i], "--check"))  opt_check = 1;
        else if (!strcmp(argv[i], "--tokens")) opt_tokens = 1;
        else if (!strcmp(argv[i], "--ast"))    opt_ast = 1;
        else if (!strcmp(argv[i], "-h") || !strcmp(argv[i], "--help")) { usage(argv[0]); return 0; }
        else if (argv[i][0] == '-') { fprintf(stderr, "unknown option '%s'\n", argv[i]); usage(argv[0]); return 2; }
        else input = argv[i];
    }
    if (!input) { usage(argv[0]); return 2; }

    if (errors_init(input) != 0 || !(yyin = fopen(input, "r"))) {
        fprintf(stderr, "gsc: cannot open '%s'\n", input);
        return 2;
    }

    /* ---- phase 1 (alone): token dump ---- */
    if (opt_tokens) return dump_tokens() ? 1 : 0;

    /* ---- phase 1+2: lexing and parsing are driven together by the parser ---- */
    int rc = yyparse();
    fclose(yyin);
    if (errors_count(ERR_LEXICAL) || errors_count(ERR_SYNTAX) || rc != 0 || !g_ast) {
        summary_failed();
        return 1;
    }
    if (opt_ast) { ast_print(g_ast); return 0; }

    /* ---- phase 3: semantic analysis ---- */
    if (semantic_analyze(g_ast) > 0) { summary_failed(); return 1; }

    if (opt_check) {
        printf("ok: %s passed all checks (%d warning(s))\n", input, warnings_count());
        return 0;
    }

    /* ---- phase 4: code generation ---- */
    char defname[1024];
    if (!output) {
        size_t n = strlen(input);
        if (n > 3 && !strcmp(input + n - 3, ".gs")) n -= 3;
        snprintf(defname, sizeof defname, "%.*s.py", (int)n, input);
        output = defname;
    }
    FILE *fo = fopen(output, "w");
    if (!fo) { fprintf(stderr, "gsc: cannot write '%s'\n", output); return 2; }
    codegen_python(g_ast, fo, runtime_dir);
    fclose(fo);
    printf("ok: wrote %s (%d warning(s))\n", output, warnings_count());
    errors_free();
    return 0;
}
