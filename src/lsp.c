// spell の言語サーバー（LSP）。v1 は診断（publishDiagnostics）のみ。
// stdio で JSON-RPC を話し、didOpen / didChange のたびに全文を
// チェックして診断を push する。
//
// チェックは fork した子プロセスで1回使い切りにする。コンパイラ本体は
// グローバル状態を積み上げる一回限りの設計で、プロセス内に状態を
// 残したまま再チェックすると前回の定義が残ってしまうため。
// 子プロセスでは error.c のキャプチャを有効にし、最初のエラーを
// 診断としてパイプで親へ返す。codegen は実行しない。
#define _POSIX_C_SOURCE 200809L
#include "spell.h"
#include <unistd.h>
#include <strings.h>
#include <errno.h>
#include <fcntl.h>
#include <sys/types.h>
#include <sys/wait.h>

// ===== 最小 JSON パーサ =====
// 必要な値（null / bool / 数値 / 文字列 / 配列 / オブジェクト）だけを
// 木構造へ読み、使い終わったら json_free で解放する

typedef enum { J_NULL, J_BOOL, J_NUM, J_STR, J_ARR, J_OBJ } JType;

typedef struct Json Json;
struct Json {
    JType type;
    char* str;    // J_STR
    double num;   // J_NUM
    bool boolean; // J_BOOL
    Json** items; // J_ARR / J_OBJ の値
    char** keys;  // J_OBJ のキー
    int count;
};

typedef struct {
    char* p;
    int depth;
} JParser;

// これより深い入れ子は不正な入力として扱う（再帰でスタックを
// 使い切ってサーバーごと落ちるのを防ぐ）
#define JSON_MAX_DEPTH 100

static Json* json_new(JType type) {
    Json* j = calloc(1, sizeof(Json));
    j->type = type;
    return j;
}

static void json_skip_ws(JParser* ps) {
    while (*ps->p == ' ' || *ps->p == '\t' || *ps->p == '\n' || *ps->p == '\r') {
        ps->p++;
    }
}

static void buf_putc(char** buf, long* len, long* cap, char c) {
    if (*len + 1 >= *cap) {
        *cap = *cap ? *cap * 2 : 32;
        *buf = realloc(*buf, *cap);
    }
    (*buf)[(*len)++] = c;
    (*buf)[*len] = '\0';
}

static void buf_put_utf8(char** buf, long* len, long* cap, long cp) {
    if (cp < 0x80) {
        buf_putc(buf, len, cap, (char)cp);
    } else if (cp < 0x800) {
        buf_putc(buf, len, cap, (char)(0xC0 | (cp >> 6)));
        buf_putc(buf, len, cap, (char)(0x80 | (cp & 0x3F)));
    } else if (cp < 0x10000) {
        buf_putc(buf, len, cap, (char)(0xE0 | (cp >> 12)));
        buf_putc(buf, len, cap, (char)(0x80 | ((cp >> 6) & 0x3F)));
        buf_putc(buf, len, cap, (char)(0x80 | (cp & 0x3F)));
    } else {
        buf_putc(buf, len, cap, (char)(0xF0 | (cp >> 18)));
        buf_putc(buf, len, cap, (char)(0x80 | ((cp >> 12) & 0x3F)));
        buf_putc(buf, len, cap, (char)(0x80 | ((cp >> 6) & 0x3F)));
        buf_putc(buf, len, cap, (char)(0x80 | (cp & 0x3F)));
    }
}

static int hex_val(char c) {
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'f') return c - 'a' + 10;
    if (c >= 'A' && c <= 'F') return c - 'A' + 10;
    return -1;
}

// "..." を読み、エスケープを戻した文字列を返す。失敗時は NULL
static char* json_parse_string_raw(JParser* ps) {
    if (*ps->p != '"') {
        return NULL;
    }
    ps->p++;
    char* buf = NULL;
    long len = 0, cap = 0;
    while (*ps->p && *ps->p != '"') {
        char c = *ps->p;
        if (c == '\\') {
            ps->p++;
            char e = *ps->p;
            if (e == '\0') {
                free(buf);
                return NULL;
            }
            ps->p++;
            switch (e) {
                case '"': buf_putc(&buf, &len, &cap, '"'); break;
                case '\\': buf_putc(&buf, &len, &cap, '\\'); break;
                case '/': buf_putc(&buf, &len, &cap, '/'); break;
                case 'b': buf_putc(&buf, &len, &cap, '\b'); break;
                case 'f': buf_putc(&buf, &len, &cap, '\f'); break;
                case 'n': buf_putc(&buf, &len, &cap, '\n'); break;
                case 'r': buf_putc(&buf, &len, &cap, '\r'); break;
                case 't': buf_putc(&buf, &len, &cap, '\t'); break;
                case 'u': {
                    long cp = 0;
                    for (int i = 0; i < 4; i++) {
                        int h = hex_val(ps->p[i]);
                        if (h < 0) {
                            free(buf);
                            return NULL;
                        }
                        cp = cp * 16 + h;
                    }
                    ps->p += 4;
                    // サロゲートペアを1文字へ
                    if (cp >= 0xD800 && cp <= 0xDBFF && ps->p[0] == '\\' && ps->p[1] == 'u') {
                        long lo = 0;
                        bool ok = true;
                        for (int i = 0; i < 4; i++) {
                            int h = hex_val(ps->p[2 + i]);
                            if (h < 0) {
                                ok = false;
                                break;
                            }
                            lo = lo * 16 + h;
                        }
                        if (ok && lo >= 0xDC00 && lo <= 0xDFFF) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            ps->p += 6;
                        }
                    }
                    buf_put_utf8(&buf, &len, &cap, cp);
                    break;
                }
                default:
                    free(buf);
                    return NULL;
            }
        } else {
            buf_putc(&buf, &len, &cap, c);
            ps->p++;
        }
    }
    if (*ps->p != '"') {
        free(buf);
        return NULL;
    }
    ps->p++;
    if (!buf) {
        buf = calloc(1, 1);
    }
    return buf;
}

static Json* json_parse_value(JParser* ps);
static void json_free(Json* j);

static void json_push(Json* parent, char* key, Json* value) {
    parent->items = realloc(parent->items, sizeof(Json*) * (parent->count + 1));
    if (parent->type == J_OBJ) {
        parent->keys = realloc(parent->keys, sizeof(char*) * (parent->count + 1));
        parent->keys[parent->count] = key;
    }
    parent->items[parent->count] = value;
    parent->count++;
}

static Json* json_parse_value(JParser* ps) {
    json_skip_ws(ps);
    char c = *ps->p;
    if (c == '{' || c == '[') {
        if (++ps->depth > JSON_MAX_DEPTH) {
            return NULL;
        }
        Json* container = json_new(c == '{' ? J_OBJ : J_ARR);
        char close = (c == '{') ? '}' : ']';
        ps->p++;
        json_skip_ws(ps);
        if (*ps->p == close) {
            ps->p++;
            ps->depth--;
            return container;
        }
        while (true) {
            char* key = NULL;
            if (container->type == J_OBJ) {
                json_skip_ws(ps);
                key = json_parse_string_raw(ps);
                if (!key) {
                    json_free(container);
                    return NULL;
                }
                json_skip_ws(ps);
                if (*ps->p != ':') {
                    free(key);
                    json_free(container);
                    return NULL;
                }
                ps->p++;
            }
            Json* value = json_parse_value(ps);
            if (!value) {
                free(key);
                json_free(container);
                return NULL;
            }
            json_push(container, key, value);
            json_skip_ws(ps);
            if (*ps->p == ',') {
                ps->p++;
                continue;
            }
            if (*ps->p == close) {
                ps->p++;
                ps->depth--;
                return container;
            }
            json_free(container);
            return NULL;
        }
    }
    if (c == '"') {
        char* s = json_parse_string_raw(ps);
        if (!s) {
            return NULL;
        }
        Json* j = json_new(J_STR);
        j->str = s;
        return j;
    }
    if (c == 't' && !strncmp(ps->p, "true", 4)) {
        ps->p += 4;
        Json* j = json_new(J_BOOL);
        j->boolean = true;
        return j;
    }
    if (c == 'f' && !strncmp(ps->p, "false", 5)) {
        ps->p += 5;
        return json_new(J_BOOL);
    }
    if (c == 'n' && !strncmp(ps->p, "null", 4)) {
        ps->p += 4;
        return json_new(J_NULL);
    }
    if (c == '-' || (c >= '0' && c <= '9')) {
        char* end;
        double v = strtod(ps->p, &end);
        if (end == ps->p) {
            return NULL;
        }
        ps->p = end;
        Json* j = json_new(J_NUM);
        j->num = v;
        return j;
    }
    return NULL;
}

static Json* json_parse(char* text) {
    JParser ps = { text };
    Json* j = json_parse_value(&ps);
    if (!j) {
        return NULL;
    }
    json_skip_ws(&ps);
    if (*ps.p != '\0') {
        json_free(j);
        return NULL;
    }
    return j;
}

static void json_free(Json* j) {
    if (!j) {
        return;
    }
    free(j->str);
    for (int i = 0; i < j->count; i++) {
        json_free(j->items[i]);
        if (j->keys) {
            free(j->keys[i]);
        }
    }
    free(j->items);
    free(j->keys);
    free(j);
}

static Json* json_obj_get(Json* j, const char* key) {
    if (!j || j->type != J_OBJ) {
        return NULL;
    }
    for (int i = 0; i < j->count; i++) {
        if (!strcmp(j->keys[i], key)) {
            return j->items[i];
        }
    }
    return NULL;
}

static Json* json_arr_get(Json* j, int idx) {
    if (!j || j->type != J_ARR || idx < 0 || idx >= j->count) {
        return NULL;
    }
    return j->items[idx];
}

static const char* json_str(Json* j) {
    return (j && j->type == J_STR) ? j->str : NULL;
}

// ===== 送信側の文字列バッファと JSON 生成 =====

typedef struct {
    char* data;
    long len;
    long cap;
} Buf;

static void buf_addn(Buf* b, const char* s, long n) {
    if (b->len + n + 1 > b->cap) {
        long cap = b->cap ? b->cap : 256;
        while (b->len + n + 1 > cap) {
            cap *= 2;
        }
        b->cap = cap;
        b->data = realloc(b->data, cap);
    }
    memcpy(b->data + b->len, s, n);
    b->len += n;
    b->data[b->len] = '\0';
}

static void buf_adds(Buf* b, const char* s) {
    buf_addn(b, s, (long)strlen(s));
}

static void buf_addf(Buf* b, const char* fmt, ...) {
    char tmp[1024];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(tmp, sizeof(tmp), fmt, ap);
    va_end(ap);
    buf_adds(b, tmp);
}

// 文字列を JSON の文字列リテラルとして（引用符つきで）追加する
static void buf_add_json_string(Buf* b, const char* s) {
    buf_adds(b, "\"");
    for (const unsigned char* p = (const unsigned char*)s; *p; p++) {
        unsigned char c = *p;
        switch (c) {
            case '"': buf_adds(b, "\\\""); break;
            case '\\': buf_adds(b, "\\\\"); break;
            case '\n': buf_adds(b, "\\n"); break;
            case '\r': buf_adds(b, "\\r"); break;
            case '\t': buf_adds(b, "\\t"); break;
            case '\b': buf_adds(b, "\\b"); break;
            case '\f': buf_adds(b, "\\f"); break;
            default:
                if (c < 0x20) {
                    buf_addf(b, "\\u%04x", c);
                } else {
                    buf_addn(b, (const char*)p, 1);
                }
        }
    }
    buf_adds(b, "\"");
}

// リクエストの id をそのまま返すために JSON として書き戻す
static void buf_add_id(Buf* b, Json* id) {
    if (!id) {
        buf_adds(b, "null");
    } else if (id->type == J_NUM) {
        if (id->num == (double)(long)id->num) {
            buf_addf(b, "%ld", (long)id->num);
        } else {
            buf_addf(b, "%g", id->num);
        }
    } else if (id->type == J_STR) {
        buf_add_json_string(b, id->str);
    } else if (id->type == J_BOOL) {
        buf_adds(b, id->boolean ? "true" : "false");
    } else {
        buf_adds(b, "null");
    }
}

// ===== トランスポート（Content-Length フレーミング） =====

typedef struct {
    char buf[8192];
    long len;
    long pos;
} Reader;

static int reader_getc(Reader* r) {
    if (r->pos >= r->len) {
        ssize_t n = read(STDIN_FILENO, r->buf, sizeof(r->buf));
        if (n <= 0) {
            return -1;
        }
        r->len = n;
        r->pos = 0;
    }
    return (unsigned char)r->buf[r->pos++];
}

static bool reader_read_exact(Reader* r, char* out, long n) {
    long got = 0;
    while (got < n) {
        if (r->pos < r->len) {
            long take = r->len - r->pos;
            if (take > n - got) {
                take = n - got;
            }
            memcpy(out + got, r->buf + r->pos, take);
            r->pos += take;
            got += take;
        } else {
            ssize_t m = read(STDIN_FILENO, out + got, n - got);
            if (m <= 0) {
                return false;
            }
            got += m;
        }
    }
    return true;
}

// 1メッセージ分の本体を読み、malloc した文字列で返す。EOF では NULL
static char* read_message(Reader* r) {
    long content_length = -1;
    // ヘッダを空行まで読む
    while (true) {
        char line[1024];
        long n = 0;
        while (true) {
            int c = reader_getc(r);
            if (c < 0) {
                return NULL;
            }
            if (c == '\n') {
                break;
            }
            if (n < (long)sizeof(line) - 1) {
                line[n++] = (char)c;
            }
        }
        if (n > 0 && line[n - 1] == '\r') {
            n--;
        }
        line[n] = '\0';
        if (n == 0) {
            break;
        }
        if (!strncasecmp(line, "Content-Length:", 15)) {
            content_length = strtol(line + 15, NULL, 10);
        }
    }
    // 異常な長さは受け付けない（オーバーフローと巨大な確保の防止）
    if (content_length < 0 || content_length > 64L * 1024 * 1024) {
        return NULL;
    }
    char* body = malloc(content_length + 1);
    if (!body) {
        return NULL;
    }
    if (!reader_read_exact(r, body, content_length)) {
        free(body);
        return NULL;
    }
    body[content_length] = '\0';
    return body;
}

static void write_all(const char* data, long n) {
    long sent = 0;
    while (sent < n) {
        ssize_t m = write(STDOUT_FILENO, data + sent, n - sent);
        if (m <= 0) {
            _exit(1);
        }
        sent += m;
    }
}

static void send_message(const char* body) {
    char header[64];
    int hn = snprintf(header, sizeof(header), "Content-Length: %ld\r\n\r\n", (long)strlen(body));
    write_all(header, hn);
    write_all(body, (long)strlen(body));
}

// ===== 開いているドキュメントの保持 =====

typedef struct {
    char* uri;
    char* text;
    long version;
} Doc;

static Doc* docs;
static int doc_count;
static int doc_cap;

static Doc* find_doc(const char* uri) {
    for (int i = 0; i < doc_count; i++) {
        if (!strcmp(docs[i].uri, uri)) {
            return &docs[i];
        }
    }
    return NULL;
}

static Doc* open_doc(const char* uri, const char* text, long version) {
    Doc* doc = find_doc(uri);
    if (!doc) {
        if (doc_count == doc_cap) {
            doc_cap = doc_cap ? doc_cap * 2 : 8;
            docs = realloc(docs, sizeof(Doc) * doc_cap);
        }
        doc = &docs[doc_count++];
        doc->uri = strdup(uri);
        doc->text = NULL;
    }
    free(doc->text);
    doc->text = strdup(text);
    doc->version = version;
    return doc;
}

static void close_doc(const char* uri) {
    for (int i = 0; i < doc_count; i++) {
        if (!strcmp(docs[i].uri, uri)) {
            free(docs[i].uri);
            free(docs[i].text);
            docs[i] = docs[--doc_count];
            return;
        }
    }
}

// ===== チェック（fork した子プロセスでコンパイラを回す） =====

typedef struct {
    int has_diag;
    long start_line;
    long start_char;
    long end_line;
    long end_char;
    char message[4096];
} CheckResult;

// バイトオフセットを行・列（0始まり、列は文字＝コードポイント単位）へ
static void offset_to_pos(const char* text, long off, long* line, long* ch) {
    long l = 0;
    long c = 0;
    for (long i = 0; i < off && text[i]; i++) {
        if (text[i] == '\n') {
            l++;
            c = 0;
        } else if (((unsigned char)text[i] & 0xC0) != 0x80) {
            c++;
        }
    }
    *line = l;
    *ch = c;
}

static long utf8_len_at(const char* text, long off) {
    unsigned char c = (unsigned char)text[off];
    if (c < 0x80) return 1;
    if ((c & 0xE0) == 0xC0) return 2;
    if ((c & 0xF0) == 0xE0) return 3;
    if ((c & 0xF8) == 0xF0) return 4;
    return 1;
}

// 子プロセス側: パイプラインを回し、結果をパイプへ書いて _exit する
static void run_check_child(const char* text, int out_fd) {
    int devnull = open("/dev/null", O_WRONLY);
    if (devnull >= 0) {
        dup2(devnull, STDOUT_FILENO);
        dup2(devnull, STDERR_FILENO);
    }
    CheckResult res;
    memset(&res, 0, sizeof(res));
    char* copy = strdup(text);
    if (setjmp(*error_capture_begin()) == 0) {
        char prelude_path[4096];
        spell_prelude_path(prelude_path, sizeof(prelude_path));
        Function* prog = load_program(prelude_path);
        Function* user_prog = load_program_text(copy, (char*)"spell-lsp");
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
        error_capture_end();
    } else {
        res.has_diag = 1;
        snprintf(res.message, sizeof(res.message), "%s", error_captured_message);
        // 位置が利用者のソースを指している場合だけ範囲を変換する。
        // prelude 由来のエラーは先頭に出す
        if (error_captured_has_pos && error_captured_src == copy) {
            long so = error_captured_start;
            long eo = error_captured_end;
            long text_len = (long)strlen(copy);
            if (so < 0) {
                so = 0;
            }
            if (so > text_len) {
                so = text_len;
            }
            if (eo <= so) {
                eo = so + (so < text_len ? utf8_len_at(copy, so) : 0);
            }
            if (eo > text_len) {
                eo = text_len;
            }
            offset_to_pos(copy, so, &res.start_line, &res.start_char);
            offset_to_pos(copy, eo, &res.end_line, &res.end_char);
        }
    }
    const char* p = (const char*)&res;
    long left = (long)sizeof(res);
    while (left > 0) {
        ssize_t n = write(out_fd, p, left);
        if (n <= 0) {
            break;
        }
        p += n;
        left -= n;
    }
    _exit(0);
}

static CheckResult run_check(const char* text) {
    CheckResult res;
    memset(&res, 0, sizeof(res));
    int fds[2];
    if (pipe(fds) < 0) {
        res.has_diag = 1;
        snprintf(res.message, sizeof(res.message), "言語サーバーの内部でパイプを作成できませんでした");
        return res;
    }
    pid_t pid = fork();
    if (pid < 0) {
        close(fds[0]);
        close(fds[1]);
        res.has_diag = 1;
        snprintf(res.message, sizeof(res.message), "言語サーバーの内部でプロセスを作成できませんでした");
        return res;
    }
    if (pid == 0) {
        close(fds[0]);
        run_check_child(text, fds[1]);
    }
    close(fds[1]);
    char* p = (char*)&res;
    long got = 0;
    while (got < (long)sizeof(res)) {
        ssize_t n = read(fds[0], p + got, (long)sizeof(res) - got);
        if (n <= 0) {
            break;
        }
        got += n;
    }
    close(fds[0]);
    int status;
    while (waitpid(pid, &status, 0) < 0 && errno == EINTR) {
        // シグナルで中断されたら待ち直す
    }
    if (got < (long)sizeof(res)) {
        // 子が結果を返す前に落ちた（コンパイラのクラッシュ等）
        memset(&res, 0, sizeof(res));
        res.has_diag = 1;
        snprintf(res.message, sizeof(res.message), "コンパイラが異常終了しました");
    }
    return res;
}

// ===== 診断の送信 =====

static void publish_diagnostics(const Doc* doc) {
    CheckResult r = run_check(doc->text);
    Buf b = {0};
    buf_adds(&b, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":");
    buf_add_json_string(&b, doc->uri);
    buf_addf(&b, ",\"version\":%ld,\"diagnostics\":[", doc->version);
    if (r.has_diag) {
        buf_addf(&b, "{\"range\":{\"start\":{\"line\":%ld,\"character\":%ld},\"end\":{\"line\":%ld,\"character\":%ld}},\"severity\":1,\"source\":\"spell\",\"message\":",
                 r.start_line, r.start_char, r.end_line, r.end_char);
        buf_add_json_string(&b, r.message);
        buf_adds(&b, "}");
    }
    buf_adds(&b, "]}}");
    send_message(b.data);
    free(b.data);
}

static void publish_empty_diagnostics(const char* uri) {
    Buf b = {0};
    buf_adds(&b, "{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":");
    buf_add_json_string(&b, uri);
    buf_adds(&b, ",\"diagnostics\":[]}}");
    send_message(b.data);
    free(b.data);
}

static void respond_result(Json* id, const char* result_json) {
    Buf b = {0};
    buf_adds(&b, "{\"jsonrpc\":\"2.0\",\"id\":");
    buf_add_id(&b, id);
    buf_adds(&b, ",\"result\":");
    buf_adds(&b, result_json);
    buf_adds(&b, "}");
    send_message(b.data);
    free(b.data);
}

static void respond_error(Json* id, long code, const char* message) {
    Buf b = {0};
    buf_adds(&b, "{\"jsonrpc\":\"2.0\",\"id\":");
    buf_add_id(&b, id);
    buf_addf(&b, ",\"error\":{\"code\":%ld,\"message\":", code);
    buf_add_json_string(&b, message);
    buf_adds(&b, "}}");
    send_message(b.data);
    free(b.data);
}

// ===== メインループ =====

int main(void) {
    Reader reader = {0};
    bool shutdown = false;
    char* body;
    while ((body = read_message(&reader)) != NULL) {
        Json* msg = json_parse(body);
        free(body);
        if (!msg) {
            continue;
        }
        Json* id = json_obj_get(msg, "id");
        const char* method = json_str(json_obj_get(msg, "method"));
        if (!method) {
            json_free(msg);
            continue;
        }
        if (!strcmp(method, "initialize")) {
            respond_result(id, "{\"capabilities\":{\"textDocumentSync\":1},"
                               "\"serverInfo\":{\"name\":\"spell-lsp\",\"version\":\"0.1.0\"}}");
        } else if (!strcmp(method, "shutdown")) {
            shutdown = true;
            respond_result(id, "null");
        } else if (!strcmp(method, "exit")) {
            json_free(msg);
            return shutdown ? 0 : 1;
        } else if (!strcmp(method, "textDocument/didOpen")) {
            Json* params = json_obj_get(msg, "params");
            Json* td = json_obj_get(params, "textDocument");
            const char* uri = json_str(json_obj_get(td, "uri"));
            const char* text = json_str(json_obj_get(td, "text"));
            Json* version = json_obj_get(td, "version");
            if (uri && text) {
                Doc* doc = open_doc(uri, text, version ? (long)version->num : 0);
                publish_diagnostics(doc);
            }
        } else if (!strcmp(method, "textDocument/didChange")) {
            Json* params = json_obj_get(msg, "params");
            Json* td = json_obj_get(params, "textDocument");
            const char* uri = json_str(json_obj_get(td, "uri"));
            Json* version = json_obj_get(td, "version");
            Json* changes = json_obj_get(params, "contentChanges");
            Json* first = json_arr_get(changes, 0);
            const char* text = json_str(json_obj_get(first, "text"));
            if (uri && text) {
                Doc* doc = open_doc(uri, text, version ? (long)version->num : 0);
                publish_diagnostics(doc);
            }
        } else if (!strcmp(method, "textDocument/didClose")) {
            Json* params = json_obj_get(msg, "params");
            Json* td = json_obj_get(params, "textDocument");
            const char* uri = json_str(json_obj_get(td, "uri"));
            if (uri) {
                publish_empty_diagnostics(uri);
                close_doc(uri);
            }
        } else if (id) {
            respond_error(id, -32601, "対応していないメソッドです");
        }
        json_free(msg);
    }
    return 0;
}
