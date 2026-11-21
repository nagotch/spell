#include "spell.h"

int main(int argc, char **argv) {
    bool only_assembly = false;
    char* output_name = "a.out";
    char* input_name = NULL;
    for (int i=1; i<argc; i++) {
        if (strlen(argv[i]) == 2 && !memcmp(argv[i], "-S", 2)) {
            only_assembly = true;
        } else if (strlen(argv[i]) == 2 && !memcmp(argv[i], "-o", 2)) {
            (argv[i])++;
            output_name = argv[i];
        } else {
            input_name = argv[i];
        }
        (argv[i])++;
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

    char assembly_output_name[256];
    snprintf(assembly_output_name, 256, "%s.s", output_name);
    freopen(assembly_output_name, "w", stdout);

    printf(".intel_syntax noprefix\n");

    codegen(prog);

    printf(".section .note.GNU-stack,\"\",@progbits\n");

    char command[256];
    snprintf(command, 256, "cc -o \"%s\" \"%s\"", output_name, assembly_output_name);
    if (!only_assembly) {
        if (system(command)) {
            fprintf(stderr, "アセンブリからバイナリに変換するのを失敗");
            exit(1);
        }
    }

    return 0;
}
