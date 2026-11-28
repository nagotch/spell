#include "spell.h"
#include "parse_internal.h"

static Node* array_literal();
static bool ctor_typeargs_ahead(Token* ident);
static bool typearg_ctor_ahead(Token* ident);
static Token* scan_type(Token* t);
static void expect_type_end();

// 「(引数, ...)」の中身を読む。開き括弧は消費済み。引数なしなら NULL
static bool suppress_multi = false;

// for の頭の式（反復対象・回数）と if / while の条件を読んでいる
// 間は真。その位置では式の直後の `:` が suite の区切りと競合する
// ため、型注釈の `:` は「型 = 」と続く場合だけ注釈として扱う
// （annotation_ahead）
bool suppress_anno = false;

int expr_depth = 0;

// 深さ0で、次のトークンが前のトークンより後の行にあれば、式はそこで
// 終わり（文の終わりは改行でもよい: step 55）。演算子の後など、式が
// 未完の位置ではこの判定を使わないため、被演算子は次の行に置ける
static bool at_expr_line_break() {
    return expr_depth == 0 && token->prev && token->line > token->prev->line;
}

static Node* parse_args() {
    expr_depth++;
    Node* result;
    if (consume(")")) {
        result = NULL;
    } else {
        Node head;
        head.next = NULL;
        Node* cur = &head;
        bool saved = suppress_multi;
        suppress_multi = true;
        do {
            cur->next = expr();
            cur = cur->next;
        } while (consume(","));
        suppress_multi = saved;
        expect(")");
        result = head.next;
    }
    expr_depth--;
    return result;
}

// `>>` 文の代入対象を1つ読む。対象の解析中はカンマ区切りの複数代入
// （a, b = ...）と誤認しないよう suppress_multi を立てる。対象の形は
// 単独形の `>>` と同じ（識別子・添字・メンバー）で、束縛まで行う
Node* parse_input_target() {
    bool saved = suppress_multi;
    suppress_multi = true;
    Node* target = bind_target_node(expr());
    suppress_multi = saved;
    return target;
}

// 入力文の対象が「個数形」（未束縛の識別子＋`[式]`）かどうかを、現在の
// トークン位置から判定する（消費はしない）。基底名が束縛済みなら従来の
// 添字形（既存配列の1要素への読み取り）になる（step 72 の振り分け規則）
bool input_target_is_count_form() {
    if (token->kind != TK_IDENT) {
        return false;
    }
    Token* next = token->next;
    if (!next || next->kind != TK_SYMBOL || next->len != 1 || next->str[0] != '[') {
        return false;
    }
    return !ident_is_bound(token);
}

// 長さ・個数として書かれた式が、リテラルだけからなる定数形か
// どうかを判定し、定数形なら値を返す（step 85 の追加決定:
// 負の長さのリテラルはコンパイルエラーにするための判定）。
// 整数リテラルと、リテラル同士の + - *（単項マイナスは 0 - x の
// 形なので二項に含まれる）を畳み込む。畳み込みは実行時の整数
// 演算と同じラップ（符号なし演算経由）で行うため、定数形の値は
// 実行時に得られる値と一致する。それ以外（変数・呼び出しなど）
// は定数形ではない
bool const_int_value(Node* node, long* out) {
    if (!node) {
        return false;
    }
    if (node->kind == ND_NUM) {
        *out = node->val;
        return true;
    }
    if (node->kind == ND_BINOP) {
        long l;
        long r;
        if (!const_int_value(node->left_child, &l) || !const_int_value(node->right_child, &r)) {
            return false;
        }
        switch (node->op.binop) {
            case OP_ADD: *out = (long)((unsigned long)l + (unsigned long)r); return true;
            case OP_SUB: *out = (long)((unsigned long)l - (unsigned long)r); return true;
            case OP_MUL: *out = (long)((unsigned long)l * (unsigned long)r); return true;
            default: return false;
        }
    }
    return false;
}

// 個数形の対象 `名前[個数式][個数式]...` を読み、基底名トークンと、
// 個数式の並び（next で連結）を返す（step 75: n 次元への一般化。
// 角括弧の並びは全て個数として読む）。角括弧の継続は、空白区切りの
// 対象継続と同じく同じ行の中だけとする（次の行の先頭の `[` を飲み
// 込まない）。呼び出し前に input_target_is_count_form() で判定する
// こと。個数式の解析条件は parse_input_target と同じ
// （suppress_multi を立てる）
Node* parse_input_count(Token** name_out) {
    *name_out = consume_ident();
    Node head;
    head.next = NULL;
    Node* cur = &head;
    do {
        consume("[");
        bool saved = suppress_multi;
        suppress_multi = true;
        expr_depth++;
        Token* count_tok = token;
        cur->next = expr();
        expr_depth--;
        suppress_multi = saved;
        // 個数がリテラルの負数ならコンパイルエラー（step 85 の
        // 追加決定）。実行時に決まる負数は展開先で 0 に丸められる
        long cv;
        if (const_int_value(cur->next, &cv) && cv < 0) {
            error_at(token_start(count_tok), token_end(count_tok), "個数に負の数は指定できません");
        }
        cur = cur->next;
        expect("]");
    } while (token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == '['
             && token->prev && token->line == token->prev->line);
    return head.next;
}

// put 文の引数列（括弧なし、カンマ区切り）を読む
Node* parse_put_args() {
    Node head;
    head.next = NULL;
    Node* cur = &head;
    bool saved = suppress_multi;
    suppress_multi = true;
    do {
        cur->next = expr();
        cur = cur->next;
    } while (consume(","));
    suppress_multi = saved;
    return head.next;
}

// 真理値リテラル（step 70）: 式の位置で `true` / `T` を真、`false` / `F`
// を偽として扱う。該当すれば 1 / 0、しなければ -1 を返す。型の位置
// （型パラメータ `T` など）やメンバ名は primary を通らないので、
// この変換の影響を受けない
static int bool_literal_value(Token* tok) {
    if (tok->len == 4 && !memcmp(tok->str, "true", 4)) {
        return 1;
    }
    if (tok->len == 5 && !memcmp(tok->str, "false", 5)) {
        return 0;
    }
    if (tok->len == 1 && tok->str[0] == 'T') {
        return 1;
    }
    if (tok->len == 1 && tok->str[0] == 'F') {
        return 0;
    }
    return -1;
}

// 括弧の中（深さ1）に単独の `=` があるか。`(a, b = 1, 2)` の
// 括弧つき複数代入は、内側の式として読む従来の形を保つ必要がある
// ため、Pos リテラルとして読む前にトークン列だけで振り分ける。
// 括弧・角括弧・波括弧の入れ子の中の `=` は数えない（ラムダ本体
// などの代入を、並びの代入と誤認しないため）
static bool paren_has_top_assign() {
    int depth = 0;
    for (Token* t = token; t && t->kind != TK_EOF; t = t->next) {
        if (t->kind != TK_SYMBOL) {
            continue;
        }
        if (t->len == 1 && (t->str[0] == '(' || t->str[0] == '[' || t->str[0] == '{')) {
            depth++;
        } else if (t->len == 1 && (t->str[0] == ')' || t->str[0] == ']' || t->str[0] == '}')) {
            if (depth == 0) {
                return false;
            }
            depth--;
        } else if (depth == 0 && t->len == 1 && t->str[0] == '=') {
            return true;
        }
    }
    return false;
}

// 括弧で囲まれた式を読む。開き括弧は消費済み。
// 中身がカンマ区切りの並びなら Pos リテラル（step 77）として読み、
// 要素2つで Pos2、3つで Pos3 の構築に読み替える。要素が1つだけ
// なら従来どおりの括弧式。`(x, y) -> 式` のラムダは assign 側で
// 先に振り分けられるため、ここへは来ない
static Node* paren_expr(Token* open) {
    if (paren_has_top_assign()) {
        expr_depth++;
        Node* node = expr();
        expr_depth--;
        expect(")");
        return node;
    }
    expr_depth++;
    bool saved = suppress_multi;
    suppress_multi = true;
    Node* first = expr();
    if (!consume(",")) {
        suppress_multi = saved;
        expr_depth--;
        expect(")");
        return first;
    }
    Node head;
    head.next = NULL;
    Node* cur = &head;
    cur->next = first;
    cur = cur->next;
    int count = 1;
    do {
        cur->next = expr();
        cur = cur->next;
        count++;
    } while (consume(","));
    suppress_multi = saved;
    expr_depth--;
    expect(")");
    if (count > 3) {
        error_at(token_start(open), token_end(open), "Pos リテラルの要素は2個か3個ですが、%d個が指定されています", count);
    }
    Node* node = create_node(ND_FUNCALL);
    node->name = count == 2 ? "Pos2" : "Pos3";
    node->len = 4;
    node->tok = open;
    node->args = head.next;
    return node;
}

// デコード済みの文字列データから、通常の文字列リテラルと全く同じ
// ノード（文字コード配列を `str(...)` で包んだ形）を作る。char_array
// が現在のトークンを読むため、偽の TK_STR トークンを一時的に差し
// 込む（parse.c の make_input_fill と同じ方式）
static Node* make_str_literal(char* decoded, int len, Token* tok) {
    Token* fake = calloc(1, sizeof(Token));
    fake->kind = TK_STR;
    fake->str = decoded;
    fake->len = len;
    fake->line = tok->line;
    fake->next = token;
    token = fake;
    Node* chars = char_array();
    chars->tok = tok;
    Node* node = create_node(ND_FUNCALL);
    node->name = "str";
    node->len = 3;
    node->tok = tok;
    node->args = chars;
    return node;
}

// 式部の中の位置 p がリテラルの開始かどうか。引用符そのものか、
// 識別子の境界にある `f` + 引用符（入れ子の f-string）なら開始。
// start は式部の先頭で、そこでは直前が `{` なので常に境界になる。
// 識別子の途中の `f`（例: `xf"..."` の `f`）は開始ではない
static bool fstr_at_literal(char* p, char* start) {
    if (*p == '"' || *p == '\'') {
        return true;
    }
    if (*p == 'f' && (p[1] == '"' || p[1] == '\'')) {
        if (p == start) {
            return true;
        }
        char c = p[-1];
        return !(('a' <= c && c <= 'z') || ('A' <= c && c <= 'Z') || c == '_' || ('0' <= c && c <= '9'));
    }
    return false;
}

// f-string の式部 [es, q) を、その場でトークン化して式として読む。
// q は対応する `}` の位置で、一時的に '\0' を置くことで部分文字列
// だけを tokenize できる。トークンは実ソースを指すため、式の中の
// エラー位置は正確になる。この間は文単位のエラー回復を切り、式部
// のエラーを字句解析エラーと同じ「チェック全体の打ち切り」にす
// る（longjmp で復元処理が飛ばされても、検査自体が終わるため
// 大域状態の不整合が残らない）
static Node* parse_fstr_expr(char* es, char* q, Token* ftok) {
    // コメントは式部の終端判定（文字列の読み飛ばし）と相性が悪く、
    // 部分トークン化では行末まで読み飛ばされて式が黙って欠ける
    // ため、文字列の外にあれば明示的にエラーにする
    for (char* p = es; p < q;) {
        if (fstr_at_literal(p, es)) {
            p = fstr_skip_literal(p);
            continue;
        }
        if (p[0] == '/' && p + 1 < q && (p[1] == '/' || p[1] == '*')) {
            error_at(p - src, p - src + 2, "f-string の式の中ではコメントは書けません");
        }
        p++;
    }
    char saved = *q;
    *q = '\0';
    Token* saved_token = token;
    jmp_buf* saved_recover = error_capture_set_recover(NULL);
    bool saved_anno = suppress_anno;
    bool saved_multi = suppress_multi;
    suppress_anno = true;
    suppress_multi = true;
    token = tokenize(es);
    Node* e = expr();
    suppress_anno = saved_anno;
    suppress_multi = saved_multi;
    error_capture_set_recover(saved_recover);
    Token* after = token;
    token = saved_token;
    *q = saved;
    if (after->kind == TK_SYMBOL && after->len == 1 && after->str[0] == ':') {
        // 式として読めた後に残る深さ0の `:` は書式指定（スライスや
        // 型注釈の `:` は式の内側で消費される）
        error_at(token_start(after), token_end(after), "f-string の書式指定（`:`以降）は未対応です");
    }
    if (after->kind != TK_EOF) {
        error_at(token_start(after), token_end(after), "f-string の式の後に %s は書けません", token_text(after));
    }
    if (e->kind == ND_ASSIGN && e->anno) {
        error_at(token_start(e->tok), token_end(e->tok), "f-string の式に型注釈つき代入は書けません");
    }
    (void)ftok;
    return e;
}

// f-string リテラル（step 93）。現在のトークンが TK_FSTR の状態で
// 呼ぶ。生の内側テキストをリテラル部と式部に分割し、リテラル部は
// 通常の文字列リテラルと同じノードに、全体は ND_FSTR（args が
// 部品の並び）にする。補間が1つも無ければ、通常の文字列リテラル
// のノードをそのまま返す
static Node* parse_fstring(Token* ftok) {
    char* raw = ftok->str;
    char quote = raw[1];
    char* inner = raw + 2;
    char* inner_end = raw + ftok->len - 1;
    token = token->next;
    Node* head = NULL;
    Node** tail = &head;
    bool has_expr = false;
    char* p = inner;
    char* lit = inner;
    while (p < inner_end) {
        if (p[0] == '{' && p + 1 < inner_end && p[1] == '{') {
            p += 2;
            continue;
        }
        if (p[0] == '}' && p + 1 < inner_end && p[1] == '}') {
            p += 2;
            continue;
        }
        if (p[0] == '\\') {
            p += 2;
            continue;
        }
        if (p[0] == '{') {
            if (p > lit) {
                int dlen;
                char* decoded = decode_fstr_literal(lit, p, &dlen, raw, quote);
                *tail = make_str_literal(decoded, dlen, ftok);
                tail = &(*tail)->next;
            }
            char* es = p + 1;
            char* q = es;
            while (true) {
                if (fstr_at_literal(q, es)) {
                    q = fstr_skip_literal(q);
                    continue;
                }
                if (*q == '}') {
                    break;
                }
                q++;
            }
            *tail = parse_fstr_expr(es, q, ftok);
            tail = &(*tail)->next;
            has_expr = true;
            p = q + 1;
            lit = p;
            continue;
        }
        p++;
    }
    if (inner_end > lit) {
        int dlen;
        char* decoded = decode_fstr_literal(lit, inner_end, &dlen, raw, quote);
        *tail = make_str_literal(decoded, dlen, ftok);
        tail = &(*tail)->next;
    }
    if (!has_expr) {
        if (!head) {
            // f"" : 空の文字列リテラル
            return make_str_literal("", 0, ftok);
        }
        return head;
    }
    Node* node = create_node(ND_FSTR);
    node->tok = ftok;
    node->args = head;
    return node;
}

static Node* primary() {
    Node* node;
    if (token->kind == TK_KEYWORD && token->keyword == KW_FN) {
        return parse_lambda(false, token);
    }
    if (token->kind == TK_KEYWORD && (token->keyword == KW_IN || token->keyword == KW_IN_STR || token->keyword == KW_IN_LINE)) {
        // 入力構文: in / in[n] / in[h][w]、in_str / in_str[n]、in_line
        Token* kw = token;
        int mode = token->keyword == KW_IN ? 0 : token->keyword == KW_IN_STR ? 1 : 2;
        token = token->next;
        node = create_node(ND_INPUT);
        node->tok = kw;
        node->val = mode;
        Node* args = NULL;
        Node** tail = &args;
        int nb = 0;
        while (consume("[")) {
            expr_depth++;
            Node* elem = expr();
            expr_depth--;
            expect("]");
            *tail = elem;
            tail = &elem->next;
            nb++;
        }
        if (mode == 2 && nb > 0) {
            error_at(token_start(kw), token_end(kw), "`in_line`に添え字は付けられません");
        }
        if ((mode == 0 && nb > 2) || (mode == 1 && nb > 1)) {
            error_at(token_start(kw), token_end(kw), "`%.*s`の添え字は%d個までです", kw->len, kw->str, mode == 0 ? 2 : 1);
        }
        node->args = args;
        return node;
    }
    Token* ident_token = consume_ident();
    if (token->kind == TK_FSTR) {
        node = parse_fstring(token);
    } else if (token->kind == TK_STR) {
        Token* str_tok = token;
        Node* arr = char_array();
        node = create_node(ND_FUNCALL);
        node->name = "str";
        node->len = 3;
        node->tok = str_tok;
        node->args = arr;
    } else if (!ident_token && token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == '[') {
        Token* bracket = token;
        token = token->next;
        node = array_literal();
        node->tok = bracket;
    } else if (ident_token && bool_literal_value(ident_token) >= 0) {
        // 真理値リテラル（step 70）。変数の解決より優先する: この4つの
        // 綴りは式の位置では変数名として使えない
        node = create_node(ND_BOOL);
        node->val = bool_literal_value(ident_token);
        node->name = ident_token->str;
        node->len = ident_token->len;
        node->tok = ident_token;
    } else if (ident_token && token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == '(' && ident_is_value_call(ident_token)) {
        // 変数に収まった関数値の呼び出しは、名前呼び出しではなく値経由にする。
        // `(` は後置ループ側で ND_VCALL として消費する
        node = create_ident_node(ident_token);
    } else if (ident_token && ctor_typeargs_ahead(ident_token)) {
        // 構築時の明示的な型引数（step 76）: `名前<型, ...>(引数...)`。
        // 型引数は構築呼び出し専用で、呼び出しなしの形は作らない。
        // 型引数の読み取りは型注釈と同じ parse_type を使うため、入れ子
        // （Vec<Vec<int>>）の `>>` 分割もそちらに任せられる
        node = create_node(ND_FUNCALL);
        node->name = ident_token->str;
        node->len = ident_token->len;
        node->tok = ident_token;
        consume("<");
        do {
            Type* ta = parse_type();
            if (node->ctor_targ_count < MAX_TPARAMS) {
                node->ctor_targs[node->ctor_targ_count] = ta;
            }
            node->ctor_targ_count++;
        } while (consume(","));
        expect_type_end();
        expect("(");
        node->args = parse_args();
    } else if (ident_token && typearg_ctor_ahead(ident_token)) {
        // 型を引数に渡す構築（step 77）: `Deque(int)` のように、引数が
        // 型1つだけなら「要素型の指定＋ゼロ値ファクトリの自動生成」
        // として扱う。要素型は構築時の型引数として記録し、ファクトリ
        // 引数そのものは typecheck が合成する（ctor_type_arg）
        node = create_node(ND_FUNCALL);
        node->name = ident_token->str;
        node->len = ident_token->len;
        node->tok = ident_token;
        consume("(");
        node->ctor_targs[0] = parse_type();
        node->ctor_targ_count = 1;
        node->ctor_type_arg = true;
        expect(")");
    } else if (ident_token && consume("(")) {
        node = create_node(ND_FUNCALL);
        node->name = ident_token->str;
        node->len = ident_token->len;
        node->tok = ident_token;
        node->args = parse_args();
    } else if (ident_token) {
        node = create_ident_node(ident_token);
    } else if (consume("(")) {
        node = paren_expr(token->prev);
    } else {
        Token* num_tok = token;
        long num = expect_number();
        node = create_number_node(num);
        node->tok = num_tok;
    }
    while (true) {
        Token* postfix_tok = token;
        if (at_expr_line_break()) {
            break;
        }
        if (consume("(")) {
            // 値経由の呼び出し: 関数値・ラムダ・括弧つき式などの直後の `(`
            Node* call = create_node(ND_VCALL);
            call->tok = postfix_tok;
            call->left_child = node;
            call->args = parse_args();
            node = call;
            continue;
        }
        bool inc = consume("++");
        if (inc || consume("--")) {
            Node* assign_node = create_node(ND_OPASSIGN);
            // 対象の動的な部分（オブジェクト・添え字）を1回だけ評価
            // するための一時スロットを2つ確保しておく（step 87。使うか
            // どうかは typecheck の check_assign が対象の形で決める）
            register_any();
            assign_node->offset = locals->offset;
            register_any();
            assign_node->op.binop = inc ? OP_ADD : OP_SUB;
            assign_node->tok = postfix_tok;
            assign_node->left_child = node;
            assign_node->right_child = create_number_node(1);
            node = assign_node;
            continue;
        }
        if (token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == '?'
                && token->next && token->next->kind == TK_SYMBOL && token->next->len == 1 && token->next->str[0] == '['
                && (expr_depth > 0 || token->next->line == token->line)) {
            // 安全添え字（step 81）: 対象の直後の `?` と、それに続く
            // 添え字の連鎖 `[i][j]...`。`?` の直後に `[` が続く場合
            // だけ式として読み、それ以外の `?` は従来どおり式を
            // 終端させる（文頭の if 糖衣や、条件の直後の `?` の
            // エラー判定は parse.c 側の担当）。改行が文終端になる
            // 文脈（入れ子の外）では、角括弧の継続は個数形の入力
            // 対象と同じく同じ行の中だけとする。括弧の中では通常の
            // 添え字と同じく行をまたげる
            Token* q_tok = token;
            token = token->next;
            Node* snode = create_node(ND_SAFEIDX);
            snode->tok = q_tok;
            snode->left_child = node;
            Node head;
            head.next = NULL;
            Node* cur = &head;
            int nb = 0;
            while (token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == '['
                    && (expr_depth > 0 || nb == 0 || token->line == token->prev->line)) {
                consume("[");
                expr_depth++;
                // 角括弧の中では `:` はスライスの印なので、型注釈の
                // 判定は先読み方式にする（条件式と同じ。step 85）
                bool saved_anno = suppress_anno;
                suppress_anno = true;
                cur->next = expr();
                suppress_anno = saved_anno;
                expr_depth--;
                cur = cur->next;
                if (token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == ':') {
                    // 安全添え字の中のスライスは対象外（step 85）
                    error_at(token_start(token), token_end(token), "安全添え字の中ではスライスは使えません");
                }
                expect("]");
                nb++;
            }
            snode->args = head.next;
            snode->array_len = nb;
            // 一時スロット: 先頭が対象の退避先、以降は段ごとの
            // （長さ・添え字・中間値）と Pos の退避先。角括弧1つは
            // 最大で Pos3 の3段に展開されるため、1つにつき10枠を
            // 確保する（使うかどうかは型検査が決める。Pos 添字の
            // 一時置き場と同じ方式）
            register_any();
            snode->offset = locals->offset;
            for (int i = 0; i < nb * 10; i++) {
                register_any();
            }
            node = snode;
            continue;
        }
        if (consume("[")) {
            expr_depth++;
            // スライス（step 85）: `[下限:上限]` の形。端は省略でき、
            // 省略した側は NULL で運ぶ。ステップ（2つ目の `:`）は無い。
            // 角括弧の中では `:` はスライスの印なので、境界の式を
            // 読む間は型注釈の判定を先読み方式にする（条件式と同じ）
            bool saved_anno = suppress_anno;
            suppress_anno = true;
            Node* lo = NULL;
            Node* hi = NULL;
            bool is_slice = false;
            if (token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == ':') {
                is_slice = true;
            } else {
                lo = expr();
                if (token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == ':') {
                    is_slice = true;
                }
            }
            if (is_slice) {
                consume(":");
                if (token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == ':') {
                    error_at(token_start(token), token_end(token), "スライスにステップは指定できません");
                }
                if (!(token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == ']')) {
                    hi = expr();
                    if (token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == ':') {
                        error_at(token_start(token), token_end(token), "スライスにステップは指定できません");
                    }
                }
            }
            suppress_anno = saved_anno;
            expr_depth--;
            expect("]");
            if (is_slice) {
                Node* snode = create_node(ND_SLICE);
                snode->tok = postfix_tok;
                snode->left_child = node;
                snode->right_child = lo;
                snode->args = hi;
                // 一時スロット: 対象・正規化前の下限・上限の退避先
                // の3枠（使うかどうかは型検査が決める。Pos 添字の
                // 一時置き場と同じ方式）
                register_any();
                snode->offset = locals->offset;
                register_any();
                register_any();
                node = snode;
                continue;
            }
            Node* new_node = create_node(ND_INDEX);
            new_node->tok = postfix_tok;
            new_node->left_child = node;
            new_node->right_child = lo;
            // Pos 添字の糖衣（step 77）で使う一時置き場。添字の型は
            // パース時点では分からないため、全ての添字式に1枠ずつ
            // 確保しておき、使うかどうかは型検査が決める（複数代入の
            // 一時置き場と同じ方式）
            register_any();
            new_node->offset = locals->offset;
            node = new_node;
            continue;
        }
        if (!consume(".")) {
            break;
        }
        Token* member_token = consume_ident();
        if (!member_token) {
            char* actual = token_text(token);
            if (actual) {
                error_at(token_start(token), token_end(token), "フィールド名が必要ですが、%sが指定されています", actual);
            } else {
                error_at(token_start(token), token_end(token), "フィールド名が必要ですが、ファイルの末尾に達しました");
            }
        }
        Node* new_node = create_node(ND_MEMBER);
        new_node->left_child = node;
        new_node->name = member_token->str;
        new_node->len = member_token->len;
        new_node->tok = member_token;
        node = new_node;
        if (!at_expr_line_break() && consume("(")) {
            new_node->kind = ND_MCALL;
            new_node->args = parse_args();
        }
    }
    return node;
}

static Node* unary() {
    Token* op_tok = token;
    if (consume("+")) {
        return primary();
    } else if (consume("-")) {
        Node* node = create_binary_operation_node(OP_SUB, create_number_node(0), primary());
        node->tok = op_tok;
        return node;
    } else if (consume("*")) {
        Node* node = create_node(ND_UNARY);
        node->op.unop = OP_DEREF;
        node->tok = op_tok;
        node->left_child = unary();
        return node;
    } else if (consume("&")) {
        Node* node = create_node(ND_UNARY);
        node->op.unop = OP_ADDR;
        node->tok = op_tok;
        node->left_child = unary();
        return node;
    } else if (consume("!") || consume_keyword(KW_NOT)) {
        // `not` は `!` の英字形（step 94）。意味も優先順位も `!` と
        // 完全に同じ（単項の前置で強く結ぶ。Python の `not` とは違う）
        Node* node = create_node(ND_UNARY);
        node->op.unop = OP_NOT;
        node->tok = op_tok;
        node->left_child = unary();
        return node;
    }
    return primary();
}

static Node* mul() {
    Node* node = unary();
    while (true) {
        Token* op_tok = token;
        if (at_expr_line_break()) {
            break;
        }
        if (consume("*")) {
            node = create_binary_operation_node(OP_MUL, node, unary());
            node->tok = op_tok;
        } else if (consume("/")) {
            node = create_binary_operation_node(OP_DIV, node, unary());
            node->tok = op_tok;
        } else if (consume("%")) {
            node = create_binary_operation_node(OP_MOD, node, unary());
            node->tok = op_tok;
        } else {
            break;
        }
    }
    return node;
}

static Node* add() {
    Node* node = mul();
    while (true) {
        Token* op_tok = token;
        if (at_expr_line_break()) {
            break;
        }
        if (consume("+")) {
            node = create_binary_operation_node(OP_ADD, node, mul());
            node->tok = op_tok;
        } else if (consume("-")) {
            node = create_binary_operation_node(OP_SUB, node, mul());
            node->tok = op_tok;
        } else {
            break;
        }
    }
    return node;
}

// 関係演算子を1つ読む。> / >= は左右を入れ替えて < / <= にそろえる
static bool read_rel_op(BinOp* op, bool* swapped) {
    if (at_expr_line_break()) {
        return false;
    }
    if (consume("<=")) {
        *op = OP_LE;
        *swapped = false;
    } else if (consume(">=")) {
        *op = OP_LE;
        *swapped = true;
    } else if (consume("<")) {
        *op = OP_LT;
        *swapped = false;
    } else if (consume(">")) {
        *op = OP_LT;
        *swapped = true;
    } else {
        return false;
    }
    return true;
}

// 比較が1つだけなら従来どおりの二項演算、2つ以上連なれば ND_CHAIN にする
static Node* make_cmp_chain(Node* first, Node* pairs, int npairs) {
    if (npairs == 1) {
        Node* single;
        if (pairs->val) {
            single = create_binary_operation_node(pairs->op.binop, pairs->left_child, first);
        } else {
            single = create_binary_operation_node(pairs->op.binop, first, pairs->left_child);
        }
        single->tok = pairs->tok;
        return single;
    }
    Node* node = create_node(ND_CHAIN);
    node->tok = pairs->tok;
    node->left_child = first;
    node->body = pairs;
    register_any();
    node->offset = locals->offset;
    register_any();
    return node;
}

static Node* relational() {
    Node* first = add();
    BinOp op;
    bool swapped;
    Token* op_tok = token;
    if (!read_rel_op(&op, &swapped)) {
        return first;
    }
    Node* pairs = NULL;
    Node** tail = &pairs;
    int npairs = 0;
    while (true) {
        Node* pair = create_node(ND_BINOP);
        pair->op.binop = op;
        pair->tok = op_tok;
        pair->left_child = add();
        pair->val = swapped ? 1 : 0;
        *tail = pair;
        tail = &pair->next;
        npairs++;
        op_tok = token;
        if (!read_rel_op(&op, &swapped)) {
            break;
        }
    }
    return make_cmp_chain(first, pairs, npairs);
}

static bool read_eq_op(BinOp* op) {
    if (at_expr_line_break()) {
        return false;
    }
    if (consume("==")) {
        *op = OP_EQ;
    } else if (consume("!=")) {
        *op = OP_NE;
    } else {
        return false;
    }
    return true;
}

static Node* equality() {
    Node* first = relational();
    BinOp op;
    Token* op_tok = token;
    if (!read_eq_op(&op)) {
        return first;
    }
    Node* pairs = NULL;
    Node** tail = &pairs;
    int npairs = 0;
    while (true) {
        Node* pair = create_node(ND_BINOP);
        pair->op.binop = op;
        pair->tok = op_tok;
        pair->left_child = relational();
        pair->val = 0;
        *tail = pair;
        tail = &pair->next;
        npairs++;
        op_tok = token;
        if (!read_eq_op(&op)) {
            break;
        }
    }
    return make_cmp_chain(first, pairs, npairs);
}

static Node* and() {
    Node* node = equality();
    Token* op_tok = token;
    // `and` は `&&` の英字形（step 94）。意味も優先順位も完全に同じ
    if (!at_expr_line_break() && (consume("&&") || consume_keyword(KW_AND))) {
        node = create_binary_operation_node(OP_AND, node, and());
        node->tok = op_tok;
    }
    return node;
}

static Node* or() {
    Node* node = and();
    Token* op_tok = token;
    // `or` は `||` の英字形（step 94）。意味も優先順位も完全に同じ
    if (!at_expr_line_break() && (consume("||") || consume_keyword(KW_OR))) {
        node = create_binary_operation_node(OP_OR, node, or());
        node->tok = op_tok;
    }
    return node;
}

static Node* range() {
    Node* node = or();
    Token* op_tok = token;
    int incl;
    if (at_expr_line_break()) {
        return node;
    }
    if (consume("..=")) {
        incl = 1;
    } else if (consume("..")) {
        incl = 0;
    } else {
        return node;
    }
    Node* rhs = or();
    Node* call = create_node(ND_FUNCALL);
    call->name = "Range";
    call->len = 5;
    call->tok = op_tok;
    node->next = rhs;
    rhs->next = create_number_node(incl);
    call->args = node;
    return call;
}

static bool sym_is(Token* t, const char* symbol) {
    int len = (int)strlen(symbol);
    return t && t->kind == TK_SYMBOL && t->len == len && !memcmp(symbol, t->str, t->len);
}

// 構築時の明示的な型引数 `名前<型, ...>(` が続くかの先読み（step 76）。
// トークン列だけを見て判定し、消費はしない。対象はクラス名だけで、
// 変数に隠れている名前は値として扱う（比較式 `a < b > (c)` の形を
// 壊さないため）。角括弧の対応だけを数え、`>>` / `>>>` は複数の
// 閉じ括弧として数える。中身が型の形かどうかは、確定後に parse_type
// が検証する
static bool ctor_typeargs_ahead(Token* ident) {
    if (!find_class(ident->str, ident->len)) {
        return false;
    }
    if (ident_is_value_call(ident)) {
        return false;
    }
    Token* t = token;
    if (!sym_is(t, "<")) {
        return false;
    }
    int depth = 1;
    for (t = t->next; t && t->kind != TK_EOF; t = t->next) {
        if (sym_is(t, "<")) {
            depth++;
        } else if (sym_is(t, ">")) {
            depth--;
        } else if (sym_is(t, ">>")) {
            depth -= 2;
        } else if (sym_is(t, ">>>")) {
            depth -= 3;
        } else {
            continue;
        }
        if (depth == 0) {
            return sym_is(t->next, "(");
        }
        if (depth < 0) {
            return false;
        }
    }
    return false;
}

// 型を引数に渡す構築 `Vec(型)` / `Deque(型)` が続くかの先読み
// （step 77）。トークン列だけを見て判定し、消費はしない。対象は
// prelude の Vec / Deque だけで、変数に隠れている名前は値として
// 扱う。引数がちょうど1つの型として読み飛ばせて、直後が `)` の
// 場合だけ確定する。ただし、その型が単独の識別子で、変数として
// 解決される場合は式として扱う（ファクトリを収めた変数を渡す形を
// 壊さないため。型と変数の優先順位は create_ident_node と同じく
// 変数が勝つ）
static bool typearg_ctor_ahead(Token* ident) {
    if (!find_class(ident->str, ident->len)) {
        return false;
    }
    bool is_vec = ident->len == 3 && !memcmp(ident->str, "Vec", 3);
    bool is_deque = ident->len == 5 && !memcmp(ident->str, "Deque", 5);
    if (!is_vec && !is_deque) {
        return false;
    }
    if (ident_is_value_call(ident)) {
        return false;
    }
    if (!sym_is(token, "(")) {
        return false;
    }
    Token* arg = token->next;
    Token* after = scan_type(arg);
    if (!after || !sym_is(after, ")")) {
        return false;
    }
    if (arg->kind == TK_IDENT && after == arg->next && ident_is_value_call(arg)) {
        return false;
    }
    return true;
}

// 型の名前としてあり得るか（注釈かどうかの見分け専用。意味の検証は
// 注釈として確定してから parse_type が行う）
static bool type_name_known(Token* t) {
    if (t->len == 3 && !memcmp(t->str, "int", 3)) {
        return true;
    }
    if (find_class(t->str, t->len)) {
        return true;
    }
    if (parsing_klass) {
        for (int i = 0; i < parsing_klass->tparam_count; i++) {
            if (parsing_klass->tparam_lens[i] == t->len && !memcmp(t->str, parsing_klass->tparams[i], t->len)) {
                return true;
            }
        }
    }
    return false;
}

// トークン列だけを見て型を1つ読み飛ばし、直後のトークンを返す。
// 型の形でなければ NULL。parse_type は破壊的（`>>` の書き換えや
// エラーでの終了）なので、試し読みには使えない。型引数の中身は
// 括弧の対応だけを見て読み飛ばす
static Token* scan_type(Token* t) {
    if (!t || t->kind == TK_EOF) {
        return NULL;
    }
    if (t->kind == TK_KEYWORD && t->keyword == KW_FN) {
        t = t->next;
        if (!sym_is(t, "(")) {
            return NULL;
        }
        t = t->next;
        if (!sym_is(t, ")")) {
            while (true) {
                t = scan_type(t);
                if (!t) {
                    return NULL;
                }
                if (sym_is(t, ",")) {
                    t = t->next;
                    continue;
                }
                break;
            }
        }
        if (!sym_is(t, ")")) {
            return NULL;
        }
        t = t->next;
        if (!sym_is(t, "->")) {
            return NULL;
        }
        return scan_type(t->next);
    }
    if (sym_is(t, "[")) {
        t = scan_type(t->next);
        if (!t || !sym_is(t, "]")) {
            return NULL;
        }
        return t->next;
    }
    if (t->kind != TK_IDENT || !type_name_known(t)) {
        return NULL;
    }
    t = t->next;
    if (sym_is(t, "<")) {
        int depth = 0;
        while (t && t->kind != TK_EOF) {
            if (sym_is(t, "<")) {
                depth++;
                t = t->next;
            } else if (sym_is(t, ">")) {
                depth--;
                t = t->next;
                if (depth == 0) {
                    return t;
                }
            } else if (sym_is(t, ">>")) {
                depth -= 2;
                t = t->next;
                if (depth == 0) {
                    return t;
                }
                if (depth < 0) {
                    return NULL;
                }
            } else if (sym_is(t, ">>>")) {
                depth -= 3;
                t = t->next;
                if (depth == 0) {
                    return t;
                }
                if (depth < 0) {
                    return NULL;
                }
            } else {
                t = t->next;
            }
        }
        return NULL;
    }
    return t;
}

// 現在のトークンの `:` が型注釈のものか。suite の `:` と競合する
// 位置（suppress_anno）でのみ使う。`:` の後ろに「型 = 」が続く
// 場合だけ注釈とみなす。型の名前が実在することも条件にするため、
// suite の最初の文が `名前 = ...` という代入でも、その名前が
// 型でなければ注釈と誤認しない
static bool annotation_ahead() {
    if (!sym_is(token, ":")) {
        return false;
    }
    Token* after = scan_type(token->next);
    return after && sym_is(after, "=");
}

static Node* assign() {
    if (lambda_arrow_ahead()) {
        // `(x, y) -> 式` は形でラムダと区別できるので常に従来どおり。
        // 裸の識別子 `x -> 式` は、x が現在のスコープで束縛済みなら
        // 矢印演算子（step 66）として通常の解析へ進み、未束縛なら
        // ラムダの引数名として扱う。帰結として、束縛済みの変数と
        // 同名の引数を持つ裸ラムダは書けない（fn 形で書く）
        if (token->kind != TK_IDENT || !ident_is_bound(token)) {
            return parse_lambda(true, token);
        }
    }
    Token* first_token = token;
    Node* node = range();
    Token* error_token = token;
    // 矢印演算子（step 66）: 代入に準ずる低い優先順位で、右側は完全な式を
    // 取る。型による解決（dunder 呼び出し・代入・int の比較）は typecheck
    // で行う。代入先になり得る側は、通常の代入文と同じく bind_target_node
    // を通す（関数名と同名なら新しい変数への束縛として扱う）
    if (!at_expr_line_break()) {
        Token* arrow_tok = token;
        bool is_left_arrow = consume("<-");
        bool is_right_arrow = !is_left_arrow && consume("->");
        if (is_left_arrow || is_right_arrow) {
            Node* anode = create_node(is_left_arrow ? ND_ARROW_L : ND_ARROW_R);
            anode->tok = arrow_tok;
            anode->left_child = is_left_arrow ? bind_target_node(node) : node;
            bool saved = suppress_multi;
            suppress_multi = true;
            Node* rhs = assign();
            suppress_multi = saved;
            anode->right_child = is_right_arrow ? bind_target_node(rhs) : rhs;
            return anode;
        }
    }
    if (!suppress_multi && token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == ',') {
        // 複数代入: a, b = 値, 値。値は全部評価してから一時置き場経由で代入する
        Node* targets = node;
        Node* targets_tail = node;
        int ntargets = 1;
        while (consume(",")) {
            targets_tail->next = range();
            targets_tail = targets_tail->next;
            ntargets++;
        }
        expect("=");
        bool saved = suppress_multi;
        suppress_multi = true;
        Node* values = assign();
        Node* values_tail = values;
        int nvalues = 1;
        while (consume(",")) {
            values_tail->next = assign();
            values_tail = values_tail->next;
            nvalues++;
        }
        suppress_multi = saved;
        if (nvalues == 1 && values->kind == ND_INPUT && !values->args && values->val != 2 && ntargets > 1) {
            // a, b = in の形: 対象の数だけ読み取りを並べ替える
            Node* chain = NULL;
            Node** ctail = &chain;
            for (int i = 0; i < ntargets; i++) {
                Node* in_node = create_node(ND_INPUT);
                in_node->val = values->val;
                *ctail = in_node;
                ctail = &in_node->next;
            }
            values = chain;
            nvalues = ntargets;
        }
        if (ntargets != nvalues) {
            error_at(token_start(first_token), token_end(first_token), "代入先と値の個数が一致しません（代入先%d個、値%d個）", ntargets, nvalues);
        }
        for (Node* t = targets; t; t = t->next) {
            Node* bound = bind_target_node(t);
            if (bound != t) {
                Node* saved_next = t->next;
                *t = *bound;
                t->next = saved_next;
            }
        }
        Node* multi = create_node(ND_MULTIASSIGN);
        multi->tok = first_token;
        multi->body = targets;
        multi->args = values;
        multi->array_len = nvalues;
        for (int i = 0; i < nvalues; i++) {
            register_any();
            if (i == 0) {
                multi->offset = locals->offset;
            }
        }
        return multi;
    }
    if (suppress_anno ? annotation_ahead() && consume(":") : consume(":")) {
        Node* assign_node = create_node(ND_ASSIGN);
        node = bind_target_node(node);
        if (node->kind != ND_LVAR && node->kind != ND_GVAR) {
            error_at(token_start(error_token), token_end(error_token), "型注釈は変数にだけ付けられます");
        }
        assign_node->anno = parse_type();
        expect("=");
        assign_node->tok = node->tok;
        assign_node->left_child = node;
        assign_node->right_child = assign();
        node = assign_node;
    } else if (consume("=")) {
        Node* assign_node = create_node(ND_ASSIGN);
        assign_node->tok = node->tok;
        assign_node->left_child = bind_target_node(node);
        assign_node->right_child = assign();
        node = assign_node;
    } else if (consume("+=")) {
        Node* assign_node = create_node(ND_OPASSIGN);
        // 対象の動的な部分（オブジェクト・添え字）を1回だけ評価する
        // ための一時スロットを2つ確保しておく（step 87。使うかどうかは
        // typecheck の check_assign が対象の形で決める）
        register_any();
        assign_node->offset = locals->offset;
        register_any();
        assign_node->op.binop = OP_ADD;
        assign_node->tok = node->tok;
        assign_node->left_child = node;
        assign_node->right_child = assign();
        node = assign_node;
    } else if (consume("-=")) {
        Node* assign_node = create_node(ND_OPASSIGN);
        // 対象の動的な部分（オブジェクト・添え字）を1回だけ評価する
        // ための一時スロットを2つ確保しておく（step 87。使うかどうかは
        // typecheck の check_assign が対象の形で決める）
        register_any();
        assign_node->offset = locals->offset;
        register_any();
        assign_node->op.binop = OP_SUB;
        assign_node->tok = node->tok;
        assign_node->left_child = node;
        assign_node->right_child = assign();
        node = assign_node;
    } else if (consume("*=")) {
        Node* assign_node = create_node(ND_OPASSIGN);
        // 対象の動的な部分（オブジェクト・添え字）を1回だけ評価する
        // ための一時スロットを2つ確保しておく（step 87。使うかどうかは
        // typecheck の check_assign が対象の形で決める）
        register_any();
        assign_node->offset = locals->offset;
        register_any();
        assign_node->op.binop = OP_MUL;
        assign_node->tok = node->tok;
        assign_node->left_child = node;
        assign_node->right_child = assign();
        node = assign_node;
    } else if (consume("/=")) {
        Node* assign_node = create_node(ND_OPASSIGN);
        // 対象の動的な部分（オブジェクト・添え字）を1回だけ評価する
        // ための一時スロットを2つ確保しておく（step 87。使うかどうかは
        // typecheck の check_assign が対象の形で決める）
        register_any();
        assign_node->offset = locals->offset;
        register_any();
        assign_node->op.binop = OP_DIV;
        assign_node->tok = node->tok;
        assign_node->left_child = node;
        assign_node->right_child = assign();
        node = assign_node;
    } else if (consume("%=")) {
        Node* assign_node = create_node(ND_OPASSIGN);
        // 対象の動的な部分（オブジェクト・添え字）を1回だけ評価する
        // ための一時スロットを2つ確保しておく（step 87。使うかどうかは
        // typecheck の check_assign が対象の形で決める）
        register_any();
        assign_node->offset = locals->offset;
        register_any();
        assign_node->op.binop = OP_MOD;
        assign_node->tok = node->tok;
        assign_node->left_child = node;
        assign_node->right_child = assign();
        node = assign_node;
    }
    return node;
}

Node* expr() {
    return assign();
}

static Node* array_literal() {
    Node* node;
    expr_depth++;
    if (consume("]")) {
        node = create_node(ND_ARRAY);
        register_array(0);
        node->array_len = 0;
        node->body = NULL;
        node->offset = locals->offset;
        expr_depth--;
        return node;
    }
    bool saved = suppress_multi;
    suppress_multi = true;
    Node* first_expr = expr();
    if (consume(";")) {
        Token* len_tok = token;
        Node* len_expr = expr();
        // 長さがリテラルの負数ならコンパイルエラー（step 85 の
        // 追加決定）。実行時に決まる負数は 0 に丸められる（step 82）
        long lv;
        if (const_int_value(len_expr, &lv) && lv < 0) {
            error_at(token_start(len_tok), token_end(len_tok), "長さに負の数は指定できません");
        }
        node = create_node(ND_NEWARRAY);
        node->left_child = first_expr;
        node->right_child = len_expr;
    } else {
        node = create_node(ND_ARRAY);
        int array_len = 1;
        Node body;
        Node* cur = &body;
        cur->next = first_expr;
        cur = cur->next;
        while (consume(",")) {
            cur->next = expr();
            cur = cur->next;
            array_len++;
        }
        register_array(array_len);
        node->array_len = array_len;
        node->body = body.next;
        node->offset = locals->offset;
    }
    suppress_multi = saved;
    expect("]");
    expr_depth--;
    return node;
}

// 型引数の閉じ括弧。入れ子（GBox<Pair<int, str>>）では >> と1トークンに
// なるため、>> の前半だけを消費して後半を > として残す。3重の入れ子
// （Vec<Vec<Vec<int>>>）では >>> と1トークンになるため、同じく1つだけ
// 消費して残り（>>）を次の閉じ括弧に回す
static void expect_type_end() {
    if (token->kind == TK_SYMBOL && token->len >= 2 && token->str[0] == '>' && token->str[1] == '>') {
        token->str++;
        token->len--;
        return;
    }
    expect(">");
}

Type* parse_type() {
    if (token->kind == TK_KEYWORD && token->keyword == KW_FN) {
        token = token->next;
        expect("(");
        Type* params[6] = {};
        int nparams = 0;
        if (!consume(")")) {
            do {
                Type* pt = parse_type();
                if (nparams < 6) {
                    params[nparams] = pt;
                }
                nparams++;
            } while (consume(","));
            expect(")");
        }
        if (nparams > 6) {
            error_at(token_start(token), token_end(token), "関数型の引数は6個までですが、%d個が指定されています", nparams);
        }
        expect("->");
        Type* ret = parse_type();
        return func_type(params, nparams, ret);
    }
    if (consume("[")) {
        Type* elem = parse_type();
        expect("]");
        return array_type(elem);
    }
    Token* ident = consume_ident();
    if (!ident) {
        if (token->kind == TK_EOF) {
            error_at(token_start(token), token_end(token), "型名が必要ですが、ファイルの末尾に達しました");
        }
        error_at(token_start(token), token_end(token), "型名が必要ですが、%sが見つかりました", token_text(token));
    }
    if (parsing_klass) {
        for (int i = 0; i < parsing_klass->tparam_count; i++) {
            if (parsing_klass->tparam_lens[i] == ident->len && !memcmp(ident->str, parsing_klass->tparams[i], ident->len)) {
                Type* var = calloc(1, sizeof(Type));
                var->kind = TY_VAR;
                var->name = ident->str;
                var->len = ident->len;
                return var;
            }
        }
    }
    Type* anno = NULL;
    char* type_name[] = {
        "int",
    };
    Type* types[] = {
        &ty_int,
    };
    for (int i = 0; i < sizeof(type_name) / sizeof(type_name[0]); i++) {
        int len = strlen(type_name[i]);
        if (ident->len == len && !memcmp(ident->str, type_name[i], len)) {
            anno = types[i];
        }
    }
    if (!anno) {
        Class* klass = find_class(ident->str, ident->len);
        if (klass) {
            if (consume("<")) {
                Type* applied = calloc(1, sizeof(Type));
                applied->kind = TY_CLASS;
                applied->klass = klass;
                do {
                    if (applied->targ_count < MAX_TPARAMS) {
                        applied->targs[applied->targ_count] = parse_type();
                    } else {
                        parse_type();
                    }
                    applied->targ_count++;
                } while (consume(","));
                expect_type_end();
                if (applied->targ_count != klass->tparam_count) {
                    error_at(token_start(ident), token_end(ident), "クラス`%.*s`の型引数は%d個ですが、%d個が指定されています", klass->len, klass->name, klass->tparam_count, applied->targ_count);
                }
                return applied;
            }
            anno = klass->type;
        }
    }
    if (!anno) {
        error_at(token_start(ident), token_end(ident), "型`%.*s`は定義されていません", ident->len, ident->str);
    }
    return anno;
}
