#include "spell.h"

Node* code[100];
LVar* locals = NULL;

bool consume(char* symbol) {
    int len = strlen(symbol);
    if (token -> kind == TK_SYMBOL && token -> len == len && !memcmp(symbol, token -> str, token -> len)) {
        token = token -> next;
        return true;
    }
    return false;
}

Token* consume_ident() {
    if (token -> kind == TK_IDENT) {
        Token* pre_token = token;
        token = token -> next;
        return pre_token;
    }
    return NULL;
}

bool consume_keyword(TokenKind kind) {
    if (token -> kind == kind) {
        token = token -> next;
        return true;
    }
    return false;
}

void expect(char* symbol) {
    int len = strlen(symbol);
    if (token -> kind == TK_SYMBOL && token -> len == len && !memcmp(symbol, token -> str, token -> len)) {
        token = token -> next;
        return;
    }
    fprintf(stderr, "予期されないToken\n");
    exit(1);
}

long expect_number() {
    if (token -> kind == TK_NUM) {
        long num = token -> val;
        token = token -> next;
        return num;
    }
    fprintf(stderr, "予期されない数値\n");
    exit(1);
}

bool at_eof() {
    if (token -> kind == TK_EOF) {
        return true;
    }
    return false;
}

Node* create_binary_operation_node(NodeKind kind, Node* left_child, Node* right_child) {
    Node* new_node_ptr = calloc(1, sizeof(Node));
    new_node_ptr -> kind = kind;
    new_node_ptr -> left_child = left_child;
    new_node_ptr -> right_child = right_child;
    return new_node_ptr;
}

Node* create_number_node(long num) {
    Node* num_node_ptr = calloc(1, sizeof(Node));
    num_node_ptr -> kind = ND_NUM;
    num_node_ptr -> val = num;
    return num_node_ptr;
}

Node* create_ident_node(Token* tok) {
    LVar* var = find_lvar(tok);
    Node* ident_node_ptr = calloc(1, sizeof(Node));
    ident_node_ptr -> kind = ND_LVAR;
    if (var) {
        ident_node_ptr -> offset = var -> offset;
    } else {
        LVar* lvar = calloc(1, sizeof(LVar));
        lvar -> name = tok -> str;
        lvar -> len = tok -> len;
        int offset;
        if (locals) {
            offset = locals -> offset + 8;
        } else {
            offset = 8;
        }
        lvar -> offset = offset;
        ident_node_ptr -> offset = offset;
        lvar -> next = locals;
        locals = lvar;
    }
    return ident_node_ptr;
}

Node* primary() {
    Token* ident_token = consume_ident();
    if (ident_token != NULL) {
        Node* node = create_ident_node(ident_token);
        return node;
    } else if (consume("(")) {
        Node* node = expr();
        expect(")");
        return node;
    } else {
        long num = expect_number();
        Node* node = create_number_node(num);
        return node;
    }
}

Node* unary() {
    if (consume("+")) {
        return primary();
    } else if (consume("-")) {
        Node* node = create_binary_operation_node(ND_SUB, create_number_node(0), primary());
        return node;
    }
    return primary();
}

Node* mul() {
    Node* node = unary();
    while (true) {
        if (consume("*")) {
            node = create_binary_operation_node(ND_MUL, node, unary());
        } else if (consume("/")) {
            node = create_binary_operation_node(ND_DIV, node, unary());
        } else {
            break;
        }
    }
    return node;
}

Node* add() {
    Node* node = mul();
    while (true) {
        if (consume("+")) {
            node = create_binary_operation_node(ND_ADD, node, mul());
        } else if (consume("-")) {
            node = create_binary_operation_node(ND_SUB, node, mul());
        } else {
            break;
        }
    }
    return node;
}

Node* relational() {
    Node* node = add();
    while (true) {
        if (consume("<=")) {
            node = create_binary_operation_node(ND_LE, node, add());
        } else if (consume(">=")) {
            node = create_binary_operation_node(ND_LE, add(), node);
        } else if (consume("<")) {
            node = create_binary_operation_node(ND_LT, node, add());
        } else if (consume(">")) {
            node = create_binary_operation_node(ND_LT, add(), node);
        } else {
            break;
        }
    }
    return node;
}

Node* equality() {
    Node* node = relational();
    while (true) {
        if (consume("==")) {
            node = create_binary_operation_node(ND_EQ, node, relational());
        } else if (consume("!=")) {
            node = create_binary_operation_node(ND_NE, node, relational());
        } else {
            break;
        }
    }
    return node;
}

Node* assign() {
    Node* node = equality();
    if (consume("=")) {
        node = create_binary_operation_node(ND_ASSIGN, node, assign());
    }
    return node;
}

Node* expr() {
    return assign();
}

Node* stmt() {
    Node* node;
    if (consume_keyword(TK_RETURN)) {
        token = token -> next;
        node = calloc(1, sizeof(Node));
        node -> kind = ND_RETURN;
        node -> left_child = expr();
    } else if (consume_keyword(TK_IF)) {
        node = calloc(1, sizeof(Node));
        node -> kind = ND_IF;
        expect("(");
        node -> cond = expr();
        expect(")");
        node -> then = stmt();
        if (consume_keyword(TK_ELSE)) {
            node -> els = stmt();
        }
    } else if (consume_keyword(TK_WHILE)) {
        node = calloc(1, sizeof(Node));
        node -> kind = ND_WHILE;
        expect("(");
        node -> cond = expr();
        expect(")");
        node -> then = stmt();
    } else if (consume_keyword(TK_FOR)) {
        node = calloc(1, sizeof(Node));
        node -> kind = ND_FOR;
        expect("(");
        if (!(token -> kind == TK_SYMBOL && token -> str[0] == ';')) {
            node -> init = expr();
            expect(";");
        }
        if (!(token -> kind == TK_SYMBOL && token -> str[0] == ';')) {
            node -> cond = expr();
            expect(";");
        }
        if (!(token -> kind == TK_SYMBOL && token -> str[0] == ')')) {
            node -> inc = expr();
            expect(")");
        }
    } else {
        node = expr();
    }
    expect(";");
    return node;
}

void program() {
    int i = 0;
    while (!at_eof()) {
        code[i] = stmt();
        i++;
    }
    code[i] = NULL;
}

LVar* find_lvar(Token* tok) {
    for (LVar* var = locals; var; var = var -> next) {
        if (var -> len == tok -> len && !memcmp(var -> name, tok -> str, tok -> len)) {
            return var;
        }
    }
    return NULL;
}
