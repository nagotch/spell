#include "spell.h"

void gen(Node* node) {
    if (node -> kind == ND_LVAR) {
        gen_lval(node);
        printf("  pop rax\n");
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    } 
    if (node -> kind == ND_ASSIGN) {
        gen_lval(node -> left_child);
        gen(node -> right_child);
        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  mov [rax], rdi\n");
        printf("  push rdi\n");
        return;
    }
    if (node -> kind == ND_NUM) {
        printf("  push %ld\n", node -> val);
        return;
    }

    if (node -> kind == ND_RETURN) {
        gen(node -> left_child);
        printf("  pop rax\n");
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
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

void gen_lval(Node* node) {
    if (node -> kind != ND_LVAR) {
        fprintf(stderr, "  不明な代入");
        exit(1);
    }

    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", node -> offset);
    printf("  push rax\n");
}
