#include "spell.h"

Token* token = NULL;

Token* new_token(TokenKind token_kind, Token* pre_token_ptr) {
    Token* next_token_ptr = calloc(1, sizeof(Token));
    pre_token_ptr -> next = next_token_ptr;
    next_token_ptr -> kind = token_kind;
    return next_token_ptr;
}

bool can_use_variables_first(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

bool can_use_variables(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_' || ('0' <= c && c <= '9');
}

Token* tokenize(char* s) {
    Token first_token;
    first_token.next = NULL;
    Token* last_token_ptr = &first_token;

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
            "{",
            "}",
            ",",
            "&",
            "[",
            "]",
            ".",
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

        char* keywords[] = {
            "return",
            "if",
            "else",
            "while",
            "for",
        };
        TokenKind kinds[] = {
            TK_RETURN,
            TK_IF,
            TK_ELSE,
            TK_WHILE,
            TK_FOR,
        };
        bool is_keyword = false;
        size_t keywords_size = sizeof(keywords) / sizeof(keywords[0]);
        for (int i = 0; i < keywords_size; i++) {
            char* keyword = keywords[i];
            size_t keyword_size = strlen(keyword);
            TokenKind kind = kinds[i];
            if (!strncmp(*endptr, keyword, keyword_size) && !can_use_variables((*endptr)[keyword_size])) {
                last_token_ptr = new_token(kind, last_token_ptr);
                last_token_ptr -> len = keyword_size;
                last_token_ptr -> str = *endptr;
                *endptr += keyword_size;
                is_keyword = true;
                break;
            }
        }
        if (is_keyword) {
            continue;
        }

        if (**endptr == '\"') {
            last_token_ptr = new_token(TK_STR, last_token_ptr);
            int len = 0;
            (*endptr)++;
            char* buf = malloc(strlen(*endptr) + 1);
            while (**endptr != '\"') {
                if (**endptr == '\0' || **endptr == '\n') {
                    fprintf(stderr, "\"で閉じられていない");
                    exit(1);
                }
                if (**endptr == '\\') {
                    if ((*endptr)[1] == 'n') {
                        buf[len] = 10;
                    } else if ((*endptr)[1] == 't') {
                        buf[len] = 9;
                    } else if ((*endptr)[1] == '\\') {
                        buf[len] = 92;
                    } else if ((*endptr)[1] == '\"') {
                        buf[len] = 34;
                    } else if ((*endptr)[1] == '0') {
                        buf[len] = 0;
                    } else {
                        buf[len] = (*endptr)[1];
                    }
                    (*endptr)++;
                } else {
                    buf[len] = **endptr;
                }
                (*endptr)++;
                len++;
            }
            if (**endptr != '\"') {
                fprintf(stderr, "\"で閉じられていない");
                exit(1);
            }
            (*endptr)++;
            last_token_ptr -> str = buf;
            last_token_ptr -> len = len;
            continue;
        }

        if (can_use_variables_first(**endptr)) {
            last_token_ptr = new_token(TK_IDENT, last_token_ptr);
            last_token_ptr -> str = *endptr;
            last_token_ptr -> len = 0;
            while (can_use_variables(**endptr)) {
                last_token_ptr -> len++;
                (*endptr)++;
            }
            continue;
        }

        fprintf(stderr, "不明な文字%s", *endptr);
        exit(1);
    }
    last_token_ptr = new_token(TK_EOF, last_token_ptr);
    return first_token.next;
}
