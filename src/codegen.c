#include "spell.h"

static void gen_unary(Node* node);
static void gen_lvar(Node* node);
static void gen_assign(Node* node);
static void gen_num(Node* node);
static void gen_return(Node* node);
static void gen_array(Node* node);
static void gen_index(Node* node);
static void gen_len(Node* node);
static void gen_newarray(Node* node);
static void gen_if(Node* node);
static void gen_while(Node* node);
static void gen_for(Node* node);
static void gen_block(Node* node);
static void gen_funcall(Node* node);
static void gen_binop(Node* node);

static int max_label = 0;
static int break_label = -1;
static int continue_label = -1;

static char* resister[] = {
    "rdi",
    "rsi",
    "rdx",
    "rcx",
    "r8",
    "r9",
};

static void gen(Node* node) {
    switch (node->kind) {
        case ND_UNARY: gen_unary(node); break;
        case ND_LVAR:
        case ND_GVAR: gen_lvar(node); break;
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
        case ND_BREAK:
            if (break_label < 0) {
                error_plain("ここでは`break`を使えません");
            }
            printf("  jmp .Lend%d\n", break_label);
            break;
        case ND_CONTINUE:
            if (continue_label < 0) {
                error_plain("ここでは`continue`を使えません");
            }
            printf("  jmp .Lcont%d\n", continue_label);
            break;
    }
}

static void gen_index_addr(Node* node) {
    gen(node->left_child);
    gen(node->right_child);
    printf("  pop rdi\n");
    printf("  pop rax\n");
    printf("  imul rdi, 8\n");
    printf("  add rax, rdi\n");
}

static void gen_lval(Node* node) {
    if (node->kind == ND_GVAR) {
        printf("  lea rax, [rip + %.*s]\n", node->len, node->name);
        printf("  push rax\n");
        return;
    }
    if (node->kind == ND_LVAR) {
        printf("  mov rax, rbp\n");
        printf("  sub rax, %d\n", node->offset);
        printf("  push rax\n");
        return;
    }
    if (node->kind == ND_UNARY && node->op.unop == OP_DEREF) {
        gen(node->left_child);
        return;
    }
    if (node->kind == ND_INDEX) {
        gen_index_addr(node);
        printf("  push rax\n");
        return;
    }
    error_internal("代入の処理で想定外の形式を検出しました");
}

void codegen(Function* prog) {
    bool has_init = false;
    for (Function* fn = prog; fn; fn = fn->next) {
        if (fn->len == 12 && !memcmp(fn->name, "__spell_init", 12)) {
            has_init = true;
        }
    }
    if (globals) {
        printf(".bss\n");
        for (LVar* var = globals; var; var = var->next) {
            printf("%.*s:\n", var->len, var->name);
            printf("  .zero 8\n");
        }
        printf(".text\n");
    }
    while (prog) {
        printf(".global %.*s\n", prog->len, prog->name);
        printf("%.*s:\n", prog->len, prog->name);
        printf("  push rbp\n");
        printf("  mov rbp, rsp\n");
        printf("  sub rsp, %d\n", prog->stack_size);
        if (has_init && prog->len == 4 && memcmp(prog->name, "main", 4) == 0) {
            printf("  call __spell_init\n");
        }
        for (int i = 0; i < prog->nparams; i++) {
            printf("  mov [rbp-%d], %s\n", (i+1)*8, resister[i]);
        }
        Node* tail = prog->body;
        while (tail) {
            gen(tail);
            tail = tail->next;
            printf("  pop rax\n");
        }
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
        prog = prog->next;
    }
}

static void gen_unary(Node* node) {
    switch (node->op.unop) {
        case OP_ADDR:
            gen_lval(node->left_child);
            break;
        case OP_DEREF:
            gen(node->left_child);
            printf("  pop rax\n");
            printf("  mov rax, [rax]\n");
            printf("  push rax\n");
            break;
        case OP_NOT:
            gen(node->left_child);
            printf("  pop rax\n");
            printf("  cmp rax, 0\n");
            printf("  sete al\n");
            printf("  movzb rax, al\n");
            printf("  push rax\n");
            break;
        default:
            error_internal("単項演算の処理で想定外の演算子を検出しました");
    }
}

static void gen_lvar(Node* node) {
    gen_lval(node);
    printf("  pop rax\n");
    printf("  mov rax, [rax]\n");
    printf("  push rax\n");
}

static void gen_assign(Node* node) {
    gen_lval(node->left_child);
    gen(node->right_child);
    printf("  pop rdi\n");
    printf("  pop rax\n");
    printf("  mov [rax], rdi\n");
    printf("  push rdi\n");
}

static void gen_num(Node* node) {
    printf("  push %ld\n", node->val);
}

static void gen_return(Node* node) {
    gen(node->left_child);
    printf("  pop rax\n");
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

static void gen_array(Node* node) {
    printf("  mov rax, %d\n", node->array_len);
    printf("  mov [rbp-%d], rax\n", node->offset + 8 * node->array_len);
    Node* tail;
    tail = node->body;
    int i = 0;
    for (int i = 0; tail; i++) {
        gen(tail);
        printf("  pop rax\n");
        printf("  mov [rbp-%d], rax\n", node->offset + 8 * (node->array_len - i - 1));
        tail = tail->next;
    }
    printf("  mov rax, rbp\n");
    printf("  sub rax, %d\n", node->offset + 8 * (node->array_len - 1));
    printf("  push rax\n");
}

static void gen_index(Node* node) {
    gen_index_addr(node);
    printf("  mov rax, [rax]\n");
    printf("  push rax\n");
}

static void gen_len(Node* node) {
    gen(node->left_child);
    printf("  pop rax\n");
    printf("  mov rax, [rax-8]\n");
    printf("  push rax\n");
}

static void call(int label, int len, char* name) {
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

static void gen_newarray(Node* node) {
    int label = max_label;
    max_label++;
    gen(node->left_child);
    gen(node->right_child);
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

static void gen_if(Node* node) {
    if (node->els == NULL) {
        int label = max_label;
        max_label++;
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .Lelse%d\n", label);
        gen(node->then);
        printf("  jmp .Lend%d\n", label);
        printf(".Lelse%d:\n", label);
        printf("  push 0\n");
        printf(".Lend%d:\n", label);
    } else {
        int label = max_label;
        max_label++;
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .Lelse%d\n", label);
        gen(node->then);
        printf("  jmp .Lend%d\n", label);
        printf(".Lelse%d:\n", label);
        gen(node->els);
        printf(".Lend%d:\n", label);
    }
}

static void gen_while(Node* node) {
    int label = max_label;
    max_label++;
    int saved_break = break_label;
    int saved_continue = continue_label;
    break_label = label;
    continue_label = label;
    printf(".Lbegin%d:\n", label);
    printf(".Lcont%d:\n", label);
    gen(node->cond);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je .Lend%d\n", label);
    gen(node->then);
    printf("  pop rax\n");
    break_label = saved_break;
    continue_label = saved_continue;
    printf("  jmp .Lbegin%d\n", label);
    printf(".Lend%d:\n", label);
    printf("  push 0\n");
}

static void gen_for(Node* node) {
    int label = max_label;
    max_label++;
    int saved_break = break_label;
    int saved_continue = continue_label;
    break_label = label;
    continue_label = label;
    if (node->init) {
        gen(node->init);
        printf("  pop rax\n");
    }
    printf(".Lbegin%d:\n", label);
    if (node->cond) {
        gen(node->cond);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .Lend%d\n", label);
    }
    gen(node->then);
    printf("  pop rax\n");
    break_label = saved_break;
    continue_label = saved_continue;
    printf(".Lcont%d:\n", label);
    if (node->inc) {
        gen(node->inc);
        printf("  pop rax\n");
    }
    printf("  jmp .Lbegin%d\n", label);
    printf(".Lend%d:\n", label);
    printf("  push 0\n");
}

static void gen_block(Node* node) {
    Node* block_head = node->body;
    while (block_head) {
        gen(block_head);
        block_head = block_head->next;
        printf("  pop rax\n");
    }
    printf("  push rax\n");
}

static void gen_funcall(Node* node) {
    int label = max_label;
    max_label++;
    Node* head = node->args;
    int args_cnt = 0;
    while (head) {
        gen(head);
        head = head->next;
        args_cnt++;
    }
    if (args_cnt > 6) {
        error_plain("関数に渡せる引数は6個までですが、%ld個が指定されています", args_cnt);
    }
    for (int i = args_cnt - 1; i >= 0; i--) {
        printf("  pop %s\n", resister[i]);
    }
    call(label, node->len, node->name);
    if (node->len == 7 && (!memcmp(node->name, "getchar", 7) || !memcmp(node->name, "putchar", 7))) {
        printf("  movsxd rax, eax\n");
    }
    printf("  push rax\n");
}

static void gen_binop(Node* node) {
    if (node->op.binop == OP_AND) {
        int label = max_label;
        max_label++;
        gen(node->left_child);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .Lfalse%d\n", label);
        gen(node->right_child);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  je .Lfalse%d\n", label);
        printf("  push 1\n");
        printf("  jmp .Lend%d\n", label);
        printf(".Lfalse%d:\n", label);
        printf("  push 0\n");
        printf(".Lend%d:", label);
        return;
    } else if (node->op.binop == OP_OR) {
        int label = max_label;
        max_label++;
        gen(node->left_child);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  jne .Ltrue%d\n", label);
        gen(node->right_child);
        printf("  pop rax\n");
        printf("  cmp rax, 0\n");
        printf("  jne .Ltrue%d\n", label);
        printf("  push 0\n");
        printf("  jmp .Lend%d\n", label);
        printf(".Ltrue%d:\n", label);
        printf("  push 1\n");
        printf(".Lend%d:", label);
        return;
    }
    gen(node->left_child);
    gen(node->right_child);
    printf("  pop rdi\n");
    printf("  pop rax\n");
    switch (node->op.binop) {
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
        case OP_MOD:
            printf("  cqo\n");
            printf("  idiv rdi\n");
            printf("  mov rax, rdx\n");
            break;
        default:
            error_internal("二項演算の処理で想定外の演算子を検出しました");
            break;
    }
    printf("  push rax\n");
}
