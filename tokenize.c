#include "spell.h"

struct Token* token = NULL;

Token* new_token(TokenKind token_kind, struct Token* pre_token_ptr) {
    struct Token* next_token_ptr = calloc(1, sizeof(struct Token));
    pre_token_ptr -> next = next_token_ptr;
    next_token_ptr -> kind = token_kind;
    return next_token_ptr;
}

struct Token* tokenize(char* s) {
    struct Token first_token;
    first_token.next = NULL;
    struct Token* last_token_ptr = &first_token;

    char** endptr = &s;
    while (**endptr != '\0') {
        if (isspace(**endptr)) {
            (*endptr)++;
            continue;
        }

        bool is_symbol = false;
        char* targets[] = {
            "==",
            "!=",
            "<=",
            ">=",
            "+",
            "-",
            "*",
            "/",
            "(",
            ")",
            "<",
            ">",
            "=",
            ";",
        };
        size_t targets_size = sizeof(targets) / sizeof(targets[0]);
        for (int i = 0; i < targets_size; i++) {
            char* target = targets[i];
            int target_len = strlen(target);
            if (!strncmp(*endptr, target, target_len)) {
                last_token_ptr = new_token(TK_SYMBOL, last_token_ptr);
                last_token_ptr -> str = *endptr;
                last_token_ptr -> len = target_len;
                (*endptr) += target_len;
                is_symbol = true;
                break;
            }
        }
        if (is_symbol) {
            continue;
        }

        if (isdigit(**endptr)) {
            char* prev_endptr = *endptr;
            long num = strtol(*endptr, endptr, 10);
            last_token_ptr = new_token(TK_NUM, last_token_ptr);
            last_token_ptr -> val = num;
            last_token_ptr -> len = *endptr - prev_endptr;
            last_token_ptr -> str = prev_endptr;
            continue;
        }

        if ('a' <= **endptr && **endptr <= 'z') {
            last_token_ptr = new_token(TK_IDENT, last_token_ptr);
            last_token_ptr -> str = *endptr;
            last_token_ptr -> len = 1;
            (*endptr)++;
        }

        fprintf(stderr, "不明な文字%s", *endptr);
        exit(1);
    }
    last_token_ptr = new_token(TK_EOF, last_token_ptr);
    return first_token.next;
}
