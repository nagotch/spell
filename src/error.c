#include "spell.h"
#include <stdarg.h>

static long utf8_read(long i, long* cp);
static long char_width(long cp);

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

static long utf8_read(long i, long* cp) {
    unsigned char c = (unsigned char)src[i];
    if (c < 0x80) {
        *cp = c;
        return 1;
    } else if ((c & 0xE0) == 0xC0) {
        *cp = ((c & 0x1F) << 6) | ((unsigned char)src[i + 1] & 0x3F);
        return 2;
    } else if ((c & 0xF0) == 0xE0) {
        *cp = ((c & 0x0F) << 12) | ((unsigned char)src[i + 1] & 0x3F) << 6 |
              ((unsigned char)src[i + 2] & 0x3F);
        return 3;
    } else {
        *cp = ((c & 0x07) << 18) | ((unsigned char)src[i + 1] & 0x3F) << 12 |
              ((unsigned char)src[i + 2] & 0x3F) << 6 | ((unsigned char)src[i + 3] & 0x3F);
        return 4;
    }
}

static long char_width(long cp) {
    if ((cp >= 0x1100 && cp <= 0x115F) || (cp >= 0x2E80 && cp <= 0x9FFF) ||
        (cp >= 0xAC00 && cp <= 0xD7A3) || (cp >= 0xF900 && cp <= 0xFAFF) ||
        (cp >= 0xFE30 && cp <= 0xFE6F) || (cp >= 0xFF00 && cp <= 0xFF60) ||
        (cp >= 0xFFE0 && cp <= 0xFFE6) || cp >= 0x20000) {
        return 2;
    }
    return 1;
}
