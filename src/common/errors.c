#include "errors.h"
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static char       *src_text = NULL;
static const char *src_name = "<input>";
static int         counts[3] = {0, 0, 0};
static int         warn_count = 0;

static const char *kind_name[] = {"lexical error", "syntax error", "semantic error"};

int errors_init(const char *filename) {
    FILE *f = fopen(filename, "rb");
    if (!f) return -1;
    fseek(f, 0, SEEK_END);
    long n = ftell(f);
    fseek(f, 0, SEEK_SET);
    src_text = malloc((size_t)n + 1);
    if (!src_text) { fclose(f); return -1; }
    size_t got = fread(src_text, 1, (size_t)n, f);
    src_text[got] = '\0';
    fclose(f);
    src_name = filename;
    return 0;
}

void errors_free(void) { free(src_text); src_text = NULL; }

/* Print source line number `line` (1-based) followed by a caret under `col`. */
static void print_context(int line, int col) {
    if (!src_text || line < 1) return;
    const char *p = src_text;
    for (int l = 1; l < line && *p; p++)
        if (*p == '\n') l++;
    const char *end = strchr(p, '\n');
    int len = end ? (int)(end - p) : (int)strlen(p);
    if (len > 0 && p[len - 1] == '\r') len--;
    fprintf(stderr, "    %.*s\n    ", len, p);
    for (int i = 1; i < col && i <= len; i++) fputc(p[i - 1] == '\t' ? '\t' : ' ', stderr);
    fputs("^\n", stderr);
}

/* Diagnostics can be collected ("batched") and printed sorted by position.
 * Semantic analysis uses this: it works in several passes, so its errors are
 * found out of source order, but the user wants to read them top to bottom. */
typedef struct { int line, col; char label[24]; char *msg; } Diag;
static Diag *batch = NULL;
static int   batch_n = 0, batch_cap = 0, batching = 0;

static void print_diag(const char *label, int line, int col, const char *msg) {
    fprintf(stderr, "%s:%d:%d: %s: %s\n", src_name, line, col, label, msg);
    print_context(line, col);
}

static void emit(const char *label, int line, int col, const char *msg) {
    if (!batching) { print_diag(label, line, col, msg); return; }
    if (batch_n == batch_cap) {
        batch_cap = batch_cap ? batch_cap * 2 : 32;
        batch = realloc(batch, (size_t)batch_cap * sizeof *batch);
    }
    Diag *d = &batch[batch_n++];
    d->line = line; d->col = col; d->msg = strdup(msg);
    snprintf(d->label, sizeof d->label, "%s", label);
}

void errors_batch_begin(void) { batching = 1; }

void errors_batch_flush(void) {
    for (int i = 1; i < batch_n; i++) {            /* stable insertion sort */
        Diag key = batch[i];
        int j = i - 1;
        while (j >= 0 && (batch[j].line > key.line ||
                          (batch[j].line == key.line && batch[j].col > key.col))) {
            batch[j + 1] = batch[j]; j--;
        }
        batch[j + 1] = key;
    }
    for (int i = 0; i < batch_n; i++) {
        print_diag(batch[i].label, batch[i].line, batch[i].col, batch[i].msg);
        free(batch[i].msg);
    }
    batch_n = 0; batching = 0;
}

static void vemit(const char *label, int line, int col, const char *fmt, va_list ap) {
    char msg[1024];
    vsnprintf(msg, sizeof msg, fmt, ap);
    emit(label, line, col, msg);
}

void report_error(ErrorKind kind, int line, int col, const char *fmt, ...) {
    counts[kind]++;
    va_list ap;
    va_start(ap, fmt);
    vemit(kind_name[kind], line, col, fmt, ap);
    va_end(ap);
}

void report_warning(int line, int col, const char *fmt, ...) {
    warn_count++;
    va_list ap;
    va_start(ap, fmt);
    vemit("warning", line, col, fmt, ap);
    va_end(ap);
}

int warnings_count(void) { return warn_count; }

int errors_count(ErrorKind kind) { return counts[kind]; }
int errors_total(void) { return counts[0] + counts[1] + counts[2]; }
