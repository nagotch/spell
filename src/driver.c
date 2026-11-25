// コンパイルのパイプラインの入口。ファイルを読み、字句解析して
// 構文木（関数リスト）を返すところまで。CLI（main.c）と LSP
// サーバー（lsp.c）の両方から使う
#define _POSIX_C_SOURCE 200809L
#include "spell.h"
#include <unistd.h>

static char* read_file(char* src_name);
static Function* load_program_with(char* text, char* name);

// 実行ファイルと同じ場所にある prelude.sp のパスを buf へ書く
void spell_prelude_path(char* buf, long size) {
    ssize_t path_len = readlink("/proc/self/exe", buf, size - 16);
    if (path_len < 0) {
        error_plain("コンパイラの実行ファイルの場所を特定できませんでした");
    }
    buf[path_len] = '\0';
    char* slash = strrchr(buf, '/');
    strcpy(slash + 1, "prelude.sp");
}

Function* load_program(char* path) {
    return load_program_with(read_file(path), path);
}

// ファイルではなく、メモリ上のソース文字列から読み込む（LSP サーバー用）
Function* load_program_text(char* text, char* name) {
    return load_program_with(text, name);
}

static Function* load_program_with(char* text, char* name) {
    src_name = name;
    src = text;
    token = tokenize(src);
    return program();
}

static char* read_file(char* src_name) {
    FILE *fp = fopen(src_name, "r");
    if (!fp) {
        error_plain("ファイル`%s`を開けませんでした", src_name);
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);
    char* src = malloc(size + 1);
    int read_size = fread(src, 1, size, fp);
    if (read_size != size) {
        error_plain("ファイル`%s`から%ldバイト読み取る予定でしたが、%ldバイトを読み取りました", src_name, size, read_size);
    }
    src[size] = '\0';
    fclose(fp);
    return src;
}
