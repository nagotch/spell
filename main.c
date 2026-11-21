#include "spell.h"

int main(int argc, char **argv) {
    char** tail = argv;
    (*tail)++;
    bool only_assembly = false;
    char* output_name = "a.out";
    char* input_name = NULL;
    while (tail) {
        if (strlen(*tail) == 2 && !memcmp(*tail, "-S", 2)) {
            only_assembly = true;
        } else if (strlen(*tail) == 2 && !memcmp(*tail, "-o", 2)) {
            (*tail)++;
            output_name = *tail;
        } else {
            input_name = *tail;
        }
        (*tail)++;
    }

    FILE *fp = fopen(input_name, "r");
    if (!fp) {
        fprintf(stderr, "ファイルが開けません: %s", input_name);
        exit(1);
    }
    fseek(fp, 0, SEEK_END);
    long size = ftell(fp);
    rewind(fp);
    char* src = malloc(size + 1);
    fread(src, 1, size, fp);
    src[size] = '\0';
    fclose(fp);

    token = tokenize(src);

    Function* prog = program();

    char* assembly_output_name;
    snprintf(assembly_output_name, 100, "%s.s", output_name);
    freopen(assembly_output_name, "w", stdout);

    printf(".intel_syntax noprefix\n");

    codegen(prog);

    printf(".section .note.GNU-stack,\"\",@progbits\n");

    char* command;
    snprintf(command, 100, "cc -o \"%s\" \"%s\"", output_name, assembly_output_name);
    if (!only_assembly) {
        if (system(command)) {
            fprintf(stderr, "アセンブリからバイナリに変換するのを失敗");
            exit(1);
        }
    }

    return 0;
}
