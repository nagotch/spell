#include "spell.h"

Token* token = NULL;

// 字句解析中の現在の行番号。new_token が各トークンへ記録する（step 55:
// 文の終わりを改行で判定するために使う）
static int cur_line = 1;

static Token* new_token(TokenKind token_kind, Token* pre_token_ptr) {
    Token* next_token_ptr = calloc(1, sizeof(Token));
    pre_token_ptr->next = next_token_ptr;
    next_token_ptr->kind = token_kind;
    next_token_ptr->line = cur_line;
    next_token_ptr->prev = pre_token_ptr;
    return next_token_ptr;
}

static bool can_use_variables_first(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_';
}

static bool can_use_variables(char c) {
    return ('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_' || ('0' <= c && c <= '9');
}

static Token* tokenize_str_literal(char** endptr, Token* last_token_ptr);

Token* tokenize(char* s) {
    Token first_token;
    first_token.next = NULL;
    Token* last_token_ptr = &first_token;
    cur_line = 1;

    char** endptr = &s;
    while (**endptr != '\0') {
        if (isspace(**endptr)) {
            if (**endptr == '\n') {
                cur_line++;
            }
            (*endptr)++;
            continue;
        }

        if ((*endptr)[1] != '\0' && !memcmp(*endptr, "//", 2)) {
            while (**endptr != '\n' && **endptr != '\0') {
                (*endptr)++;
            }
            continue;
        }

        if (!memcmp(*endptr, "/*", 2)) {
            char* q = strstr(*endptr + 2, "*/");
            if (!q) {
                error_at(*endptr - src, *endptr - src + 2, "コメントを閉じる`*/`がないまま、ファイルの末尾に達しました");
            }
            for (char* p = *endptr; p < q + 2; p++) {
                if (*p == '\n') {
                    cur_line++;
                }
            }
            *endptr = q + 2;
            continue;
        }

        bool is_symbol = false;
        char* targets[] = {
            "==",
            "!=",
            "<=",
            ">=",
            "<<",
            ">>",
            "&&",
            "||",
            "+=",
            "-=",
            "*=",
            "/=",
            "%=",
            "->",
            "..=",
            "..",
            "++",
            "--",
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
            "%",
            "!",
            ":",
            "@",
            "^",
        };
        size_t targets_size = sizeof(targets) / sizeof(targets[0]);
        for (int i = 0; i < targets_size; i++) {
            char* target = targets[i];
            int target_len = strlen(target);
            if (!strncmp(*endptr, target, target_len)) {
                last_token_ptr = new_token(TK_SYMBOL, last_token_ptr);
                last_token_ptr->str = *endptr;
                last_token_ptr->len = target_len;
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
            last_token_ptr->val = num;
            last_token_ptr->len = *endptr - prev_endptr;
            last_token_ptr->str = prev_endptr;
            continue;
        }

        char* keywords[] = {
            "return",
            "if",
            "else",
            "while",
            "for",
            "break",
            "continue",
            "def",
            "class",
            "in",
            "elif",
            "in_str",
            "in_line",
            "put",
            "yn",
            "fn",
        };
        Keyword kinds[] = {
            KW_RETURN,
            KW_IF,
            KW_ELSE,
            KW_WHILE,
            KW_FOR,
            KW_BREAK,
            KW_CONTINUE,
            KW_DEF,
            KW_CLASS,
            KW_IN,
            KW_ELIF,
            KW_IN_STR,
            KW_IN_LINE,
            KW_PUT,
            KW_YN,
            KW_FN,
        };
        bool is_keyword = false;
        size_t keywords_size = sizeof(keywords) / sizeof(keywords[0]);
        for (int i = 0; i < keywords_size; i++) {
            char* keyword = keywords[i];
            size_t keyword_size = strlen(keyword);
            Keyword kind = kinds[i];
            if (!strncmp(*endptr, keyword, keyword_size) && !can_use_variables((*endptr)[keyword_size])) {
                last_token_ptr = new_token(TK_KEYWORD, last_token_ptr);
                last_token_ptr->keyword = kind;
                last_token_ptr->len = keyword_size;
                last_token_ptr->str = *endptr;
                *endptr += keyword_size;
                is_keyword = true;
                break;
            }
        }
        if (is_keyword) {
            continue;
        }

        if (**endptr == '\"') {
            last_token_ptr = tokenize_str_literal(endptr, last_token_ptr);
            continue;
        }

        if (can_use_variables_first(**endptr)) {
            last_token_ptr = new_token(TK_IDENT, last_token_ptr);
            last_token_ptr->str = *endptr;
            last_token_ptr->len = 0;
            while (can_use_variables(**endptr)) {
                last_token_ptr->len++;
                (*endptr)++;
            }
            continue;
        }

        error_at(*endptr - src, *endptr - src + 1, "ここでは文字`%c`を使えません", **endptr);
    }
    last_token_ptr = new_token(TK_EOF, last_token_ptr);
    last_token_ptr->str = *endptr;
    // 先頭トークンの prev は、スタック上のダミーを指したままにしない
    first_token.next->prev = NULL;
    return first_token.next;
}

static Token* tokenize_str_literal(char** endptr, Token* last_token_ptr) {
    char* first = *endptr;
    last_token_ptr = new_token(TK_STR, last_token_ptr);
    int len = 0;
    (*endptr)++;
    char* buf = malloc(strlen(*endptr) + 1);
    while (**endptr != '\"') {
        if (**endptr == '\0' || **endptr == '\n') {
            error_at(first - src, first - src + 1, "文字列を閉じる`\"`がないまま、ファイルの末尾に達しました");
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
        error_at(first - src, first - src + 1, "文字列を閉じる`\"`がないまま、ファイルの末尾に達しました");
        exit(1);
    }
    (*endptr)++;
    last_token_ptr->str = buf;
    last_token_ptr->len = len;
    return last_token_ptr;
}

long token_start(Token* tok) {
    return tok->str - src;
}

long token_end(Token* tok) {
    return token_start(tok) + tok->len;
}

char* token_text(Token* tok) {
    static char buf[256];
    switch (tok->kind) {
        case TK_NUM:
            snprintf(buf, sizeof(buf), "数値`%ld`", tok->val);
            break;
        case TK_EOF:
            return NULL;
            break;
        case TK_IDENT:
            snprintf(buf, sizeof(buf), "名前`%.*s`", tok->len, tok->str);
            break;
        case TK_KEYWORD:
        case TK_SYMBOL:
            snprintf(buf, sizeof(buf), "`%.*s`", tok->len, tok->str);
            break;
        case TK_STR:
            snprintf(buf, sizeof(buf), "文字列");
            break;
    }
    return buf;
}
