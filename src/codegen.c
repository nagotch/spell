// コード生成: 型検査済みの AST から x86-64 アセンブリ（GAS 形式）を標準出力へ出す。
// スタックマシン方式で、各 gen_* は「式の値を 1 つ push した状態」で戻る。
#include "spell.h"

static void gen(Node* node);

static int max_label = 0;
static int break_label = -1;
static int continue_label = -1;

// 関数呼び出しの引数レジスタ（System V ABI の順）
static char* arg_regs[] = {
    "rdi",
    "rsi",
    "rdx",
    "rcx",
    "r8",
    "r9",
};

// ---- ラベルと呼び出し ----

// ジェネリックな実体化は Box$int のようなラベルになる
static void print_type_label(Type* t) {
    switch (t->kind) {
        case TY_INT:
            printf("int");
            break;
        case TY_ARRAY:
            printf("array$");
            print_type_label(t->elem);
            break;
        case TY_CLASS:
            printf("%.*s", t->klass->len, t->klass->name);
            if (t->targ) {
                printf("$");
                print_type_label(t->targ);
            }
            break;
        default:
            printf("unknown");
            break;
    }
}

static void print_klass_label(Class* klass) {
    printf("%.*s", klass->len, klass->name);
    if (klass->origin) {
        printf("$");
        print_type_label(klass->targ);
    }
}

// rsp の 16 バイト境界をそろえてから call する。klass があればメソッド呼び出し
static void emit_call(int label, Class* klass, int len, char* name) {
    printf("  mov rax, rsp\n");
    printf("  and rax, 15\n");
    printf("  jnz .Lcall%d\n", label);
    printf("  mov rax, 0\n");
    printf("  call ");
    if (klass) {
        print_klass_label(klass);
        printf(".");
    }
    printf("%.*s\n", len, name);
    printf("  jmp .Lend%d\n", label);
    printf(".Lcall%d:\n", label);
    printf("  sub rsp, 8\n");
    printf("  mov rax, 0\n");
    printf("  call ");
    if (klass) {
        print_klass_label(klass);
        printf(".");
    }
    printf("%.*s\n", len, name);
    printf("  add rsp, 8\n");
    printf(".Lend%d:\n", label);
}

// ---- アドレスの生成 ----

static void gen_index_addr(Node* node) {
    gen(node->left_child);
    gen(node->right_child);
    printf("  pop rdi\n");
    printf("  pop rax\n");
    printf("  imul rdi, 8\n");
    printf("  add rax, rdi\n");
}

// 左辺値のアドレスを push する
static void gen_addr(Node* node) {
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
    if (node->kind == ND_MEMBER) {
        gen(node->left_child);
        printf("  pop rax\n");
        printf("  add rax, %d\n", node->offset);
        printf("  push rax\n");
        return;
    }
    error_internal("代入の処理で想定外の形式を検出しました");
}

// ---- 式と文の生成 ----

static void gen_unary(Node* node) {
    switch (node->op.unop) {
        case OP_ADDR:
            gen_addr(node->left_child);
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

static void gen_var(Node* node) {
    gen_addr(node);
    printf("  pop rax\n");
    printf("  mov rax, [rax]\n");
    printf("  push rax\n");
}

static void gen_assign(Node* node) {
    gen_addr(node->left_child);
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
    Node* tail = node->body;
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
    emit_call(label, NULL, 6, "malloc");
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
    int label = max_label;
    max_label++;
    gen(node->cond);
    printf("  pop rax\n");
    printf("  cmp rax, 0\n");
    printf("  je .Lelse%d\n", label);
    gen(node->then);
    printf("  jmp .Lend%d\n", label);
    printf(".Lelse%d:\n", label);
    if (node->els) {
        gen(node->els);
    } else {
        printf("  push 0\n");
    }
    printf(".Lend%d:\n", label);
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

static void gen_construct(Node* node) {
    int args_cnt = 0;
    for (Node* arg = node->args; arg; arg = arg->next) {
        gen(arg);
        args_cnt++;
    }
    if (args_cnt > 5) {
        error_plain("`__init__`に渡せる引数は5個までですが、%d個が指定されています", args_cnt);
    }
    int size = 0;
    for (Field* f = node->klass->fields; f; f = f->next) {
        size += 8;
    }
    if (size == 0) {
        size = 8;
    }
    int label = max_label++;
    printf("  mov rdi, 1\n");
    printf("  mov rsi, %d\n", size);
    emit_call(label, NULL, 6, "calloc");
    printf("  push rax\n");
    printf("  pop rdi\n");
    for (int k = args_cnt - 1; k >= 0; k--) {
        printf("  pop %s\n", arg_regs[k+1]);
    }
    printf("  push rdi\n");
    if (node->func) {
        emit_call(max_label++, node->func->klass, node->func->len, node->func->name);
    }
}

static void gen_funcall(Node* node) {
    if (node->klass) {
        gen_construct(node);
        return;
    }
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
        printf("  pop %s\n", arg_regs[i]);
    }
    emit_call(label, NULL, node->len, node->name);
    if (node->len == 7 && (!memcmp(node->name, "getchar", 7) || !memcmp(node->name, "putchar", 7))) {
        printf("  movsxd rax, eax\n");
    }
    printf("  push rax\n");
}

static void gen_member(Node* node) {
    gen(node->left_child);
    if (node->func) {
        printf("  pop rdi\n");
        emit_call(max_label++, node->func->klass, node->func->len, node->func->name);
        printf("  push rax\n");
        return;
    }
    printf("  pop rax\n");
    printf("  mov rax, [rax + %d]\n", node->offset);
    printf("  push rax\n");
}

static void gen_mcall(Node* node) {
    gen(node->left_child);
    int args_cnt = 0;
    for (Node* arg = node->args; arg; arg = arg->next) {
        gen(arg);
        args_cnt++;
    }
    if (args_cnt > 5) {
        error_plain("メソッドに渡せる引数は5個までですが、%d個が指定されています", args_cnt);
    }
    for (int k = args_cnt - 1; k >= 0; k--) {
        printf("  pop %s\n", arg_regs[k + 1]);
    }
    printf("  pop rdi\n");
    emit_call(max_label++, node->func->klass, node->func->len, node->func->name);
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

static void gen(Node* node) {
    switch (node->kind) {
        case ND_UNARY: gen_unary(node); break;
        case ND_LVAR:
        case ND_GVAR: gen_var(node); break;
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
        case ND_MEMBER: gen_member(node); break;
        case ND_MCALL: gen_mcall(node); break;
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
        default:
            error_internal("コード生成で想定外の形式を検出しました");
            break;
    }
}

// ---- 関数ごとの出力 ----

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
        if (prog->klass && prog->klass->tparam) {
            prog = prog->next;
            continue;
        }
        if (prog->klass) {
            printf(".global ");
            print_klass_label(prog->klass);
            printf(".%.*s\n", prog->len, prog->name);
            print_klass_label(prog->klass);
            printf(".%.*s:\n", prog->len, prog->name);
        } else {
            printf(".global %.*s\n", prog->len, prog->name);
            printf("%.*s:\n", prog->len, prog->name);
        }
        printf("  push rbp\n");
        printf("  mov rbp, rsp\n");
        printf("  sub rsp, %d\n", prog->stack_size);
        if (has_init && prog->len == 4 && memcmp(prog->name, "main", 4) == 0) {
            printf("  call __spell_init\n");
        }
        for (int i = 0; i < prog->nparams; i++) {
            printf("  mov [rbp-%d], %s\n", (i+1)*8, arg_regs[i]);
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
