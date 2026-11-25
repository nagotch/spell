#include "spell.h"
#include <stdarg.h>

// 現在処理中のソースとその名前。main.c と lsp.c の両方から使うため、
// 実体はここ（共通オブジェクト側）に置く
char* src;
char* src_name;

static long utf8_read(long i, long* cp);
static long char_width(long cp);

// エラーの回収（LSP サーバー用）。通常モードでは capture_enabled が
// false で、以下の分岐はすべて素通りする
static bool capture_enabled;
static jmp_buf capture_jmp;
char error_captured_message[4096];
long error_captured_start;
long error_captured_end;
bool error_captured_has_pos;
char* error_captured_src;

jmp_buf* error_capture_begin(void) {
    capture_enabled = true;
    return &capture_jmp;
}

void error_capture_end(void) {
    capture_enabled = false;
}

static void capture_error(long start, long end, bool has_pos, char* fmt, va_list ap) {
    vsnprintf(error_captured_message, sizeof(error_captured_message), fmt, ap);
    error_captured_start = start;
    error_captured_end = end;
    error_captured_has_pos = has_pos;
    error_captured_src = src;
    longjmp(capture_jmp, 1);
}

void error_at(long start, long end, char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    error_at_v(start, end, fmt, ap);
    va_end(ap);
}

void error_at_v(long start, long end, char* fmt, va_list ap) {
    if (capture_enabled) {
        capture_error(start, end, true, fmt, ap);
    }
    long line = 1;
    long line_start = 0;
    for (long i = 0; i < start; i++) {
        if (src[i] == '\n') {
            line++;
            line_start = i + 1;
        }
    }
    long col = 1;
    for (long i = line_start; i < start; ) {
        long cp;
        i += utf8_read(i, &cp);
        col++;
    }

    fprintf(stderr, "%s:%ld:%ld: エラー: ", src_name, line, col);

    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");

    long line_end = line_start;
    while (src[line_end] != '\n' && src[line_end] != '\0') {
        line_end++;
    }
    fprintf(stderr, "%ld | ", line);
    long disp = 0;
    for (long i = line_start; i < line_end; ) {
        if (src[i] == '\t') {
            long n = 4 - (disp % 4);
            for (long k = 0; k < n; k++) {
                fputc(' ', stderr);
            }
            disp += n;
            i++;
        } else {
            long cp;
            long len = utf8_read(i, &cp);
            fwrite(&src[i], 1, len, stderr);
            disp += char_width(cp);
            i += len;
        }
    }
    fprintf(stderr, "\n");

    long digits = 1;
    for (long t = line; t >= 10; t /= 10) {
        digits++;
    }
    for (long k = 0; k < digits; k++) {
        fputc(' ', stderr);
    }
    fprintf(stderr, " | ");

    long udisp = 0;
    for (long i = line_start; i < start && i < line_end; ) {
        if (src[i] == '\t') {
            udisp += 4 - (udisp % 4);
            i++;
        } else {
            long cp;
            long len = utf8_read(i, &cp);
            udisp += char_width(cp);
            i += len;
        }
    }
    for (long k = 0; k < udisp; k++) {
        fputc(' ', stderr);
    }

    long w = 0;
    for (long i = start; i < end && i < line_end; ) {
        if (src[i] == '\t') {
            w += 4 - ((udisp + w) % 4);
            i++;
        } else {
            long cp;
            long len = utf8_read(i, &cp);
            w += char_width(cp);
            i += len;
        }
    }
    if (w == 0) {
        w = 1;
    }
    for (long k = 0; k < w; k++) {
        fputc('^', stderr);
    }
    fprintf(stderr, "\n");

    exit(1);
}

void error_plain(char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    error_plain_v(fmt, ap);
    va_end(ap);
}

void error_plain_v(char* fmt, va_list ap) {
    if (capture_enabled) {
        capture_error(0, 0, false, fmt, ap);
    }
    fprintf(stderr, "エラー: ");
    vfprintf(stderr, fmt, ap);
    fprintf(stderr, "\n");
    exit(1);
}

void error_internal(char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    if (capture_enabled) {
        capture_error(0, 0, false, fmt, ap);
    }
    fprintf(stderr, "内部エラー: ");
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fprintf(stderr, "\n");
    fprintf(stderr, "プログラムではなくコンパイラ側の問題です\n");
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
