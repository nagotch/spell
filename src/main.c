#define _POSIX_C_SOURCE 200809L
#include "spell.h"
#include <unistd.h>

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
    spell_prelude_path(prelude_path, sizeof(prelude_path));

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
    prog = ensure_entry_point(prog);
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
