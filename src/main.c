#include "spell.h"

char* src;
char* src_name;

static char* read_file(char* src_name);

int main(int argc, char **argv) {
    bool only_assembly = false;
    char* output_name = "a.out";
    src_name = NULL;
    for (int i=1; i<argc; i++) {
        if (strlen(argv[i]) == 2 && !memcmp(argv[i], "-S", 2)) {
            only_assembly = true;
        } else if (strlen(argv[i]) == 2 && !memcmp(argv[i], "-o", 2)) {
            output_name = argv[++i];
        } else {
            src_name = argv[i];
        }
    }

    if (!src_name) {
        error_plain("入力ファイルが指定されていません");
    }

    src = read_file(src_name);

    token = tokenize(src);

    Function* prog = program();

    char assembly_output_name[256];
    snprintf(assembly_output_name, 256, "%s.s", output_name);
    if (!freopen(assembly_output_name, "w", stdout)) {
        error_plain("ファイル%sを開けませんでした", src_name);
        exit(1);
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
            exit(1);
        }
    }

    return 0;
}

static char* read_file(char* src_name) {
    FILE *fp = fopen(src_name, "r");
    if (!fp) {
        error_plain("ファイル%sを開けませんでした", src_name);
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);
    char* src = malloc(size + 1);
    int read_size = fread(src, 1, size, fp);
    if (read_size != size) {
        error_plain("ファイル%sから%ldバイト読み取る予定でしたが、%ldバイトを読み取りました", src_name, size, read_size);
        exit(1);
    }
    src[size] = '\0';
    fclose(fp);
    return src;
}
