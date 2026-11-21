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
    lvar -> offset = locals ? locals -> offset + 8 * (locals -> array_len + 1): 8;
    lvar -> next = locals;
    locals = lvar;
}

void resist_array(int array_len) {
    LVar* lvar = calloc(1, sizeof(LVar));
    lvar -> offset = locals ? locals -> offset + 8 * (locals -> array_len + 1): 8;
    lvar -> array_len = array_len;
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
    Node* node;
    if (!ident_token && consume("[")) {
        node = create_node(ND_ARRAY);
        Node* first_expr = expr();
        if (consume(";")) {
            Node* len_expr = expr();
            node = create_node(ND_NEWARRAY);
            node -> left_child = first_expr;
            node -> right_child = len_expr;
        } else {
            int array_len = 1;
            Node body;
            Node* cur = &body;
            cur -> next = first_expr;
            cur = cur -> next;
            while (consume(",")) {
                cur -> next = expr();
                cur = cur -> next;
                array_len++;
            }
            resist_array(array_len);
            node -> array_len = array_len;
            node -> body = body.next;
            node -> offset = locals -> offset;
        }
        expect("]");
    } else if (ident_token && consume("(")) {
        node = create_node(ND_FUNCALL);
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
            new_node -> left_child = node;
            new_node -> right_child = right_child;
            node = new_node;
        } else if (consume(".")) {
            ident_token = consume_ident();
            if (ident_token -> len == 3 && !memcmp(ident_token -> str, "len", 3)) {
                Node* new_node = create_node(ND_LEN);
                new_node -> left_child = node;
                node = new_node;
            } else {
                fprintf(stderr, "不明なメンバ");
                exit(1);
            }
        } else {
            break;
        }
    }
    return node;
}

Node* unary() {
    if (consume("+")) {
        return primary();
    } else if (consume("-")) {
        Node* node = create_binary_operation_node(ND_SUB, create_number_node(0), primary());
        return node;
    } else if (consume("*")) {
        Node* node = create_node(ND_DEREF);
        node -> left_child = unary();
        return node;
    } else if (consume("&")) {
        Node* node = create_node(ND_ADDR);
        node -> left_child = unary();
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
    int nparams = 0;
    if (!consume(")")) {
        do {
            resist_lvar(consume_ident());
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
    LVar* head_lvar = locals;
    LVar* cur_lvar = head_lvar;
    int max_offset = 0;
    while (cur_lvar) {
        int offset = cur_lvar -> offset + 8 * (cur_lvar -> array_len);
        if (max_offset < offset) {
            max_offset = offset;
        }
        cur_lvar = cur_lvar -> next;
    }
    func -> stack_size = (max_offset + 15) / 16 * 16;
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
