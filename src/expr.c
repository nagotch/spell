#include "spell.h"
#include "parse_internal.h"

static Node* array_literal();

static Node* primary() {
    Node* node;
    Token* ident_token = consume_ident();
    if (token->kind == TK_STR) {
        node = char_array();
    } else if (!ident_token && consume("[")) {
        node = array_literal();
    } else if (ident_token && consume("(")) {
        node = create_node(ND_FUNCALL);
        node->name = ident_token->str;
        node->len = ident_token->len;
        if (!consume(")")) {
            Node head;
            head.next = NULL;
            Node* cur = &head;
            do {
                cur->next = expr();
                cur = cur->next;
            } while (consume(","));
            node->args = head.next;
            expect(")");
        }
    } else if (ident_token) {
        node = create_ident_node(ident_token);
    } else if (consume("(")) {
        node = expr();
        expect(")");
    } else {
        long num = expect_number();
        node = create_number_node(num);
    }
    while (true) {
        if (consume("[")) {
            Node* right_child = expr();
            expect("]");
            Node* new_node = create_node(ND_INDEX);
            new_node->left_child = node;
            new_node->right_child = right_child;
            node = new_node;
        } else if (consume(".")) {
            ident_token = consume_ident();
            if (ident_token && ident_token->len == 3 && !memcmp(ident_token->str, "len", 3)) {
                Node* new_node = create_node(ND_LEN);
                new_node->left_child = node;
                node = new_node;
            } else {
                if (ident_token) {
                    Node* new_node = create_node(ND_MEMBER);
                    new_node->left_child = node;
                    new_node->name = ident_token->str;
                    new_node->len = ident_token->len;
                    node = new_node;
                }
                char* actual = token_text(token);
                if (actual) {
                    error_at(token_start(token), token_end(token), "配列では`len`だけが使えますが、%sが指定されています", actual);
                } else {
                    error_at(token_start(token), token_end(token), "配列では`len`だけが使えますが、ファイルの末尾に達しました");
                }
            }
        } else {
            break;
        }
    }
    return node;
}

static Node* unary() {
    if (consume("+")) {
        return primary();
    } else if (consume("-")) {
        Node* node = create_binary_operation_node(OP_SUB, create_number_node(0), primary());
        return node;
    } else if (consume("*")) {
        Node* node = create_node(ND_UNARY);
        node->op.unop = OP_DEREF;
        node->left_child = unary();
        return node;
    } else if (consume("&")) {
        Node* node = create_node(ND_UNARY);
        node->op.unop = OP_ADDR;
        node->left_child = unary();
        return node;
    } else if (consume("!")) {
        Node* node = create_node(ND_UNARY);
        node->op.unop = OP_NOT;
        node->left_child = unary();
        return node;
    }
    return primary();
}

static Node* mul() {
    Node* node = unary();
    while (true) {
        if (consume("*")) {
            node = create_binary_operation_node(OP_MUL, node, unary());
        } else if (consume("/")) {
            node = create_binary_operation_node(OP_DIV, node, unary());
        } else if (consume("%")) {
            node = create_binary_operation_node(OP_MOD, node, unary());
        } else {
            break;
        }
    }
    return node;
}

static Node* add() {
    Node* node = mul();
    while (true) {
        if (consume("+")) {
            node = create_binary_operation_node(OP_ADD, node, mul());
        } else if (consume("-")) {
            node = create_binary_operation_node(OP_SUB, node, mul());
        } else {
            break;
        }
    }
    return node;
}

static Node* relational() {
    Node* node = add();
    while (true) {
        if (consume("<=")) {
            node = create_binary_operation_node(OP_LE, node, add());
        } else if (consume(">=")) {
            node = create_binary_operation_node(OP_LE, add(), node);
        } else if (consume("<")) {
            node = create_binary_operation_node(OP_LT, node, add());
        } else if (consume(">")) {
            node = create_binary_operation_node(OP_LT, add(), node);
        } else {
            break;
        }
    }
    return node;
}

static Node* equality() {
    Node* node = relational();
    while (true) {
        if (consume("==")) {
            node = create_binary_operation_node(OP_EQ, node, relational());
        } else if (consume("!=")) {
            node = create_binary_operation_node(OP_NE, node, relational());
        } else {
            break;
        }
    }
    return node;
}

static Node* and() {
    Node* node = equality();
    if (consume("&&")) {
        node = create_binary_operation_node(OP_AND, node, and());
    }
    return node;
}

static Node* or() {
    Node* node = and();
    if (consume("||")) {
        node = create_binary_operation_node(OP_OR, node, or());
    }
    return node;
}

static Node* assign() {
    Node* node = or();
    Token* error_token = token;
    if (consume(":")) {
        Node* assign_node = create_node(ND_ASSIGN);
        if (node->kind != ND_LVAR && node->kind != ND_GVAR) {
            error_at(token_start(error_token), token_end(error_token), "型注釈は変数にだけ付けられます");
        }
        assign_node->anno = parse_type();
        expect("=");
        assign_node->left_child = node;
        assign_node->right_child = assign();
        node = assign_node;
    } else if (consume("=")) {
        Node* assign_node = create_node(ND_ASSIGN);
        assign_node->left_child = node;
        assign_node->right_child = assign();
        node = assign_node;
    } else if (consume("+=")) {
        Node* assign_node = create_node(ND_OPASSIGN);
        assign_node->op.binop = OP_ADD;
        assign_node->left_child = node;
        assign_node->right_child = assign();
        node = assign_node;
    } else if (consume("-=")) {
        Node* assign_node = create_node(ND_OPASSIGN);
        assign_node->op.binop = OP_SUB;
        assign_node->left_child = node;
        assign_node->right_child = assign();
        node = assign_node;
    } else if (consume("*=")) {
        Node* assign_node = create_node(ND_OPASSIGN);
        assign_node->op.binop = OP_MUL;
        assign_node->left_child = node;
        assign_node->right_child = assign();
        node = assign_node;
    } else if (consume("/=")) {
        Node* assign_node = create_node(ND_OPASSIGN);
        assign_node->op.binop = OP_DIV;
        assign_node->left_child = node;
        assign_node->right_child = assign();
        node = assign_node;
    } else if (consume("%=")) {
        Node* assign_node = create_node(ND_OPASSIGN);
        assign_node->op.binop = OP_MOD;
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
    if (consume("]")) {
        node = create_node(ND_ARRAY);
        register_array(0);
        node->array_len = 0;
        node->body = NULL;
        node->offset = locals->offset;
        return node;
    }
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
    expect("]");
    return node;
}

Type* parse_type() {
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
    Type* anno = NULL;
    char* type_name[] = {
        "int",
        "bool",
        "char",
    };
    Type* types[] = {
        &ty_int,
        &ty_bool,
        &ty_char,
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
            anno = klass->type;
        }
    }
    if (!anno) {
        error_at(token_start(ident), token_end(ident), "型`%.*s`は定義されていません", ident->len, ident->str);
    }
    return anno;
}
