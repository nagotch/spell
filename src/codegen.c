// コード生成: 型検査済みの AST から x86-64 アセンブリ（GAS 形式）を標準出力へ出す。
// 値の受け渡しは3通りある:
//   gen(node)       … 式の値を 1 つ push した状態で戻る（従来のスタックマシン方式）
//   gen_rax(node)   … 式の値を rax に入れて戻る（スタック増減なし）
//   gen_discard(node) … 文として評価し、値は捨てる（スタック増減なし。評価自体は省略しない）
// 移行済みのノード種別では gen は gen_rax + push として定義され、
// gen_rax のフォールバックは未移行の種別に対してだけ gen + pop を使う（片方向）。
#include "spell.h"

static void gen(Node* node);
static void gen_rax(Node* node);
static void gen_discard(Node* node);
static void gen_push(Node* node);
static void gen_rax_num(Node* node);
static bool fits_imm32(long val);
static Function* find_plain_function(char* name, int len);

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

// ---- ユーザー由来シンボルの接頭辞（step 78） ----
//
// グローバル変数と素の関数の名前は、そのままだと asm のラベルや
// [rip + 名前] の参照になる。名前が x86 のレジスタ名（si, sp, ax,
// rip, xmm0 など）と一致すると、アセンブラが組み立てを拒否したり、
// `call si` がレジスタ間接呼び出しに化けて実行時に落ちたりする。
// そこでユーザー由来のシンボルには種別ごとの接頭辞を付ける:
//   グローバル変数 … spell_g_<名前>
//   素の関数       … spell_f_<名前>
// 接頭辞まで含めた形は、ユーザー識別子にもレジスタ名にもコンパイラ
// 内部のシンボル（__spell_* / __rt_spell_*）にも一致しないため、
// 接頭辞を付ける変換は単射になる。種別ごとに接頭辞を分けるのは、
// 変数と関数が同名でも衝突しないようにするため。
// メソッドのラベルは `クラス.メソッド` で必ずドットを含み、
// ジェネリクスの実体化ラベルは `$` を含むので、どちらもレジスタ名
// にはなり得ず対象外。main と __spell_init は、リンカ（C の入口）と
// ドライバ（main の本体から __spell_init を呼ぶ）の都合で変えない。
// libc の外部シンボル（malloc 等）も変えない

static bool is_entry_symbol(int len, char* name) {
    return (len == 4 && !memcmp(name, "main", 4))
        || (len == 12 && !memcmp(name, "__spell_init", 12));
}

// 素の関数のラベルを出す（定義・呼び出し・関数参照で共用する）
static void print_func_label(int len, char* name) {
    if (is_entry_symbol(len, name)) {
        printf("%.*s", len, name);
    } else {
        printf("spell_f_%.*s", len, name);
    }
}

// グローバル変数のラベルを出す（定義と全ての参照で共用する）
static void print_global_label(int len, char* name) {
    printf("spell_g_%.*s", len, name);
}

// [rip + グローバル変数] の参照を出す
static void print_global_ref(int len, char* name) {
    printf("[rip + ");
    print_global_label(len, name);
    printf("]");
}

// 名前がプログラム内の素の関数（prelude の関数を含む）を指すか。
// 指さない名前は libc 側の外部シンボルとしてそのまま出す
static bool is_plain_func_name(int len, char* name) {
    return find_plain_function(name, len) != NULL;
}

// rsp の 16 バイト境界をそろえてから call する。klass があればメソッド呼び出し。
// mangle が真なら、素の関数のラベルとして接頭辞つきで出す
static void print_call_target(Class* klass, int len, char* name, bool mangle) {
    printf("  call ");
    if (klass) {
        print_klass_label(klass);
        printf(".");
        printf("%.*s\n", len, name);
    } else if (mangle) {
        print_func_label(len, name);
        printf("\n");
    } else {
        printf("%.*s\n", len, name);
    }
}

static void emit_call_mangled(int label, Class* klass, int len, char* name, bool mangle) {
    printf("  mov rax, rsp\n");
    printf("  and rax, 15\n");
    printf("  jnz .Lcall%d\n", label);
    printf("  mov rax, 0\n");
    print_call_target(klass, len, name, mangle);
    printf("  jmp .Lend%d\n", label);
    printf(".Lcall%d:\n", label);
    printf("  sub rsp, 8\n");
    printf("  mov rax, 0\n");
    print_call_target(klass, len, name, mangle);
    printf("  add rsp, 8\n");
    printf(".Lend%d:\n", label);
}

static void emit_call(int label, Class* klass, int len, char* name) {
    emit_call_mangled(label, klass, len, name, false);
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
        printf("  lea rax, ");
        print_global_ref(node->len, node->name);
        printf("\n");
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

static void gen_array(Node* node) {
    if (node->array_len == 0) {
        // 空のリテラルはヒープに置く。フレーム上に置くと、関数を抜けた
        // 後まで保持される場合（フィールドへの代入など）に壊れる。
        // ブロックは [cap][len][要素...]（step 65）。cap は要素0の
        // 16バイト手前、len は 8バイト手前で、生成時は cap = len
        int label = new_label();
        printf("  mov rdi, 16\n");
        emit_call(label, NULL, 6, "malloc");
        printf("  mov rcx, 0\n");
        printf("  mov [rax], rcx\n");
        printf("  mov [rax+8], rcx\n");
        printf("  add rax, 16\n");
        printf("  push rax\n");
        return;
    }
    // 長さと容量（= 長さ）を、要素0から見て -8 と -16 の位置へ書く
    printf("  mov rax, %d\n", node->array_len);
    printf("  mov [rbp-%d], rax\n", node->offset + 8 * node->array_len);
    printf("  mov [rbp-%d], rax\n", node->offset + 8 * (node->array_len + 1));
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

// 配列への append（step 65）。新しい長さを push して戻る。
// 評価順序は仕様で固定: 対象スロットのアドレスを1回だけ計算 → 値を
// 評価 → スロットから現在の配列ポインタを読み直す。値を評価する間に
// 同じスロットへ別の append が走っても、読み直しで新しいポインタと
// 長さが見える。容量内なら共有ヘッダの長さを増やすだけで、別名からも
// 伸びが見える。容量超過時は realloc を使わず（動くと別名が宙に浮く）、
// 新しいブロックへコピーして対象スロットにだけ書き戻す。古いブロック
// は解放しない（生成コードは全体にそういう方針）
// レジスタ: rbx = スロットのアドレス、r12 = 追加する値、
// r13 = 配列ポインタ、r14 = 長さ、r15 = 新しい容量（成長時）。
// malloc の呼び出しをまたぐので、全部 callee-saved に置く
static void gen_append(Node* node) {
    gen_addr(node->left_child);
    gen(node->args);
    printf("  pop rdi\n");
    printf("  pop rax\n");
    printf("  push rbx\n");
    printf("  push r12\n");
    printf("  push r13\n");
    printf("  push r14\n");
    printf("  push r15\n");
    printf("  mov rbx, rax\n");
    printf("  mov r12, rdi\n");
    printf("  mov r13, [rbx]\n");
    printf("  mov r14, [r13-8]\n");
    int label = new_label();
    printf("  cmp r14, [r13-16]\n");
    printf("  jb .Lappend_fast%d\n", label);
    printf("  mov r15, [r13-16]\n");
    printf("  add r15, r15\n");
    printf("  cmp r15, 8\n");
    printf("  jae .Lappend_cap%d\n", label);
    printf("  mov r15, 8\n");
    printf(".Lappend_cap%d:\n", label);
    printf("  mov rdi, r15\n");
    printf("  imul rdi, 8\n");
    printf("  add rdi, 16\n");
    emit_call(new_label(), NULL, 6, "malloc");
    printf("  mov [rax], r15\n");
    printf("  mov [rax+8], r14\n");
    printf("  xor rcx, rcx\n");
    printf(".Lappend_copy%d:\n", label);
    printf("  cmp rcx, r14\n");
    printf("  jae .Lappend_copied%d\n", label);
    printf("  mov rdx, [r13+rcx*8]\n");
    printf("  mov [rax+rcx*8+16], rdx\n");
    printf("  inc rcx\n");
    printf("  jmp .Lappend_copy%d\n", label);
    printf(".Lappend_copied%d:\n", label);
    printf("  mov [rax+r14*8+16], r12\n");
    printf("  inc r14\n");
    printf("  mov [rax+8], r14\n");
    printf("  add rax, 16\n");
    printf("  mov [rbx], rax\n");
    printf("  jmp .Lappend_done%d\n", label);
    printf(".Lappend_fast%d:\n", label);
    printf("  mov [r13+r14*8], r12\n");
    printf("  inc r14\n");
    printf("  mov [r13-8], r14\n");
    printf(".Lappend_done%d:\n", label);
    printf("  mov rax, r14\n");
    printf("  pop r15\n");
    printf("  pop r14\n");
    printf("  pop r13\n");
    printf("  pop r12\n");
    printf("  pop rbx\n");
    printf("  push rax\n");
}

static void gen_len(Node* node) {
    gen(node->left_child);
    printf("  pop rax\n");
    printf("  mov rax, [rax-8]\n");
    printf("  push rax\n");
}

// [fill; n] の fill が配列リテラルの形（ND_ARRAY / ND_NEWARRAY）の
// 場合の生成（step 82）。fill を1回だけ評価し、各スロットへは
// __rt_spell_copy_array で構造の複製を入れる。リテラルは「値の雛形」
// であって1つの配列オブジェクトの名前ではないため、`[[-1; w]; h]`
// のようなグリッド構築で行同士が同じ配列を共有すると、書き込みが
// 全行へ伝播して静かに壊れる（step 75 の個数形が小配列を共有しない
// のと同じ線引き）。リテラルでない fill（変数や呼び出しの結果）は
// 従来どおり、評価した1つの値を全スロットへ入れる
static void gen_newarray_fresh(Node* node) {
    // 複製の深さ: fill の型の配列の入れ子段数 - 1（fill の値そのものの
    // ブロックは常時複製し、内側の配列を何段まで複製するか）
    int depth = 0;
    for (Type* t = node->left_child->type; t && t->kind == TY_ARRAY; t = t->elem) {
        depth++;
    }
    if (depth > 0) {
        depth--;
    }
    printf("  push rbx\n");
    printf("  push r12\n");
    printf("  push r13\n");
    printf("  push r14\n");
    gen(node->left_child);
    gen(node->right_child);
    printf("  pop rax\n");
    // 負の長さは空配列として扱う（そのまま確保サイズの計算へ入れると
    // ヘッダの書き込みが確保領域を超えるため。step 82 で追加）
    int clamp_label = new_label();
    printf("  cmp rax, 0\n");
    printf("  jge .Lend%d\n", clamp_label);
    printf("  mov rax, 0\n");
    printf(".Lend%d:\n", clamp_label);
    printf("  push rax\n");
    printf("  add rax, 2\n");
    printf("  imul rax, 8\n");
    printf("  mov rdi, rax\n");
    int label = new_label();
    emit_call(label, NULL, 6, "malloc");
    printf("  pop r12\n");
    // ブロックは [cap][len][要素...]（step 65）。生成時は cap = len
    printf("  mov [rax], r12\n");
    printf("  mov [rax+8], r12\n");
    printf("  pop rbx\n");
    printf("  add rax, 16\n");
    printf("  mov r13, rax\n");
    printf("  mov r14, rax\n");
    label = new_label();
    printf(".Lbegin%d:\n", label);
    printf("  cmp r12, 0\n");
    printf("  jle .Lend%d\n", label);
    printf("  mov rdi, rbx\n");
    printf("  mov rsi, %d\n", depth);
    int call_label = new_label();
    emit_call(call_label, NULL, 21, "__rt_spell_copy_array");
    printf("  mov [r13], rax\n");
    printf("  add r13, 8\n");
    printf("  sub r12, 1\n");
    printf("  jmp .Lbegin%d\n", label);
    printf(".Lend%d:\n", label);
    printf("  mov rax, r14\n");
    printf("  pop r14\n");
    printf("  pop r13\n");
    printf("  pop r12\n");
    printf("  pop rbx\n");
    printf("  push rax\n");
}

static void gen_newarray(Node* node) {
    if (node->left_child->kind == ND_ARRAY || node->left_child->kind == ND_NEWARRAY) {
        gen_newarray_fresh(node);
        return;
    }
    int label = new_label();
    gen(node->left_child);
    gen(node->right_child);
    printf("  pop rax\n");
    // 負の長さは空配列として扱う（gen_newarray_fresh と同じ。step 82）
    printf("  cmp rax, 0\n");
    printf("  jge .Lend%d\n", label);
    printf("  mov rax, 0\n");
    printf(".Lend%d:\n", label);
    label = new_label();
    printf("  push rax\n");
    printf("  add rax, 2\n");
    printf("  imul rax, 8\n");
    printf("  mov rdi, rax\n");
    printf("  mov rdi, rax\n");
    emit_call(label, NULL, 6, "malloc");
    printf("  pop rcx\n");
    // ブロックは [cap][len][要素...]（step 65）。生成時は cap = len
    printf("  mov [rax], rcx\n");
    printf("  mov [rax+8], rcx\n");
    printf("  pop rsi\n");
    printf("  add rax, 16\n");
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

// ---- 呼び出しの引数（step 61） ----
//
// 引数を評価して arg_regs[offset..] へ入れる。評価順は従来どおり左から右。
// レジスタへ書くのは全引数の評価が終わってから（従来の pop_args と同じ
// 時点）なので、後の引数の評価が前の引数を壊すことはない。ただし:
//  - 全引数がリーフ（即値に収まるリテラル・単純なローカル/グローバル
//    読み）なら副作用もスタック操作も無いので、直接レジスタへ mov する
//  - 引数が 1 個だけなら rax 経由で渡す（push / pop の往復を省く）
//  - それ以外は最後の引数だけ rax に残し、残りを従来どおり push する
static bool arg_is_leaf(Node* arg) {
    if (arg->kind == ND_NUM) {
        return fits_imm32(arg->val);
    }
    return arg->kind == ND_LVAR || arg->kind == ND_GVAR;
}

static void emit_leaf_to_reg(Node* arg, char* reg) {
    if (arg->kind == ND_NUM) {
        printf("  mov %s, %ld\n", reg, arg->val);
    } else if (arg->kind == ND_LVAR) {
        printf("  mov %s, [rbp-%d]\n", reg, arg->offset);
    } else {
        printf("  mov %s, ", reg);
        print_global_ref(arg->len, arg->name);
        printf("\n");
    }
}

static void gen_call_args(Node* args, int n, int offset) {
    if (n == 0) {
        return;
    }
    bool all_leaf = true;
    for (Node* a = args; a; a = a->next) {
        if (!arg_is_leaf(a)) {
            all_leaf = false;
            break;
        }
    }
    if (all_leaf) {
        int i = 0;
        for (Node* a = args; a; a = a->next) {
            emit_leaf_to_reg(a, arg_regs[offset + i]);
            i++;
        }
        return;
    }
    if (n == 1) {
        gen_rax(args);
        printf("  mov %s, rax\n", arg_regs[offset]);
        return;
    }
    Node* last = args;
    while (last->next) {
        last = last->next;
    }
    for (Node* a = args; a != last; a = a->next) {
        gen(a);
    }
    gen_rax(last);
    pop_args(n - 1, offset);
    printf("  mov %s, rax\n", arg_regs[offset + n - 1]);
}

// __init__ の本体が「self.<フィールド> = 引数かリテラル」という代入文
// だけからなるか。構築地点では calloc の直後に引数がレジスタへ揃うので、
// その形なら __init__ を呼ばずに代入文をそのまま書き込める。少しでも
// 違う形（条件分岐・呼び出しを含む右辺・他オブジェクトへの代入など）
// なら false を返し、従来どおり __init__ を呼ぶ（保守的な判定）
// ponytail: 対象は引数とリテラルだけ。引数への単純な演算まで広げるなら
// 評価用レジスタの割り当てが要るので、実測で必要になってから
static bool init_is_trivial(Function* init, int nargs) {
    if (!init) {
        return false;
    }
    for (Node* st = init->body; st; st = st->next) {
        if (st->kind != ND_ASSIGN) {
            return false;
        }
        Node* lhs = st->left_child;
        if (lhs->kind != ND_MEMBER || lhs->func) {
            return false;
        }
        Node* self = lhs->left_child;
        if (self->kind != ND_LVAR || self->offset != 8) {
            return false;
        }
        Node* rhs = st->right_child;
        if (rhs->kind == ND_NUM) {
            continue;
        }
        if (rhs->kind == ND_LVAR && rhs->offset % 8 == 0) {
            // 引数 p（self が 0）の置き場所はオフセット (p+1)*8
            int p = rhs->offset / 8 - 1;
            if (1 <= p && p <= nargs) {
                continue;
            }
        }
        return false;
    }
    return true;
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
    bool inlined = init_is_trivial(node->func, args_cnt);
    printf("  mov rdi, rax\n");
    pop_args(args_cnt, 1);
    if (inlined) {
        // calloc のゼロ埋めの上に __init__ の代入文をそのまま書き込む。
        // 引数 p は arg_regs[p] にある（self が rdi のため 1 つずれる）。
        // 代入文の順序も、同じフィールドへの再代入の上書きも、呼び出し時
        // と同じになる
        for (Node* st = node->func->body; st; st = st->next) {
            Node* rhs = st->right_child;
            int foff = st->left_child->offset;
            if (rhs->kind == ND_LVAR) {
                printf("  mov [rdi + %d], %s\n", foff, arg_regs[rhs->offset / 8 - 1]);
            } else if (fits_imm32(rhs->val)) {
                printf("  mov qword ptr [rdi + %d], %ld\n", foff, rhs->val);
            } else {
                gen_rax_num(rhs);
                printf("  mov [rdi + %d], rax\n", foff);
            }
        }
    }
    printf("  push rdi\n");
    if (node->func && !inlined) {
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
        int args_cnt = 0;
        for (Node* a = node->args; a; a = a->next) {
            args_cnt++;
        }
        gen_call_args(node->args, args_cnt, 0);
        char* rt = builtin_rt_name(node->builtin);
        emit_call(label, NULL, (int)strlen(rt), rt);
        printf("  push rax\n");
        return;
    }
    int label = new_label();
    int args_cnt = 0;
    for (Node* a = node->args; a; a = a->next) {
        args_cnt++;
    }
    if (args_cnt > 6) {
        error_plain("関数に渡せる引数は6個までですが、%d個が指定されています", args_cnt);
    }
    gen_call_args(node->args, args_cnt, 0);
    // プログラム内の素の関数なら接頭辞つきラベルへ、そうでなければ
    // libc の外部シンボル（getchar 等）としてそのまま呼ぶ（step 78）
    emit_call_mangled(label, NULL, node->len, node->name,
                      is_plain_func_name(node->len, node->name));
    if (node->len == 7 && (!memcmp(node->name, "getchar", 7) || !memcmp(node->name, "putchar", 7))) {
        printf("  movsxd rax, eax\n");
    }
    printf("  push rax\n");
}

// 関数値の組（[0]=コード、[8]=環境ポインタ）をヒープに作って push する
static void gen_funcref(Node* node) {
    printf("  mov rdi, 16\n");
    emit_call(new_label(), NULL, 6, "malloc");
    printf("  lea rcx, [rip + ");
    if (is_plain_func_name(node->len, node->name)) {
        print_func_label(node->len, node->name);
    } else {
        printf("%.*s", node->len, node->name);
    }
    printf("]\n");
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
        printf("  lea rcx, [rip + ");
        print_func_label(node->func->len, node->func->name);
        printf("]\n");
        printf("  mov [rax], rcx\n");
        printf("  mov qword ptr [rax + 8], 0\n");
        printf("  push rax\n");
        return;
    }
    printf("  mov rdi, %d\n", 16 + 8 * ncaptures);
    emit_call(new_label(), NULL, 6, "malloc");
    printf("  lea rcx, [rip + ");
    print_func_label(node->func->len, node->func->name);
    printf("]\n");
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
        args_cnt++;
    }
    if (args_cnt > 6) {
        error_plain("関数に渡せる引数は6個までですが、%d個が指定されています", args_cnt);
    }
    gen_call_args(node->args, args_cnt, 0);
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
        args_cnt++;
    }
    if (args_cnt > 5) {
        error_plain("メソッドに渡せる引数は5個までですが、%d個が指定されています", args_cnt);
    }
    gen_call_args(node->args, args_cnt, 1);
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

// ---- rax 経路（step 60） ----
//
// 移行済みのノード種別は、値をスタックではなく rax で受け渡す。
// 不変条件: gen_rax / gen_discard はどの実行経路でもスタック増減ゼロで戻る。
// レジスタ規約: rax = 結果、rcx = 二項演算の右辺の一時置き場。
// rcx は再帰的な生成をまたいで生きていることは決してない（右辺の生成が
// 終わった直後にだけ使い、生成の前に必要な値は push で退避する）。

// 符号付き 32 ビット即値に収まるか（push / mov の符号拡張即値と同じ範囲）
static bool fits_imm32(long val) {
    return val >= INT32_MIN && val <= INT32_MAX;
}

static void gen_rax_num(Node* node) {
    if (fits_imm32(node->val)) {
        printf("  mov rax, %ld\n", node->val);
        return;
    }
    // 即値に収まらないリテラルは従来の push 経路のままにする
    // （`push` の大きな即値をアセンブラが受け付けない点まで含め、挙動を変えない）
    gen_num(node);
    printf("  pop rax\n");
}

static void gen_rax_var(Node* node) {
    if (node->kind == ND_LVAR) {
        printf("  mov rax, [rbp-%d]\n", node->offset);
        return;
    }
    if (node->kind == ND_GVAR) {
        printf("  mov rax, ");
        print_global_ref(node->len, node->name);
        printf("\n");
        return;
    }
    // ND_CAPVAR: フレームに退避済みの環境ポインタ経由で読む
    printf("  mov rax, [rbp-%d]\n", current_env_offset);
    printf("  mov rax, [rax + %d]\n", node->offset);
}

static void gen_rax_assign(Node* node) {
    Node* left = node->left_child;
    if (left->kind == ND_LVAR) {
        // 単純なローカル代入: アドレスは rbp からの定数オフセットで副作用が
        // ないため、右辺を rax で評価してそのままストアできる
        gen_rax(node->right_child);
        printf("  mov [rbp-%d], rax\n", left->offset);
        return;
    }
    if (left->kind == ND_GVAR) {
        // グローバルのアドレスも同様に副作用がない
        gen_rax(node->right_child);
        printf("  mov ");
        print_global_ref(left->len, left->name);
        printf(", rax\n");
        return;
    }
    // 複雑な左辺（メンバ・添字・捕捉変数など）は従来の push 経路のまま。
    // 左辺のアドレス計算が右辺より先に走る評価順も従来どおり
    // ponytail: 複雑な左辺の rax 化は次ステップ送り
    gen_assign(node);
    printf("  pop rax\n");
}

static void gen_rax_return(Node* node) {
    gen_rax(node->left_child);
    printf("  mov rsp, rbp\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

static void gen_rax_if(Node* node) {
    int label = new_label();
    gen_rax(node->cond);
    printf("  cmp rax, 0\n");
    printf("  je .Lelse%d\n", label);
    gen_rax(node->then);
    printf("  jmp .Lend%d\n", label);
    printf(".Lelse%d:\n", label);
    if (node->els) {
        gen_rax(node->els);
    } else {
        printf("  mov rax, 0\n");
    }
    printf(".Lend%d:\n", label);
}

static void gen_rax_while(Node* node) {
    int label = new_label();
    int saved_break = break_label;
    int saved_continue = continue_label;
    break_label = label;
    continue_label = label;
    printf(".Lbegin%d:\n", label);
    printf(".Lcont%d:\n", label);
    gen_rax(node->cond);
    printf("  cmp rax, 0\n");
    printf("  je .Lend%d\n", label);
    gen_discard(node->then);
    break_label = saved_break;
    continue_label = saved_continue;
    printf("  jmp .Lbegin%d\n", label);
    printf(".Lend%d:\n", label);
    printf("  mov rax, 0\n");
}

static void gen_rax_block(Node* node) {
    Node* tail = node->body;
    if (!tail) {
        // 空ブロックの値は従来どおり「その時点の rax」（push rax していた）
        return;
    }
    while (tail->next) {
        gen_discard(tail);
        tail = tail->next;
    }
    gen_rax(tail);
}

// 除算・剰余以外の二項演算を `op rax, <operand>` の形で出す。
// operand は即値・ローカルのメモリ参照・rcx のいずれかで、
// 読み取りは左辺の評価が終わった後に行われる（従来の評価順と同じ）
static void emit_binop_operand(BinOp op, char* operand) {
    switch (op) {
        case OP_ADD:
            printf("  add rax, %s\n", operand);
            break;
        case OP_SUB:
            printf("  sub rax, %s\n", operand);
            break;
        case OP_MUL:
            printf("  imul rax, %s\n", operand);
            break;
        case OP_EQ:
            printf("  cmp rax, %s\n", operand);
            printf("  sete al\n");
            printf("  movzb rax, al\n");
            break;
        case OP_NE:
            printf("  cmp rax, %s\n", operand);
            printf("  setne al\n");
            printf("  movzb rax, al\n");
            break;
        case OP_LT:
            printf("  cmp rax, %s\n", operand);
            printf("  setl al\n");
            printf("  movzb rax, al\n");
            break;
        case OP_LE:
            printf("  cmp rax, %s\n", operand);
            printf("  setle al\n");
            printf("  movzb rax, al\n");
            break;
        default:
            error_internal("二項演算の処理で想定外の演算子を検出しました");
            break;
    }
}

static void gen_rax_binop(Node* node) {
    if (node->op.binop == OP_AND) {
        int label = new_label();
        gen_rax(node->left_child);
        printf("  cmp rax, 0\n");
        printf("  je .Lfalse%d\n", label);
        gen_rax(node->right_child);
        printf("  cmp rax, 0\n");
        printf("  je .Lfalse%d\n", label);
        printf("  mov rax, 1\n");
        printf("  jmp .Lend%d\n", label);
        printf(".Lfalse%d:\n", label);
        printf("  mov rax, 0\n");
        printf(".Lend%d:\n", label);
        return;
    } else if (node->op.binop == OP_OR) {
        int label = new_label();
        gen_rax(node->left_child);
        printf("  cmp rax, 0\n");
        printf("  jne .Ltrue%d\n", label);
        gen_rax(node->right_child);
        printf("  cmp rax, 0\n");
        printf("  jne .Ltrue%d\n", label);
        printf("  mov rax, 0\n");
        printf("  jmp .Lend%d\n", label);
        printf(".Ltrue%d:\n", label);
        printf("  mov rax, 1\n");
        printf(".Lend%d:\n", label);
        return;
    }
    Node* right = node->right_child;
    bool divmod = node->op.binop == OP_DIV || node->op.binop == OP_MOD;
    gen_rax(node->left_child);
    if (!divmod && right->kind == ND_NUM && fits_imm32(right->val)) {
        // 右辺がリテラルなら即値オペランドにする（評価するものはない）
        char operand[32];
        snprintf(operand, sizeof(operand), "%ld", right->val);
        emit_binop_operand(node->op.binop, operand);
        return;
    }
    if (!divmod && right->kind == ND_LVAR) {
        // 右辺が単純なローカル読みならメモリオペランドにする。
        // 読み取りは左辺の評価の後なので、左辺が同じ変数を書き換えても
        // 従来どおり新しい値が見える
        char operand[32];
        snprintf(operand, sizeof(operand), "[rbp-%d]", right->offset);
        emit_binop_operand(node->op.binop, operand);
        return;
    }
    if (divmod && right->kind == ND_NUM && fits_imm32(right->val)) {
        printf("  mov rdi, %ld\n", right->val);
    } else if (divmod && right->kind == ND_LVAR) {
        printf("  mov rdi, [rbp-%d]\n", right->offset);
    } else {
        // 一般形: 左辺を退避してから右辺を評価し、rcx へ移す
        printf("  push rax\n");
        gen_rax(right);
        printf("  mov rcx, rax\n");
        printf("  pop rax\n");
        if (!divmod) {
            emit_binop_operand(node->op.binop, "rcx");
            return;
        }
        printf("  mov rdi, rcx\n");
    }
    // ここへ来るのは除算・剰余だけ。除数は rdi にあり、rax が被除数
    printf("  cqo\n");
    printf("  idiv rdi\n");
    emit_divmod_adjust(node->op.binop == OP_MOD);
    if (node->op.binop == OP_MOD) {
        printf("  mov rax, rdx\n");
    }
}

// 値を rax に入れて戻る（スタック増減なし）。
// 移行済みの種別だけをここで扱い、それ以外は gen + pop に落とす。
// gen は移行済みの種別に対してだけ gen_rax を呼ぶので、互いに
// 再帰し合うことはない（種別ごとに片方向）
static void gen_rax(Node* node) {
    switch (node->kind) {
        case ND_NUM: gen_rax_num(node); break;
        case ND_BOOL: gen_rax_num(node); break;
        case ND_LVAR:
        case ND_GVAR:
        case ND_CAPVAR: gen_rax_var(node); break;
        case ND_ASSIGN: gen_rax_assign(node); break;
        case ND_RETURN: gen_rax_return(node); break;
        case ND_IF: gen_rax_if(node); break;
        case ND_WHILE: gen_rax_while(node); break;
        case ND_BLOCK: gen_rax_block(node); break;
        case ND_BINOP: gen_rax_binop(node); break;
        default:
            gen(node);
            printf("  pop rax\n");
            break;
    }
}

// 文として評価し、値を捨てる（スタック増減なし）。評価自体は省略しない
static void gen_discard(Node* node) {
    if (node->kind == ND_BLOCK) {
        for (Node* tail = node->body; tail; tail = tail->next) {
            gen_discard(tail);
        }
        return;
    }
    gen_rax(node);
}

// 値を 1 つ push して戻る（従来の契約）。
// 移行済みの種別は gen_rax + push として定義し、それ以外は gen_push に落とす
static void gen(Node* node) {
    switch (node->kind) {
        case ND_NUM:
        case ND_BOOL:
        case ND_LVAR:
        case ND_GVAR:
        case ND_CAPVAR:
        case ND_ASSIGN:
        case ND_IF:
        case ND_WHILE:
        case ND_BLOCK:
        case ND_BINOP:
            gen_rax(node);
            printf("  push rax\n");
            break;
        case ND_RETURN:
            // return は値を push しない（従来どおり独自のエピローグで戻る）
            gen_rax(node);
            break;
        default:
            gen_push(node);
            break;
    }
}

// 未移行の種別だけを扱う従来の push 方式の生成。
// 移行済みの種別は gen が先に捌くので、ここへは来ない
static void gen_push(Node* node) {
    switch (node->kind) {
        case ND_UNARY: gen_unary(node); break;
        case ND_FUNCREF: gen_funcref(node); break;
        case ND_LAMBDA: gen_lambda(node); break;
        case ND_VCALL: gen_vcall(node); break;
        case ND_ARRAY: gen_array(node); break;
        case ND_APPEND: gen_append(node); break;
        case ND_INDEX: gen_index(node); break;
        case ND_LEN: gen_len(node); break;
        case ND_NEWARRAY: gen_newarray(node); break;
        case ND_FUNCALL: gen_funcall(node); break;
        case ND_MEMBER: gen_member(node); break;
        case ND_MCALL: gen_mcall(node); break;
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
// 組み込みプリミティブの実体。到達可能なコードが使うプリミティブに
// 対応するものだけを出す（step 63）。配列のレイアウトは
// コンパイラが作るものと同一（要素の16バイト手前に容量、8バイト手前に
// 長さ。生成時は cap = len）で、[int] の
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
    printf("  add rdi, 16\n");
    printf("  call malloc\n");
    printf("  mov r13, rax\n");
    printf("  mov [r13], r12\n");
    printf("  mov [r13+8], r12\n");
    printf("  xor r14, r14\n");
    printf(".Lrt_argv_fill:\n");
    printf("  cmp r14, r12\n");
    printf("  jae .Lrt_argv_ret\n");
    printf("  movzx rcx, byte ptr [rbx+r14]\n");
    printf("  mov [r13+r14*8+16], rcx\n");
    printf("  inc r14\n");
    printf("  jmp .Lrt_argv_fill\n");
    printf(".Lrt_argv_ret:\n");
    printf("  lea rax, [r13+16]\n");
    printf("  jmp .Lrt_argv_epilogue\n");
    printf(".Lrt_argv_empty:\n");
    printf("  mov rdi, 16\n");
    printf("  call malloc\n");
    printf("  mov qword ptr [rax], 0\n");
    printf("  mov qword ptr [rax+8], 0\n");
    printf("  add rax, 16\n");
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
    printf("  add rdi, 16\n");
    printf("  call malloc\n");
    printf("  mov [rax], r12\n");
    printf("  mov [rax+8], r12\n");
    printf("  xor rcx, rcx\n");
    printf(".Lrt_read_fill:\n");
    printf("  cmp rcx, r12\n");
    printf("  jae .Lrt_read_ret\n");
    printf("  movzx rdx, byte ptr [rbx+rcx]\n");
    printf("  mov [rax+rcx*8+16], rdx\n");
    printf("  inc rcx\n");
    printf("  jmp .Lrt_read_fill\n");
    printf(".Lrt_read_ret:\n");
    printf("  add rax, 16\n");
    printf("  jmp .Lrt_read_epilogue\n");
    printf(".Lrt_read_oom:\n");
    printf("  mov rdi, [rsp]\n");
    printf("  call fclose\n");
    printf(".Lrt_read_fail:\n");
    printf("  mov qword ptr [rip + __spell_io_status], 0\n");
    printf("  mov rdi, 16\n");
    printf("  call malloc\n");
    printf("  mov qword ptr [rax], 0\n");
    printf("  mov qword ptr [rax+8], 0\n");
    printf("  add rax, 16\n");
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

// used は SpellBuiltin を添字とする使用ビット（到達可能性の走査で集める）。
// chars_to_bytes は3つのヘルパから呼ばれる部品で、それ自体は
// プリミティブではないため、呼び手の使用ビットから出すかどうかを決める
// 配列ブロックの構造複製（step 82）: rdi = 要素0の位置、
// rsi = 内側の配列をあと何段複製するか。新しいブロック
// （cap = len）を malloc して要素を写し、深さが残っていれば
// 要素の配列も再帰的に複製する。[fill; n] でリテラルな fill を
// 各スロットへ独立に入れるために使う
static void emit_rt_copy_array() {
    printf("__rt_spell_copy_array:\n");
    printf("  push rbp\n");
    printf("  mov rbp, rsp\n");
    printf("  push rbx\n");
    printf("  push r12\n");
    printf("  push r13\n");
    printf("  push r14\n");
    printf("  push r15\n");
    printf("  sub rsp, 8\n");
    printf("  mov rbx, rdi\n");
    printf("  mov r15, rsi\n");
    printf("  mov r12, [rdi-8]\n");
    printf("  lea rdi, [r12+2]\n");
    printf("  imul rdi, 8\n");
    printf("  call malloc\n");
    printf("  mov r13, rax\n");
    printf("  mov [r13], r12\n");
    printf("  mov [r13+8], r12\n");
    printf("  xor rcx, rcx\n");
    printf(".Lrt_copy_elems:\n");
    printf("  cmp rcx, r12\n");
    printf("  jae .Lrt_copy_recurse\n");
    printf("  mov rdx, [rbx+rcx*8]\n");
    printf("  mov [r13+rcx*8+16], rdx\n");
    printf("  inc rcx\n");
    printf("  jmp .Lrt_copy_elems\n");
    printf(".Lrt_copy_recurse:\n");
    printf("  cmp r15, 0\n");
    printf("  je .Lrt_copy_done\n");
    printf("  xor r14, r14\n");
    printf(".Lrt_copy_loop:\n");
    printf("  cmp r14, r12\n");
    printf("  jae .Lrt_copy_done\n");
    printf("  mov rdi, [r13+r14*8+16]\n");
    printf("  mov rsi, r15\n");
    printf("  sub rsi, 1\n");
    printf("  call __rt_spell_copy_array\n");
    printf("  mov [r13+r14*8+16], rax\n");
    printf("  inc r14\n");
    printf("  jmp .Lrt_copy_loop\n");
    printf(".Lrt_copy_done:\n");
    printf("  lea rax, [r13+16]\n");
    printf("  add rsp, 8\n");
    printf("  pop r15\n");
    printf("  pop r14\n");
    printf("  pop r13\n");
    printf("  pop r12\n");
    printf("  pop rbx\n");
    printf("  pop rbp\n");
    printf("  ret\n");
}

static void emit_runtime_helpers(int used) {
    if (used & ((1 << BUILTIN_READ_FILE_CHARS) | (1 << BUILTIN_WRITE_FILE_CHARS) | (1 << BUILTIN_STDERR_CHARS))) {
        emit_rt_chars_to_bytes();
    }
    if (used & (1 << BUILTIN_ARGC)) {
        emit_rt_argc();
    }
    if (used & (1 << BUILTIN_ARGV_CHARS)) {
        emit_rt_argv_chars();
    }
    if (used & (1 << BUILTIN_READ_FILE_CHARS)) {
        emit_rt_read_file_chars();
    }
    if (used & (1 << BUILTIN_IO_OK)) {
        emit_rt_io_ok();
    }
    if (used & (1 << BUILTIN_WRITE_FILE_CHARS)) {
        emit_rt_write_file_chars();
    }
    if (used & (1 << BUILTIN_STDERR_CHARS)) {
        emit_rt_stderr_chars();
    }
    if (used & (1 << BUILTIN_EXIT)) {
        emit_rt_exit();
    }
}

// ---- 到達可能性（step 63） ----
//
// 出力する関数を、入口（main と __spell_init）から実際に辿れるものだけに
// 絞る。prelude の全関数を常に出していると、簡単なプログラムでも
// アセンブリが数千行になり、提出コードが膨らんでしまうため。
// 辿り方は codegen が出す呼び出しと正確に対応させる:
//   ND_FUNCALL … 組み込みなら使用ビットを立てる。__init__ が解決されて
//     いれば（構築を含む）node->func への辺。素の呼び出しは codegen も
//     名前で出すので、関数一覧から名前で引く（getchar のような libc 側
//     の名前は一覧に無いので辺にならない）
//   ND_MCALL / ND_MEMBER（func 付き）… node->func への辺
//   ND_FUNCREF … 関数値として使われるので、指す関数への辺
//   ND_LAMBDA … node->func（ラムダ本体）への辺。値はどこからでも
//     呼ばれうるが、辺はラムダ式のある場所からだけ張る
// 子ノードは Node の全ポインタフィールドを辿る（desugar と同じ集合）。
// 関数には辿る前に印を付けるので、再帰・相互再帰でも止まる

static Function* cg_functions;
static int cg_used_builtins;
static int cg_used_copy_array;

static Function* find_plain_function(char* name, int len) {
    for (Function* fn = cg_functions; fn; fn = fn->next) {
        if (!fn->klass && fn->len == len && !memcmp(fn->name, name, len)) {
            return fn;
        }
    }
    return NULL;
}

static void mark_function(Function* fn);

static void walk_reachable(Node* node) {
    for (Node* n = node; n; n = n->next) {
        if (n->kind == ND_NEWARRAY && n->left_child
                && (n->left_child->kind == ND_ARRAY || n->left_child->kind == ND_NEWARRAY)) {
            // リテラルな fill の [fill; n] は構造複製のヘルパが要る
            cg_used_copy_array = 1;
        }
        if (n->kind == ND_FUNCALL) {
            if (n->builtin) {
                cg_used_builtins |= 1 << n->builtin;
            } else if (n->func) {
                mark_function(n->func);
            } else if (!n->klass) {
                mark_function(find_plain_function(n->name, n->len));
            }
        } else if (n->kind == ND_MCALL) {
            mark_function(n->func);
        } else if (n->kind == ND_MEMBER) {
            if (n->func) {
                mark_function(n->func);
            }
        } else if (n->kind == ND_FUNCREF) {
            if (n->func) {
                mark_function(n->func);
            } else {
                mark_function(find_plain_function(n->name, n->len));
            }
        } else if (n->kind == ND_LAMBDA) {
            if (n->func) {
                mark_function(n->func);
            } else {
                // ジェネリクスのクローンでは func がリセットされることが
                // ある（typecheck の check_lambda と同じ引き直し）
                for (Function* f = cg_functions; f; f = f->next) {
                    if (f->is_lambda && f->len == n->len && !memcmp(f->name, n->name, n->len)) {
                        mark_function(f);
                        break;
                    }
                }
            }
        }
        walk_reachable(n->left_child);
        walk_reachable(n->right_child);
        walk_reachable(n->cond);
        walk_reachable(n->then);
        walk_reachable(n->els);
        walk_reachable(n->init);
        walk_reachable(n->inc);
        walk_reachable(n->body);
        walk_reachable(n->args);
    }
}

static void mark_function(Function* fn) {
    if (!fn || fn->reachable) {
        return;
    }
    fn->reachable = true;
    walk_reachable(fn->body);
}

static void mark_reachable(Function* prog) {
    cg_functions = prog;
    cg_used_builtins = 0;
    cg_used_copy_array = 0;
    for (Function* fn = prog; fn; fn = fn->next) {
        if (fn->klass) {
            continue;
        }
        if ((fn->len == 4 && !memcmp(fn->name, "main", 4))
                || (fn->len == 12 && !memcmp(fn->name, "__spell_init", 12))) {
            mark_function(fn);
        }
    }
}

// ---- 関数ごとの出力 ----

void codegen(Function* prog) {
    mark_reachable(prog);
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
            print_global_label(var->len, var->name);
            printf(":\n");
            printf("  .zero 8\n");
        }
    }
    printf(".text\n");
    while (prog) {
        if (prog->klass && prog->klass->tparam_count > 0) {
            prog = prog->next;
            continue;
        }
        if (!prog->reachable) {
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
            printf(".global ");
            print_func_label(prog->len, prog->name);
            printf("\n");
            print_func_label(prog->len, prog->name);
            printf(":\n");
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
            gen_discard(tail);
            tail = tail->next;
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
    emit_runtime_helpers(cg_used_builtins);
    if (cg_used_copy_array) {
        emit_rt_copy_array();
    }
}
