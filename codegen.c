#include "spell.h"

void gen_unary(Node* node);
void gen_lvar(Node* node);
void gen_assign(Node* node);
void gen_num(Node* node);
void gen_return(Node* node);
void gen_array(Node* node);
void gen_index(Node* node);
void gen_len(Node* node);
void gen_newarray(Node* node);
void gen_if(Node* node);
void gen_while(Node* node);
void gen_for(Node* node);
void gen_block(Node* node);
void gen_funcall(Node* node);
void gen_binop(Node* node);

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
    switch (node -> kind) {
        case ND_UNARY: gen_unary(node); break;
        case ND_LVAR: gen_lvar(node); break;
        case ND_ASSIGN: gen_assign(node); break;
        case ND_NUM: gen_num(node); break;
        case ND_RETURN: gen_return(node); break;
        case ND_ARRAY: gen_array(node); break;
        case ND_INDEX: gen_index(node); break;
        case ND_LEN: gen_len(node); break;
        case ND_NEWARRAY: gen_newarray(node); break;
        case ND_IF: gen_if(node); break;
        case ND_WHILE: gen_while(node); break;
        case ND_FOR: gen_for(node); break;
        case ND_BLOCK: gen_block(node); break;
        case ND_FUNCALL: gen_funcall(node); break;
        case ND_BINOP: gen_binop(node); break;
    }
}

void gen_index_addr(Node* node) {
    gen(node -> left_child);
    gen(node -> right_child);
    printf("  pop rdi\n");
    printf("  pop rax\n");
    printf("  imul rdi, 8\n");
    printf("  add rax, rdi\n");
}

void gen_lval(Node* node) {
    if (node -> kind == ND_LVAR) {
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node -> offset);
        printf("  push rax\n");
        return;
    }
    if (node -> kind == ND_UNARY && node -> op.unop == OP_DEREF) {
        gen(node -> left_child);
        return;
    }
    if (node -> kind == ND_INDEX) {
        gen_index_addr(node);
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

void gen_unary(Node* node) {
    switch (node -> op.unop) {
        case OP_ADDR:
            gen_lval(node -> left_child);
            break;
        case OP_DEREF:
            gen(node -> left_child);
            printf("  pop rax\n");
            printf("  mov rax, [rax]\n");
            printf("  push rax\n");
            break;
        default:
            fprintf(stderr, "不明な単項演算子\n");
            exit(1);
    }
}

void gen_lvar(Node* node) {
    gen_lval(node);
    printf("  pop rax\n");
    printf("  mov rax, [rax]\n");
    printf("  push rax\n");
}

void gen_assign(Node* node) {
    gen_lval(node -> left_child);
    gen(node -> right_child);
    printf("  pop rdi\n");
    printf("  pop rax\n");
    printf("  mov [rax], rdi\n");
    printf("  push rdi\n");
}

void gen_num(Node* node) {
    printf("  push %ld\n", node -> val);
}

void gen_return(Node* node) {
    gen(node -> left_child);
    printf("  pop rax\n");
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

void gen_array(Node* node) {
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
}

void gen_index(Node* node) {
    gen_index_addr(node);
    printf("  mov rax, [rax]\n");
    printf("  push rax\n");
}

void gen_len(Node* node) {
    gen(node -> left_child);
    printf("  pop rax\n");
    printf("  mov rax, [rax-8]\n");
    printf("  push rax\n");
}

void call(int label, int len, char* name) {
    printf("  mov rax, rsp\n");
    printf("  and rax, 15\n");
    printf("  jnz .Lcall%d\n", label);
    printf("  mov rax, 0\n");
    printf("  call %.*s\n", len, name);
    printf("  jmp .Lend%d\n", label);
    printf(".Lcall%d:\n", label);
    printf("  sub rsp, 8\n");
    printf("  mov rax, 0\n");
    printf("  call %.*s\n", len, name);
    printf("  add rsp, 8\n");
    printf(".Lend%d:\n", label);
}

void gen_newarray(Node* node) {
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
    call(label, 6, "malloc");
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
}

void gen_if(Node* node) {
    if (node -> els == NULL) {
        int label = max_label;
        max_label++;
        gen(node -> cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .Lend%d\n", label);
        gen(node -> then);
        printf(".Lend%d:\n", label);
    } else {
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
    }
}

void gen_while(Node* node) {
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
}

void gen_for(Node* node) {
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
}

void gen_block(Node* node) {
    Node* block_head = node -> body;
    while (block_head) {
        gen(block_head);
        block_head = block_head -> next;
        printf("  pop rax\n");
    }
    printf("  push rax\n");
}

void gen_funcall(Node* node) {
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
    call(label, node -> len, node -> name);
    printf("  push rax\n");
}

void gen_binop(Node* node) {
    gen(node -> left_child);
    gen(node -> right_child);
    printf("  pop rdi\n");
    printf("  pop rax\n");
    switch (node -> op.binop) {
        case OP_ADD:
            printf("  add rax, rdi\n");
            break;
        case OP_SUB:
            printf("  sub rax, rdi\n");
            break;
        case OP_MUL:
            printf("  imul rax, rdi\n");
            break;
        case OP_DIV:
            printf("  cqo\n");
            printf("  idiv rdi\n");
            break;
        case OP_EQ:
            printf("  cmp rax, rdi\n");
            printf("  sete al\n");
            printf("  movzb rax, al\n");
            break;
        case OP_NE:
            printf("  cmp rax, rdi\n");
            printf("  setne al\n");
            printf("  movzb rax, al\n");
            break;
        case OP_LT:
            printf("  cmp rax, rdi\n");
            printf("  setl al\n");
            printf("  movzb rax, al\n");
            break;
        case OP_LE:
            printf("  cmp rax, rdi\n");
            printf("  setle al\n");
            printf("  movzb rax, al\n");
            break;
        default:
            fprintf(stderr, "無効な2項演算子\n");
            exit(1);
            break;
    }
    printf("  push rax\n");
}
