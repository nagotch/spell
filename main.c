#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>

typedef enum { TK_NUM, TK_SYMBOL, TK_EOF } TokenKind;

typedef struct Token Token;
struct Token {
    TokenKind kind;
    long val;
    char symbol;
    Token *next;
};

Token* new_token(TokenKind token_kind, struct Token* pre_token_ptr) {
    struct Token* next_token_ptr = calloc(1, sizeof(struct Token));
    pre_token_ptr -> next = next_token_ptr;
    next_token_ptr -> kind = token_kind;
    return next_token_ptr;
}

struct Token* tokenize(char* s) {
    struct Token first_token;
    first_token.next = NULL;
    struct Token* last_token_ptr = &first_token;

    char** endptr = &s;
    while (**endptr != '\0') {
        if (isspace(**endptr)) {
            (*endptr)++;
            continue;
        }

        bool is_symbol = false;
        char* targets = "+-*/()";
        for (int i = 0; targets[i] != '\0'; i++) {
            char target = targets[i];
            if (**endptr == target) {
                (*endptr)++;
                last_token_ptr = new_token(TK_SYMBOL, last_token_ptr);
                last_token_ptr -> symbol = target;
                is_symbol = true;
                break;
            }
        }
        if (is_symbol) {
            continue;
        }

        if (isdigit(**endptr)) {
            long num = strtol(*endptr, endptr, 10);
            last_token_ptr = new_token(TK_NUM, last_token_ptr);
            last_token_ptr -> val = num;
            continue;
        }

        fprintf(stderr, "不明な文字%s", *endptr);
        exit(1);
    }
    last_token_ptr = new_token(TK_EOF, last_token_ptr);
    return first_token.next;
}

struct Token* token = NULL;

bool consume(char symbol) {
    if (token -> kind == TK_SYMBOL && token -> symbol == symbol) {
        token = token -> next;
        return true;
    }
    return false;
}

void expect(char symbol) {
    if (token -> kind == TK_SYMBOL && token -> symbol == symbol) {
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

typedef enum {ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_NUM} NodeKind;

typedef struct Node Node;
struct Node {
    NodeKind kind;
    Node* left_child;
    Node* right_child;
    long val;
};

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

Node* expr();

Node* primary() {
    if (consume('(')) {
        Node* node = expr();
        expect(')');
        return node;
    } else {
        long num = expect_number();
        Node* node = create_number_node(num);
        return node;
    }
}

Node* unary() {
    Node* node = primary();
    if (consume('-')) {
        node = create_binary_operation_node(ND_SUB, create_number_node(0), node);
    }
    return node;
}

Node* mul() {
    Node* node = unary();
    while (true) {
        if (consume('*')) {
            node = create_binary_operation_node(ND_MUL, node, primary());
        } else if (consume('/')) {
            node = create_binary_operation_node(ND_DIV, node, primary());
        } else {
            break;
        }
    }
    return node;
}

Node* expr() {
    Node* node = mul();
    while (true) {
        if (consume('+')) {
            node = create_binary_operation_node(ND_ADD, node, mul());
        } else if (consume('-')) {
            node = create_binary_operation_node(ND_SUB, node, mul());
        } else {
            break;
        }
    }
    return node;
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
