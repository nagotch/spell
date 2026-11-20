#include "spell.h"

int max_label = 0;

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
        return;
    }

    int label = max_label;
    max_label++;

    if (node -> kind == ND_IF && node -> els == NULL) {
        gen(node -> cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .Lend%d\n", label);
        gen(node -> then);
        printf(".Lend%d:\n", label);
        return;
    }

    if (node -> kind == ND_IF) {
        gen(node -> cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .Lelse%d\n", label);
        gen(node -> then);
        printf("  jmp .Lend%d\n", label);
        printf(".Lelse%d:\n", label);
        gen(node -> els);
        printf(".Lend%d:\n", label);
        return;
    }

    if (node -> kind == ND_WHILE) {
        printf(".Lbegin%d:\n", label);
        gen(node -> cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .Lend%d\n", label);
        gen(node -> then);
        printf("  jmp .Lbegin%d\n", label);
        printf(".Lend%d:\n", label);
        return;
    }

    if (node -> kind == ND_FOR) {
        if (node -> init) {
            gen(node -> init);
        }
        printf(".Lbegin%d:\n", label);
        if (node -> cond) {
            gen(node -> cond);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  je .Lend%d\n", label);
        }
        gen(node -> then);
        if (node -> inc) {
            gen(node -> inc);
        }
        printf("  jmp .Lbegin%d\n", label);
        printf(".Lend%d:\n", label);
        return;
    }

    if (node -> kind == ND_BLOCK) {
        Node* block_head = node -> body;
        while (block_head) {
            gen(block_head);
            block_head = block_head -> next;
            printf("  pop rax\n");
        }
        return;
    }

    if (node -> kind == ND_FUNCALL) {
        Node* head = node -> args;
        int args_cnt = 0;
        while (head) {
            gen(head);
            head = head -> next;
            args_cnt++;
        }
        if (args_cnt > 6) {
            fprintf(stderr, "関数の引数が6より多い");
            exit(1);
        }
        char* resister[] = {
            "r9",
            "r8",
            "rcx",
            "rdx",
            "rsi",
            "rdi",
        };
        for (int i = 0; i < args_cnt; i++) {
            printf("  pop %s\n", resister[i]);
        }
        printf("  mov rax, rsp\n");
        printf("  and rax, 15\n");
        printf("  jnz .Lcall%d\n", label);
        printf("  mov rax, 0\n");
        printf("  call %.*s\n", node -> len, node -> name);
        printf("  jmp .Lend%d\n", label);
        printf(".Lcall%d:\n", label);
        printf("  sub rsp, 8\n");
        printf("  mov rax, 0\n");
        printf("  call %.*s\n", node -> len, node -> name);
        printf("  add rsp, 8\n");
        printf(".Lend%d:\n", label);
        printf("  push rax\n");
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

void gen_lval(Node* node) {
    if (node -> kind != ND_LVAR) {
        fprintf(stderr, "  不明な代入");
        exit(1);
    }

    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", node -> offset);
    printf("  push rax\n");
}
