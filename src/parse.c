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

// 名前が現在のスコープで束縛済みか（ローカル・捕捉可能な外側の変数・
// グローバル・関数名のいずれかに解決されるか）。`x -> 式` の振り分けに
// 使う（step 66）: 束縛済みなら矢印演算子、未束縛ならラムダの引数名。
// 束縛の構造はパーサの名前表がそのまま型検査の使うものなので、
// 振り分けはここで決めてよい（型による push/pop/比較の解決は typecheck）
bool ident_is_bound(Token* tok) {
    if (ident_is_value_call(tok)) {
        return true;
    }
    return is_func_name(tok->str, tok->len);
}

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

bool consume_keyword(Keyword keyword) {
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

LVar* register_lvar(Token* tok) {
    register_any();
    locals->name = tok->str;
    locals->len = tok->len;
    return locals;
}

// トップレベル文の解析で is_top_level を落とした区間（内包表記の
// 解析中、step 95）に登録されたスロットは note_top_used を通らない。
// その区間が終わった後に locals 全体をフレーム使用量へ反映し直す
void note_top_all_locals() {
    if (!is_top_level) {
        return;
    }
    for (LVar* v = locals; v; v = v->next) {
        note_top_used(v->offset, v->array_len);
    }
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
static Node* stmt_branch(Token* hdr);
static Node* suite_body(Token* hdr);
static Node* header_expr();
static Node* cond_expr();

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

// 個数形の葉の値（整数形（>>）は 0、文字列形（>>>）は ""）を
// 毎回新しいノードで作る
static Node* make_input_fill(Token* name, int mode) {
    if (mode == 0) {
        Node* zero = create_number_node(0);
        zero->tok = name;
        return zero;
    }
    // "" の文字配列を str へ変換する（文字列リテラルと同じ形）
    Token* fake = calloc(1, sizeof(Token));
    fake->kind = TK_STR;
    fake->str = "";
    fake->len = 0;
    fake->line = name->line;
    fake->next = token;
    token = fake;
    Node* chars = char_array();
    Node* str_node = create_node(ND_FUNCALL);
    str_node->name = "str";
    str_node->len = 3;
    str_node->tok = name;
    str_node->args = chars;
    return str_node;
}

// 個数形の「下ごしらえ」リテラル: 深さ depth の入れ子配列リテラル
// （要素は全て1つだけで、最深部が葉の値）。`[このリテラル] * 個数`
// の形で使うと、目的の次元の型を持つ長さ1の雛形になる。雛形の
// 小配列は、読み取りループの中で枠ごとに新しい配列へ置き換えられ
// てから使われるので、共有されても書き換えられることはない
static Node* make_input_placeholder(Token* name, int mode, int depth) {
    if (depth == 0) {
        return make_input_fill(name, mode);
    }
    Node* inner = make_input_placeholder(name, mode, depth - 1);
    Node* lit = create_node(ND_ARRAY);
    register_array(1);
    lit->array_len = 1;
    lit->offset = locals->offset;
    lit->tok = name;
    lit->body = inner;
    return lit;
}

static Node* make_lvar_ref(int offset, Token* tok) {
    Node* node = create_node(ND_LVAR);
    node->offset = offset;
    node->tok = tok;
    return node;
}

// 添字列のパス `名前[i0][i1]...`（depth 個の添字）を新しいノードで作る
static Node* make_index_path(Token* name, int* var_offsets, int depth) {
    Node* node = create_ident_node(name);
    for (int d = 0; d < depth; d++) {
        Node* idx = create_node(ND_INDEX);
        idx->tok = name;
        idx->left_child = node;
        idx->right_child = make_lvar_ref(var_offsets[d], name);
        node = idx;
    }
    return node;
}

// 個数形の対象 `a[d1][d2]...[dk]` 1つ分の展開を文の並びの末尾に
// 足し、足した文の数を返す（step 72、step 75 で n 次元化）:
//   tmp1 = d1; ...; tmpk = dk
//   a = [雛形] * tmp1
//   @tmp1 { a[i1] = [雛形] * tmp2; @tmp2 { ... @tmpk { a[i1]...[ik] = 入力 } } }
// 個数式は一時スロットへ左から順に1回だけ評価する。葉の fill は
// 整数形（>>）が 0、文字列形（>>>）が ""。小配列は、ループの中で
// 枠ごとに `[雛形] * 個数` で新しい配列を作って割り当ててから中身を
// 読むため、行同士が同じ配列を共有することはない（prelude の
// read_ints2 と同じ作り方）。ループは make_for_count と同じ形の
// ND_FOR（__count_range 経由）。負数や0の振る舞いは、展開先の
// `[fill] * n` と回数ループの振る舞いそのまま
static int append_count_input(Node** cur, Token* start, int mode) {
    Token* name = NULL;
    Node* counts = parse_input_count(&name);
    int k = 0;
    for (Node* c = counts; c; c = c->next) {
        k++;
    }
    int* tmp_offsets = malloc(sizeof(int) * k);
    int* loop_offsets = malloc(sizeof(int) * k);
    int* var_offsets = malloc(sizeof(int) * k);
    int nstmts = 0;
    // tmp_j = 個数式（左から順に、評価は1回だけ）
    int j = 0;
    for (Node* c = counts; c; c = c->next, j++) {
        register_any();
        tmp_offsets[j] = locals->offset;
        Node* s = create_node(ND_ASSIGN);
        s->left_child = make_lvar_ref(tmp_offsets[j], name);
        s->right_child = c;
        s->tok = name;
        (*cur)->next = s;
        *cur = s;
        nstmts++;
    }
    // ループ変数と反復子のスロットを先に確保する
    for (j = 0; j < k; j++) {
        register_any();
        loop_offsets[j] = locals->offset;
        register_any();
        var_offsets[j] = locals->offset;
    }
    // a = [雛形] * tmp1 で束縛する
    Node* mul = create_binary_operation_node(
        OP_MUL, make_input_placeholder(name, mode, k),
        make_lvar_ref(tmp_offsets[0], name));
    mul->tok = name;
    Node* s2 = create_node(ND_ASSIGN);
    s2->left_child = bind_target_node(create_ident_node(name));
    s2->right_child = mul;
    s2->tok = name;
    (*cur)->next = s2;
    *cur = s2;
    nstmts++;
    // 入れ子のループを内側から組み立てる。最深部は要素への読み取り、
    // それより外の各段では、まずその枠へ新しい小配列を割り当てる
    Node* input = create_node(ND_INPUT);
    input->val = mode;
    input->tok = start;
    Node* inner = create_node(ND_ASSIGN);
    inner->left_child = make_index_path(name, var_offsets, k);
    inner->right_child = input;
    inner->tok = name;
    for (j = k - 1; j >= 0; j--) {
        Node* body_head = inner;
        if (j < k - 1) {
            Node* cmul = create_binary_operation_node(
                OP_MUL, make_input_placeholder(name, mode, k - j - 1),
                make_lvar_ref(tmp_offsets[j + 1], name));
            cmul->tok = name;
            Node* ca = create_node(ND_ASSIGN);
            ca->left_child = make_index_path(name, var_offsets, j + 1);
            ca->right_child = cmul;
            ca->tok = name;
            ca->next = inner;
            body_head = ca;
        }
        Node* block = create_node(ND_BLOCK);
        block->body = body_head;
        block->tok = name;
        Node* loop = create_node(ND_FOR);
        loop->tok = start;
        Node* range_call = create_node(ND_FUNCALL);
        range_call->name = "__count_range";
        range_call->len = 13;
        range_call->tok = name;
        range_call->args = make_lvar_ref(tmp_offsets[j], name);
        loop->init = range_call;
        loop->offset = loop_offsets[j];
        loop->left_child = make_lvar_ref(var_offsets[j], name);
        loop->then = block;
        inner = loop;
    }
    (*cur)->next = inner;
    *cur = inner;
    nstmts++;
    free(tmp_offsets);
    free(loop_offsets);
    free(var_offsets);
    return nstmts;
}

// 入力文 `>> 対象...` / `>>> 対象...`（step 72）。mode は ND_INPUT の
// val（0 = 整数 in、1 = 文字列トークン in_str）。対象はカンマ区切り・
// 空白区切りのどちらでも並べられ、混在もできる。空白区切りの継続は
// 同じ行の中だけにする（次の行の先頭の識別子を対象として飲み込ま
// ない）。対象は複数文に展開されることがある（個数形は次元の数＋2文）
// ため、文の並びを作って、1つならそのまま、複数ならブロックに包んで返す
static Node* stmt_input(Token* start, int mode) {
    Node head;
    head.next = NULL;
    Node* cur = &head;
    int nstmts = 0;
    do {
        if (input_target_is_count_form()) {
            nstmts += append_count_input(&cur, start, mode);
        } else {
            Node* target = parse_input_target();
            Node* input = create_node(ND_INPUT);
            input->val = mode;
            input->tok = start;
            Node* node = create_node(ND_ASSIGN);
            node->left_child = target;
            node->right_child = input;
            node->tok = target->tok;
            cur->next = node;
            cur = node;
            nstmts++;
        }
    } while (consume(",")
             || (token->kind == TK_IDENT && token->prev && token->line == token->prev->line));
    expect_stmt_end("文");
    if (nstmts == 1) {
        return head.next;
    }
    Node* block = create_node(ND_BLOCK);
    block->body = head.next;
    block->tok = start;
    return block;
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
    } else if (consume("?")) {
        // 文頭の `?` は if の糖衣（step 73）。条件・ブロック・elif /
        // else の連鎖まで通常の if 文と同一（stmt_if をそのまま使う）。
        // `?` が使えるのは文の頭だけで、式の中では定義しない
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
        if (consume("?")) {
            // `@? 条件` は while の糖衣（step 80）。`@ x` が「x 回／x の
            // 反復」の意味を持つため、条件ループには条件の印 `?` を足して
            // 判別できるようにした。読み方・本体・エラーは通常の while と
            // 完全に同じ（stmt_while をそのまま使う）
            return stmt_while(start);
        }
        if (token->kind == TK_SYMBOL && token->len == 1 && (token->str[0] == '{' || token->str[0] == ':')) {
            // `@ {}` / `@:` は while true の省略形（無限ループ）
            Node* node = create_node(ND_WHILE);
            node->tok = start;
            node->cond = create_node(ND_NUM);
            node->cond->val = 1;
            node->cond->tok = start;
            node->then = stmt_branch(start);
            return node;
        }
        return make_for_count(header_expr(), start);
    } else if (token->kind == TK_IDENT && token->next->kind == TK_SYMBOL && token->next->len == 1 && token->next->str[0] == '@') {
        Token* var = consume_ident();
        consume("@");
        return make_for_iter(var, header_expr(), start);
    } else if (consume("<<")) {
        // 出力の省略形: << 式（print と同じ）
        Node* node = create_node(ND_FUNCALL);
        node->name = "print";
        node->len = 5;
        node->tok = start;
        node->args = expr();
        expect_stmt_end("文");
        return node;
    } else if (consume(">>>")) {
        // 入力の省略形（文字列トークン）: >>> 対象 [, 対象 ...]
        return stmt_input(start, 1);
    } else if (consume(">>")) {
        // 入力の省略形: >> 対象 [, 対象 ...]
        // 対象が複数なら、各対象へ in を左から順に代入する文の並びに
        // 展開する（>> a, b は a = in; b = in; と同じ）。対象が1つなら
        // 従来どおり代入文そのものを返す
        return stmt_input(start, 0);
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
// それらを消費できる呼び出し側（stmt_if 等）がいる場所だけにする。
// 本体が `:` で始まればインデント形式のブロック（suite）として読む。
// hdr は頭（if/while/for/@ など）のトークンで、suite のインデントが
// 頭の行より深いことの判定に使う
static Node* stmt_branch(Token* hdr) {
    if (token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == ':') {
        Node* node = create_node(ND_BLOCK);
        node->tok = hdr;
        node->body = suite_body(hdr);
        return node;
    }
    Token* t = token;
    bool skipped_semi = false;
    while (t->kind == TK_SYMBOL && t->len == 1 && t->str[0] == ';') {
        t = t->next;
        skipped_semi = true;
    }
    if (skipped_semi
            && (t->kind == TK_EOF
                || (t->kind == TK_SYMBOL && t->len == 1 && (t->str[0] == '}' || t->str[0] == '|'))
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

// トークンのある行のインデント（行頭の空白の列数）。空白は1列、
// タブは次の8の倍数の列まで進める（Python と同じ数え方）。
// トークンの行番号から行頭を探すので、文字列トークン（str が
// ソースを指さない）でも安全に呼べる
static int line_indent_of(Token* tok) {
    char* p = src;
    for (int line = 1; line < tok->line; line++) {
        p = strchr(p, '\n');
        if (!p) {
            return 0;
        }
        p++;
    }
    int col = 0;
    while (*p == ' ' || *p == '\t') {
        if (*p == ' ') {
            col++;
        } else {
            col = (col / 8 + 1) * 8;
        }
        p++;
    }
    return col;
}

// トークンがその行の最初のトークンか。空行とコメントだけの行は
// トークンを生まないため、インデント判定に自然に影響しない
static bool token_starts_line(Token* tok) {
    return !tok->prev || tok->line > tok->prev->line;
}

// suite 内で文の解析に失敗したときのリカバリ（キャプチャ有効時）:
// 失敗した文の残りを読み飛ばし、suite の段以下へ戻る行頭か、
// 文の中で開いたブロックの閉じか、文の区切り `;` で止まる。
// 1文字も進まずに戻ると呼び出し側が同じ場所で失敗を繰り返すため、
// 段が同じ行頭で止まるのは、何か消費した後に限る
static void skip_suite_rest(int suite_indent) {
    int depth = 0;
    bool moved = false;
    while (token->kind != TK_EOF) {
        if (depth == 0 && token_starts_line(token)) {
            int ind = line_indent_of(token);
            if (ind < suite_indent) {
                return;
            }
            if (ind == suite_indent && moved) {
                return;
            }
        }
        if (token_is_char(token, '{')) {
            depth++;
        } else if (token_is_char(token, '}')) {
            if (depth == 0) {
                return;
            }
            depth--;
            token = token->next;
            moved = true;
            if (depth == 0) {
                return;
            }
            continue;
        } else if (depth == 0 && token_is_char(token, ';')) {
            token = token->next;
            return;
        }
        token = token->next;
        moved = true;
    }
}

// `:` で始まるインデント形式のブロック（suite）を読む。現在の
// トークンが `:` であることが前提。文の並び（next でつないだ
// 連鎖）を返す。hdr は頭のトークンで、suite の段が頭の行より
// 深いことの判定に使う。
// INDENT / DEDENT トークンは字句側で出さず、パーサ側でトークンの
// 行番号と行頭の空白から段を判定する。理由: spell では深さ0でも
// 演算子の後などで式が改行をまたいで続く（step 55）ため、論理行の
// 終わりは字句側だけでは決められず、文の完了を知っている stmt() に
// 合わせて段を見るほうが既存の書き方を壊さない。振る舞いは
// Python と同じで、段は suite ごとの再帰で持ち、dedent がどの
// 段にも合わない場合はエラーにする
static Node* suite_body(Token* hdr) {
    Token* colon = token;
    expect(":");
    // 1行形: `:` の同じ行に文を1つ書く
    if (token->line == colon->line) {
        if (token->kind == TK_EOF || token_is_char(token, '}') || token_is_char(token, ';')) {
            error_at(token_start(colon), token_end(colon), "`:`の後に文がありません");
        }
        return stmt();
    }
    if (token->kind == TK_EOF) {
        error_at(token_start(colon), token_end(colon), "`:`の後に文がありません");
    }
    int ref = line_indent_of(hdr);
    int suite_indent = line_indent_of(token);
    if (suite_indent <= ref) {
        char* actual = token_text(token);
        if (actual) {
            error_at(token_start(token), token_end(token), "インデントされたブロックが必要ですが、%sが見つかりました", actual);
        } else {
            error_at(token_start(token), token_end(token), "インデントされたブロックが必要ですが、ファイルの末尾に達しました");
        }
    }
    Node head;
    head.next = NULL;
    Node* cur = &head;
    int attempts = 0;
    while (true) {
        // 文の頭にある余分な `;` は読み飛ばす（文末の `;` は
        // expect_stmt_end が消費済みなので、ここで見るのは余りだけ）
        while (token_is_char(token, ';')) {
            token = token->next;
        }
        if (token->kind == TK_EOF || token_is_char(token, '}')) {
            break;
        }
        if (token_starts_line(token)) {
            int ind = line_indent_of(token);
            if (ind < suite_indent) {
                // dedent で suite が終わる。頭の行より深い段への
                // dedent は、どの suite の段にも一致しない中途半端な
                // 位置なのでエラーにする（外側に同じ段の suite が
                // ある場合は、頭の行の段以下へ戻る形になる）
                if (ind > ref) {
                    error_at(token_start(token), token_end(token), "インデントが揃っていません");
                }
                break;
            }
            if (ind > suite_indent) {
                error_at(token_start(token), token_end(token), "インデントが揃っていません");
            }
        }
        attempts++;
        if (!error_capture_enabled()) {
            cur->next = stmt();
            cur = cur->next;
            continue;
        }
        // キャプチャ有効時: 文の解析に失敗したら、その文を捨てて
        // suite の境界まで読み飛ばし、次の文から再開する
        // （parse_stmt_list のリカバリと同じ方針）
        jmp_buf jb;
        LVar* saved_locals = locals;
        LVar* saved_globals = globals;
        LambdaParse* saved_lambda = lambda_stack;
        int saved_depth = expr_depth;
        bool saved_anno = suppress_anno;
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
            suppress_anno = saved_anno;
            if (token->kind == TK_EOF) {
                break;
            }
            skip_suite_rest(suite_indent);
        }
    }
    if (attempts == 0) {
        error_at(token_start(colon), token_end(colon), "`:`の後に文がありません");
    }
    return head.next;
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
        bool saved_anno = suppress_anno;
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
            suppress_anno = saved_anno;
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

// 条件の直後に `:` も `{` も介さず `?` が来たらエラーにする（step 78）。
// `if a?[0] == 10:` のような v0 の安全添字のつもりの書き方が、条件を
// `a` だけにして、残りを入れ子の if 糖衣文として静かに飲み込んで
// しまうため。コロン形・波括弧形の正当な入れ子（`if c: ? d: ...` や
// `if c { ? d {} }`）は、stmt_branch が区切りを消費してから stmt() を
// 呼ぶので、ここには影響しない
static void reject_cond_question() {
    if (token_is_char(token, '?')) {
        error_at(token_start(token), token_end(token),
                 "条件の直後に`?`は書けません（`?`は文の頭でのみ if の糖衣として使えます）");
    }
}

// for のヘッダ（反復対象・回数）の直後に `[` を伴わない `?` が来たら
// エラーにする（step 81、reject_cond_question の for 版）。そのまま
// 進むと stmt_branch 経由の stmt() が `?` を文頭の if 糖衣として
// 飲み込み、ヘッダの意味を静かに変えてしまうため。安全添え字
// `x@a?[0]` は式として読まれるため、ここには当たらない
static void reject_for_question() {
    if (token_is_char(token, '?')) {
        error_at(token_start(token), token_end(token),
                 "for のヘッダの直後に`?`は書けません（`?`は文の頭でのみ if の糖衣として使えます）");
    }
}

static Node* stmt_if(Token* kw) {
    Node* node;
    node = create_node(ND_IF);
    node->tok = kw;
    node->cond = cond_expr();
    reject_cond_question();
    node->then = stmt_branch(kw);
    Token* else_tok = token;
    if (consume_keyword(KW_ELSE)) {
        node->els = stmt_branch(else_tok);
    } else if (token->kind == TK_KEYWORD && token->keyword == KW_ELIF) {
        Token* elif_tok = token;
        token = token->next;
        node->els = stmt_if(elif_tok);
    } else if (token_is_char(token, '|')) {
        // step 74: `|` は else / elif の糖衣（spell_v0 から移植）。
        // 直後がブロック開始（`:` か `{`）なら else、式が続けば elif
        // として読み、キーワード形と完全に同じ意味にする
        Token* pipe_tok = token;
        token = token->next;
        if (token_is_char(token, ':') || token_is_char(token, '{')) {
            node->els = stmt_branch(pipe_tok);
        } else {
            node->els = stmt_if(pipe_tok);
        }
    }
    return node;
}

static Node* stmt_while(Token* kw) {
    Node* node;
    node = create_node(ND_WHILE);
    node->tok = kw;
    node->cond = cond_expr();
    reject_cond_question();
    node->then = stmt_branch(kw);
    return node;
}

static Node* make_for_iter(Token* var, Node* init, Token* kw) {
    reject_for_question();
    Node* node = create_node(ND_FOR);
    node->tok = kw;
    node->init = init;
    register_any();
    node->offset = locals->offset;
    // step 98: 借用モードの添え字スロットも並べて確保しておく
    register_any();
    node->idx_offset = locals->offset;
    node->left_child = bind_target_node(create_ident_node(var));
    node->then = stmt_branch(kw);
    Token* else_tok = token;
    if (consume_keyword(KW_ELSE) || consume("|")) {
        // step 74: for...else でも `|` を else として受ける（v0 と同じ）
        register_any();
        node->val = locals->offset;
        node->els = stmt_branch(else_tok);
    }
    return node;
}

static Node* make_for_count(Node* count, Token* kw) {
    reject_for_question();
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
    node->then = stmt_branch(kw);
    Token* else_tok = token;
    if (consume_keyword(KW_ELSE) || consume("|")) {
        // step 74: for...else でも `|` を else として受ける（v0 と同じ）
        register_any();
        node->val = locals->offset;
        node->els = stmt_branch(else_tok);
    }
    return node;
}

// if / elif / while の条件を読む（step 69: 条件の括弧は任意）。
// 条件は通常の完全な式として読み、終わりはブロックの開始（`{` か
// `:`）で判断する。括弧形は primary の括弧式としてそのまま読める
// ため、従来の書き方もそのまま動く。for の頭と同じく、直後の `:`
// が suite の区切りと競合するため、型注釈の `:` は「型 = 」と
// 続く場合だけ注釈として扱う（expr.c の suppress_anno）
static Node* cond_expr() {
    bool saved = suppress_anno;
    suppress_anno = true;
    Node* node = expr();
    suppress_anno = saved;
    return node;
}

// for の頭の式（反復対象・回数）を読む。その直後の `:` は suite の
// 区切りになり得るため、式の中の型注釈は「型 = 」と続く場合だけ
// 注釈として扱う（expr.c の suppress_anno）
static Node* header_expr() {
    bool saved = suppress_anno;
    suppress_anno = true;
    Node* node = expr();
    suppress_anno = saved;
    return node;
}

static Node* stmt_for(Token* kw) {
    if (token->kind == TK_IDENT && token->next->kind == TK_KEYWORD && token->next->keyword == KW_IN) {
        Token* var = consume_ident();
        consume_keyword(KW_IN);
        return make_for_iter(var, header_expr(), kw);
    }
    return make_for_count(header_expr(), kw);
}

// 「(引数, ...)」の中身を読む。開き括弧は消費済み。引数の数を返す
static int parse_params(Type* param_types[6]) {
    int nparams = 0;
    if (!consume(")")) {
        do {
            Token* ptok = consume_ident();
            if (!ptok) {
                // 名前の無いパラメータや、リストの途中での EOF。ここで
                // 止めないと register_lvar が NULL を読んで落ちる（step 87）
                char* actual = token_text(token);
                if (actual) {
                    error_at(token_start(token), token_end(token), "引数名が必要ですが、%sが見つかりました", actual);
                } else {
                    error_at(token_start(token), token_end(token), "引数名が必要ですが、ファイルの末尾に達しました");
                }
            }
            register_lvar(ptok);
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
    Node* body;
    if (consume("{")) {
        body = parse_stmt_list();
    } else if (token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == ':') {
        body = suite_body(tok);
    } else {
        expect("{");
        body = NULL;
    }
    Function* func = calloc(1, sizeof(Function));
    func->name = name;
    func->len = len;
    func->name_tok = tok;
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
            Token* p = consume_ident();
            if (!p) {
                char* actual = token_text(token);
                if (actual) {
                    error_at(token_start(token), token_end(token), "引数名が必要ですが、%sが見つかりました", actual);
                } else {
                    error_at(token_start(token), token_end(token), "引数名が必要ですが、ファイルの末尾に達しました");
                }
            }
            register_lvar(p);
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

// クラス本体を `:` ＋インデント形式で読む。現在のトークンが `:`
// であることが前提。parsing_klass は呼び出し側（class_decl）が
// 設定済み。文の suite（suite_body）と対になる、メンバ版の読み取り
static void class_colon_body(Function** cur, Class* klass, int* offset, Token* hdr) {
    Token* colon = token;
    expect(":");
    // 1行形: `:` の同じ行にメンバを並べる
    if (token->line == colon->line) {
        int attempts = 0;
        while (token->kind != TK_EOF && !token_is_char(token, '}')) {
            attempts++;
            class_member(cur, klass, offset);
            if (token_starts_line(token)) {
                break;
            }
        }
        if (attempts == 0) {
            error_at(token_start(colon), token_end(colon), "`:`の後にメンバがありません");
        }
        return;
    }
    if (token->kind == TK_EOF) {
        error_at(token_start(colon), token_end(colon), "`:`の後にメンバがありません");
    }
    int ref = line_indent_of(hdr);
    int member_indent = line_indent_of(token);
    if (member_indent <= ref) {
        char* actual = token_text(token);
        if (actual) {
            error_at(token_start(token), token_end(token), "インデントされたブロックが必要ですが、%sが見つかりました", actual);
        } else {
            error_at(token_start(token), token_end(token), "インデントされたブロックが必要ですが、ファイルの末尾に達しました");
        }
    }
    int attempts = 0;
    while (true) {
        if (token->kind == TK_EOF || token_is_char(token, '}')) {
            break;
        }
        if (token_starts_line(token)) {
            int ind = line_indent_of(token);
            if (ind < member_indent) {
                // suite_body と同じく、頭の行より深い段への
                // dedent はどの段にも一致しないためエラーにする
                if (ind > ref) {
                    error_at(token_start(token), token_end(token), "インデントが揃っていません");
                }
                break;
            }
            if (ind > member_indent) {
                error_at(token_start(token), token_end(token), "インデントが揃っていません");
            }
        }
        attempts++;
        if (!error_capture_enabled()) {
            class_member(cur, klass, offset);
            continue;
        }
        // キャプチャ有効時: メンバの解析に失敗したら、そのメンバを
        // 捨てて境界まで読み飛ばし、次のメンバから再開する
        jmp_buf jb;
        LVar* saved_locals = locals;
        LVar* saved_globals = globals;
        LambdaParse* saved_lambda = lambda_stack;
        int saved_depth = expr_depth;
        bool saved_anno = suppress_anno;
        jmp_buf* saved_recover = error_capture_set_recover(&jb);
        if (setjmp(jb) == 0) {
            class_member(cur, klass, offset);
            error_capture_set_recover(saved_recover);
        } else {
            error_capture_set_recover(saved_recover);
            locals = saved_locals;
            globals = saved_globals;
            lambda_stack = saved_lambda;
            expr_depth = saved_depth;
            suppress_anno = saved_anno;
            if (token->kind == TK_EOF) {
                break;
            }
            skip_suite_rest(member_indent);
        }
    }
    if (attempts == 0) {
        error_at(token_start(colon), token_end(colon), "`:`の後にメンバがありません");
    }
}

static void class_decl(Function** cur) {
    consume_keyword(KW_CLASS);
    Token* tok = consume_ident();
    if (!tok) {
        error_at(token_start(token), token_end(token), "クラス名が必要ですが、%sが見つかりました", token_text(token));
    }
    Class* klass = register_class(tok->str, tok->len);
    // 同名クラスの再定義はエラー（step 91）。先行スキャンの仮登録では
    // declared は立たないので、ここで初めて宣言されたものだけが通る。
    // prelude のクラスも宣言済みのため、利用者が同名を再定義しても
    // ここで止まる。最初の宣言が同じソース内にあれば行も併記する
    if (klass->declared) {
        Token* first = klass->decl_tok;
        bool same_src = false;
        if (first && first->str && src) {
            uintptr_t p = (uintptr_t)first->str;
            uintptr_t lo = (uintptr_t)src;
            uintptr_t hi = lo + (uintptr_t)strlen(src);
            same_src = p >= lo && p + (uintptr_t)first->len <= hi;
        }
        if (same_src) {
            error_at(token_start(tok), token_end(tok), "クラス`%.*s`は既に定義されています（%d行目）", tok->len, tok->str, first->line);
        } else {
            error_at(token_start(tok), token_end(tok), "クラス`%.*s`は既に定義されています", tok->len, tok->str);
        }
    }
    klass->declared = true;
    klass->decl_tok = tok;
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
    int offset = 0;
    parsing_klass = klass;
    if (token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == ':') {
        class_colon_body(cur, klass, &offset, tok);
        parsing_klass = NULL;
        return;
    }
    expect("{");
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
        bool saved_anno = suppress_anno;
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
            suppress_anno = saved_anno;
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
    // 波括弧の深さ0にある def 名と `:` 形の関数名だけを対象にする。
    // `:` ＋インデント形式のクラスは波括弧を開かないため、その本体
    // （メソッド定義）まで深さ0に見えてしまう。クラス行のインデントを
    // 覚えておき、本体の中では登録しない（波括弧形式のクラスで
    // メソッド名が登録されないのと同じ扱いにする）
    int depth = 0;
    int class_indent = -1;
    for (Token* t = head_t; t->kind != TK_EOF; t = t->next) {
        if (class_indent >= 0 && t->prev && t->line > t->prev->line && line_indent_of(t) <= class_indent) {
            class_indent = -1;
        }
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
        if (class_indent >= 0) {
            continue;
        }
        if (t->kind == TK_KEYWORD && t->keyword == KW_CLASS && t->next->kind == TK_IDENT) {
            // ヘッダ（名前と型パラメータ）の直後が `{` なら波括弧形式、
            // `:` ならインデント形式。ヘッダの途中では名前・`<`・
            // `>`・`,` しか現れないので、それ以外が来たら諦める
            for (Token* u = t->next; u->kind != TK_EOF; u = u->next) {
                if (token_is_symbol(u, "{")) {
                    break;
                }
                if (token_is_symbol(u, ":")) {
                    class_indent = line_indent_of(t);
                    break;
                }
                if (u->kind != TK_IDENT && !token_is_symbol(u, "<") && !token_is_symbol(u, ">") && !token_is_symbol(u, ",")) {
                    break;
                }
            }
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
        bool saved_anno = suppress_anno;
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
            suppress_anno = saved_anno;
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
    bool has_main = false;
    for (Function* fn = head.next; fn; fn = fn->next) {
        if (!fn->klass && fn->len == 4 && !memcmp(fn->name, "main", 4)) {
            has_main = true;
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

// 結合済みのプログラム全体に入口（main）が無い場合に、空の main を
// 足して返す（step 88）。トップレベル文も `def main` も無いプログラ
// ム（定義だけのファイルや空ファイル）は、入口が無いとリンクできな
// いため。program() は prelude とユーザーコードで別々に呼ばれるの
// で、合成は結合後の全体に対して呼び出し側（main.c）が1回だけ行う
Function* ensure_entry_point(Function* prog) {
    for (Function* fn = prog; fn; fn = fn->next) {
        if (!fn->klass && fn->len == 4 && !memcmp(fn->name, "main", 4)) {
            return prog;
        }
    }
    Node* ret = create_node(ND_RETURN);
    ret->left_child = create_number_node(0);
    Node* block = create_node(ND_BLOCK);
    block->body = ret;
    Function* init = calloc(1, sizeof(Function));
    init->body = block;
    init->stack_size = 0;
    init->name = "main";
    init->len = 4;
    init->next = prog;
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
