#ifndef GS_ERRORS_H
#define GS_ERRORS_H

/* The three error families required by the project. Each is counted
 * separately so the driver can report "2 lexical, 1 syntax, 0 semantic". */
typedef enum { ERR_LEXICAL, ERR_SYNTAX, ERR_SEMANTIC } ErrorKind;

/* Load the source file so error messages can quote the offending line. */
int  errors_init(const char *filename);
void errors_free(void);

/* Report an error at (line, col), both 1-based. printf-style message. */
void report_error(ErrorKind kind, int line, int col, const char *fmt, ...);

/* Warnings never fail the compilation; they are counted separately. */
void report_warning(int line, int col, const char *fmt, ...);
int  warnings_count(void);

/* Collect diagnostics and print them sorted by (line, column). */
void errors_batch_begin(void);
void errors_batch_flush(void);

int  errors_count(ErrorKind kind);
int  errors_total(void);

#endif
