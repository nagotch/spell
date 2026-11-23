#include "spell.h"
#include <stdarg.h>

void error_at(long start, long end, char* fmt, ...) {
    long line = 1;
    long line_start = 0;
    for (long i = 0; i < start; i++) {
        if (src[i] == '\n') {
            line++;
            line_start = i + 1;
        }
    }
    long col = start - line_start + 1;

    fprintf(stderr, "%s:%ld:%ld: エラー: ", src_name, line, col);
    
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");

    long line_end = line_start;
    while (src[line_end] != '\n' && src[line_end] != '\0') {
        line_end++;
    }
    fprintf(stderr, "%ld | ", line);
    fwrite(&src[line_start], 1, line_end - line_start, stderr);
    fprintf(stderr, "\n");

    exit(1);
}
