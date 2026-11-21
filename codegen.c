#include "spell.h"

int max_label = 0;

char* resister[] = {
    "rdi",
    "rsi",
    "rdx",
    "rcx",
    "r8",
    "r9",
};

void gen(Node* node) {
    if (node -> kind == ND_ADDR) {
        gen_lval(node -> left_child);
        return;
    }

    if (node -> kind == ND_DEREF) {
        gen(node -> left_child);
        printf("  pop rax\n");
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    }

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
    if (node -> kind == ND_ARRAY) {
        printf("  mov rax, %d\n", node -> array_len);
        printf("  mov [rbp-%d], rax\n", node -> offset + 8 * node -> array_len);
        Node* tail;
        tail = node -> body;
        int i = 0;
        for (int i = 0; tail; i++) {
            gen(tail);
            printf("  pop rax\n");
            printf("  mov [rbp-%d], rax\n", node -> offset + 8 * (node -> array_len - i - 1));
            tail = tail -> next;
        }
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node -> offset + 8 * (node -> array_len - 1));
        printf("  push rax\n");
        return;
    }
    if (node -> kind == ND_INDEX) {
        gen(node -> left_child);
        gen(node -> right_child);
        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  imul rdi, 8\n");
        printf("  add rax, rdi\n");
        printf("  mov rax, [rax]\n");
        printf("  push rax\n");
        return;
    }
    if (node -> kind == ND_LEN) {
        gen(node -> left_child);
        printf("  pop rax\n");
        printf("  mov rax, [rax-8]\n");
        printf("  push rax\n");
        return;
    }
    if (node -> kind == ND_NEWARRAY) {
        int label = max_label;
        max_label++;
        gen(node -> left_child);
        gen(node -> right_child);
        printf("  pop rax\n");
        printf("  push rax\n");
        printf("  add rax, 1\n");
        printf("  imul rax, 8\n");
        printf("  mov rdi, rax\n");
        printf("  mov rdi, rax\n");
        printf("  mov rax, rsp\n");
        printf("  and rax, 15\n");
        printf("  jnz .Lcall%d\n", label);
        printf("  mov rax, 0\n");
        printf("  call malloc\n");
        printf("  jmp .Lend%d\n", label);
        printf(".Lcall%d:\n", label);
        printf("  sub rsp, 8\n");
        printf("  mov rax, 0\n");
        printf("  call malloc\n");
        printf("  add rsp, 8\n");
        printf(".Lend%d:\n", label);
        printf("  pop rcx\n");
        printf("  mov [rax], rcx\n");
        printf("  pop rsi\n");
        printf("  add rax, 8\n");
        printf("  push rax\n");
        label = max_label;
        max_label++;
        printf("  mov rdx, rax\n");
        printf(".Lbegin%d:\n", label);
        printf("  cmp rcx, 0\n");
        printf("  je .Lend%d\n", label);
        printf("  mov [rdx], rsi\n");
        printf("  add rdx, 8\n");
        printf("  sub rcx, 1\n");
        printf("  jmp .Lbegin%d\n", label);
        printf(".Lend%d:\n", label);
        return;
    }

    if (node -> kind == ND_IF && node -> els == NULL) {
        int label = max_label;
        max_label++;
        gen(node -> cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .Lend%d\n", label);
        gen(node -> then);
        printf(".Lend%d:\n", label);
        return;
    }
    if (node -> kind == ND_IF) {
        int label = max_label;
        max_label++;
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
        int label = max_label;
        max_label++;
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
        int label = max_label;
        max_label++;
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
        printf("  push rax\n");
        return;
    }
    if (node -> kind == ND_FUNCALL) {
        int label = max_label;
        max_label++;
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
        for (int i = args_cnt - 1; i >= 0; i--) {
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
    if (node -> kind == ND_BINOP) {
        gen(node -> left_child);
        gen(node -> right_child);
        printf("  pop rdi\n");
        printf("  pop rax\n");
        switch (node -> op) {
            case ND_ADD:
                printf("  add rax, rdi\n");
                break;
            case ND_SUB:
                printf("  sub rax, rdi\n");
                break;
            case ND_MUL:
                printf("  imul rax, rdi\n");
                break;
            case ND_DIV:
                printf("  cqo\n");
                printf("  idiv rdi\n");
                break;
            case ND_EQ:
                printf("  cmp rax, rdi\n");
                printf("  sete al\n");
                printf("  movzb rax, al\n");
                break;
            case ND_NE:
                printf("  cmp rax, rdi\n");
                printf("  setne al\n");
                printf("  movzb rax, al\n");
                break;
            case ND_LT:
                printf("  cmp rax, rdi\n");
                printf("  setl al\n");
                printf("  movzb rax, al\n");
                break;
            case ND_LE:
                printf("  cmp rax, rdi\n");
                printf("  setle al\n");
                printf("  movzb rax, al\n");
                break;
        }
        printf("  push rax\n");
    }
}

void gen_lval(Node* node) {
    if (node -> kind == ND_LVAR) {
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node -> offset);
        printf("  push rax\n");
        return;
    }
    if (node -> kind == ND_DEREF) {
        gen(node -> left_child);
        return;
    }
    if (node -> kind == ND_INDEX) {
        gen(node -> left_child);
        gen(node -> right_child);
        printf("  pop rdi\n");
        printf("  pop rax\n");
        printf("  imul rdi, 8\n");
        printf("  add rax, rdi\n");
        printf("  push rax\n");
        return;
    }
    fprintf(stderr, "  不明な代入");
    exit(1);
}

void codegen(Function* prog) {
    while (prog) {
        printf(".global %.*s\n", prog -> len, prog -> name);
        printf("%.*s:\n", prog -> len, prog -> name);
        printf("  push rbp\n");
        printf("  mov rbp, rsp\n");
        printf("  sub rsp, %d\n", prog -> stack_size);
        for (int i = 0; i < prog -> nparams; i++) {
            printf("  mov [rbp-%d], %s\n", (i+1)*8, resister[i]);
        }
        Node* tail = prog -> body;
        while (tail) {
            gen(tail);
            tail = tail -> next;
            printf("  pop rax\n");
        }
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
        prog = prog -> next;
    }
}
