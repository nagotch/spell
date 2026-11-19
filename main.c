#include "spell.h"

bool consume(char* symbol) {
    int len = strlen(symbol);
    if (token -> kind == TK_SYMBOL && token -> len == len && !memcmp(symbol, token -> str, token -> len)) {
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
    struct Node* new_node_ptr = calloc(1, sizeof(struct Node));
    new_node_ptr -> kind = kind;
    new_node_ptr -> left_child = left_child;
    new_node_ptr -> right_child = right_child;
    return new_node_ptr;
}

Node* create_number_node(long num) {
    struct Node* num_node_ptr = calloc(1, sizeof(struct Node));
    num_node_ptr -> kind = ND_NUM;
    num_node_ptr -> val = num;
    return num_node_ptr;
}

Node* primary() {
    if (consume("(")) {
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

Node* expr() {
    return equality();
}

void gen(Node* node) {
    if (node -> kind == ND_NUM) {
        printf("  push %ld\n", node -> val);
        return;
    }
    gen(node -> left_child);
    gen(node -> right_child);
    printf("  pop rdi\n");
    printf("  pop rax\n");
    if (node -> kind == ND_ADD) {
        printf("  add rax, rdi\n");
    } else if (node -> kind == ND_SUB) {
        printf("  sub rax, rdi\n");
    } else if (node -> kind == ND_MUL) {
        printf("  imul rax, rdi\n");
    } else if (node -> kind == ND_DIV) {
        printf("  cqo\n");
        printf("  idiv rdi\n");
    } else if (node -> kind == ND_EQ) {
        printf("  cmp rax, rdi\n");
        printf("  sete al\n");
        printf("  movzb rax, al\n");
    } else if (node -> kind == ND_NE) {
        printf("  cmp rax, rdi\n");
        printf("  setne al\n");
        printf("  movzb rax, al\n");
    } else if (node -> kind == ND_LT) {
        printf("  cmp rax, rdi\n");
        printf("  setl al\n");
        printf("  movzb rax, al\n");
    } else if (node -> kind == ND_LE) {
        printf("  cmp rax, rdi\n");
        printf("  setle al\n");
        printf("  movzb rax, al\n");
    }
    printf("  push rax\n");
}

int main(int argc, char **argv) {
    if (argc != 2) {
        fprintf(stderr, "1つの引数を指定してください\n");
        return 1;
    }

    token = tokenize(argv[1]);
    Node* tree = expr();

    printf(".intel_syntax noprefix\n");
    printf(".global main\n");
    printf("main:\n");

    gen(tree);
    printf("  pop rax\n");
    
    printf("  ret\n");
    printf(".section .note.GNU-stack,\"\",@progbits\n");

    return 0;
}
