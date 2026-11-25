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

static void register_any() {
    LVar* lvar = calloc(1, sizeof(LVar));
    lvar->offset = locals ? locals->offset + 8 * (locals->array_len + 1): 8;
    lvar->next = locals;
    locals = lvar;
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
}

Node* create_ident_node(Token* tok) {
    if (!is_top_level) {
        LVar* lvar = find_lvar(tok);
        if (lvar) {
            Node* node = create_node(ND_LVAR);
            node->name = tok->str;
            node->len = tok->len;
            node->offset = lvar->offset;
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
            return node;
        }
        register_gvar(tok);
    }
    Node* node = create_node(ND_GVAR);
    node->name = tok->str;
    node->len = tok->len;
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
    return node;
}

static Node* stmt_block();
static Node* stmt_return();
static Node* stmt_if();
static Node* stmt_while();
static Node* stmt_for();

static Node* stmt() {
    if (consume("{")) {
        return stmt_block();
    } else if (consume_keyword(KW_RETURN)) {
        return stmt_return();
    } else if (consume_keyword(KW_IF)) {
        return stmt_if();
    } else if (consume_keyword(KW_WHILE)) {
        return stmt_while();
    } else if (consume_keyword(KW_FOR)) {
        return stmt_for();
    } else if (consume_keyword(KW_BREAK)) {
        Node* node = create_node(ND_BREAK);
        expect(";");
        return node;
    } else if (consume_keyword(KW_CONTINUE)) {
        Node* node = create_node(ND_CONTINUE);
        expect(";");
        return node;
    } else {
        Node* node = expr();
        expect(";");
        return node;
    }
}

static Node* stmt_block() {
    Node* node;
    node = create_node(ND_BLOCK);
    Node head;
    head.next = NULL;
    Node* cur = &head;
    while (!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
    }
    node->body = head.next;
    return node;
}

static Node* stmt_return() {
    Node* node;
    node = create_node(ND_RETURN);
    node->left_child = expr();
    expect(";");
    return node;
}

static Node* stmt_if() {
    Node* node;
    node = create_node(ND_IF);
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();
    if (consume_keyword(KW_ELSE)) {
        node->els = stmt();
    }
    return node;
}

static Node* stmt_while() {
    Node* node;
    node = create_node(ND_WHILE);
    expect("(");
    node->cond = expr();
    expect(")");
    node->then = stmt();
    return node;
}

static Node* stmt_for() {
    Node* node;
    node = create_node(ND_FOR);
    expect("(");
    if (!consume(";")) {
        node->init = expr();
        expect(";");
    }
    if (!consume(";")) {
        node->cond = expr();
        expect(";");
    }
    if (!consume(")")) {
        node->inc = expr();
        expect(")");
    }
    node->then = stmt();
    return node;
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
    Node head;
    head.next = NULL;
    Node* cur = &head;
    while (!consume("}")) {
        cur->next = stmt();
        cur = cur->next;
    }
    Function* func = calloc(1, sizeof(Function));
    func->name = name;
    func->len = len;
    func->body = head.next;
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
        Token* tparam = consume_ident();
        klass->tparam = tparam->str;
        klass->tparam_len = tparam->len;
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

Function* program() {
    Token* head_t = token;
    for (Token* t = head_t; t->kind != TK_EOF; t = t->next) {
        if (t->kind == TK_KEYWORD && t->keyword == KW_CLASS && t->next->kind == TK_IDENT) {
            register_class(t->next->str, t->next->len);
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
