#define _POSIX_C_SOURCE 200809L
#include "spell.h"
#include <unistd.h>

char* src;
char* src_name;

static char* read_file(char* src_name);
static Function* load_program(char* path);

int main(int argc, char **argv) {
    bool only_assembly = false;
    char* output_name = "a.out";
    src_name = NULL;
    char* input_name = NULL;
    for (int i=1; i<argc; i++) {
        if (strlen(argv[i]) == 2 && !memcmp(argv[i], "-S", 2)) {
            only_assembly = true;
        } else if (strlen(argv[i]) == 2 && !memcmp(argv[i], "-o", 2)) {
            output_name = argv[++i];
        } else {
            input_name = argv[i];
        }
    }

    if (!input_name) {
        error_plain("入力ファイルが指定されていません");
    }

    char prelude_path[4096];
    ssize_t path_len = readlink("/proc/self/exe", prelude_path, sizeof(prelude_path));
    if (path_len < 0) {
        error_plain("コンパイラの実行ファイルの場所を特定できませんでした");
    }
    prelude_path[path_len] = '\0';
    char* slash = strrchr(prelude_path, '/');
    strcpy(slash + 1, "prelude.sp");

    Function* prog = load_program(prelude_path);
    Function* user_prog = load_program(input_name);
    if (prog) {
        Function* tail = prog;
        while (tail->next) {
            tail = tail->next;
        }
        tail->next = user_prog;
    } else {
        prog = user_prog;
    }
    desugar(prog);
    typecheck(prog);

    char assembly_output_name[256];
    snprintf(assembly_output_name, 256, "%s.s", output_name);
    if (!freopen(assembly_output_name, "w", stdout)) {
        error_plain("ファイル`%s`を開けませんでした", assembly_output_name);
    }

    printf(".intel_syntax noprefix\n");

    codegen(prog);

    printf(".section .note.GNU-stack,\"\",@progbits\n");

    fflush(stdout);

    char command[512];
    snprintf(command, 512, "cc -o \"%s\" \"%s\"", output_name, assembly_output_name);
    if (!only_assembly) {
        if (system(command)) {
            error_plain("実行ファイルを作成できませんでした");
        }
    }

    return 0;
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

static Function* load_program(char* path) {
    src_name = path;
    src = read_file(path);
    token = tokenize(src);
    return program();
}
