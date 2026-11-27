#include "spell.h"
#include "parse_internal.h"

LVar* locals = NULL;
LVar* globals = NULL;
static Class* classes;
bool is_top_level = false;

// トップレベル関数の名前表。関数参照（関数名を値として使う）の解決に使う。
// クラス表と同じく program() をまたいで残る（prelude の関数も対象になる）
typedef struct FuncName FuncName;
struct FuncName {
    char* name;
    int len;
    FuncName* next;
};
static FuncName* func_names;

static void register_func_name(char* name, int len) {
    for (FuncName* f = func_names; f; f = f->next) {
        if (f->len == len && !memcmp(f->name, name, len)) {
            return;
        }
    }
    FuncName* f = calloc(1, sizeof(FuncName));
    f->name = name;
    f->len = len;
    f->next = func_names;
    func_names = f;
}

static bool is_func_name(char* name, int len) {
    for (FuncName* f = func_names; f; f = f->next) {
        if (f->len == len && !memcmp(f->name, name, len)) {
            return true;
        }
    }
    return false;
}

// ラムダ解析中の文脈。外側のスコープは、ラムダへ入った時点の locals 先頭への
// スナップショットでたどる（ローカルは先頭追加のみで、既存ノードのリンクは
// 変わらないため、凍結として扱える）。値は凍結せず、実行時にラムダ式を
// 評価した時点でコピーされる
typedef struct LambdaParse LambdaParse;
struct LambdaParse {
    Capture* captures;
    int ncaptures;
    LVar* outer_locals;
    LambdaParse* outer;
    Function* func;
};
static LambdaParse* lambda_stack;

// 解析済みラムダの Function。program() の最後に関数チェーンへ足す
static Function* pending_lambdas;

// 外側スコープを内側から順に探し、名前の在り処を返す。
// level は「そのラムダが直接取り込む」階層、lvar はローカルとして、
// cap は外側ラムダの捕捉として見つけた場合に設定される
typedef struct CaptureHit CaptureHit;
struct CaptureHit {
    LambdaParse* level;
    LVar* lvar;
    Capture* cap;
};

static bool find_outer(Token* tok, CaptureHit* hit) {
    for (LambdaParse* level = lambda_stack; level; level = level->outer) {
        for (LVar* v = level->outer_locals; v; v = v->next) {
            if (v->name && v->len == tok->len && !memcmp(v->name, tok->str, tok->len)) {
                hit->level = level;
                hit->lvar = v;
                hit->cap = NULL;
                return true;
            }
        }
        if (level->outer) {
            for (Capture* c = level->outer->captures; c; c = c->next) {
                if (c->len == tok->len && !memcmp(c->name, tok->str, tok->len)) {
                    hit->level = level;
                    hit->lvar = NULL;
                    hit->cap = c;
                    return true;
                }
            }
        }
    }
    return false;
}

// ラムダ文脈 lp に捕捉を追加する（同名が既にあればそれを返す）
static Capture* add_capture(LambdaParse* lp, char* name, int len, int src_offset, bool src_is_env, Capture* src_cap) {
    for (Capture* c = lp->captures; c; c = c->next) {
        if (c->len == len && !memcmp(c->name, name, len)) {
            return c;
        }
    }
    Capture* c = calloc(1, sizeof(Capture));
    c->name = name;
    c->len = len;
    c->src_offset = src_offset;
    c->src_is_env = src_is_env;
    c->src_cap = src_cap;
    c->env_offset = lp->ncaptures * 8;
    lp->ncaptures++;
    c->next = lp->captures;
    lp->captures = c;
    return c;
}

// 名前が外側スコープで見つかれば、見つけた階層から現在のラムダまで
// 各ラムダへ捕捉を積みながら鎖をつなぎ、現在のラムダの捕捉を返す
static Capture* resolve_capture(Token* tok) {
    CaptureHit hit;
    if (!find_outer(tok, &hit)) {
        return NULL;
    }
    Capture* cap;
    if (hit.lvar) {
        cap = add_capture(hit.level, tok->str, tok->len, hit.lvar->offset, false, NULL);
    } else {
        cap = add_capture(hit.level, tok->str, tok->len, hit.cap->env_offset, true, hit.cap);
    }
    LambdaParse* chain[64];
    int n = 0;
    for (LambdaParse* p = lambda_stack; p != hit.level; p = p->outer) {
        if (n >= 64) {
            error_at(token_start(tok), token_end(tok), "ラムダの入れ子が深すぎます（64段まで）");
        }
        chain[n++] = p;
    }
    for (int i = n - 1; i >= 0; i--) {
        cap = add_capture(chain[i], tok->str, tok->len, cap->env_offset, true, cap);
    }
    return cap;
}

static LVar* find_lvar(Token* tok);
static LVar* find_gvar(Token* tok);

// 識別子＋`(` を、名前呼び出しではなく値経由の呼び出しに振り分けるかの判定。
// 識別子そのものの解決（create_ident_node）と同じく、変数が関数名・クラス名
// より優先される: 変数（ローカル・捕捉可能な外側の変数・グローバル）に
// 解決されれば値経由の呼び出しにする
bool ident_is_value_call(Token* tok) {
    if (!is_top_level) {
        if (find_lvar(tok)) {
            return true;
        }
        if (lambda_stack) {
            CaptureHit hit;
            if (find_outer(tok, &hit)) {
                return true;
            }
        }
    }
    return find_gvar(tok) != NULL;
}

bool consume(char* symbol) {
    int len = strlen(symbol);
    if (token->kind == TK_SYMBOL && token->len == len && !memcmp(symbol, token->str, token->len)) {
        token = token->next;
        return true;
    }
    return false;
}

Token* consume_ident() {
    if (token->kind == TK_IDENT) {
        Token* pre_token = token;
        token = token->next;
        return pre_token;
    }
    return NULL;
}

static bool consume_keyword(Keyword keyword) {
    if (token->kind == TK_KEYWORD && token->keyword == keyword) {
        token = token->next;
        return true;
    }
    return false;
}

void expect(char* symbol) {
    if (!consume(symbol)) {
        char* actual = token_text(token);
        if (actual) {
            error_at(token_start(token), token_end(token), "`%s`が必要ですが、%sが見つかりました", symbol, actual);
        } else {
            error_at(token_start(token), token_end(token), "`%s`が必要ですが、ファイルの末尾に達しました", symbol);
        }
    }
}

long expect_number() {
    if (token->kind == TK_NUM) {
        long num = token->val;
        token = token->next;
        return num;
    }
    char* actual = token_text(token);
    if (actual) {
        error_at(token_start(token), token_end(token), "数値が必要ですが、%sが見つかりました", actual);
    } else {
        error_at(token_start(token), token_end(token), "数値が必要ですが、ファイルの末尾に達しました");
    }
    return 0;
}

static bool at_eof() {
    if (token->kind == TK_EOF) {
        return true;
    }
    return false;
}

Node* create_node(NodeKind kind) {
    Node* node = calloc(1, sizeof(Node));
    node->kind = kind;
    return node;
}

Node* create_binary_operation_node(BinOp kind, Node* left_child, Node* right_child) {
    Node* new_node_ptr = create_node(ND_BINOP);
    new_node_ptr->op.binop = kind;
    new_node_ptr->left_child = left_child;
    new_node_ptr->right_child = right_child;
    return new_node_ptr;
}

Node* create_number_node(long num) {
    Node* num_node_ptr = create_node(ND_NUM);
    num_node_ptr->val = num;
    return num_node_ptr;
}

// トップレベル文の解析中に確保されたスタック量の最大値。
// 合成関数（__spell_init / main）のフレーム確保に使う
static int top_max_offset = 0;

static void note_top_used(int offset, int array_len) {
    if (is_top_level) {
        int used = offset + 8 * array_len;
        if (top_max_offset < used) {
            top_max_offset = used;
        }
    }
}

void register_any() {
    LVar* lvar = calloc(1, sizeof(LVar));
    lvar->offset = locals ? locals->offset + 8 * (locals->array_len + 1): 8;
    lvar->next = locals;
    locals = lvar;
    note_top_used(lvar->offset, 0);
}

static void register_lvar(Token* tok) {
    register_any();
    locals->name = tok->str;
    locals->len = tok->len;
}

static void register_gvar(Token* tok) {
    LVar* gvar = calloc(1, sizeof(LVar));
    gvar->next = globals;
    globals = gvar;
    globals->name = tok->str;
    globals->len = tok->len;
}

void register_array(int array_len) {
    register_any();
    // フレーム上のリテラルは、要素と長さに加えて容量（cap）の置き場所も
    // 1スロット余分に取る（step 65: ブロックは [cap][len][要素...]）
    locals->array_len = array_len + 1;
    note_top_used(locals->offset, array_len + 1);
}

Node* create_ident_node(Token* tok) {
    if (!is_top_level) {
        LVar* lvar = find_lvar(tok);
        if (lvar) {
            Node* node = create_node(ND_LVAR);
            node->name = tok->str;
            node->len = tok->len;
            node->offset = lvar->offset;
            node->tok = tok;
            return node;
        }
        if (lambda_stack) {
            Capture* cap = resolve_capture(tok);
            if (cap) {
                Node* node = create_node(ND_CAPVAR);
                node->name = tok->str;
                node->len = tok->len;
                node->offset = cap->env_offset;
                node->tok = tok;
                return node;
            }
        }
    }
    LVar* gvar = find_gvar(tok);
    if (!gvar) {
        if (is_func_name(tok->str, tok->len)) {
            Node* node = create_node(ND_FUNCREF);
            node->name = tok->str;
            node->len = tok->len;
            node->tok = tok;
            return node;
        }
        if (!is_top_level) {
            register_lvar(tok);
            Node* node = create_node(ND_LVAR);
            node->name = tok->str;
            node->len = tok->len;
            node->offset = locals->offset;
            node->tok = tok;
            return node;
        }
        register_gvar(tok);
    }
    Node* node = create_node(ND_GVAR);
    node->name = tok->str;
    node->len = tok->len;
    node->tok = tok;
    return node;
}

// 代入先として使われた関数参照ノードを、新しい変数への束縛に作り替える。
// 関数名と同じ名前への代入は、関数ではなく変数の初回束縛として扱う
// （読み取りでは関数値のまま）。それ以外のノードはそのまま返す
Node* bind_target_node(Node* node) {
    if (node->kind != ND_FUNCREF || !node->tok) {
        return node;
    }
    if (is_top_level) {
        register_gvar(node->tok);
        Node* g = create_node(ND_GVAR);
        g->name = node->name;
        g->len = node->len;
        g->tok = node->tok;
        return g;
    }
    register_lvar(node->tok);
    Node* v = create_node(ND_LVAR);
    v->name = node->name;
    v->len = node->len;
    v->tok = node->tok;
    v->offset = locals->offset;
    return v;
}

Node* char_array() {
    Node* node;
    Token* str_token = token;
    token = token->next;
    node = create_node(ND_ARRAY);
    Node body;
    body.next = NULL;
    Node* cur = &body;
    for (int i = 0; i < str_token->len; i++) {
        cur->next = create_number_node(str_token->str[i]);
        cur = cur->next;
    }
    register_array(str_token->len);
    node->array_len = str_token->len;
    node->body = body.next;
    node->offset = locals->offset;
    node->tok = str_token;
    return node;
}

static Node* stmt_block();
static Node* stmt_return(Token* kw);
static Node* stmt_if(Token* kw);
static Node* stmt_while(Token* kw);
static Node* stmt_for(Token* kw);
static Node* make_for_iter(Token* var, Node* init, Token* kw);
static Node* make_for_count(Node* count, Token* kw);
static Node* stmt_branch();

// 文（やフィールド）の終わり。`;` があれば消費する。なければ、次の
// トークンが改行の後か、`}` か、ファイル末尾でなければならない
// （step 55: `;` は省略できるが、その場合は改行が区切りになる）
static void expect_stmt_end(const char* subject) {
    if (consume(";")) {
        return;
    }
    if (token->kind == TK_EOF
            || (token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == '}')) {
        return;
    }
    if (token->prev && token->line > token->prev->line) {
        return;
    }
    char* actual = token_text(token);
    if (actual) {
        error_at(token_start(token), token_end(token), "%sの終わりには`;`か改行が必要ですが、%sが見つかりました", subject, actual);
    } else {
        error_at(token_start(token), token_end(token), "%sの終わりには`;`か改行が必要ですが、ファイルの末尾に達しました", subject);
    }
}

static Node* stmt() {
    Token* start = token;
    // 空文（余分な `;`）は読み飛ばす。1つ以上読み飛ばした直後が
    // ブロックの閉じ `}` なら空文として扱う（`}` は呼び出し側の
    // ループが消費する）。それ以外の止まり方は通常の解析に任せて
    // エラーにする（リスト文脈で消費せずに戻ると無限ループに
    // なるため。また `;` が無いのに文が無い場合まで許すと、
    // 以前はエラーだった `if (1) }` のような形を受け付けてしまう）
    bool skipped_semi = false;
    while (consume(";")) {
        skipped_semi = true;
    }
    if (skipped_semi && !is_top_level
            && token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == '}') {
        Node* node = create_node(ND_BLOCK);
        node->tok = start;
        return node;
    }
    if (consume("{")) {
        return stmt_block();
    } else if (consume_keyword(KW_RETURN)) {
        return stmt_return(start);
    } else if (consume_keyword(KW_IF)) {
        return stmt_if(start);
    } else if (consume_keyword(KW_WHILE)) {
        return stmt_while(start);
    } else if (consume_keyword(KW_FOR)) {
        return stmt_for(start);
    } else if (token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == '!'
               && (token->next->kind == TK_EOF
                   || (token->next->kind == TK_SYMBOL && token->next->len == 1
                       && (token->next->str[0] == ';' || token->next->str[0] == '}'))
                   || token->next->line > token->line)) {
        // `!` の直後が文の終わり（`;`・改行・`}`・ファイル末尾）なら
        // break の省略形。それ以外は単項 `!` の式文として扱う
        consume("!");
        consume(";");
        Node* node = create_node(ND_BREAK);
        node->tok = start;
        return node;
    } else if (consume("^")) {
        // `^` は continue の省略形（式の先頭には現れないため曖昧さはない）
        Node* node = create_node(ND_CONTINUE);
        node->tok = start;
        expect_stmt_end("文");
        return node;
    } else if (consume("@")) {
        if (token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == '{') {
            // `@ {}` は while true {} の省略形（無限ループ）
            Node* node = create_node(ND_WHILE);
            node->tok = start;
            node->cond = create_node(ND_NUM);
            node->cond->val = 1;
            node->cond->tok = start;
            node->then = stmt_branch();
            return node;
        }
        return make_for_count(expr(), start);
    } else if (token->kind == TK_IDENT && token->next->kind == TK_SYMBOL && token->next->len == 1 && token->next->str[0] == '@') {
        Token* var = consume_ident();
        consume("@");
        return make_for_iter(var, expr(), start);
    } else if (consume("<<") || consume("<")) {
        // 出力の省略形: << 式 / < 式（print と同じ）
        Node* node = create_node(ND_FUNCALL);
        node->name = "print";
        node->len = 5;
        node->tok = start;
        node->args = expr();
        expect_stmt_end("文");
        return node;
    } else if (consume(">>")) {
        // 入力の省略形: >> 対象 [, 対象 ...]
        // 対象が複数なら、各対象へ in を左から順に代入する文の並びに
        // 展開する（>> a, b は a = in; b = in; と同じ）。対象が1つなら
        // 従来どおり代入文そのものを返す
        Node head;
        head.next = NULL;
        Node* cur = &head;
        do {
            Node* target = parse_input_target();
            Node* input = create_node(ND_INPUT);
            input->val = 0;
            input->tok = start;
            Node* node = create_node(ND_ASSIGN);
            node->left_child = target;
            node->right_child = input;
            node->tok = target->tok;
            cur->next = node;
            cur = node;
        } while (consume(","));
        expect_stmt_end("文");
        if (!head.next->next) {
            return head.next;
        }
        Node* block = create_node(ND_BLOCK);
        block->body = head.next;
        block->tok = start;
        return block;
    } else if (consume_keyword(KW_PUT)) {
        Node* node = create_node(ND_PUT);
        node->tok = start;
        node->args = parse_put_args();
        expect_stmt_end("文");
        return node;
    } else if (consume_keyword(KW_YN)) {
        Node* node = create_node(ND_FUNCALL);
        node->name = "print_yn";
        node->len = 8;
        node->tok = start;
        node->args = expr();
        expect_stmt_end("文");
        return node;
    } else if (token->kind == TK_IDENT && token->next->kind == TK_SYMBOL && token->next->len == 1 && token->next->str[0] == '-'
               && token->next->next->kind == TK_SYMBOL && token->next->next->len == 1 && token->next->next->str[0] == '=') {
        // 入力修飾子: a- = in[n] は、読み取った各値から1を引く（1始まり→0始まり）
        Token* var = consume_ident();
        consume("-");
        consume("=");
        if (token->kind != TK_KEYWORD || token->keyword != KW_IN) {
            error_at(token_start(token), token_end(token), "`in`が必要ですが、%sが見つかりました", token_text(token));
        }
        token = token->next;
        expect("[");
        Node* n = expr();
        expect("]");
        expect_stmt_end("文");
        Node* call = create_node(ND_FUNCALL);
        call->name = "read_ints_dec";
        call->len = 13;
        call->args = n;
        Node* node = create_node(ND_ASSIGN);
        node->left_child = bind_target_node(create_ident_node(var));
        node->right_child = call;
        node->tok = var;
        return node;
    } else if (consume_keyword(KW_BREAK)) {
        Node* node = create_node(ND_BREAK);
        node->tok = start;
        expect_stmt_end("文");
        return node;
    } else if (consume_keyword(KW_CONTINUE)) {
        Node* node = create_node(ND_CONTINUE);
        node->tok = start;
        expect_stmt_end("文");
        return node;
    } else {
        Node* node = expr();
        expect_stmt_end("文");
        return node;
    }
}

// 分岐位置（if/while/for の本体）専用の文読み取り。区切り（`;`）だけが
// あって本体が空の場合（`if (c) ; else ...` など）は空ブロックを返す。
// リスト文脈では使わない: `}`・else・elif・末尾で消費せずに戻すのは、
// それらを消費できる呼び出し側（stmt_if 等）がいる場所だけにする
static Node* stmt_branch() {
    Token* t = token;
    bool skipped_semi = false;
    while (t->kind == TK_SYMBOL && t->len == 1 && t->str[0] == ';') {
        t = t->next;
        skipped_semi = true;
    }
    if (skipped_semi
            && (t->kind == TK_EOF
                || (t->kind == TK_SYMBOL && t->len == 1 && t->str[0] == '}')
                || (t->kind == TK_KEYWORD && (t->keyword == KW_ELSE || t->keyword == KW_ELIF)))) {
        token = t;
        Node* node = create_node(ND_BLOCK);
        node->tok = t;
        return node;
    }
    return stmt();
}

static bool token_is_char(Token* t, char c) {
    return t->kind == TK_SYMBOL && t->len == 1 && t->str[0] == c;
}

// キャプチャ有効時（LSP）の文単位リカバリ用: 失敗した文の残りを
// 読み飛ばし、次の文の先頭に token を合わせる。次の `;`（現在の
// 入れ子深さで）まで進めるか、失敗した文の中で開いたブロックの
// 閉じ `}` を消費したところで止まる。ブロック自身の閉じ `}` は
// 消費しない（呼び出し側のループが消費する）。consume_close が
// 真なら、深さ0の `}` も消費して止まる（トップレベル用: 失敗した
// 関数・クラス定義の閉じ括弧を片付ける）
static void skip_to_stmt_boundary(bool consume_close) {
    int depth = 0;
    while (token->kind != TK_EOF) {
        if (token_is_char(token, '{')) {
            depth++;
            token = token->next;
            continue;
        }
        if (token_is_char(token, '}')) {
            if (depth == 0) {
                if (consume_close) {
                    token = token->next;
                }
                return;
            }
            depth--;
            token = token->next;
            if (depth == 0) {
                return;
            }
            continue;
        }
        if (depth == 0 && token_is_char(token, ';')) {
            token = token->next;
            return;
        }
        token = token->next;
    }
}

// 「}」まで文を読み、連結リストで返す
static Node* parse_stmt_list() {
    Node head;
    head.next = NULL;
    Node* cur = &head;
    while (!consume("}")) {
        if (!error_capture_enabled()) {
            cur->next = stmt();
            cur = cur->next;
            continue;
        }
        // キャプチャ有効時: 文の解析に失敗したら、その文を捨てて
        // 境界まで読み飛ばし、次の文から再開する（診断は記録済み）
        jmp_buf jb;
        LVar* saved_locals = locals;
        LVar* saved_globals = globals;
        LambdaParse* saved_lambda = lambda_stack;
        int saved_depth = expr_depth;
        jmp_buf* saved_recover = error_capture_set_recover(&jb);
        if (setjmp(jb) == 0) {
            Node* s = stmt();
            error_capture_set_recover(saved_recover);
            cur->next = s;
            cur = cur->next;
        } else {
            error_capture_set_recover(saved_recover);
            locals = saved_locals;
            globals = saved_globals;
            lambda_stack = saved_lambda;
            expr_depth = saved_depth;
            if (at_eof()) {
                break;
            }
            skip_to_stmt_boundary(false);
        }
    }
    return head.next;
}

static Node* stmt_block() {
    Node* node;
    node = create_node(ND_BLOCK);
    node->body = parse_stmt_list();
    return node;
}

static Node* stmt_return(Token* kw) {
    Node* node;
    node = create_node(ND_RETURN);
    node->tok = kw;
    node->left_child = expr();
    expect_stmt_end("文");
    return node;
}

static Node* stmt_if(Token* kw) {
    Node* node;
    node = create_node(ND_IF);
    node->tok = kw;
    expect("(");
    expr_depth++;
    node->cond = expr();
    expr_depth--;
    expect(")");
    node->then = stmt_branch();
    if (consume_keyword(KW_ELSE)) {
        node->els = stmt_branch();
    } else if (token->kind == TK_KEYWORD && token->keyword == KW_ELIF) {
        Token* elif_tok = token;
        token = token->next;
        node->els = stmt_if(elif_tok);
    }
    return node;
}

static Node* stmt_while(Token* kw) {
    Node* node;
    node = create_node(ND_WHILE);
    node->tok = kw;
    expect("(");
    expr_depth++;
    node->cond = expr();
    expr_depth--;
    expect(")");
    node->then = stmt_branch();
    return node;
}

static Node* make_for_iter(Token* var, Node* init, Token* kw) {
    Node* node = create_node(ND_FOR);
    node->tok = kw;
    node->init = init;
    register_any();
    node->offset = locals->offset;
    node->left_child = bind_target_node(create_ident_node(var));
    node->then = stmt_branch();
    if (consume_keyword(KW_ELSE)) {
        register_any();
        node->val = locals->offset;
        node->els = stmt_branch();
    }
    return node;
}

static Node* make_for_count(Node* count, Token* kw) {
    // 束縛なしの回数リピートは Range(0, n) の要素走査として扱う
    Node* node = create_node(ND_FOR);
    node->tok = kw;
    Node* range_call = create_node(ND_FUNCALL);
    range_call->name = "__count_range";
    range_call->len = 13;
    range_call->args = count;
    node->init = range_call;
    register_any();
    node->offset = locals->offset;
    register_any();
    Node* var = create_node(ND_LVAR);
    var->offset = locals->offset;
    node->left_child = var;
    node->then = stmt_branch();
    if (consume_keyword(KW_ELSE)) {
        register_any();
        node->val = locals->offset;
        node->els = stmt_branch();
    }
    return node;
}

static Node* stmt_for(Token* kw) {
    if (token->kind == TK_IDENT && token->next->kind == TK_KEYWORD && token->next->keyword == KW_IN) {
        Token* var = consume_ident();
        consume_keyword(KW_IN);
        return make_for_iter(var, expr(), kw);
    }
    return make_for_count(expr(), kw);
}

// 「(引数, ...)」の中身を読む。開き括弧は消費済み。引数の数を返す
static int parse_params(Type* param_types[6]) {
    int nparams = 0;
    if (!consume(")")) {
        do {
            register_lvar(consume_ident());
            if (consume(":")) {
                Type* pt = parse_type();
                if (nparams < 6) {
                    param_types[nparams] = pt;
                }
            }
            nparams++;
        } while (consume(","));
        expect(")");
    }
    return nparams;
}

static Function* function(Class* klass) {
    locals = NULL;
    consume_keyword(KW_DEF);
    Token* tok = consume_ident();
    if (!tok) {
        if (token) {
            error_at(token_start(token), token_end(token), "関数名が必要ですが、%sが見つかりました", token_text(token));
        } else {
            error_at(token_start(token), token_end(token), "関数名が必要ですが、ファイルの末尾に達しました", token_text(token));
        }
    }
    char* name = tok->str;
    int len = tok->len;
    if (!consume("(")) {
        expect("(");
    }
    Type* param_types[6] = {};
    int nparams = parse_params(param_types);
    if (nparams > 6) {
        error_at(token_start(tok), token_end(tok), "引数は6個までですが、%d個が指定されています", nparams);
    }
    Type* return_type = NULL;
    if (consume("->")) {
        return_type = parse_type();
    }
    expect("{");
    Node* body = parse_stmt_list();
    Function* func = calloc(1, sizeof(Function));
    func->name = name;
    func->len = len;
    func->body = body;
    func->nparams = nparams;
    func->return_type = return_type;
    memcpy(func->param_types, param_types, sizeof(param_types));
    func->klass = klass;
    if (klass && nparams == 0) {
        error_at(token_start(tok), token_end(tok), "メソッドには最初の引数`self`が必要です");
    }
    if (klass && nparams > 0) {
        func->param_types[0] = klass->type;
    }
    LVar* head_lvar = locals;
    LVar* cur_lvar = head_lvar;
    int max_offset = 0;
    while (cur_lvar) {
        int offset = cur_lvar->offset + 8 * (cur_lvar->array_len);
        if (max_offset < offset) {
            max_offset = offset;
        }
        cur_lvar = cur_lvar->next;
    }
    func->stack_size = (max_offset + 15) / 16 * 16;
    return func;
}

static int lambda_counter = 0;

// 矢印形ラムダの先読み判定: 「識別子 ->」または「(識別子, ...) ->」で始まるか
bool lambda_arrow_ahead() {
    Token* t = token;
    if (t->kind == TK_IDENT) {
        Token* n = t->next;
        return n && n->kind == TK_SYMBOL && n->len == 2 && !memcmp(n->str, "->", 2);
    }
    if (t->kind == TK_SYMBOL && t->len == 1 && t->str[0] == '(') {
        t = t->next;
        if (t->kind == TK_SYMBOL && t->len == 1 && t->str[0] == ')') {
            t = t->next;
            return t->kind == TK_SYMBOL && t->len == 2 && !memcmp(t->str, "->", 2);
        }
        while (true) {
            if (t->kind != TK_IDENT) {
                return false;
            }
            t = t->next;
            if (t->kind == TK_SYMBOL && t->len == 1 && t->str[0] == ',') {
                t = t->next;
                continue;
            }
            if (t->kind == TK_SYMBOL && t->len == 1 && t->str[0] == ')') {
                t = t->next;
                return t->kind == TK_SYMBOL && t->len == 2 && !memcmp(t->str, "->", 2);
            }
            return false;
        }
    }
    return false;
}

// ラムダ式を解析する。arrow なら矢印形（x -> 式）、でなければ fn 完全形。
// 生成した関数は pending_lambdas へ積み、program() がチェーンへ足す
Node* parse_lambda(bool arrow, Token* kw) {
    LambdaParse* lp = calloc(1, sizeof(LambdaParse));
    lp->outer_locals = locals;
    lp->outer = lambda_stack;
    lambda_stack = lp;
    LVar* saved_locals = locals;
    bool saved_top = is_top_level;
    is_top_level = false;
    locals = NULL;

    Type* param_types[6] = {};
    int nparams = 0;
    Type* return_type = NULL;
    Node* body;
    if (arrow) {
        if (consume("(")) {
            if (!consume(")")) {
                do {
                    Token* p = consume_ident();
                    if (!p) {
                        error_at(token_start(token), token_end(token), "引数名が必要ですが、%sが見つかりました", token_text(token));
                    }
                    register_lvar(p);
                    nparams++;
                } while (consume(","));
                expect(")");
            }
        } else {
            register_lvar(consume_ident());
            nparams = 1;
        }
        if (nparams > 6) {
            error_at(token_start(kw), token_end(kw), "引数は6個までですが、%d個が指定されています", nparams);
        }
        expect("->");
        Node* ret = create_node(ND_RETURN);
        ret->left_child = expr();
        body = ret;
    } else {
        token = token->next;
        expect("(");
        nparams = parse_params(param_types);
        if (nparams > 6) {
            error_at(token_start(kw), token_end(kw), "引数は6個までですが、%d個が指定されています", nparams);
        }
        if (consume("->")) {
            return_type = parse_type();
        }
        expect("{");
        body = parse_stmt_list();
    }

    Function* func = calloc(1, sizeof(Function));
    char* name = malloc(32);
    snprintf(name, 32, "__lambda_%d", lambda_counter++);
    func->name = name;
    func->len = (int)strlen(name);
    func->body = body;
    func->nparams = nparams;
    func->return_type = return_type;
    memcpy(func->param_types, param_types, sizeof(param_types));
    func->is_lambda = true;
    func->captures = lp->captures;
    lp->func = func;
    int max_offset = 0;
    for (LVar* v = locals; v; v = v->next) {
        int off = v->offset + 8 * v->array_len;
        if (max_offset < off) {
            max_offset = off;
        }
    }
    // 環境ポインタの退避スロットは、ローカルの後ろに1つ取る
    func->env_offset = max_offset + 8;
    func->stack_size = (func->env_offset + 15) / 16 * 16;
    func->next = pending_lambdas;
    pending_lambdas = func;

    lambda_stack = lp->outer;
    locals = saved_locals;
    is_top_level = saved_top;

    Node* node = create_node(ND_LAMBDA);
    node->func = func;
    node->name = func->name;
    node->len = func->len;
    node->captures = lp->captures;
    node->tok = kw;
    return node;
}

static Class* register_class(char* name, int len) {
    Class* klass = find_class(name, len);
    if (klass) {
        return klass;
    }
    klass = calloc(1, sizeof(Class));
    klass->name = name;
    klass->len = len;
    if (len == 3 && !memcmp(name, "int", 3)) {
        klass->type = &ty_int;
    } else if (len == 5 && !memcmp(name, "array", 5)) {
        klass->type = array_type(&ty_unknown);
    } else {
        klass->type = calloc(1, sizeof(Type));
        klass->type->kind = TY_CLASS;
        klass->type->klass = klass;
    }
    klass->next = classes;
    classes = klass;
    return klass;
}

Class* parsing_klass = NULL;

static void class_member(Function** cur, Class* klass, int* offset);

static void class_decl(Function** cur) {
    consume_keyword(KW_CLASS);
    Token* tok = consume_ident();
    if (!tok) {
        error_at(token_start(token), token_end(token), "クラス名が必要ですが、%sが見つかりました", token_text(token));
    }
    Class* klass = register_class(tok->str, tok->len);
    if (consume("<")) {
        int count = 0;
        Token* extra = NULL;
        do {
            Token* tparam = consume_ident();
            if (!tparam) {
                error_at(token_start(token), token_end(token), "型パラメータ名が必要ですが、%sが見つかりました", token_text(token));
            }
            if (count < MAX_TPARAMS) {
                klass->tparams[count] = tparam->str;
                klass->tparam_lens[count] = tparam->len;
            } else if (!extra) {
                extra = tparam;
            }
            count++;
        } while (consume(","));
        klass->tparam_count = count < MAX_TPARAMS ? count : MAX_TPARAMS;
        if (count > MAX_TPARAMS) {
            error_at(token_start(extra), token_end(extra), "型パラメータは%d個までですが、%d個が指定されています", MAX_TPARAMS, count);
        }
        expect(">");
    }
    expect("{");
    int offset = 0;
    parsing_klass = klass;
    while (!consume("}")) {
        if (!error_capture_enabled()) {
            class_member(cur, klass, &offset);
            continue;
        }
        // キャプチャ有効時: メンバ（フィールド・メソッド）の解析に
        // 失敗したら、そのメンバを捨てて境界まで読み飛ばし、次の
        // メンバから再開する。本体内の文の失敗は parse_stmt_list 側で
        // 処理されるため、ここへ来るのは宣言部分の失敗だけ
        jmp_buf jb;
        LVar* saved_locals = locals;
        LVar* saved_globals = globals;
        LambdaParse* saved_lambda = lambda_stack;
        int saved_depth = expr_depth;
        jmp_buf* saved_recover = error_capture_set_recover(&jb);
        if (setjmp(jb) == 0) {
            class_member(cur, klass, &offset);
            error_capture_set_recover(saved_recover);
        } else {
            error_capture_set_recover(saved_recover);
            locals = saved_locals;
            globals = saved_globals;
            lambda_stack = saved_lambda;
            expr_depth = saved_depth;
            if (at_eof()) {
                break;
            }
            skip_to_stmt_boundary(false);
        }
    }
    parsing_klass = NULL;
}

// クラス本体のメンバを1つ読む（メソッド定義またはフィールド宣言）
static void class_member(Function** cur, Class* klass, int* offset) {
    // 空の区切り（余分な `;`）は読み飛ばす。`}` は class_decl 側が消費する
    while (consume(";")) {
    }
    if (token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == '}') {
        return;
    }
    if (token->kind == TK_KEYWORD && token->keyword == KW_DEF) {
        (*cur)->next = function(klass);
        *cur = (*cur)->next;
        return;
    }
    Token* field_tok = consume_ident();
    if (!field_tok) {
        error_at(token_start(token), token_end(token), "フィールド名が必要ですが、%sが見つかりました", token_text(token));
    }
    for (Field* f = klass->fields; f; f = f->next) {
        if (f->len == field_tok->len && !memcmp(f->name, field_tok->str, f->len)) {
            error_at(token_start(field_tok), token_end(field_tok), "フィールド`%.*s`は既に定義されています", field_tok->len, field_tok->str);
        }
    }
    expect(":");
    Field* field = calloc(1, sizeof(Field));
    field->name = field_tok->str;
    field->len = field_tok->len;
    field->type = parse_type();
    field->offset = *offset;
    *offset += 8;
    field->next = klass->fields;
    klass->fields = field;
    expect_stmt_end("フィールド");
}

static bool token_is_symbol(Token* t, char* symbol) {
    int len = strlen(symbol);
    return t->kind == TK_SYMBOL && t->len == len && !memcmp(symbol, t->str, t->len);
}

// トップレベルの項目を1つ読む（クラス定義・関数定義・文）
static void parse_top_item(Function** cur, Node** stmt_cur) {
    if (token->kind == TK_KEYWORD && token->keyword == KW_CLASS) {
        class_decl(cur);
    } else if (token->kind == TK_KEYWORD && token->keyword == KW_DEF || consume(":")) {
        (*cur)->next = function(NULL);
        *cur = (*cur)->next;
    } else {
        is_top_level = true;
        (*stmt_cur)->next = stmt();
        *stmt_cur = (*stmt_cur)->next;
        is_top_level = false;
    }
}

Function* program() {
    top_max_offset = 0;
    Token* head_t = token;
    for (Token* t = head_t; t->kind != TK_EOF; t = t->next) {
        if (t->kind == TK_KEYWORD && t->keyword == KW_CLASS && t->next->kind == TK_IDENT) {
            Class* klass = register_class(t->next->str, t->next->len);
            // 型パラメータも先に登録しておく。宣言より前の注釈（前方参照）でも
            // 型引数の個数を検証できるようにするため
            Token* u = t->next->next;
            if (u && token_is_symbol(u, "<")) {
                int count = 0;
                u = u->next;
                while (u && u->kind == TK_IDENT) {
                    if (count < MAX_TPARAMS) {
                        klass->tparams[count] = u->str;
                        klass->tparam_lens[count] = u->len;
                    }
                    count++;
                    u = u->next;
                    if (!u || !token_is_symbol(u, ",")) {
                        break;
                    }
                    u = u->next;
                }
                // 個数は丸めずに記録する。上限超えの宣言では、注釈側の個数検証で
                // 隠さず class_decl 側の上限エラーを出させるため
                klass->tparam_count = count;
            }
        }
    }
    // トップレベル関数の名前も先に登録しておく。関数参照（関数名を値として
    // 使う）が、定義より前の位置からでも解決できるようにするため。
    // 波括弧の深さ0にある def 名と `:` 形の関数名だけを対象にする
    int depth = 0;
    for (Token* t = head_t; t->kind != TK_EOF; t = t->next) {
        if (token_is_symbol(t, "{")) {
            depth++;
            continue;
        }
        if (token_is_symbol(t, "}")) {
            depth--;
            continue;
        }
        if (depth != 0) {
            continue;
        }
        if (t->kind == TK_KEYWORD && t->keyword == KW_DEF && t->next->kind == TK_IDENT) {
            register_func_name(t->next->str, t->next->len);
        } else if (token_is_symbol(t, ":")) {
            Token* u = t->next;
            if (u->kind == TK_KEYWORD && u->keyword == KW_DEF) {
                u = u->next;
            }
            if (u->kind == TK_IDENT && token_is_symbol(u->next, "(")) {
                register_func_name(u->str, u->len);
            }
        }
    }
    token = head_t;

    Function head;
    head.next = NULL;
    Function* cur = &head;
    Node stmt_head;
    stmt_head.next = NULL;
    Node* stmt_cur = &stmt_head;
    while (!at_eof()) {
        if (!error_capture_enabled()) {
            parse_top_item(&cur, &stmt_cur);
            continue;
        }
        // キャプチャ有効時: トップレベルの項目（文・関数・クラス）の
        // 解析に失敗したら、その項目を捨てて境界まで読み飛ばし、
        // 次の項目から再開する。本体内の文の失敗は parse_stmt_list
        // 側のリカバリで処理されるため、ここへ来るのは項目の
        // 外側（引数リストやフィールド宣言など）の失敗だけ
        jmp_buf jb;
        LVar* saved_locals = locals;
        LVar* saved_globals = globals;
        LambdaParse* saved_lambda = lambda_stack;
        int saved_depth = expr_depth;
        jmp_buf* saved_recover = error_capture_set_recover(&jb);
        if (setjmp(jb) == 0) {
            parse_top_item(&cur, &stmt_cur);
            error_capture_set_recover(saved_recover);
        } else {
            error_capture_set_recover(saved_recover);
            locals = saved_locals;
            globals = saved_globals;
            lambda_stack = saved_lambda;
            expr_depth = saved_depth;
            is_top_level = false;
            if (at_eof()) {
                break;
            }
            skip_to_stmt_boundary(true);
        }
    }
    // 解析中に作られたラムダを関数チェーンの末尾へ足す
    if (pending_lambdas) {
        Function* tail = head.next;
        while (tail && tail->next) {
            tail = tail->next;
        }
        while (pending_lambdas) {
            Function* f = pending_lambdas;
            pending_lambdas = f->next;
            f->next = NULL;
            if (tail) {
                tail->next = f;
            } else {
                head.next = f;
            }
            tail = f;
        }
    }
    if (!stmt_head.next) {
        return head.next;
    }
    Node* ret = create_node(ND_RETURN);
    ret->left_child = create_number_node(0);
    stmt_cur->next = ret;
    Node* block = create_node(ND_BLOCK);
    block->body = stmt_head.next;
    Function* init = calloc(1, sizeof(Function));
    init->body = block;
    init->stack_size = (top_max_offset + 15) / 16 * 16;
    bool has_main = false;
    for (Function* fn = head.next; fn; fn = fn->next) {
        if (fn->len == 4 && !memcmp(fn->name, "main", 4)) {
            has_main = true;
        }
    }
    if (has_main) {
        init->name = "__spell_init";
        init->len = 12;
    } else {
        init->name = "main";
        init->len = 4;
    }
    init->next = head.next;
    return init;
}

static LVar* find_lvar(Token* tok) {
    for (LVar* var = locals; var; var = var->next) {
        if (var->len == tok->len && !memcmp(var->name, tok->str, tok->len)) {
            return var;
        }
    }
    return NULL;
}

static LVar* find_gvar(Token* tok) {
    for (LVar* var = globals; var; var = var->next) {
        if (var->len == tok->len && !memcmp(var->name, tok->str, tok->len)) {
            return var;
        }
    }
    return NULL;
}

Class* find_class(char* name, int len) {
    for (Class* c = classes; c; c = c->next) {
        if (c->len == len && !memcmp(c->name, name, len)) {
            return c;
        }
    }
    return NULL;
}
