// コード生成: 型検査済みの AST から x86-64 アセンブリ（GAS 形式）を標準出力へ出す。
// スタックマシン方式で、各 gen_* は「式の値を 1 つ push した状態」で戻る。
#include "spell.h"

static void gen(Node* node);

static int max_label = 0;

// 新しいラベル番号を1つ取る
static int new_label() {
    return max_label++;
}
static int break_label = -1;
static int continue_label = -1;

// 生成中の関数がラムダの場合、その環境ポインタ退避スロットのオフセット。
// ラムダでなければ 0（捕捉変数や環境経由の捕捉は現れない）
static int current_env_offset = 0;

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
            for (int i = 0; i < t->targ_count; i++) {
                printf("$");
                print_type_label(t->targs[i]);
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
        for (int i = 0; i < klass->targ_count; i++) {
            printf("$");
            print_type_label(klass->targs[i]);
        }
    }
}

// rsp の 16 バイト境界をそろえてから call する。klass があればメソッド呼び出し
static void print_call_target(Class* klass, int len, char* name) {
    printf("  call ");
    if (klass) {
        print_klass_label(klass);
        printf(".");
    }
    printf("%.*s\n", len, name);
}

static void emit_call(int label, Class* klass, int len, char* name) {
    printf("  mov rax, rsp\n");
    printf("  and rax, 15\n");
    printf("  jnz .Lcall%d\n", label);
    printf("  mov rax, 0\n");
    print_call_target(klass, len, name);
    printf("  jmp .Lend%d\n", label);
    printf(".Lcall%d:\n", label);
    printf("  sub rsp, 8\n");
    printf("  mov rax, 0\n");
    print_call_target(klass, len, name);
    printf("  add rsp, 8\n");
    printf(".Lend%d:\n", label);
}

// r11 に入っているコードポインタへの間接呼び出し（値経由の呼び出し用）。
// 境界調整は rax と rsp だけを使い、引数レジスタと r10（環境）・r11 を壊さない
static void emit_call_reg(int label) {
    printf("  mov rax, rsp\n");
    printf("  and rax, 15\n");
    printf("  jnz .Lcall%d\n", label);
    printf("  mov rax, 0\n");
    printf("  call r11\n");
    printf("  jmp .Lend%d\n", label);
    printf(".Lcall%d:\n", label);
    printf("  sub rsp, 8\n");
    printf("  mov rax, 0\n");
    printf("  call r11\n");
    printf("  add rsp, 8\n");
    printf(".Lend%d:\n", label);
}

// ---- アドレスの生成 ----

// スタック上の引数を arg_regs[offset] から逆順に pop する
static void pop_args(int n, int offset) {
    for (int i = n - 1; i >= 0; i--) {
        printf("  pop %s\n", arg_regs[i + offset]);
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
    if (node->kind == ND_CAPVAR) {
        // 環境ポインタ（フレームに退避済み）＋環境内のスロットオフセット
        printf("  mov rax, [rbp-%d]\n", current_env_offset);
        printf("  add rax, %d\n", node->offset);
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
    if (node->array_len == 0) {
        // 空のリテラルはヒープに置く。フレーム上に置くと、関数を抜けた
        // 後まで保持される場合（フィールドへの代入など）に壊れる
        int label = new_label();
        printf("  mov rdi, 8\n");
        emit_call(label, NULL, 6, "malloc");
        printf("  mov rcx, 0\n");
        printf("  mov [rax], rcx\n");
        printf("  add rax, 8\n");
        printf("  push rax\n");
        return;
    }
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
    int label = new_label();
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
    label = new_label();
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
    int label = new_label();
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
    int label = new_label();
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
    int label = new_label();
    printf("  mov rdi, 1\n");
    printf("  mov rsi, %d\n", size);
    emit_call(label, NULL, 6, "calloc");
    printf("  push rax\n");
    printf("  pop rdi\n");
    pop_args(args_cnt, 1);
    printf("  push rdi\n");
    if (node->func) {
        emit_call(new_label(), node->func->klass, node->func->len, node->func->name);
    }
}

// 組み込みプリミティブ（step 59）の呼び出し先ラベル。利用者や libc の
// シンボルと衝突しないよう `__rt_spell_` を付ける
static char* builtin_rt_name(int builtin) {
    switch (builtin) {
        case BUILTIN_ARGC: return "__rt_spell_argc";
        case BUILTIN_ARGV_CHARS: return "__rt_spell_argv_chars";
        case BUILTIN_READ_FILE_CHARS: return "__rt_spell_read_file_chars";
        case BUILTIN_IO_OK: return "__rt_spell_io_ok";
        case BUILTIN_WRITE_FILE_CHARS: return "__rt_spell_write_file_chars";
        case BUILTIN_STDERR_CHARS: return "__rt_spell_stderr_chars";
        case BUILTIN_EXIT: return "__rt_spell_exit";
    }
    return NULL;
}

static void gen_funcall(Node* node) {
    if (node->klass) {
        gen_construct(node);
        return;
    }
    if (node->builtin) {
        // プリミティブは通常の関数呼び出しと同じ形で、呼び出し先だけ
        // ランタイムヘルパに替える
        int label = new_label();
        Node* head = node->args;
        int args_cnt = 0;
        while (head) {
            gen(head);
            head = head->next;
            args_cnt++;
        }
        pop_args(args_cnt, 0);
        char* rt = builtin_rt_name(node->builtin);
        emit_call(label, NULL, (int)strlen(rt), rt);
        printf("  push rax\n");
        return;
    }
    int label = new_label();
    Node* head = node->args;
    int args_cnt = 0;
    while (head) {
        gen(head);
        head = head->next;
        args_cnt++;
    }
    if (args_cnt > 6) {
        error_plain("関数に渡せる引数は6個までですが、%d個が指定されています", args_cnt);
    }
    pop_args(args_cnt, 0);
    emit_call(label, NULL, node->len, node->name);
    if (node->len == 7 && (!memcmp(node->name, "getchar", 7) || !memcmp(node->name, "putchar", 7))) {
        printf("  movsxd rax, eax\n");
    }
    printf("  push rax\n");
}

// 関数値の組（[0]=コード、[8]=環境ポインタ）をヒープに作って push する
static void gen_funcref(Node* node) {
    printf("  mov rdi, 16\n");
    emit_call(new_label(), NULL, 6, "malloc");
    printf("  lea rcx, [rip + %.*s]\n", node->len, node->name);
    printf("  mov [rax], rcx\n");
    printf("  mov qword ptr [rax + 8], 0\n");
    printf("  push rax\n");
}

// ラムダ式: 組と環境を1つのブロックに確保する（[0]=コード、[8]=環境、
// 環境はブロックの16バイト目から）。捕捉は生成時点の値をコピーする
static void gen_lambda(Node* node) {
    if (!node->func) {
        error_internal("ラムダの関数が解決されていません");
    }
    int ncaptures = 0;
    for (Capture* c = node->captures; c; c = c->next) {
        ncaptures++;
    }
    if (ncaptures == 0) {
        printf("  mov rdi, 16\n");
        emit_call(new_label(), NULL, 6, "malloc");
        printf("  lea rcx, [rip + %.*s]\n", node->func->len, node->func->name);
        printf("  mov [rax], rcx\n");
        printf("  mov qword ptr [rax + 8], 0\n");
        printf("  push rax\n");
        return;
    }
    printf("  mov rdi, %d\n", 16 + 8 * ncaptures);
    emit_call(new_label(), NULL, 6, "malloc");
    printf("  lea rcx, [rip + %.*s]\n", node->func->len, node->func->name);
    printf("  mov [rax], rcx\n");
    printf("  lea rcx, [rax + 16]\n");
    printf("  mov [rax + 8], rcx\n");
    for (Capture* c = node->captures; c; c = c->next) {
        if (c->src_is_env) {
            printf("  mov rcx, [rbp-%d]\n", current_env_offset);
            printf("  mov rcx, [rcx + %d]\n", c->src_offset);
        } else {
            printf("  mov rcx, [rbp-%d]\n", c->src_offset);
        }
        printf("  mov [rax + %d], rcx\n", 16 + c->env_offset);
    }
    printf("  push rax\n");
}

// 値経由の呼び出し: 組からコードと環境を取り、環境は r10 で渡す
static void gen_vcall(Node* node) {
    gen(node->left_child);
    int args_cnt = 0;
    for (Node* arg = node->args; arg; arg = arg->next) {
        gen(arg);
        args_cnt++;
    }
    if (args_cnt > 6) {
        error_plain("関数に渡せる引数は6個までですが、%d個が指定されています", args_cnt);
    }
    pop_args(args_cnt, 0);
    printf("  pop rax\n");
    printf("  mov r11, [rax]\n");
    printf("  mov r10, [rax + 8]\n");
    emit_call_reg(new_label());
    printf("  push rax\n");
}

static void gen_member(Node* node) {
    gen(node->left_child);
    if (node->func) {
        printf("  pop rdi\n");
        emit_call(new_label(), node->func->klass, node->func->len, node->func->name);
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
    pop_args(args_cnt, 1);
    printf("  pop rdi\n");
    emit_call(new_label(), node->func->klass, node->func->len, node->func->name);
    printf("  push rax\n");
}

// `/` と `%` は Python 式（床除算、剰余は除数と同じ符号）。
// idiv は切り捨て除算なので、剰余が 0 でなく除数と符号が異なるときだけ直す。
// 呼び出し時点のレジスタ: rax = 商、rdx = 剰余、rdi = 除数。
static void emit_divmod_adjust(bool is_mod) {
    int label = new_label();
    printf("  test rdx, rdx\n");
    printf("  je .Ldivmod%d\n", label);
    printf("  mov rcx, rdx\n");
    printf("  xor rcx, rdi\n");
    printf("  jns .Ldivmod%d\n", label);
    if (is_mod) {
        printf("  add rdx, rdi\n");
    } else {
        printf("  sub rax, 1\n");
    }
    printf(".Ldivmod%d:\n", label);
}

static void gen_binop(Node* node) {
    if (node->op.binop == OP_AND) {
        int label = new_label();
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
        int label = new_label();
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
            emit_divmod_adjust(false);
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
            emit_divmod_adjust(true);
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
        case ND_GVAR:
        case ND_CAPVAR: gen_var(node); break;
        case ND_FUNCREF: gen_funcref(node); break;
        case ND_LAMBDA: gen_lambda(node); break;
        case ND_VCALL: gen_vcall(node); break;
        case ND_ASSIGN: gen_assign(node); break;
        case ND_NUM: gen_num(node); break;
        case ND_RETURN: gen_return(node); break;
        case ND_ARRAY: gen_array(node); break;
        case ND_INDEX: gen_index(node); break;
        case ND_LEN: gen_len(node); break;
        case ND_NEWARRAY: gen_newarray(node); break;
        case ND_IF: gen_if(node); break;
        case ND_WHILE: gen_while(node); break;
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

// ---- ランタイムヘルパ（step 59） ----
//
// 組み込みプリミティブの実体。prelude のラッパ関数が常に出力される以上、
// ヘルパも常に出す（prelude 本体と同じ扱い）。配列のレイアウトは
// コンパイラが作るものと同一（要素の8バイト手前に長さ）で、[int] の
// 文字コード配列と int だけを相手にする。libc のデータシンボルには
// 触らない（stderr へは fd 2 への write で出す）。一時バッファは
// 解放しない（生成コードは全体にそういう方針）
// ponytail: 一時バッファはリークする。ドライバ用途では問題にならない

// [int]（文字コード列）を NUL 終端のバイト列へ変換する。
// 入力 rdi = 文字コード配列、出力 rax = バイト列、rdx = 長さ
static void emit_rt_chars_to_bytes() {
    printf("__rt_spell_chars_to_bytes:\n");
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  push rbx\n");
    printf("  push r12\n");
    printf("  mov rbx, rdi\n");
    printf("  mov r12, [rdi-8]\n");
    printf("  lea rdi, [r12+1]\n");
    printf("  call malloc\n");
    printf("  xor rcx, rcx\n");
    printf(".Lrt_c2b_loop:\n");
    printf("  cmp rcx, r12\n");
    printf("  jae .Lrt_c2b_done\n");
    printf("  mov rdx, [rbx+rcx*8]\n");
    printf("  mov [rax+rcx], dl\n");
    printf("  inc rcx\n");
    printf("  jmp .Lrt_c2b_loop\n");
    printf(".Lrt_c2b_done:\n");
    printf("  mov byte ptr [rax+r12], 0\n");
    printf("  mov rdx, r12\n");
    printf("  pop r12\n");
    printf("  pop rbx\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

static void emit_rt_argc() {
    printf("__rt_spell_argc:\n");
    printf("  mov rax, [rip + __spell_argc]\n");
    printf("  ret\n");
}

// argv[i] を文字コード配列として返す。範囲外は空配列
static void emit_rt_argv_chars() {
    printf("__rt_spell_argv_chars:\n");
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  push rbx\n");
    printf("  push r12\n");
    printf("  push r13\n");
    printf("  push r14\n");
    printf("  mov rax, [rip + __spell_argv]\n");
    printf("  test rax, rax\n");
    printf("  je .Lrt_argv_empty\n");
    printf("  cmp rdi, 0\n");
    printf("  jl .Lrt_argv_empty\n");
    printf("  cmp rdi, [rip + __spell_argc]\n");
    printf("  jae .Lrt_argv_empty\n");
    printf("  mov rbx, [rax+rdi*8]\n");
    printf("  xor r12, r12\n");
    printf(".Lrt_argv_len:\n");
    printf("  cmp byte ptr [rbx+r12], 0\n");
    printf("  je .Lrt_argv_alloc\n");
    printf("  inc r12\n");
    printf("  jmp .Lrt_argv_len\n");
    printf(".Lrt_argv_alloc:\n");
    printf("  mov rdi, r12\n");
    printf("  imul rdi, 8\n");
    printf("  add rdi, 8\n");
    printf("  call malloc\n");
    printf("  mov r13, rax\n");
    printf("  mov [r13], r12\n");
    printf("  xor r14, r14\n");
    printf(".Lrt_argv_fill:\n");
    printf("  cmp r14, r12\n");
    printf("  jae .Lrt_argv_ret\n");
    printf("  movzx rcx, byte ptr [rbx+r14]\n");
    printf("  mov [r13+r14*8+8], rcx\n");
    printf("  inc r14\n");
    printf("  jmp .Lrt_argv_fill\n");
    printf(".Lrt_argv_ret:\n");
    printf("  lea rax, [r13+8]\n");
    printf("  jmp .Lrt_argv_epilogue\n");
    printf(".Lrt_argv_empty:\n");
    printf("  mov rdi, 8\n");
    printf("  call malloc\n");
    printf("  mov qword ptr [rax], 0\n");
    printf("  add rax, 8\n");
    printf(".Lrt_argv_epilogue:\n");
    printf("  pop r14\n");
    printf("  pop r13\n");
    printf("  pop r12\n");
    printf("  pop rbx\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

// ファイル全体を文字コード配列として読む。成否は __spell_io_status へ
// （1 = 読めた、0 = 開けない・読み取りエラー）。失敗時は空配列を返す
static void emit_rt_read_file_chars() {
    printf("__rt_spell_read_file_chars:\n");
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  push rbx\n");
    printf("  push r12\n");
    printf("  push r13\n");
    printf("  sub rsp, 24\n");
    printf("  call __rt_spell_chars_to_bytes\n");
    printf("  mov [rsp], rax\n");
    printf("  mov word ptr [rsp+8], 0x6272\n");
    printf("  mov byte ptr [rsp+10], 0\n");
    printf("  mov rdi, [rsp]\n");
    printf("  lea rsi, [rsp+8]\n");
    printf("  call fopen\n");
    printf("  test rax, rax\n");
    printf("  je .Lrt_read_fail\n");
    printf("  mov [rsp], rax\n");
    printf("  xor rbx, rbx\n");
    printf("  xor r12, r12\n");
    printf("  xor r13, r13\n");
    printf(".Lrt_read_loop:\n");
    printf("  mov rdi, [rsp]\n");
    printf("  call fgetc\n");
    printf("  cmp eax, -1\n");
    printf("  je .Lrt_read_eof\n");
    printf("  mov [rsp+8], eax\n");
    printf("  cmp r12, r13\n");
    printf("  jb .Lrt_read_store\n");
    printf("  cmp r13, 0\n");
    printf("  jne .Lrt_read_double\n");
    printf("  mov r13, 256\n");
    printf("  jmp .Lrt_read_grow\n");
    printf(".Lrt_read_double:\n");
    printf("  add r13, r13\n");
    printf(".Lrt_read_grow:\n");
    printf("  mov rdi, rbx\n");
    printf("  mov rsi, r13\n");
    printf("  call realloc\n");
    printf("  test rax, rax\n");
    printf("  je .Lrt_read_oom\n");
    printf("  mov rbx, rax\n");
    printf(".Lrt_read_store:\n");
    printf("  mov eax, [rsp+8]\n");
    printf("  mov [rbx+r12], al\n");
    printf("  inc r12\n");
    printf("  jmp .Lrt_read_loop\n");
    printf(".Lrt_read_eof:\n");
    printf("  mov rdi, [rsp]\n");
    printf("  call ferror\n");
    printf("  mov [rsp+16], rax\n");
    printf("  mov rdi, [rsp]\n");
    printf("  call fclose\n");
    printf("  mov rax, [rsp+16]\n");
    printf("  test rax, rax\n");
    printf("  jne .Lrt_read_fail\n");
    printf("  mov qword ptr [rip + __spell_io_status], 1\n");
    printf("  mov rdi, r12\n");
    printf("  imul rdi, 8\n");
    printf("  add rdi, 8\n");
    printf("  call malloc\n");
    printf("  mov [rax], r12\n");
    printf("  xor rcx, rcx\n");
    printf(".Lrt_read_fill:\n");
    printf("  cmp rcx, r12\n");
    printf("  jae .Lrt_read_ret\n");
    printf("  movzx rdx, byte ptr [rbx+rcx]\n");
    printf("  mov [rax+rcx*8+8], rdx\n");
    printf("  inc rcx\n");
    printf("  jmp .Lrt_read_fill\n");
    printf(".Lrt_read_ret:\n");
    printf("  add rax, 8\n");
    printf("  jmp .Lrt_read_epilogue\n");
    printf(".Lrt_read_oom:\n");
    printf("  mov rdi, [rsp]\n");
    printf("  call fclose\n");
    printf(".Lrt_read_fail:\n");
    printf("  mov qword ptr [rip + __spell_io_status], 0\n");
    printf("  mov rdi, 8\n");
    printf("  call malloc\n");
    printf("  mov qword ptr [rax], 0\n");
    printf("  add rax, 8\n");
    printf(".Lrt_read_epilogue:\n");
    printf("  add rsp, 24\n");
    printf("  pop r13\n");
    printf("  pop r12\n");
    printf("  pop rbx\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

static void emit_rt_io_ok() {
    printf("__rt_spell_io_ok:\n");
    printf("  mov rax, [rip + __spell_io_status]\n");
    printf("  ret\n");
}

// 文字コード列をファイルへ書く。戻り値と __spell_io_status に成否
// （1 = 全部書けて閉じられた、0 = 失敗）
static void emit_rt_write_file_chars() {
    printf("__rt_spell_write_file_chars:\n");
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  push rbx\n");
    printf("  push r12\n");
    printf("  push r13\n");
    printf("  push r14\n");
    printf("  push r15\n");
    printf("  sub rsp, 8\n");
    printf("  mov r15, rsi\n");
    printf("  call __rt_spell_chars_to_bytes\n");
    printf("  mov r14, rax\n");
    printf("  mov rdi, r15\n");
    printf("  call __rt_spell_chars_to_bytes\n");
    printf("  mov r12, rax\n");
    printf("  mov r13, rdx\n");
    printf("  mov word ptr [rsp], 0x6277\n");
    printf("  mov byte ptr [rsp+2], 0\n");
    printf("  mov rdi, r14\n");
    printf("  lea rsi, [rsp]\n");
    printf("  call fopen\n");
    printf("  mov rbx, rax\n");
    printf("  test rax, rax\n");
    printf("  je .Lrt_write_fail\n");
    printf("  mov rdi, r12\n");
    printf("  mov rsi, 1\n");
    printf("  mov rdx, r13\n");
    printf("  mov rcx, rbx\n");
    printf("  call fwrite\n");
    printf("  cmp rax, r13\n");
    printf("  jne .Lrt_write_bad\n");
    printf("  mov rdi, rbx\n");
    printf("  call fclose\n");
    printf("  cmp eax, 0\n");
    printf("  jne .Lrt_write_fail\n");
    printf("  mov qword ptr [rip + __spell_io_status], 1\n");
    printf("  mov rax, 1\n");
    printf("  jmp .Lrt_write_epilogue\n");
    printf(".Lrt_write_bad:\n");
    printf("  mov rdi, rbx\n");
    printf("  call fclose\n");
    printf(".Lrt_write_fail:\n");
    printf("  mov qword ptr [rip + __spell_io_status], 0\n");
    printf("  xor rax, rax\n");
    printf(".Lrt_write_epilogue:\n");
    printf("  add rsp, 8\n");
    printf("  pop r15\n");
    printf("  pop r14\n");
    printf("  pop r13\n");
    printf("  pop r12\n");
    printf("  pop rbx\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

// 文字コード列を fd 2（stderr）へ書く。write は部分的にしか書けない
// ことがあるので、全部書けるまで繰り返す。戻り値は書けたバイト数
static void emit_rt_stderr_chars() {
    printf("__rt_spell_stderr_chars:\n");
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  push rbx\n");
    printf("  push r12\n");
    printf("  push r13\n");
    printf("  sub rsp, 8\n");
    printf("  call __rt_spell_chars_to_bytes\n");
    printf("  mov rbx, rax\n");
    printf("  mov r12, rdx\n");
    printf("  xor r13, r13\n");
    printf(".Lrt_stderr_loop:\n");
    printf("  test r12, r12\n");
    printf("  je .Lrt_stderr_done\n");
    printf("  mov rdi, 2\n");
    printf("  mov rsi, rbx\n");
    printf("  mov rdx, r12\n");
    printf("  call write\n");
    printf("  cmp rax, 0\n");
    printf("  jle .Lrt_stderr_done\n");
    printf("  add rbx, rax\n");
    printf("  add r13, rax\n");
    printf("  sub r12, rax\n");
    printf("  jmp .Lrt_stderr_loop\n");
    printf(".Lrt_stderr_done:\n");
    printf("  mov rax, r13\n");
    printf("  add rsp, 8\n");
    printf("  pop r13\n");
    printf("  pop r12\n");
    printf("  pop rbx\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

// 終了コードは rdi。libc の exit を名前で呼ぶと、利用者が同名の関数を
// 定義していた場合にそちらへ束縛されてしまうため、fflush(NULL) で
// stdio を流してから _exit で終わる（生成コードは atexit を登録しない
// ので、観測できる挙動は exit と同じ）
static void emit_rt_exit() {
    printf("__rt_spell_exit:\n");
    printf("  push rdi\n");
    printf("  xor edi, edi\n");
    printf("  call fflush\n");
    printf("  pop rdi\n");
    printf("  sub rsp, 8\n");
    printf("  call _exit\n");
}

static void emit_runtime_helpers() {
    emit_rt_chars_to_bytes();
    emit_rt_argc();
    emit_rt_argv_chars();
    emit_rt_read_file_chars();
    emit_rt_io_ok();
    emit_rt_write_file_chars();
    emit_rt_stderr_chars();
    emit_rt_exit();
}

// ---- 関数ごとの出力 ----

void codegen(Function* prog) {
    bool has_init = false;
    for (Function* fn = prog; fn; fn = fn->next) {
        if (fn->len == 12 && !memcmp(fn->name, "__spell_init", 12)) {
            has_init = true;
        }
    }
    printf(".bss\n");
    printf("__spell_argc:\n");
    printf("  .zero 8\n");
    printf("__spell_argv:\n");
    printf("  .zero 8\n");
    printf("__spell_io_status:\n");
    printf("  .zero 8\n");
    if (globals) {
        for (LVar* var = globals; var; var = var->next) {
            printf("%.*s:\n", var->len, var->name);
            printf("  .zero 8\n");
        }
    }
    printf(".text\n");
    while (prog) {
        if (prog->klass && prog->klass->tparam_count > 0) {
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
        if (!prog->klass && prog->len == 4 && memcmp(prog->name, "main", 4) == 0) {
            // プロセスの入口: argc（rdi）と argv（rsi）を、プリミティブが
            // 読めるよう .bss へ退避する。引数レジスタはこの後も書き換え
            // られるため、プロローグより前に置く
            printf("  mov [rip + __spell_argc], rdi\n");
            printf("  mov [rip + __spell_argv], rsi\n");
        }
        printf("  push rbp\n");
        printf("  mov rbp, rsp\n");
        printf("  sub rsp, %d\n", prog->stack_size);
        current_env_offset = prog->is_lambda ? prog->env_offset : 0;
        if (prog->is_lambda) {
            // 隠し引数（r10）の環境ポインタをフレームへ退避する
            printf("  mov [rbp-%d], r10\n", prog->env_offset);
        }
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
        if (!prog->klass && prog->len == 4 && memcmp(prog->name, "main", 4) == 0) {
            // C の main と同じく、末尾まで落ちたら 0 を返す。return 文は
            // 独自のエピローグを持つので、ここは到達しない経路には影響しない
            printf("  mov rax, 0\n");
        }
        printf("  mov rsp, rbp\n");
        printf("  pop rbp\n");
        printf("  ret\n");
        prog = prog->next;
    }
    emit_runtime_helpers();
}
