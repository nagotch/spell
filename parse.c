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

Node* create_node(NodeKind kind) {
    Node* node = calloc(1, sizeof(Node));
    node -> kind = kind;
    return node;
}

Node* create_binary_operation_node(NodeKind kind, Node* left_child, Node* right_child) {
    Node* new_node_ptr = create_node(kind);
    new_node_ptr -> left_child = left_child;
    new_node_ptr -> right_child = right_child;
    return new_node_ptr;
}

Node* create_number_node(long num) {
    Node* num_node_ptr = create_node(ND_NUM);
    num_node_ptr -> val = num;
    return num_node_ptr;
}

void resist_lvar(Token* tok) {
    LVar* lvar = calloc(1, sizeof(LVar));
    lvar -> name = tok -> str;
    lvar -> len = tok -> len;
    lvar -> offset = locals ? locals -> offset + 8 : 8;
    lvar -> next = locals;
    locals = lvar;
}

Node* create_ident_node(Token* tok) {
    LVar* var = find_lvar(tok);
    Node* ident_node_ptr = create_node(ND_LVAR);
    if (var) {
        ident_node_ptr -> offset = var -> offset;
    } else {
        resist_lvar(tok);
        ident_node_ptr -> offset = locals -> offset;
    }
    return ident_node_ptr;
}

Node* primary() {
    Token* ident_token = consume_ident();
    if (ident_token && consume("(")) {
        Node* node = create_node(ND_FUNCALL);
        node -> name = ident_token -> str;
        node -> len = ident_token -> len;
        if (!consume(")")) {
            Node head;
            head.next = NULL;
            Node* cur = &head;
            do {
                cur -> next = expr();
                cur = cur -> next;
            } while (consume(","));
            node -> args = head.next;
            expect(")");
        }
        return node;
    } else if (ident_token) {
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
    if (consume("{")) {
        node = create_node(ND_BLOCK);
        Node head;
        head.next = NULL;
        Node* cur = &head;
        while (!consume("}")) {
            cur -> next = stmt();
            cur = cur -> next;
        }
        node -> body = head.next;
        return node;
    } else if (consume_keyword(TK_RETURN)) {
        node = create_node(ND_RETURN);
        node -> left_child = expr();
    } else if (consume_keyword(TK_IF)) {
        node = create_node(ND_IF);
        expect("(");
        node -> cond = expr();
        expect(")");
        node -> then = stmt();
        if (consume_keyword(TK_ELSE)) {
            node -> els = stmt();
        }
        return node;
    } else if (consume_keyword(TK_WHILE)) {
        node = create_node(ND_WHILE);
        expect("(");
        node -> cond = expr();
        expect(")");
        node -> then = stmt();
        return node;
    } else if (consume_keyword(TK_FOR)) {
        node = create_node(ND_FOR);
        expect("(");
        if (!consume(";")) {
            node -> init = expr();
            expect(";");
        }
        if (!consume(";")) {
            node -> cond = expr();
            expect(";");
        }
        if (!consume(")")) {
            node -> inc = expr();
            expect(")");
        }
        node -> then = stmt();
        return node;
    } else {
        node = expr();
    }
    expect(";");
    return node;
}

Function* function() {
    locals = NULL;
    Token* tok = consume_ident();
    if (!tok) {
        fprintf(stderr, "関数名を定義してください\n");
        exit(1);
    }
    char* name = tok -> str;
    int len = tok -> len;
    if (!consume("(")) {
        fprintf(stderr, "\"(\"が必要です");
        exit(1);
    }
    if (!consume(")")) {
        int nparams = 0;
        do {
            resist_lvar(token);
            nparams++;
        } while (consume(","));
        expect(")");
    }
    expect("{");
    Node head;
    head.next = NULL;
    Node* cur = &head;
    while (!consume("}")) {
        cur -> next = stmt();
        cur = cur -> next;
    }
    Function* func = calloc(1, sizeof(Function));
    func -> name = name;
    func -> len = len;
    func -> body = head.next;
    func -> nparams = nparams;
    func -> stack_size = locals ? (locals -> offset + 15) / 16 * 16 : 0;
    return func;
}

Function* program() {
    Function head;
    head.next = NULL;
    Function* cur = &head;
    while (!at_eof()) {
        cur -> next = function();
        cur = cur -> next;
    }
    return head.next;
}

LVar* find_lvar(Token* tok) {
    for (LVar* var = locals; var; var = var -> next) {
        if (var -> len == tok -> len && !memcmp(var -> name, tok -> str, tok -> len)) {
            return var;
        }
    }
    return NULL;
}
