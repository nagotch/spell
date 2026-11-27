#include "spell.h"
#include "parse_internal.h"

static Node* array_literal();

// 「(引数, ...)」の中身を読む。開き括弧は消費済み。引数なしなら NULL
static bool suppress_multi = false;

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
    if (token->kind == TK_STR) {
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
    } else if (ident_token && token->kind == TK_SYMBOL && token->len == 1 && token->str[0] == '(' && ident_is_value_call(ident_token)) {
        // 変数に収まった関数値の呼び出しは、名前呼び出しではなく値経由にする。
        // `(` は後置ループ側で ND_VCALL として消費する
        node = create_ident_node(ident_token);
    } else if (ident_token && consume("(")) {
        node = create_node(ND_FUNCALL);
        node->name = ident_token->str;
        node->len = ident_token->len;
        node->tok = ident_token;
        node->args = parse_args();
    } else if (ident_token) {
        node = create_ident_node(ident_token);
    } else if (consume("(")) {
        expr_depth++;
        node = expr();
        expr_depth--;
        expect(")");
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
            assign_node->op.binop = inc ? OP_ADD : OP_SUB;
            assign_node->tok = postfix_tok;
            assign_node->left_child = node;
            assign_node->right_child = create_number_node(1);
            node = assign_node;
            continue;
        }
        if (consume("[")) {
            expr_depth++;
            Node* right_child = expr();
            expr_depth--;
            expect("]");
            Node* new_node = create_node(ND_INDEX);
            new_node->tok = postfix_tok;
            new_node->left_child = node;
            new_node->right_child = right_child;
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
    } else if (consume("!")) {
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
    if (!at_expr_line_break() && consume("&&")) {
        node = create_binary_operation_node(OP_AND, node, and());
        node->tok = op_tok;
    }
    return node;
}

static Node* or() {
    Node* node = and();
    Token* op_tok = token;
    if (!at_expr_line_break() && consume("||")) {
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
    if (consume(":")) {
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
        assign_node->op.binop = OP_ADD;
        assign_node->tok = node->tok;
        assign_node->left_child = node;
        assign_node->right_child = assign();
        node = assign_node;
    } else if (consume("-=")) {
        Node* assign_node = create_node(ND_OPASSIGN);
        assign_node->op.binop = OP_SUB;
        assign_node->tok = node->tok;
        assign_node->left_child = node;
        assign_node->right_child = assign();
        node = assign_node;
    } else if (consume("*=")) {
        Node* assign_node = create_node(ND_OPASSIGN);
        assign_node->op.binop = OP_MUL;
        assign_node->tok = node->tok;
        assign_node->left_child = node;
        assign_node->right_child = assign();
        node = assign_node;
    } else if (consume("/=")) {
        Node* assign_node = create_node(ND_OPASSIGN);
        assign_node->op.binop = OP_DIV;
        assign_node->tok = node->tok;
        assign_node->left_child = node;
        assign_node->right_child = assign();
        node = assign_node;
    } else if (consume("%=")) {
        Node* assign_node = create_node(ND_OPASSIGN);
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
        Node* len_expr = expr();
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
// なるため、>> の前半だけを消費して後半を > として残す
static void expect_type_end() {
    if (token->kind == TK_SYMBOL && token->len == 2 && !memcmp(token->str, ">>", 2)) {
        token->str++;
        token->len = 1;
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
