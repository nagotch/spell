#include "spell.h"
#include "parse_internal.h"

LVar* locals = NULL;
LVar* globals = NULL;
static Class* classes;
bool is_top_level = false;

static LVar* find_lvar(Token* tok);
static LVar* find_gvar(Token* tok);

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
    locals->array_len = array_len;
    note_top_used(locals->offset, array_len);
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
    }
    LVar* gvar = find_gvar(tok);
    if (!gvar) {
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

static Node* stmt() {
    Token* start = token;
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
               && token->next->kind == TK_SYMBOL && token->next->len == 1 && token->next->str[0] == ';') {
        // `!;` は break の省略形
        consume("!");
        consume(";");
        Node* node = create_node(ND_BREAK);
        node->tok = start;
        return node;
    } else if (consume("^")) {
        // `^;` は continue の省略形
        Node* node = create_node(ND_CONTINUE);
        node->tok = start;
        expect(";");
        return node;
    } else if (consume("@")) {
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
        expect(";");
        return node;
    } else if (consume(">>")) {
        // 入力の省略形: >> 対象
        Node* target = expr();
        expect(";");
        Node* input = create_node(ND_INPUT);
        input->val = 0;
        input->tok = start;
        Node* node = create_node(ND_ASSIGN);
        node->left_child = target;
        node->right_child = input;
        node->tok = target->tok;
        return node;
    } else if (consume_keyword(KW_PUT)) {
        Node* node = create_node(ND_PUT);
        node->tok = start;
        node->args = parse_put_args();
        expect(";");
        return node;
    } else if (consume_keyword(KW_YN)) {
        Node* node = create_node(ND_FUNCALL);
        node->name = "print_yn";
        node->len = 8;
        node->tok = start;
        node->args = expr();
        expect(";");
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
        expect(";");
        Node* call = create_node(ND_FUNCALL);
        call->name = "read_ints_dec";
        call->len = 13;
        call->args = n;
        Node* node = create_node(ND_ASSIGN);
        node->left_child = create_ident_node(var);
        node->right_child = call;
        node->tok = var;
        return node;
    } else if (consume_keyword(KW_BREAK)) {
        Node* node = create_node(ND_BREAK);
        node->tok = start;
        expect(";");
        return node;
    } else if (consume_keyword(KW_CONTINUE)) {
        Node* node = create_node(ND_CONTINUE);
        node->tok = start;
        expect(";");
        return node;
    } else {
        Node* node = expr();
        expect(";");
        return node;
    }
}

// 「}」まで文を読み、連結リストで返す
static Node* parse_stmt_list() {
    Node head;
    head.next = NULL;
    Node* cur = &head;
    while (!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
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
    expect(";");
    return node;
}

static Node* stmt_if(Token* kw) {
    Node* node;
    node = create_node(ND_IF);
    node->tok = kw;
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();
    if (consume_keyword(KW_ELSE)) {
        node->els = stmt();
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
    node->cond = expr();
    expect(")");
    node->then = stmt();
    return node;
}

static Node* make_for_iter(Token* var, Node* init, Token* kw) {
    Node* node = create_node(ND_FOR);
    node->tok = kw;
    node->init = init;
    register_any();
    node->offset = locals->offset;
    node->left_child = create_ident_node(var);
    node->then = stmt();
    if (consume_keyword(KW_ELSE)) {
        register_any();
        node->val = locals->offset;
        node->els = stmt();
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
    node->then = stmt();
    if (consume_keyword(KW_ELSE)) {
        register_any();
        node->val = locals->offset;
        node->els = stmt();
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
        if (token->kind == TK_KEYWORD && token->keyword == KW_DEF) {
            (*cur)->next = function(klass);
            *cur = (*cur)->next;
        } else {
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
            field->offset = offset;
            offset += 8;
            field->next = klass->fields;
            klass->fields = field;
            consume(";");
        }
    }
    parsing_klass = NULL;
}

static bool token_is_symbol(Token* t, char* symbol) {
    int len = strlen(symbol);
    return t->kind == TK_SYMBOL && t->len == len && !memcmp(symbol, t->str, t->len);
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
    token = head_t;

    Function head;
    head.next = NULL;
    Function* cur = &head;
    Node stmt_head;
    stmt_head.next = NULL;
    Node* stmt_cur = &stmt_head;
    while (!at_eof()) {
        if (token->kind == TK_KEYWORD && token->keyword == KW_CLASS) {
            class_decl(&cur);
        } else if (token->kind == TK_KEYWORD && token->keyword == KW_DEF || consume(":")) {
            cur->next = function(NULL);
            cur = cur->next;
        } else {
            is_top_level = true;
            stmt_cur->next = stmt();
            stmt_cur = stmt_cur->next;
            is_top_level = false;
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
