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
static Token* tokenize_fstr_literal(char** endptr, Token* last_token_ptr);

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
            // 3文字の記号は2文字より先に試す（`>>>` は文字列入力の文）
            ">>>",
            "==",
            "!=",
            "<=",
            ">=",
            "<<",
            ">>",
            "&&",
            "||",
            "|",
            "+=",
            "-=",
            "*=",
            "/=",
            "%=",
            "->",
            "<-",
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
            "?",
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

        // f-string（step 93）: 識別子 `f` の直後に引用符が続く形だけを
        // 特別扱いする。`f` という変数の直後に文字列が隣接する書き方は
        // 他に無いため、通常の識別子・文字列と衝突しない
        if (**endptr == 'f' && ((*endptr)[1] == '\"' || (*endptr)[1] == '\'')) {
            last_token_ptr = tokenize_fstr_literal(endptr, last_token_ptr);
            continue;
        }

        if (**endptr == '\"' || **endptr == '\'') {
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

static int hex_digit_value(char c) {
    if (c >= '0' && c <= '9') {
        return c - '0';
    }
    if (c >= 'a' && c <= 'f') {
        return c - 'a' + 10;
    }
    if (c >= 'A' && c <= 'F') {
        return c - 'A' + 10;
    }
    return -1;
}

// エスケープ1つ分をデコードする。*endptr が `\` を指している状態で
// 呼び、デコードしたバイトを返して *endptr をエスケープ列の直後まで
// 進める。対応表（step 90 で棚卸しして確定）: \n \t \r \\ \" \' \0 と
// \xHH（16進2桁）。それ以外は黙って文字化けさせず、位置つきエラーに
// する。first はリテラルの開始位置、quote は引用符（未終了エラーの
// 表示用）
static int decode_escape(char** endptr, char* first, char quote) {
    char e = (*endptr)[1];
    int val;
    if (e == 'n') {
        val = 10;
    } else if (e == 't') {
        val = 9;
    } else if (e == 'r') {
        val = 13;
    } else if (e == '\\') {
        val = 92;
    } else if (e == '\"') {
        val = 34;
    } else if (e == '\'') {
        val = 39;
    } else if (e == '0') {
        val = 0;
    } else if (e == 'x') {
        int hi = hex_digit_value((*endptr)[2]);
        // [2] が終端なら [3] は読みに行かない
        int lo = (*endptr)[2] ? hex_digit_value((*endptr)[3]) : -1;
        if (hi < 0 || lo < 0) {
            // 不正な場合は16進2桁分を読み進めない（直後の
            // 閉じ引用符や終端を越えて進まないため）
            error_at(*endptr - src, *endptr - src + 2, "`\\x`の後には16進数2桁が必要です");
            val = 0;
        } else {
            val = hi * 16 + lo;
            *endptr += 2;
        }
    } else if (e == '\0') {
        // バックスラッシュの直後で入力が尽きた。通常の
        // 未終了エラーとして扱う
        error_at(first - src, first - src + 1, "文字列を閉じる`%c`がないまま、ファイルの末尾に達しました", quote);
        val = 0;
    } else {
        error_at(*endptr - src, *endptr - src + 2, "文字列ではエスケープ`\\%c`は使えません", e);
        val = e;
    }
    *endptr += 2;
    return val;
}

static Token* tokenize_str_literal(char** endptr, Token* last_token_ptr) {
    char* first = *endptr;
    // 開始クォート（`"` または `'`）と同じ文字で閉じる。どちらの形でも
    // 同じ文字列リテラルで、エスケープの規則も共通
    char quote = **endptr;
    last_token_ptr = new_token(TK_STR, last_token_ptr);
    int len = 0;
    (*endptr)++;
    char* buf = malloc(strlen(*endptr) + 1);
    while (**endptr != quote) {
        if (**endptr == '\0' || **endptr == '\n') {
            error_at(first - src, first - src + 1, "文字列を閉じる`%c`がないまま、ファイルの末尾に達しました", quote);
        }
        if (**endptr == '\\') {
            buf[len] = decode_escape(endptr, first, quote);
        } else {
            buf[len] = **endptr;
            (*endptr)++;
        }
        len++;
    }
    if (**endptr != quote) {
        error_at(first - src, first - src + 1, "文字列を閉じる`%c`がないまま、ファイルの末尾に達しました", quote);
        exit(1);
    }
    (*endptr)++;
    last_token_ptr->str = buf;
    last_token_ptr->len = len;
    return last_token_ptr;
}

// f-string の式部の中にある文字列リテラルを読み飛ばす。p が引用符を
// 指している状態で呼び、閉じ引用符の直後を返す。中身の波括弧は
// 数えない（文字列の中ではただの文字）。閉じないまま入力が尽きたら
// NULL を返す（式部の中では、文字列が閉じないことは補間の `}` も
// 無いということなので、呼び出し側が `{` の位置でエラーにする）
static char* fstr_scan_skip_string(char* p) {
    char q = *p;
    p++;
    while (*p != q) {
        if (*p == '\0' || *p == '\n') {
            return NULL;
        }
        if (*p == '\\') {
            p++;
            if (*p == '\0') {
                return NULL;
            }
        }
        p++;
    }
    return p + 1;
}

static char* scan_fstr_literal(char* first);

// f-string リテラル（step 93）の構造検査。first が `f` を指している
// 状態で呼び、閉じ引用符の直後を返す。ここでは閉じ引用符の探索と
// 波括弧の構造検査だけを行い、部品への分割とデコードはパーサに
// 任せる。構造エラー（`{` の閉じ忘れ、空の `{}`、対応しない `}`、
// 式の中の `{`、未終了）は全てこの段階で位置つきエラーにする。
// 式部の中の文字列と入れ子の f-string は、それぞれ丸ごと読み
// 飛ばす（入れ子は再帰で検査する）。式部の中で文字列が閉じない
// 場合は、その補間の `}` も無いということなので、`{` の閉じ
// 忘れとして報告する
static char* scan_fstr_literal(char* first) {
    char quote = first[1];
    char* p = first + 2;
    while (true) {
        char c = *p;
        if (c == '\0' || c == '\n') {
            error_at(first - src, first - src + 2, "文字列を閉じる`%c`がないまま、ファイルの末尾に達しました", quote);
        }
        if (c == quote) {
            p++;
            break;
        }
        if (c == '\\') {
            // エスケープの次の文字は、引用符でも波括弧でもないもの
            // として読み飛ばす（正しさはデコード時に判定する）
            if (p[1] == '\0') {
                error_at(first - src, first - src + 2, "文字列を閉じる`%c`がないまま、ファイルの末尾に達しました", quote);
            }
            p += 2;
            continue;
        }
        if (c == '{') {
            if (p[1] == '{') {
                p += 2;
                continue;
            }
            // 式部: 対応する `}` まで。中の文字列は丸ごと読み飛ばし、
            // 波括弧の入れ子（ラムダ本体などのブロック）は v1 では
            // 未対応としてエラーにする
            char* brace = p;
            p++;
            if (*p == '}') {
                error_at(brace - src, brace - src + 2, "f-string の`{}`には式が必要です");
            }
            while (true) {
                char d = *p;
                if (d == '\0' || d == '\n') {
                    error_at(brace - src, brace - src + 1, "f-string の`{`を閉じる`}`がありません");
                }
                if (d == 'f' && (p[1] == '"' || p[1] == '\'') && !can_use_variables(p[-1])) {
                    // 入れ子の f-string は1リテラルとして読み飛ばす
                    // （その式部の中の文字列の波括弧に惑わされない）
                    p = scan_fstr_literal(p);
                    continue;
                }
                if (d == '"' || d == '\'') {
                    char* np = fstr_scan_skip_string(p);
                    if (!np) {
                        error_at(brace - src, brace - src + 1, "f-string の`{`を閉じる`}`がありません");
                    }
                    p = np;
                    continue;
                }
                if (d == '{') {
                    error_at(p - src, p - src + 1, "f-string の式の中では`{`は使えません");
                }
                if (d == '}') {
                    p++;
                    break;
                }
                p++;
            }
            continue;
        }
        if (c == '}') {
            if (p[1] == '}') {
                p += 2;
                continue;
            }
            error_at(p - src, p - src + 1, "f-string の`}`には対応する`{`がありません");
        }
        p++;
    }
    return p;
}

// *endptr が `f` を指している状態で呼ぶ。リテラル全体を1つの
// TK_FSTR トークンにする（str は `f` の位置、len は全体の長さ）
static Token* tokenize_fstr_literal(char** endptr, Token* last_token_ptr) {
    char* first = *endptr;
    char* end = scan_fstr_literal(first);
    last_token_ptr = new_token(TK_FSTR, last_token_ptr);
    last_token_ptr->str = first;
    last_token_ptr->len = end - first;
    *endptr = end;
    return last_token_ptr;
}

// リテラル1つ分を読み飛ばし、閉じ引用符の直後を返す。p は引用符、
// または `f` の位置（直後が引用符）を指す。構造は字句解析で検査
// 済みの前提で検証はせず、字句解析と全く同じ境界で読み飛ばす。
// パーサ（expr.c）が式部の終端を探すために使う
char* fstr_skip_literal(char* p) {
    if (p[0] == 'f' && (p[1] == '"' || p[1] == '\'')) {
        char quote = p[1];
        p += 2;
        while (*p != quote) {
            if (*p == '\\') {
                p += 2;
                continue;
            }
            if (*p == '{') {
                if (p[1] == '{') {
                    p += 2;
                    continue;
                }
                p++;
                while (*p != '}') {
                    if (*p == '"' || *p == '\'' ||
                        (*p == 'f' && (p[1] == '"' || p[1] == '\'') && !can_use_variables(p[-1]))) {
                        p = fstr_skip_literal(p);
                        continue;
                    }
                    p++;
                }
                p++;
                continue;
            }
            p++;
        }
        return p + 1;
    }
    char q = *p;
    p++;
    while (*p != q) {
        if (*p == '\\') {
            p++;
        }
        p++;
    }
    return p + 1;
}

// f-string のリテラル部分 [start, end) をデコードする（spell.h の
// 宣言を参照）。波括弧のエスケープ（`{{` と `}}`）はここで処理し、
// バックスラッシュエスケープは通常の文字列と共通の decode_escape
// で解釈する
char* decode_fstr_literal(char* start, char* end, int* out_len, char* first, char quote) {
    char* buf = malloc((end - start) + 1);
    int len = 0;
    char* p = start;
    while (p < end) {
        if (p[0] == '{' && p + 1 < end && p[1] == '{') {
            buf[len++] = '{';
            p += 2;
        } else if (p[0] == '}' && p + 1 < end && p[1] == '}') {
            buf[len++] = '}';
            p += 2;
        } else if (p[0] == '\\') {
            buf[len++] = decode_escape(&p, first, quote);
        } else {
            buf[len++] = *p;
            p++;
        }
    }
    buf[len] = '\0';
    *out_len = len;
    return buf;
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
        case TK_FSTR:
            snprintf(buf, sizeof(buf), "文字列");
            break;
    }
    return buf;
}
