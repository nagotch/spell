#include <stdio.h>
#include <stdlib.h>

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "1つの引数を指定してください\n");
        return 1;
    }

    printf(".intel_syntax noprefix\n");
    printf(".global main\n");
    printf("main:\n");

    char* endptr;
    long num = strtol(argv[1], &endptr, 10);
    printf("  mov rax, %ld\n", num);

    char* new_endptr = endptr;
    while (*new_endptr != '\0') {
        if (*new_endptr != '+' && *new_endptr != '-') {
            fprintf(stderr, "不明な演算子\n");
        }
        char operator = *new_endptr;

        num = strtol(new_endptr+1, &new_endptr, 10);
        if (operator == '+') {
            printf("  add rax, %ld\n", num);
        } else {
            printf("  sub rax, %ld\n", num);
        }
    }

    printf("  ret\n");
    printf(".section .note.GNU-stack,\"\",@progbits\n");

    return 0;
}
