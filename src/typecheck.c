#include "spell.h"
#include "typecheck_internal.h"

// 型検査: AST を歩いて各ノードに型を付け、演算子を dunder メソッド呼び出しへ
// 書き換える。main からコード生成の直前に呼ばれる。
// 型の道具は types.c、ジェネリクスの実体化は generics.c、ここは検査本体。

static Type* local_types[1024];
static Function* current_fn;
static Type* current_return;
Function* functions;

// プリミティブの署名表。引数と戻り値の型は呼び出しのたびに組み立てる
// （[int] は array_type で新しく作る。型の同一性は構造で判定される）
static Type* bool_type();
typedef struct {
    char* name;
    int id;         // SpellBuiltin
    int nparams;
    Type* params[2];
    Type* ret;
} BuiltinInfo;

static BuiltinInfo* find_builtin(char* name, int len) {
    static BuiltinInfo table[7];
    static bool initialized = false;
    if (!initialized) {
        Type* chars = array_type(&ty_int);
        Type* bool_ty = bool_type();
        table[0] = (BuiltinInfo){ "__spell_argc", BUILTIN_ARGC, 0, { NULL, NULL }, &ty_int };
        table[1] = (BuiltinInfo){ "__spell_argv_chars", BUILTIN_ARGV_CHARS, 1, { &ty_int, NULL }, chars };
        table[2] = (BuiltinInfo){ "__spell_read_file_chars", BUILTIN_READ_FILE_CHARS, 1, { chars, NULL }, chars };
        table[3] = (BuiltinInfo){ "__spell_io_ok", BUILTIN_IO_OK, 0, { NULL, NULL }, bool_ty };
        table[4] = (BuiltinInfo){ "__spell_write_file_chars", BUILTIN_WRITE_FILE_CHARS, 2, { chars, chars }, &ty_int };
        table[5] = (BuiltinInfo){ "__spell_stderr_chars", BUILTIN_STDERR_CHARS, 1, { chars, NULL }, &ty_int };
        // __spell_exit は戻らない。戻り値の型は unknown にして、文としての
        // 使用を妨げない（式の位置で使うと型は分からないものとして扱う）
        table[6] = (BuiltinInfo){ "__spell_exit", BUILTIN_EXIT, 1, { &ty_int, NULL }, &ty_unknown };
        initialized = true;
    }
    for (int i = 0; i < 7; i++) {
        int nlen = (int)strlen(table[i].name);
        if (nlen == len && !memcmp(table[i].name, name, len)) {
            return &table[i];
        }
    }
    return NULL;
}

// 式に期待される型（双方向の型制約）。注釈つき代入の右辺・呼び出しの引数・
// 戻り値など、期待が分かる場所でだけ直前にセットし、ND_LAMBDA が消費する。
// check_node は入口でこれを取り込み、それ以外のノードでは子へ漏らさない
static Type* expected_type;

// ラムダ本体の検査中に、その関数が捕まえた変数の一覧
static Capture* current_captures;

// ノードの代表トークンが現在のソース（利用者のファイル）の範囲内にあれば
// 位置つきで、なければ位置なしでエラーを出す。prelude 由来のノードや
// そのジェネリクスクローンは prelude のバッファを指すため、範囲チェック
// なしに error_at へ渡すと範囲外を読みに行ってしまう
static void error_node(Node* node, char* fmt, ...) {
    va_list ap;
    va_start(ap, fmt);
    Token* tok = node ? node->tok : NULL;
    if (tok && tok->str && src) {
        // 別バッファを指すトークンとの比較になるため、ポインタのままではなく
        // 整数化してから範囲を判定する
        uintptr_t p = (uintptr_t)tok->str;
        uintptr_t lo = (uintptr_t)src;
        uintptr_t hi = lo + (uintptr_t)strlen(src);
        if (p >= lo && p + (uintptr_t)tok->len <= hi) {
            long start = (long)(p - lo);
            error_at_v(start, start + tok->len, fmt, ap);
            // 通常モードでは error_at_v は戻らない。キャプチャ有効時に
            // 戻ってきた場合は記録済みなので、位置なしで出し直さない
            va_end(ap);
            return;
        }
    }
    error_plain_v(fmt, ap);
    va_end(ap);
}

// 関数・メソッドを探す。klass が NULL なら通常の関数
static Function* find_callable(Class* klass, char* name, int len) {
    for (Function* fn = functions; fn; fn = fn->next) {
        if (fn->klass == klass && fn->len == len && !memcmp(fn->name, name, len)) {
            return fn;
        }
    }
    return NULL;
}

static Type* bool_type() {
    Class* klass = find_class("bool", 4);
    return klass ? klass->type : &ty_int;
}

static Type* scalar_cmp_result(const char* dunder) {
    Class* klass = find_class("int", 3);
    if (klass) {
        Function* method = find_callable(klass, (char*)dunder, 6);
        if (method && method->return_type) {
            return method->return_type;
        }
    }
    return &ty_int;
}

static const char* arith_dunder(BinOp op) {
    switch (op) {
        case OP_ADD: return "__add__";
        case OP_SUB: return "__sub__";
        case OP_MUL: return "__mul__";
        case OP_DIV: return "__div__";
        case OP_MOD: return "__mod__";
        default: return NULL;
    }
}

static const char* op_text(BinOp op) {
    switch (op) {
        case OP_ADD: return "+";
        case OP_SUB: return "-";
        case OP_MUL: return "*";
        case OP_DIV: return "/";
        case OP_MOD: return "%";
        case OP_EQ: return "==";
        case OP_NE: return "!=";
        case OP_LT: return "<";
        case OP_LE: return "<=";
        default: return "?";
    }
}

static const char* cmp_dunder(BinOp op) {
    switch (op) {
        case OP_EQ: return "__eq__";
        case OP_NE: return "__ne__";
        case OP_LT: return "__lt__";
        case OP_LE: return "__le__";
        default: return NULL;
    }
}

static Type* rewrite_to_mcall(Node* node, Type* lt, Type* rt, const char* name, int len, Function* method) {
    Class* klass = class_of(lt);
    if (method->nparams != 2) {
        error_node(node, "クラス`%.*s`のメソッド`%.*s`の引数は1個ですが、%d個が定義されています", klass->len, klass->name, len, name, method->nparams - 1);
    }
    if (method->param_types[1] && rt->kind != TY_UNKNOWN && !type_equals(method->param_types[1], rt)) {
        error_node(node, "クラス`%.*s`のメソッド`%.*s`の引数には`%s`が必要ですが、`%s`が指定されています", klass->len, klass->name, len, name, type_text(method->param_types[1]), type_text(rt));
    }
    node->kind = ND_MCALL;
    node->name = (char*)name;
    node->len = len;
    node->func = method;
    node->klass = klass;
    node->args = node->right_child;
    node->args->next = NULL;
    return method->return_type ? method->return_type : &ty_unknown;
}

static void rewrite_to_setitem(Node* node, Class* klass, Node* base, Node* index, Type* it, Type* rhs, Node* value) {
    Function* method = find_callable(klass, "__setitem__", 11);
    if (!method) {
        error_node(node, "クラス`%.*s`に`__setitem__`は定義されていません", klass->len, klass->name);
        // 通常モードでは error_node は戻らない。キャプチャ有効時に
        // 戻ってきた場合は、メソッドなしでは書き換えを続けられない
        return;
    }
    if (method->nparams != 3) {
        error_node(node, "クラス`%.*s`のメソッド`__setitem__`の引数は2個ですが、%d個が定義されています", klass->len, klass->name, method->nparams - 1);
    }
    if (method->param_types[1] && it->kind != TY_UNKNOWN && !type_equals(method->param_types[1], it)) {
        error_node(node, "クラス`%.*s`のメソッド`__setitem__`の引数には`%s`が必要ですが、`%s`が指定されています", klass->len, klass->name, type_text(method->param_types[1]), type_text(it));
    }
    if (method->param_types[2] && rhs->kind != TY_UNKNOWN && !type_equals(method->param_types[2], rhs)) {
        error_node(node, "クラス`%.*s`のメソッド`__setitem__`の引数には`%s`が必要ですが、`%s`が指定されています", klass->len, klass->name, type_text(method->param_types[2]), type_text(rhs));
    }
    index->next = value;
    node->kind = ND_MCALL;
    node->name = "__setitem__";
    node->len = 11;
    node->func = method;
    node->klass = klass;
    node->left_child = base;
    node->args = index;
}

static Type* check_node(Node* node);
static Type* check_funcref(Node* node);
static Node* make_mcall(Node* receiver, Class* klass, Function* fn, const char* name, int len);
static Node* boolify(Node* expr, Type* t);
static void ensure_defs(Function* fn);
// 安全添え字（step 81）: 部品の組み立てと比較への横取り
static Type* build_safe_parts(Node* node, Node** guard_out, Node** value_out);
static Type* check_safe_cmp(Node* node);

// 引数を全部検査し、型を最大 6 個まで記録する。引数の数を返す。
// expected_list があれば、各引数の期待型として渡す（ラムダの型決定用）
static int check_args_expected(Node* args, Type* arg_types[6], Type* expected_list[6]) {
    int nargs = 0;
    for (Node* arg = args; arg; arg = arg->next) {
        expected_type = expected_list && nargs < 6 ? expected_list[nargs] : NULL;
        Type* at = check_node(arg);
        expected_type = NULL;
        if (nargs < 6) {
            arg_types[nargs] = at;
        }
        nargs++;
    }
    return nargs;
}

static int check_args(Node* args, Type* arg_types[6]) {
    return check_args_expected(args, arg_types, NULL);
}

// 変数の型（注釈または記録済み）と代入する値の型が食い違えばエラー
static void check_assign_type(Node* lhs, Type* expected, Type* rhs) {
    if (expected && expected->kind != TY_UNKNOWN && rhs->kind != TY_UNKNOWN && !type_equals(expected, rhs)) {
        error_node(lhs, "変数`%.*s`の型は`%s`ですが、`%s`が代入されています", lhs->len, lhs->name, type_text(expected), type_text(rhs));
    }
}

static Type* check_array_arith(Node* node, Type* lt, Type* rt) {
    Type* ot = lt->kind == TY_ARRAY ? rt : lt;
    if (lt->kind != TY_ARRAY) {
        Node* tn = node->left_child;
        node->left_child = node->right_child;
        node->right_child = tn;
        Type* tt = lt;
        lt = rt;
        rt = tt;
    }
    if (node->op.binop == OP_ADD) {
        if (ot->kind != TY_ARRAY && ot->kind != TY_UNKNOWN) {
            error_node(node, "`%s`型の値に演算子`%s`は使えません", type_text(lt), op_text(node->op.binop));
        }
        Function* method = find_callable(class_of(lt), "__add__", 7);
        Type* result = rewrite_to_mcall(node, lt, rt, "__add__", 7, method);
        if (result->kind == TY_UNKNOWN) { result = lt; }
        return result;
    }
    if (node->op.binop == OP_MUL) {
        if (ot->kind != TY_INT && ot->kind != TY_UNKNOWN) {
            error_node(node, "`%s`型の値に演算子`%s`は使えません", type_text(lt), op_text(node->op.binop));
        }
        Function* method = find_callable(class_of(lt), "__mul__", 7);
        Type* result = rewrite_to_mcall(node, lt, rt, "__mul__", 7, method);
        if (result->kind == TY_UNKNOWN) { result = lt; }
        return result;
    }
    error_node(node, "`%s`型の値に演算子`%s`は使えません", type_text(lt), op_text(node->op.binop));
    return &ty_unknown;
}

static Type* check_array_cmp(Node* node, Type* lt, Type* rt) {
    if (node->op.binop == OP_LT || node->op.binop == OP_LE) {
        Type* at = lt->kind == TY_ARRAY ? lt : rt;
        error_node(node, "`%s`型の値に演算子`%s`は使えません", type_text(at), op_text(node->op.binop));
    }
    Type* ot = lt->kind == TY_ARRAY ? rt : lt;
    if (ot->kind == TY_ARRAY || ot->kind == TY_UNKNOWN) {
        if (node->op.binop == OP_EQ) {
            Function* method = find_callable(class_of(lt), "__eq__", 6);
            rewrite_to_mcall(node, lt, rt, "__eq__", 6, method);
        } else {
            Function* method = find_callable(class_of(lt), "__ne__", 6);
            rewrite_to_mcall(node, lt, rt, "__ne__", 6, method);
        }
    }
    return bool_type();
}

// 両辺の型が確定した後の二項演算の検査。子は検査しない
static Type* check_binop_rest(Node* node, Type* lt, Type* rt) {
    if (node->op.binop == OP_AND || node->op.binop == OP_OR) {
        // 論理演算の被演算子は真理値として解釈する（step 79）
        node->left_child = boolify(node->left_child, lt);
        node->right_child = boolify(node->right_child, rt);
        return bool_type();
    }
    const char* dunder = arith_dunder(node->op.binop);
    if (dunder) {
        if (lt->kind == TY_CLASS) {
            Function* method = find_callable(class_of(lt), (char*)dunder, 7);
            if (!method) {
                error_node(node, "クラス`%.*s`に演算子`%s`は定義されていません", lt->klass->len, lt->klass->name, op_text(node->op.binop));
                return &ty_unknown;
            }
            return rewrite_to_mcall(node, lt, rt, dunder, 7, method);
        }
        if (lt->kind == TY_ARRAY || rt->kind == TY_ARRAY) {
            return check_array_arith(node, lt, rt);
        }
        if (lt->kind != TY_UNKNOWN && rt->kind != TY_UNKNOWN && lt->kind != rt->kind) {
            error_node(node, "`%s`型と`%s`型の値に演算子`%s`は使えません", type_text(lt), type_text(rt), op_text(node->op.binop));
        }
        return &ty_int;
    }
    const char* cdunder = cmp_dunder(node->op.binop);
    if (cdunder) {
        if (lt->kind == TY_CLASS) {
            Function* method = find_callable(class_of(lt), (char*)cdunder, 6);
            if (method) {
                return rewrite_to_mcall(node, lt, rt, cdunder, 6, method);
            }
            if (node->op.binop == OP_LT || node->op.binop == OP_LE) {
                error_node(node, "クラス`%.*s`に演算子`%s`は定義されていません", lt->klass->len, lt->klass->name, op_text(node->op.binop));
            }
            return bool_type();
        }
        if (lt->kind == TY_ARRAY || rt->kind == TY_ARRAY) {
            return check_array_cmp(node, lt, rt);
        }
        if ((node->op.binop == OP_LT || node->op.binop == OP_LE)
                && lt->kind != TY_UNKNOWN && rt->kind != TY_UNKNOWN && lt->kind != rt->kind) {
            error_node(node, "`%s`型と`%s`型の値に演算子`%s`は使えません", type_text(lt), type_text(rt), op_text(node->op.binop));
        }
        return scalar_cmp_result(cdunder);
    }
    return bool_type();
}

static Type* check_binop(Node* node) {
    // 比較の直接の被演算子が安全添え字なら、範囲チェックで
    // ゲートした比較へ作り替える（step 81）
    if ((node->op.binop == OP_EQ || node->op.binop == OP_NE
                || node->op.binop == OP_LT || node->op.binop == OP_LE)
            && (node->left_child->kind == ND_SAFEIDX
                || node->right_child->kind == ND_SAFEIDX)) {
        return check_safe_cmp(node);
    }
    Type* lt = check_node(node->left_child);
    Type* rt = check_node(node->right_child);
    return check_binop_rest(node, lt, rt);
}

// 安全添え字を被演算子に持つ比較の検査（step 81）。安全添え字の側は
// 部品（guard と value）に分解し、通常の比較として検査したうえで、
// 全体を `guard && 比較` に作り替える。guard が偽（どこかの段が
// 範囲外）なら、value は評価されず比較全体が偽になる
static Type* check_safe_cmp(Node* node) {
    Node* guard = NULL;
    Type* lt;
    Type* rt;
    if (node->left_child->kind == ND_SAFEIDX) {
        Node* value;
        lt = build_safe_parts(node->left_child, &guard, &value);
        node->left_child = value;
    } else {
        lt = check_node(node->left_child);
    }
    if (node->right_child->kind == ND_SAFEIDX) {
        Node* g2;
        Node* value;
        rt = build_safe_parts(node->right_child, &g2, &value);
        node->right_child = value;
        if (g2) {
            if (guard) {
                Node* both = create_node(ND_BINOP);
                both->op.binop = OP_AND;
                both->tok = node->tok;
                both->left_child = guard;
                both->right_child = g2;
                both->type = bool_type();
                guard = both;
            } else {
                guard = g2;
            }
        }
    } else {
        rt = check_node(node->right_child);
    }
    Type* result = check_binop_rest(node, lt, rt);
    if (!guard) {
        return result;
    }
    Node* self = create_node(ND_BINOP);
    Node* nxt = node->next;
    *self = *node;
    self->next = NULL;
    self->type = result;
    Node* combined = create_node(ND_BINOP);
    combined->op.binop = OP_AND;
    combined->tok = node->tok;
    combined->left_child = guard;
    combined->right_child = self;
    combined->type = bool_type();
    *node = *combined;
    node->next = nxt;
    node->type = bool_type();
    return bool_type();
}

static Type* check_assign(Node* node) {
    // 安全添え字は読み取り専用（step 81）。複合代入・入力対象・
    // 複数代入の対象も、脱糖や check_multiassign 経由でここへ来る。
    // 右辺の検査より先に判定する（複合代入では対象ノードが右辺と
    // 共有されており、先に検査すると書き換えられてしまうため）
    if (node->left_child->kind == ND_SAFEIDX) {
        error_node(node->left_child, "安全添え字には代入できません");
    }
    // 右辺への期待型: 注釈があれば注釈、なければ代入先の記録済みの型
    if (node->anno) {
        expected_type = node->anno;
    } else if (node->left_child->kind == ND_LVAR) {
        expected_type = local_types[node->left_child->offset / 8];
    } else if (node->left_child->kind == ND_GVAR) {
        Type* gt = find_global_type(node->left_child);
        expected_type = gt->kind != TY_UNKNOWN ? gt : NULL;
    }
    Type* rhs = check_node(node->right_child);
    expected_type = NULL;
    if (node->anno) {
        rhs = adopt_type(node->anno, rhs);
    }
    Node* lhs = node->left_child;
    // func が設定されているのは、右辺の検査で添字が __getitem__ 呼び出しへ
    // 書き換え済みの場合だけ。メソッド解決に失敗した呼び出し（キャプチャ
    // 有効時にだけここへ到達する）は、引数の型も付いていないため対象外
    if (lhs->kind == ND_MCALL && lhs->func && node->right_child->kind == ND_BINOP && node->right_child->left_child == lhs) {
        Node* idx = malloc(sizeof(Node));
        memcpy(idx, lhs->args, sizeof(Node));
        idx->next = NULL;
        rewrite_to_setitem(node, lhs->klass, lhs->left_child, idx, lhs->args->type, rhs, node->right_child);
        return rhs;
    }
    if (lhs->kind == ND_LVAR) {
        Type* recorded = local_types[lhs->offset / 8];
        check_assign_type(lhs, node->anno, rhs);
        check_assign_type(lhs, recorded, rhs);
        if (!recorded && (node->anno || rhs->kind != TY_UNKNOWN)) {
            local_types[lhs->offset / 8] = node->anno ? node->anno : rhs;
        }
        lhs->type = local_types[lhs->offset / 8];
    } else if (lhs->kind == ND_GVAR) {
        Type* recorded = find_global_type(lhs);
        check_assign_type(lhs, node->anno, rhs);
        check_assign_type(lhs, recorded, rhs);
        if (recorded->kind == TY_UNKNOWN) {
            record_global_type(lhs, node->anno ? node->anno : rhs);
        }
        lhs->type = find_global_type(lhs);
    } else if (lhs->kind == ND_INDEX) {
        Type* bt = check_node(lhs->left_child);
        if (bt->kind == TY_CLASS) {
            Type* it = check_node(lhs->right_child);
            // ジェネリックなクラスでは実体化したクラスのメソッドを使う
            // （bt->klass はテンプレートで、引数型が型変数のままになる）
            rewrite_to_setitem(node, class_of(bt), lhs->left_child, lhs->right_child, it, rhs, node->right_child);
        } else {
            Type* elem = check_node(lhs);
            if (elem->kind != TY_UNKNOWN && rhs->kind != TY_UNKNOWN && !type_equals(elem, rhs)) {
                error_node(node, "配列の要素は`%s`ですが、`%s`が代入されています", type_text(elem), type_text(rhs));
            }
        }
    } else if (lhs->kind == ND_CAPVAR) {
        Type* cap_type = check_node(lhs);
        if (cap_type->kind != TY_UNKNOWN && rhs->kind != TY_UNKNOWN && !type_equals(cap_type, rhs)) {
            error_node(lhs, "変数`%.*s`の型は`%s`ですが、`%s`が代入されています", lhs->len, lhs->name, type_text(cap_type), type_text(rhs));
        }
        lhs->type = cap_type;
    } else {
        check_node(lhs);
        if (lhs->kind == ND_LEN) {
            error_node(lhs, "`len`には代入できません");
        }
        if (lhs->kind == ND_MEMBER && lhs->func) {
            error_node(lhs, "メソッド`%.*s`には代入できません", lhs->len, lhs->name);
        }
        if (lhs->kind == ND_FUNCREF) {
            error_node(lhs, "関数`%.*s`には代入できません", lhs->len, lhs->name);
        }
        if (lhs->kind == ND_MCALL || lhs->kind == ND_FUNCALL || lhs->kind == ND_VCALL) {
            error_node(lhs, "呼び出しの結果には代入できません");
        }
        if (lhs->kind == ND_LAMBDA) {
            error_node(lhs, "ラムダには代入できません");
        }
        if (lhs->kind == ND_BOOL) {
            error_node(lhs, "`%.*s`には代入できません", lhs->len, lhs->name);
        }
    }
    return rhs;
}

// prelude の Vec / Deque かどうか（実体化済みなら origin で判定する）。
// 要素型の既定 int と引数なし構築は、埋め草のファクトリが意味論を
// 持たないこの2クラスだけの特例（step 76）
static bool is_vec_or_deque(Class* klass) {
    Class* o = klass->origin ? klass->origin : klass;
    return (o->len == 3 && !memcmp(o->name, "Vec", 3))
        || (o->len == 5 && !memcmp(o->name, "Deque", 5));
}

// 要素型の既定値を返す prelude 関数の参照ノードを作る。自動で作れるのは
// int / bool / str / Pos2 / Pos3 だけで、それ以外は NULL を返す。
// 利用者が手で書いた `Deque(zero_int)` の引数と全く同じ形のノードにする
static Node* make_zero_factory(Node* node, Type* elem) {
    const char* name = NULL;
    if (elem->kind == TY_INT) {
        name = "zero_int";
    } else if (elem->kind == TY_CLASS && elem->klass) {
        Class* o = elem->klass->origin ? elem->klass->origin : elem->klass;
        if (o->len == 4 && !memcmp(o->name, "bool", 4)) {
            name = "zero_bool";
        } else if (o->len == 3 && !memcmp(o->name, "str", 3)) {
            name = "zero_str";
        } else if (o->len == 4 && !memcmp(o->name, "Pos2", 4)) {
            name = "zero_pos2";
        } else if (o->len == 4 && !memcmp(o->name, "Pos3", 4)) {
            name = "zero_pos3";
        }
    }
    if (!name) {
        return NULL;
    }
    Node* fref = calloc(1, sizeof(Node));
    fref->kind = ND_FUNCREF;
    fref->name = (char*)name;
    fref->len = (int)strlen(name);
    fref->tok = node->tok;
    return fref;
}

// 型のゼロ値を作る式を合成する（step 77 の型を引数に渡す構築用）。
// 配列・文字列のリテラルノードは合成しない: 文字列は prelude の
// zero_str 呼び出し、配列は zero_array 呼び出し（どちらも prelude 側で
// 通常に解析されたリテラルを返す）で代用し、クラスは __init__ の
// 引数を再帰的にゼロで埋めた構築にする。型検査の段階で配列リテラルを
// 合成すると、置き場所（offset）が割り当てられず壊れるため
// （step 76 の知見）。数値・真理値の葉ノードは置き場所を必要とせず、
// 既存の書き換え（check_for など）と同じく合成してよい。
// ゼロ化できない型（関数型・型変数・型不明、注釈のない引数を持つ
// __init__ など）では NULL を返す。自己参照クラスは有限の構築では
// ゼロ化できないため、展開中のクラス型の並び（path）に同じ型が
// 再び現れたら NULL を返す（配列は空配列で循環を断てるので、
// path にはクラス型だけを積む）。depth は保険の上限
static Node* build_zero_expr(Node* node, Type* type, Type* path[], int path_len, int depth) {
    if (depth > 64) {
        return NULL;
    }
    switch (type->kind) {
        case TY_INT: {
            Node* num = create_node(ND_NUM);
            num->val = 0;
            num->tok = node->tok;
            return num;
        }
        case TY_ARRAY: {
            // 要素型によらず、新しい空配列でよい（長さ0なら要素は無い）
            Node* call = create_node(ND_FUNCALL);
            call->name = "zero_array";
            call->len = 10;
            call->tok = node->tok;
            return call;
        }
        case TY_CLASS: {
            Class* klass = class_of(type);
            if (!klass) {
                return NULL;
            }
            Class* origin = klass->origin ? klass->origin : klass;
            if (origin->len == 4 && !memcmp(origin->name, "bool", 4)) {
                Node* b = create_node(ND_BOOL);
                b->val = 0;
                b->name = "false";
                b->len = 5;
                b->tok = node->tok;
                return b;
            }
            if (origin->len == 3 && !memcmp(origin->name, "str", 3)) {
                Node* call = create_node(ND_FUNCALL);
                call->name = "zero_str";
                call->len = 8;
                call->tok = node->tok;
                return call;
            }
            for (int i = 0; i < path_len; i++) {
                if (path[i]->kind == TY_CLASS && type_strict_equals(path[i], type)) {
                    return NULL;
                }
            }
            path[path_len] = type;
            Function* init = find_callable(klass, "__init__", 8);
            Node* call = create_node(ND_FUNCALL);
            call->name = origin->name;
            call->len = origin->len;
            call->tok = node->tok;
            // 実体化したクラスの構築では、型引数を明示しておく
            // （引数の値だけからの推論に頼らないため）
            if (type->targ_count > 0) {
                for (int i = 0; i < type->targ_count && i < MAX_TPARAMS; i++) {
                    call->ctor_targs[i] = type->targs[i];
                }
                call->ctor_targ_count = type->targ_count;
            }
            if (!init) {
                // __init__ が無いクラスは引数なしで構築できる
                // （フィールドは 0 で埋まる、通常の構築と同じ）
                return call;
            }
            Node head;
            head.next = NULL;
            Node* cur = &head;
            for (int i = 1; i < init->nparams && i < 6; i++) {
                Type* pt = init->param_types[i];
                if (!pt) {
                    return NULL;
                }
                Node* arg = build_zero_expr(node, pt, path, path_len + 1, depth + 1);
                if (!arg) {
                    return NULL;
                }
                cur->next = arg;
                cur = arg;
            }
            call->args = head.next;
            return call;
        }
        default:
            return NULL;
    }
}

// ゼロ値ファクトリ（fn() -> 要素型）を合成する。名前のある prelude
// 関数で済む場合はその関数参照を、そうでなければゼロ値の式を返す
// ラムダを合成して関数チェーンへ足す（generics.c の実体化が複製品を
// チェーンへ足すのと同じ方式）。合成できない場合は NULL
static Node* make_zero_value_factory(Node* node, Type* elem) {
    Node* fref = make_zero_factory(node, elem);
    if (fref) {
        return fref;
    }
    Type* path[66];
    Node* zero = build_zero_expr(node, elem, path, 0, 0);
    if (!zero) {
        return NULL;
    }
    static int synth_counter = 0;
    Function* fn = calloc(1, sizeof(Function));
    char* name = malloc(48);
    snprintf(name, 48, "__zero_factory_%d", synth_counter++);
    fn->name = name;
    fn->len = (int)strlen(name);
    fn->is_lambda = true;
    fn->nparams = 0;
    fn->return_type = elem;
    Node* ret = create_node(ND_RETURN);
    ret->left_child = zero;
    ret->tok = node->tok;
    fn->body = ret;
    // ローカルを持たない本体なので、フレームは環境スロット分だけ
    // （parse_lambda の最小形と同じ）
    fn->env_offset = 8;
    fn->stack_size = 16;
    Function* tail = functions;
    while (tail->next) {
        tail = tail->next;
    }
    tail->next = fn;
    Node* lam = create_node(ND_LAMBDA);
    lam->func = fn;
    lam->name = name;
    lam->len = fn->len;
    lam->tok = node->tok;
    return lam;
}

static Type* check_construct(Node* node, Class* klass, Type* arg_types[6], int nargs, Type* expected_ty) {
    if (klass->type->kind != TY_CLASS) {
        error_node(node, "`%.*s`はクラスではありません", klass->len, klass->name);
    }
    if (klass->tparam_count > 0) {
        Function* ginit = find_callable(klass, "__init__", 8);
        Type* gtargs[MAX_TPARAMS] = {};
        if (ginit) {
            for (int i = 0; i < nargs && i + 1 < ginit->nparams; i++) {
                unify_type(ginit->param_types[i + 1], arg_types[i], klass, gtargs);
            }
        }
        // 構築時の明示的な型引数（step 76）は、引数からの推論より優先
        // する。個数が合わなければエラー。期待型があるのに矛盾する
        // 場合もエラーにする（黙ってどちらかを採らない）
        if (node->ctor_targ_count > 0) {
            if (node->ctor_targ_count != klass->tparam_count) {
                error_node(node, "クラス`%.*s`の型引数は%d個ですが、%d個が指定されています", klass->len, klass->name, klass->tparam_count, node->ctor_targ_count);
                // 個数が合わないまま実体化すると NULL の型が紛れ込むため、
                // キャプチャ有効時（error_node から戻る場合）は打ち切る
                return &ty_unknown;
            }
            if (expected_ty && expected_ty->kind == TY_CLASS
                    && expected_ty->targ_count == klass->tparam_count) {
                Class* ek = expected_ty->klass->origin ? expected_ty->klass->origin : expected_ty->klass;
                Class* tk = klass->origin ? klass->origin : klass;
                if (ek == tk) {
                    for (int i = 0; i < klass->tparam_count; i++) {
                        if (!type_equals(expected_ty->targs[i], node->ctor_targs[i])) {
                            error_node(node, "クラス`%.*s`の型引数`%s`が、期待される型`%s`と矛盾します", klass->len, klass->name, type_text(node->ctor_targs[i]), type_text(expected_ty));
                        }
                    }
                }
            }
            for (int i = 0; i < klass->tparam_count; i++) {
                gtargs[i] = node->ctor_targs[i];
            }
        }
        // 引数から決まらなかった型引数は、この構築式への期待型（注釈・
        // 引数位置・戻り値の型）から補う。同じジェネリッククラス（origin
        // が一致）で型引数の数が合うときだけに限る。引数側の検査は
        // この後の通常の検査で必ず行われる
        if (expected_ty && expected_ty->kind == TY_CLASS
                && expected_ty->targ_count == klass->tparam_count) {
            Class* ek = expected_ty->klass->origin ? expected_ty->klass->origin : expected_ty->klass;
            Class* tk = klass->origin ? klass->origin : klass;
            if (ek == tk) {
                for (int i = 0; i < klass->tparam_count; i++) {
                    if (!gtargs[i]) {
                        gtargs[i] = expected_ty->targs[i];
                    }
                }
            }
        }
        // Vec / Deque は、それでも決まらない要素型の既定を int とする
        // （step 76）。Dict のファクトリは無いキーの初期値として観測
        // できる意味論を持つため、一般のジェネリッククラスには広げない
        if (is_vec_or_deque(klass)) {
            for (int i = 0; i < klass->tparam_count; i++) {
                if (!gtargs[i]) {
                    gtargs[i] = &ty_int;
                }
            }
        }
        bool gtargs_ok = true;
        for (int i = 0; i < klass->tparam_count; i++) {
            if (!gtargs[i]) {
                error_node(node, "クラス`%.*s`の型引数を推論できません", klass->len, klass->name);
                gtargs_ok = false;
            }
        }
        if (!gtargs_ok) {
            // 型引数が欠けたまま実体化すると NULL の型が紛れ込むため、
            // キャプチャ有効時（error_node から戻る場合）はここで打ち切る
            return &ty_unknown;
        }
        klass = instantiate(klass, gtargs, klass->tparam_count);
    }
    node->klass = klass;
    Function* init = find_callable(klass, "__init__", 8);
    node->func = init;
    // 型を引数に渡す構築（step 77）: `Deque(int)` のように要素型だけが
    // 指定された場合は、その型のゼロ値ファクトリを合成して引数へ
    // 差し込む。ゼロ化できない要素型は、ファクトリを渡すよう促す
    // エラーにする。合成した引数は、この後の通常の引数照合にかける
    if (node->ctor_type_arg && nargs == 0 && init && init->nparams == 2 && klass->targ_count == 1 && is_vec_or_deque(klass)) {
        Type* elem = klass->targs[0];
        Node* factory = make_zero_value_factory(node, elem);
        if (!factory) {
            error_node(node, "要素型`%s`のゼロ値は自動で作れないため、ファクトリを渡してください", type_text(elem));
            return &ty_unknown;
        }
        if (factory->kind == ND_FUNCREF) {
            arg_types[0] = check_funcref(factory);
        } else {
            expected_type = func_type(NULL, 0, elem);
            arg_types[0] = check_node(factory);
            expected_type = NULL;
        }
        node->args = factory;
        nargs = 1;
    }
    // Vec / Deque の引数なし構築（step 76）: 埋め草のファクトリを要素型の
    // 既定値で補う。要素型が int / bool / str 以外（クラス・配列・関数型）
    // の既定値は自動で作れないため、ファクトリを渡すよう促すエラーに
    // する。補った引数は、この後の通常の引数照合にそのままかける
    if (nargs == 0 && init && init->nparams == 2 && klass->targ_count == 1 && is_vec_or_deque(klass)) {
        Type* elem = klass->targs[0];
        Node* fref = make_zero_factory(node, elem);
        if (!fref) {
            if (elem->kind == TY_CLASS) {
                error_node(node, "要素型がクラスの場合はファクトリを渡してください");
            } else {
                error_node(node, "要素型`%s`の既定値は自動で作れないため、ファクトリを渡してください", type_text(elem));
            }
            // 補えないまま個数照合へ進むと二重の診断になるため、
            // キャプチャ有効時（error_node から戻る場合）は打ち切る
            return &ty_unknown;
        }
        arg_types[0] = check_funcref(fref);
        node->args = fref;
        nargs = 1;
    }
    int expected = init ? init->nparams - 1 : 0;
    if (nargs != expected) {
        error_node(node, "クラス`%.*s`の`__init__`の引数は%d個ですが、%d個が指定されています", klass->len, klass->name, expected, nargs);
    }
    // 個数が合わない場合（キャプチャ有効時にだけここへ到達する）は、
    // 対応する引数がある分だけ照合する（param_types の範囲外を読まない）
    for (int i = 0; init && i < nargs && i + 1 < init->nparams; i++) {
        Type* pt = init->param_types[i + 1];
        if (pt && arg_types[i]->kind != TY_UNKNOWN && !type_equals(pt, arg_types[i])) {
            error_node(node, "クラス`%.*s`の`__init__`の引数には`%s`が必要ですが、`%s`が指定されています", klass->len, klass->name, type_text(pt), type_text(arg_types[i]));
        }
    }
    return klass->type;
}

// 関数参照: 関数のシグネチャから関数型を組み立てる
static Type* check_funcref(Node* node) {
    Function* fn = find_callable(NULL, node->name, node->len);
    if (!fn) {
        return &ty_unknown;
    }
    node->func = fn;
    Type* params[6];
    for (int i = 0; i < fn->nparams && i < 6; i++) {
        params[i] = fn->param_types[i] ? fn->param_types[i] : &ty_unknown;
    }
    return func_type(params, fn->nparams, fn->return_type ? fn->return_type : &ty_unknown);
}

// ラムダ本体のオンデマンド検査。本体は外側の関数の検査の途中で呼ばれるため、
// 検査状態（local_types と現在関数まわり）を退避してから本体を歩く。
// 期待型があれば引数型と戻り値型をそれで補い、捕捉の型は外側の文脈から確定する
static Type* check_lambda(Node* node, Type* expected) {
    Function* fn = node->func;
    if (!fn) {
        // ジェネリクスのクローンでは func がリセットされるため、名前で引き直す
        for (Function* f = functions; f; f = f->next) {
            if (f->is_lambda && f->len == node->len && !memcmp(f->name, node->name, node->len)) {
                fn = f;
                break;
            }
        }
        node->func = fn;
    }
    if (!fn) {
        return &ty_unknown;
    }
    if (fn->lambda_checked) {
        if (expected && expected->kind == TY_FUNC && fn->func_type && !type_equals(expected, fn->func_type)) {
            error_node(node, "ラムダの型は`%s`ですが、`%s`が期待されています", type_text(fn->func_type), type_text(expected));
        }
        return fn->func_type ? fn->func_type : &ty_unknown;
    }
    if (expected && expected->kind == TY_FUNC) {
        if (expected->func_nparams != fn->nparams) {
            error_node(node, "ラムダの引数は%d個ですが、%d個が期待されています", fn->nparams, expected->func_nparams);
        }
        for (int i = 0; i < fn->nparams; i++) {
            Type* et = expected->func_params[i];
            if (!et || et->kind == TY_UNKNOWN) {
                continue;
            }
            if (fn->param_types[i] && !type_equals(fn->param_types[i], et)) {
                error_node(node, "ラムダの引数の型は`%s`ですが、`%s`が期待されています", type_text(fn->param_types[i]), type_text(et));
            }
            if (!fn->param_types[i]) {
                fn->param_types[i] = et;
            }
        }
        if (!fn->return_type && expected->func_ret && expected->func_ret->kind != TY_UNKNOWN) {
            fn->return_type = expected->func_ret;
        }
    }
    for (int i = 0; i < fn->nparams; i++) {
        if (!fn->param_types[i]) {
            error_node(node, "ラムダの引数の型が分かりません（型注釈を付けるか、関数型が期待される場所で使ってください）");
        }
    }
    // 捕捉の型を確定する。直接捕捉は外側の local_types から、推移的捕捉は
    // 捕まえ元の捕捉（外側ラムダの検査で確定済み）から取る
    for (Capture* c = fn->captures; c; c = c->next) {
        if (c->src_is_env) {
            c->type = c->src_cap && c->src_cap->type ? c->src_cap->type : &ty_unknown;
        } else {
            Type* t = local_types[c->src_offset / 8];
            c->type = t ? t : &ty_unknown;
        }
    }
    Type* saved_types[1024];
    memcpy(saved_types, local_types, sizeof(local_types));
    Function* saved_fn = current_fn;
    Type* saved_return = current_return;
    Capture* saved_captures = current_captures;
    memset(local_types, 0, sizeof(local_types));
    ensure_defs(fn);
    current_fn = fn;
    current_return = fn->return_type;
    current_captures = fn->captures;
    for (int i = 0; i < fn->nparams && i < 6; i++) {
        if (fn->param_types[i]) {
            local_types[i + 1] = fn->param_types[i];
        }
    }
    for (Node* stmt = fn->body; stmt; stmt = stmt->next) {
        check_node(stmt);
    }
    fn->lambda_checked = true;
    Type* params[6];
    for (int i = 0; i < fn->nparams && i < 6; i++) {
        params[i] = fn->param_types[i] ? fn->param_types[i] : &ty_unknown;
    }
    fn->func_type = func_type(params, fn->nparams, fn->return_type ? fn->return_type : &ty_unknown);
    memcpy(local_types, saved_types, sizeof(local_types));
    current_fn = saved_fn;
    current_return = saved_return;
    current_captures = saved_captures;
    return fn->func_type;
}

// 値経由の呼び出し: 対象が関数型なら、個数と引数の型を照合する
static Type* check_vcall(Node* node) {
    Type* lt = check_node(node->left_child);
    Type* expected_list[6] = {};
    if (lt->kind == TY_FUNC) {
        for (int i = 0; i < lt->func_nparams && i < 6; i++) {
            expected_list[i] = lt->func_params[i];
        }
    }
    Type* arg_types[6];
    int nargs = check_args_expected(node->args, arg_types, expected_list);
    if (lt->kind == TY_UNKNOWN) {
        return &ty_unknown;
    }
    if (lt->kind != TY_FUNC) {
        error_node(node, "`%s`型の値は呼び出せません", type_text(lt));
        // 関数型でない値の引数照合は意味を持たないため打ち切る
        return &ty_unknown;
    }
    if (nargs != lt->func_nparams) {
        error_node(node, "関数値の引数は%d個ですが、%d個が指定されています", lt->func_nparams, nargs);
    }
    for (int i = 0; i < nargs && i < lt->func_nparams && i < 6; i++) {
        Type* pt = lt->func_params[i];
        if (pt && arg_types[i]->kind != TY_UNKNOWN && !type_equals(pt, arg_types[i])) {
            error_node(node, "関数値の引数には`%s`が必要ですが、`%s`が指定されています", type_text(pt), type_text(arg_types[i]));
        }
    }
    return lt->func_ret ? lt->func_ret : &ty_unknown;
}

static Type* check_funcall(Node* node, Type* expected) {
    Function* callee = find_callable(NULL, node->name, node->len);
    Class* klass = callee ? NULL : find_class(node->name, node->len);
    BuiltinInfo* builtin = (callee || klass) ? NULL : find_builtin(node->name, node->len);
    // 引数の期待型を先に用意する（引数位置のラムダの型を決めるため）
    Type* expected_list[6] = {};
    if (callee) {
        for (int i = 0; i < callee->nparams && i < 6; i++) {
            expected_list[i] = callee->param_types[i];
        }
    } else if (builtin) {
        for (int i = 0; i < builtin->nparams; i++) {
            expected_list[i] = builtin->params[i];
        }
    } else if (klass) {
        Function* init = find_callable(klass, "__init__", 8);
        if (init) {
            for (int i = 0; i + 1 < init->nparams && i < 6; i++) {
                expected_list[i] = init->param_types[i + 1];
            }
        }
    }
    Type* arg_types[6];
    int nargs = check_args_expected(node->args, arg_types, expected_list);
    bool is_print = node->len == 5 && !memcmp(node->name, "print", 5);
    bool is_eprint = node->len == 6 && !memcmp(node->name, "eprint", 6);
    if ((is_print || is_eprint) && !callee && !klass) {
        if (nargs != 1) {
            error_node(node, "`%.*s`の引数は1個ですが、%d個が指定されています", node->len, node->name, nargs);
            // 引数が無い場合は arg_types[0] が未初期化なので打ち切る
            return &ty_unknown;
        }
        Type* at = arg_types[0];
        if (at->kind == TY_UNKNOWN) {
            error_node(node, "型が分からない値は %.*s できません", node->len, node->name);
            return &ty_unknown;
        }
        Class* klass = class_of(at);
        Function* method = klass ? find_callable(klass, "__str__", 7) : NULL;
        if (!method) {
            error_node(node, "`%s`型の値は %.*s できません", type_text(at), node->len, node->name);
            return &ty_unknown;
        }
        if (method->nparams != 1 || (method->return_type && method->return_type->kind != TY_UNKNOWN && !(method->return_type->kind == TY_CLASS && method->return_type->klass == find_class("str", 3)))) {
            error_node(node, "クラス`%.*s`の`__str__`は引数を取らず、`str`を返す必要があります", klass->len, klass->name);
        }
        Node* conv = calloc(1, sizeof(Node));
        conv->kind = ND_MCALL;
        conv->name = "__str__";
        conv->len = 7;
        conv->left_child = node->args;
        conv->klass = klass;
        conv->func = method;
        node->args = conv;
        if (is_eprint) {
            node->name = "__eprintln";
            node->len = 10;
        } else {
            node->name = "__println";
            node->len = 9;
        }
        return &ty_unknown;
    }
    // exit は prelude 関数にはしない。spell 関数のラベル `exit` が libc の
    // exit と衝突し、ヘルパからの呼び出しが自分自身へ戻ってしまうため、
    // print と同じく型検査でプリミティブ呼び出しへ書き換える
    if (node->len == 4 && !memcmp(node->name, "exit", 4) && !callee && !klass) {
        if (nargs != 1) {
            error_node(node, "関数`exit`の引数は1個ですが、%d個が指定されています", nargs);
            return &ty_unknown;
        }
        if (arg_types[0]->kind != TY_UNKNOWN && !type_equals(&ty_int, arg_types[0])) {
            error_node(node, "関数`exit`の引数には`int`が必要ですが、`%s`が指定されています", type_text(arg_types[0]));
        }
        node->name = "__spell_exit";
        node->len = 12;
        node->builtin = BUILTIN_EXIT;
        return &ty_unknown;
    }
    if (!callee) {
        if (klass) {
            return check_construct(node, klass, arg_types, nargs, expected);
        }
        if (builtin) {
            node->builtin = builtin->id;
            if (nargs != builtin->nparams) {
                error_node(node, "関数`%.*s`の引数は%d個ですが、%d個が指定されています", node->len, node->name, builtin->nparams, nargs);
            }
            for (int i = 0; i < nargs && i < builtin->nparams; i++) {
                Type* pt = builtin->params[i];
                if (pt && arg_types[i]->kind != TY_UNKNOWN && !type_equals(pt, arg_types[i])) {
                    error_node(node, "関数`%.*s`の引数には`%s`が必要ですが、`%s`が指定されています", node->len, node->name, type_text(pt), type_text(arg_types[i]));
                }
            }
            return builtin->ret;
        }
        if (node->len == 7 && (!memcmp(node->name, "getchar", 7) || !memcmp(node->name, "putchar", 7))) {
            return &ty_int;
        }
        return &ty_unknown;
    }
    if (nargs != callee->nparams) {
        error_node(node, "関数`%.*s`の引数は%d個ですが、%d個が指定されています", node->len, node->name, callee->nparams, nargs);
    }
    for (int i = 0; i < nargs && i < callee->nparams && i < 6; i++) {
        Type* pt = callee->param_types[i];
        if (pt && arg_types[i]->kind != TY_UNKNOWN && !type_equals(pt, arg_types[i])) {
            error_node(node, "関数`%.*s`の引数には`%s`が必要ですが、`%s`が指定されています", node->len, node->name, type_text(pt), type_text(arg_types[i]));
        }
    }
    return callee->return_type ? callee->return_type : &ty_unknown;
}


static Type* check_member(Node* node) {
    Type* obj = check_node(node->left_child);
    if (obj->kind == TY_UNKNOWN) {
        if (node->len == 3 && !memcmp(node->name, "len", 3)) {
            node->kind = ND_LEN;
            return &ty_int;
        }
        return &ty_unknown;
    }
    if (obj->kind == TY_ARRAY && node->len == 3 && !memcmp(node->name, "len", 3)) {
        node->kind = ND_LEN;
        return &ty_int;
    }
    Class* klass = class_of(obj);
    if (!klass) {
        error_node(node, "`%s`型の値にフィールドは使えません", type_text(obj));
        return &ty_unknown;
    }
    node->klass = klass;
    Field* field = NULL;
    for (Field* f = klass->fields; f; f = f->next) {
        if (f->len == node->len && !memcmp(f->name, node->name, f->len)) {
            field = f;
            break;
        }
    }
    if (field) {
        node->offset = field->offset;
        return field->type;
    }
    Function* method = find_callable(klass, node->name, node->len);
    if (method) {
        if (method->nparams > 1) {
            error_node(node, "メソッド`%.*s`には引数が必要ですが、括弧なしで使われています", node->len, node->name);
        }
        node->func = method;
        return method->return_type ? method->return_type : &ty_unknown;
    }
    error_node(node, "クラス`%.*s`にフィールド`%.*s`は定義されていません", klass->len, klass->name, node->len, node->name);
    return &ty_unknown;
}

// 配列への append（step 65）。prelude のメソッドではなく、型検査で
// 組み込みノード ND_APPEND へ書き換える（メソッドでは呼び出し元の
// 変数を付け替えられないため、exit の書き換えと同じ方式）。対象は
// スロットを持つ場所（変数・捕捉変数・フィールド・配列の要素）だけに
// 限る。計算された一時値への append は、書き戻す先が無いのでエラー。
// 戻り値は新しい長さ（int）
static Type* check_append(Node* node, Type* obj) {
    Node* recv = node->left_child;
    bool lvalue = recv->kind == ND_LVAR || recv->kind == ND_GVAR
        || recv->kind == ND_CAPVAR || recv->kind == ND_INDEX
        || (recv->kind == ND_MEMBER && !recv->func);
    if (!lvalue) {
        error_node(node, "`append`の対象は変数・フィールド・配列の要素でなければなりません");
    }
    Type* expected_list[6] = {};
    expected_list[0] = obj->elem;
    Type* arg_types[6];
    int nargs = check_args_expected(node->args, arg_types, expected_list);
    if (nargs != 1) {
        error_node(node, "`append`の引数は1個ですが、%d個が指定されています", nargs);
    } else if (arg_types[0]->kind != TY_UNKNOWN && obj->elem->kind != TY_UNKNOWN
            && !type_equals(obj->elem, arg_types[0])) {
        error_node(node, "`append`の引数には`%s`が必要ですが、`%s`が指定されています", type_text(obj->elem), type_text(arg_types[0]));
    }
    if (!lvalue || nargs != 1) {
        // 形が壊れた呼び出しは書き換えない。キャプチャ有効時（LSP）は
        // error_node から戻ってくるため、ND_APPEND の不変条件（対象は
        // スロット、引数は1個）を満たさないノードを残さないこと
        return &ty_unknown;
    }
    node->kind = ND_APPEND;
    return &ty_int;
}

// 矢印の pop 側の代入を仕上げる。call は検査済みの dunder 呼び出しで、
// 要素の型 elem を返す。対象がクラスへの添字なら、値としての検査で
// `__getitem__` 呼び出しへ書き換え済みなので、check_assign と同じく
// `__setitem__` 呼び出しへ組み替える。それ以外の対象は ND_ASSIGN に
// 組み替え、右辺は検査済みの前提で check_assign の対象側の型記録だけを
// なぞる（対象を値として再検査すると添字の書き換えが二重になるため）
static Type* finish_arrow_assign(Node* node, Node* target, Node* call, Type* elem, const char* opname) {
    if (target->kind == ND_MCALL && target->func
            && target->len == 11 && !memcmp(target->name, "__getitem__", 11)) {
        Node* idx = malloc(sizeof(Node));
        memcpy(idx, target->args, sizeof(Node));
        idx->next = NULL;
        rewrite_to_setitem(node, target->klass, target->left_child, idx, target->args->type, elem, call);
        return elem;
    }
    bool assignable = target->kind == ND_LVAR || target->kind == ND_GVAR
        || target->kind == ND_CAPVAR || target->kind == ND_INDEX
        || (target->kind == ND_MEMBER && !target->func);
    if (!assignable) {
        error_node(node, "演算子`%s`の代入先は変数・フィールド・配列の要素でなければなりません", opname);
        return &ty_unknown;
    }
    node->kind = ND_ASSIGN;
    node->left_child = target;
    node->right_child = call;
    if (target->kind == ND_LVAR) {
        Type* recorded = local_types[target->offset / 8];
        check_assign_type(target, recorded, elem);
        if (!recorded && elem->kind != TY_UNKNOWN) {
            local_types[target->offset / 8] = elem;
        }
        target->type = local_types[target->offset / 8];
    } else if (target->kind == ND_GVAR) {
        Type* recorded = find_global_type(target);
        check_assign_type(target, recorded, elem);
        if (recorded->kind == TY_UNKNOWN) {
            record_global_type(target, elem);
        }
        target->type = find_global_type(target);
    } else if (target->kind == ND_CAPVAR) {
        // target->type は値としての検査で確定した捕捉変数の型
        if (target->type && target->type->kind != TY_UNKNOWN && elem->kind != TY_UNKNOWN
                && !type_equals(target->type, elem)) {
            error_node(target, "変数`%.*s`の型は`%s`ですが、`%s`が代入されています", target->len, target->name, type_text(target->type), type_text(elem));
        }
    } else if (target->kind == ND_INDEX) {
        // 配列の要素への代入（クラスへの添字は上で処理済み）。
        // target->type は要素の型
        if (target->type && target->type->kind != TY_UNKNOWN && elem->kind != TY_UNKNOWN
                && !type_equals(target->type, elem)) {
            error_node(node, "配列の要素は`%s`ですが、`%s`が代入されています", type_text(target->type), type_text(elem));
        }
    }
    // ND_MEMBER（フィールド）への代入は check_assign と同様に型照合しない
    return elem;
}

// 矢印演算子（step 66）: `A <- B` と `A -> B` を、オペランドの型で
// dunder プロトコル呼び出し・代入・int の比較へ書き換える。演算子は
// 通常メソッドを直接呼ばず、step 42 の dunder 書き換えと同じ規則で
// プロトコルメソッドへだけ展開する（通常メソッドは dunder のラッパー）。
//   A <- B: 1) A.__push_back__(B)   2) A = B.__pop_front__()   3) A < (-B)
//   A -> B: 1) B.__push_front__(A)  2) B = A.__pop_back__()    3) A > (-B)
// どれにも当たらなければエラー。矢印同士の連鎖は、どちらの向きに
// 結合しても意味が曖昧なのでエラーにする
static Type* check_arrow(Node* node) {
    bool left_arrow = node->kind == ND_ARROW_L;
    const char* opname = left_arrow ? "<-" : "->";
    Node* a = node->left_child;
    Node* b = node->right_child;
    if (a->kind == ND_ARROW_L || a->kind == ND_ARROW_R
            || b->kind == ND_ARROW_L || b->kind == ND_ARROW_R) {
        error_node(node, "矢印演算子は連鎖できません");
        check_node(a);
        check_node(b);
        return &ty_unknown;
    }
    Type* lt = check_node(a);
    Type* rt = check_node(b);

    // 1) push 側: `<-` は A の `__push_back__`、`->` は B の `__push_front__`
    {
        Type* ct = left_arrow ? lt : rt;
        Type* vt = left_arrow ? rt : lt;
        const char* dunder = left_arrow ? "__push_back__" : "__push_front__";
        int dlen = left_arrow ? 13 : 14;
        Class* klass = ct->kind == TY_UNKNOWN ? NULL : class_of(ct);
        Function* method = klass ? find_callable(klass, (char*)dunder, dlen) : NULL;
        if (method) {
            if (left_arrow) {
                return rewrite_to_mcall(node, lt, rt, dunder, dlen, method);
            }
            // `A -> B` はレシーバと引数の並びが逆なので、手で組み替える
            if (method->nparams != 2) {
                error_node(node, "クラス`%.*s`のメソッド`%.*s`の引数は1個ですが、%d個が定義されています", klass->len, klass->name, dlen, dunder, method->nparams - 1);
            }
            if (method->param_types[1] && vt->kind != TY_UNKNOWN && !type_equals(method->param_types[1], vt)) {
                error_node(node, "クラス`%.*s`のメソッド`%.*s`の引数には`%s`が必要ですが、`%s`が指定されています", klass->len, klass->name, dlen, dunder, type_text(method->param_types[1]), type_text(vt));
            }
            node->kind = ND_MCALL;
            node->name = (char*)dunder;
            node->len = dlen;
            node->func = method;
            node->klass = klass;
            node->left_child = b;
            node->args = a;
            node->args->next = NULL;
            return method->return_type ? method->return_type : &ty_unknown;
        }
    }

    // 2) pop 側: `<-` は B.__pop_front__() を A へ、`->` は A.__pop_back__() を B へ
    {
        Node* cont = left_arrow ? b : a;
        Type* ct = left_arrow ? rt : lt;
        Node* target = left_arrow ? a : b;
        const char* dunder = left_arrow ? "__pop_front__" : "__pop_back__";
        int dlen = left_arrow ? 13 : 12;
        Class* klass = ct->kind == TY_UNKNOWN ? NULL : class_of(ct);
        Function* method = klass ? find_callable(klass, (char*)dunder, dlen) : NULL;
        if (method) {
            if (method->nparams != 1) {
                error_node(node, "クラス`%.*s`の`%.*s`は引数を取れません", klass->len, klass->name, dlen, dunder);
                return &ty_unknown;
            }
            Type* elem = method->return_type ? method->return_type : &ty_unknown;
            Node* call = create_node(ND_MCALL);
            call->tok = node->tok;
            call->left_child = cont;
            call->klass = klass;
            call->func = method;
            call->name = (char*)dunder;
            call->len = dlen;
            call->type = elem;
            return finish_arrow_assign(node, target, call, elem, opname);
        }
    }

    // 3) 両辺が int なら比較（相手を符号反転したもの）。右辺のノードは
    // 1回だけ現れるので、評価はちょうど1回になる
    if (lt->kind == TY_INT && rt->kind == TY_INT) {
        Node* neg = create_node(ND_BINOP);
        neg->op.binop = OP_SUB;
        neg->tok = node->tok;
        neg->left_child = create_number_node(0);
        neg->right_child = b;
        neg->type = &ty_int;
        node->kind = ND_BINOP;
        node->op.binop = OP_LT;
        if (left_arrow) {
            node->left_child = a;
            node->right_child = neg;
        } else {
            node->left_child = neg;
            node->right_child = a;
        }
        return bool_type();
    }

    error_node(node, "`%s`型と`%s`型の値に演算子`%s`は使えません", type_text(lt), type_text(rt), opname);
    return &ty_unknown;
}

static Type* check_mcall(Node* node) {
    Type* obj = check_node(node->left_child);
    if (node->len == 3 && !memcmp(node->name, "len", 3) && (obj->kind == TY_ARRAY || obj->kind == TY_UNKNOWN)) {
        Type* arg_types[6];
        int nargs = check_args(node->args, arg_types);
        if (nargs != 0) {
            error_node(node, "`len`の引数は0個ですが、%d個が指定されています", nargs);
        }
        node->kind = ND_LEN;
        return &ty_int;
    }
    if (obj->kind == TY_UNKNOWN) {
        Type* arg_types[6];
        check_args(node->args, arg_types);
        return &ty_unknown;
    }
    if (obj->kind == TY_ARRAY && node->len == 6 && !memcmp(node->name, "append", 6)) {
        return check_append(node, obj);
    }
    Class* klass = class_of(obj);
    if (!klass) {
        error_node(node, "`%s`型の値にメソッドは使えません", type_text(obj));
        return &ty_unknown;
    }
    Function* method = find_callable(klass, node->name, node->len);
    if (!method) {
        // メソッドではなくフィールドなら、関数型フィールドの呼び出しとして
        // 値経由の呼び出しに書き換える（設計メモの決定）
        for (Field* f = klass->fields; f; f = f->next) {
            if (f->len == node->len && !memcmp(f->name, node->name, f->len)) {
                Node* member = create_node(ND_MEMBER);
                member->left_child = node->left_child;
                member->name = node->name;
                member->len = node->len;
                member->tok = node->tok;
                member->klass = klass;
                member->offset = f->offset;
                member->type = f->type;
                node->kind = ND_VCALL;
                node->left_child = member;
                return check_vcall(node);
            }
        }
        error_node(node, "クラス`%.*s`にメソッド`%.*s`は定義されていません", klass->len, klass->name, node->len, node->name);
        return &ty_unknown;
    }
    node->func = method;
    node->klass = klass;
    Type* expected_list[6] = {};
    for (int i = 0; i + 1 < method->nparams && i < 6; i++) {
        expected_list[i] = method->param_types[i + 1];
    }
    Type* arg_types[6];
    int nargs = check_args_expected(node->args, arg_types, expected_list);
    if (nargs != method->nparams - 1) {
        error_node(node, "クラス`%.*s`のメソッド`%.*s`の引数は%d個ですが、%d個が指定されています", klass->len, klass->name, node->len, node->name, method->nparams - 1, nargs);
    }
    for (int i = 0; i < nargs && i + 1 < method->nparams; i++) {
        Type* pt = method->param_types[i + 1];
        if (pt && arg_types[i]->kind != TY_UNKNOWN && !type_equals(pt, arg_types[i])) {
            error_node(node, "クラス`%.*s`のメソッド`%.*s`の引数には`%s`が必要ですが、`%s`が指定されています", klass->len, klass->name, node->len, node->name, type_text(pt), type_text(arg_types[i]));
        }
    }
    return method->return_type ? method->return_type : &ty_unknown;
}

static Type* check_arraylit(Node* node, Type* expected) {
    Type* elem = &ty_unknown;
    Type* elem_expected = expected && expected->kind == TY_ARRAY ? expected->elem : NULL;
    for (Node* e = node->body; e; e = e->next) {
        expected_type = elem_expected;
        Type* et = check_node(e);
        expected_type = NULL;
        if (et->kind == TY_UNKNOWN) {
            continue;
        }
        if (elem->kind == TY_UNKNOWN) {
            elem = et;
            continue;
        }
        if (!type_equals(elem, et)) {
            error_node(e, "配列の要素は`%s`ですが、`%s`が含まれています", type_text(elem), type_text(et));
        }
    }
    return array_type(elem);
}

// Pos2 / Pos3 かどうかと、その次元を返す（Pos でなければ 0）
static int pos_dim(Class* klass) {
    if (!klass) {
        return 0;
    }
    Class* o = klass->origin ? klass->origin : klass;
    if (o->len == 4 && !memcmp(o->name, "Pos2", 4)) {
        return 2;
    }
    if (o->len == 4 && !memcmp(o->name, "Pos3", 4)) {
        return 3;
    }
    return 0;
}

// ---- 安全添え字（step 81）と、Pos 添字の末尾 str の段で使う道具 ----

// str クラスかどうか
static bool is_str_type(Type* t) {
    if (!t || t->kind != TY_CLASS || !t->klass) {
        return false;
    }
    Class* o = t->klass->origin ? t->klass->origin : t->klass;
    return o->len == 3 && !memcmp(o->name, "str", 3);
}

// 1段アクセスした後の要素の型。配列は要素型、str は `__getitem__`
// の戻り型。str の段は通常の `s[i]` と全く同じ扱いで、戻り型は
// prelude の定義に従うだけ（ここで int 前提の特別扱いはしない）
static Type* stage_elem_type(Type* cont_type) {
    if (cont_type->kind == TY_ARRAY) {
        return cont_type->elem;
    }
    Function* fn = find_callable(class_of(cont_type), "__getitem__", 11);
    return fn ? fn->return_type : &ty_unknown;
}

// 一時スロットを読むノード。型は local_types にも記録しておく
static Node* safe_slot(Node* at, int offset, Type* type) {
    Node* slot = create_node(ND_LVAR);
    slot->offset = offset;
    slot->tok = at->tok;
    slot->type = type;
    local_types[offset / 8] = type;
    return slot;
}

static Node* safe_assign(Node* at, Node* slot, Node* value, Type* type) {
    Node* a = create_node(ND_ASSIGN);
    a->tok = at->tok;
    a->left_child = slot;
    a->right_child = value;
    a->type = type;
    return a;
}

static Node* safe_binop(Node* at, BinOp op, Node* l, Node* r) {
    Node* b = create_node(ND_BINOP);
    b->op.binop = op;
    b->tok = at->tok;
    b->left_child = l;
    b->right_child = r;
    b->type = bool_type();
    return b;
}

static Node* safe_num(Node* at, long val) {
    Node* n = create_number_node(val);
    n->tok = at->tok;
    n->type = &ty_int;
    return n;
}

// コンテナ式の長さを読むノード。配列は ND_LEN、str は len() 呼び出し
static Node* safe_len(Node* at, Node* cont, Type* cont_type) {
    if (is_str_type(cont_type)) {
        Class* klass = class_of(cont_type);
        Function* fn = find_callable(klass, "len", 3);
        Node* call = make_mcall(cont, klass, fn, "len", 3);
        call->tok = at->tok;
        call->type = &ty_int;
        return call;
    }
    Node* n = create_node(ND_LEN);
    n->tok = at->tok;
    n->left_child = cont;
    n->type = &ty_int;
    return n;
}

// スロットに退避したコンテナと添え字から、1段分の要素アクセスを作る。
// 配列は通常の添え字、str は通常の `s[i]` と同じ __getitem__ 呼び出し
// （戻り型は prelude の定義どおり）
static Node* safe_elem(Node* at, Node* cont, Type* cont_type, Node* idx) {
    if (is_str_type(cont_type)) {
        Class* klass = class_of(cont_type);
        Function* fn = find_callable(klass, "__getitem__", 11);
        Node* call = make_mcall(cont, klass, fn, "__getitem__", 11);
        call->tok = at->tok;
        call->args = idx;
        call->type = fn ? fn->return_type : &ty_unknown;
        return call;
    }
    Node* n = create_node(ND_INDEX);
    n->tok = at->tok;
    n->left_child = cont;
    n->right_child = idx;
    n->type = cont_type->kind == TY_ARRAY ? cont_type->elem : &ty_unknown;
    return n;
}

// Pos の d 番目の成分を取り出すノード（レシーバは呼び出し側で用意する）
static Node* safe_pos_comp(Node* at, Node* recv, Type* pos_type, int d) {
    Class* klass = class_of(pos_type);
    Function* fn = find_callable(klass, "__getitem__", 11);
    Node* call = make_mcall(recv, klass, fn, "__getitem__", 11);
    call->tok = at->tok;
    call->args = safe_num(at, d);
    call->type = &ty_int;
    return call;
}

// 安全添え字（ND_SAFEIDX）を検査し、部品に分解する:
// - guard: 全段の範囲チェック。対象・各添え字・中間値は、パース時に
//   確保された一時スロット（node->offset から連続）へ、&& の短絡を
//   使って浅い段から順に1回だけ評価・退避する代入を含む
// - value: guard が真のときにだけ評価してよい要素アクセス。
//   スロット読みだけで組み立てる
// 対象の型が分からない場合は guard を NULL にし、value に通常の
// 添え字の連鎖を入れて従来どおり緩く通す。戻り値は要素の型
static Type* build_safe_parts(Node* node, Node** guard_out, Node** value_out) {
    *guard_out = NULL;
    *value_out = NULL;
    Type* lt = check_node(node->left_child);
    int nb = 0;
    for (Node* a = node->args; a; a = a->next) {
        nb++;
    }
    Node** idx_nodes = malloc(sizeof(Node*) * (nb ? nb : 1));
    Type** idx_types = malloc(sizeof(Type*) * (nb ? nb : 1));
    int bi = 0;
    for (Node* a = node->args; a; a = a->next, bi++) {
        idx_nodes[bi] = a;
        idx_types[bi] = check_node(a);
    }
    if (lt->kind == TY_UNKNOWN) {
        Node* cur = node->left_child;
        for (int i = 0; i < nb; i++) {
            Node* step = create_node(ND_INDEX);
            step->tok = node->tok;
            step->left_child = cur;
            step->right_child = idx_nodes[i];
            step->type = &ty_unknown;
            cur = step;
        }
        *value_out = cur;
        free(idx_nodes);
        free(idx_types);
        return &ty_unknown;
    }
    // 段の計画: コンテナの型と添え字式の列を作る。Pos 添え字は
    // 成分の数だけ段に展開する
    int max_stages = nb * 3;
    Type** stage_cont = malloc(sizeof(Type*) * max_stages);
    Node** stage_index = malloc(sizeof(Node*) * max_stages);
    int ns = 0;
    Type* cont = lt;
    int base = node->offset;
    for (int b = 0; b < nb; b++) {
        Node* idx = idx_nodes[b];
        Type* it = idx_types[b];
        if (cont->kind == TY_ARRAY && it->kind == TY_CLASS && pos_dim(it->klass) > 0) {
            int dim = pos_dim(it->klass);
            // 深さは配列の入れ子の段数。末尾の str はもう1段と数える
            int depth = 0;
            Type* t = cont;
            while (t->kind == TY_ARRAY) {
                depth++;
                t = t->elem;
            }
            if (is_str_type(t)) {
                depth++;
            }
            if (depth != dim) {
                error_node(node, "座標`%s`は%d次元ですが、配列の入れ子は%d段です", type_text(it), dim, depth);
                *value_out = safe_num(node, 0);
                return &ty_unknown;
            }
            int pos_off = base + 8 * (1 + 9 * nb + b);
            for (int d = 0; d < dim; d++) {
                Node* recv;
                if (d == 0) {
                    recv = safe_assign(node, safe_slot(node, pos_off, it), idx, it);
                } else {
                    recv = safe_slot(node, pos_off, it);
                }
                stage_cont[ns] = cont;
                stage_index[ns] = safe_pos_comp(node, recv, it, d);
                ns++;
                cont = stage_elem_type(cont);
            }
        } else {
            if (cont->kind != TY_ARRAY && !is_str_type(cont)) {
                if (cont->kind == TY_CLASS) {
                    error_node(node, "安全添え字は配列と文字列にだけ使えます（`%s`型には使えません）", type_text(cont));
                } else {
                    error_node(node, "`%s`型の値に安全添え字は使えません", type_text(cont));
                }
                *value_out = safe_num(node, 0);
                return &ty_unknown;
            }
            if (it->kind != TY_UNKNOWN && it->kind != TY_INT) {
                error_node(idx, "添え字には`int`が必要ですが、`%s`が指定されています", type_text(it));
                *value_out = safe_num(node, 0);
                return &ty_unknown;
            }
            stage_cont[ns] = cont;
            stage_index[ns] = idx;
            ns++;
            cont = stage_elem_type(cont);
        }
    }
    Type* elem_type = cont;
    // guard と value の組み立て。スロットの割り当て（base からの
    // スロット番号）: 0 = 対象（C0）、段 k（0始まり）ごとに
    // 1+3k = 長さ、2+3k = 添え字、3+3k = 次のコンテナ（C{k+1}）
    Node* guard = NULL;
    Node* prev_access = NULL;
    for (int k = 0; k < ns; k++) {
        Type* ct = stage_cont[k];
        int cont_off = base + 8 * (k == 0 ? 0 : 3 * k);
        int len_off = base + 8 * (1 + 3 * k);
        int idx_off = base + 8 * (2 + 3 * k);
        Node* cont_expr;
        if (k == 0) {
            cont_expr = safe_assign(node, safe_slot(node, cont_off, ct), node->left_child, ct);
        } else {
            cont_expr = safe_assign(node, safe_slot(node, cont_off, ct), prev_access, ct);
        }
        Node* len_assign = safe_assign(node, safe_slot(node, len_off, &ty_int),
                                       safe_len(node, cont_expr, ct), &ty_int);
        Node* a = safe_binop(node, OP_LE, safe_num(node, 0), len_assign);
        Node* idx_assign = safe_assign(node, safe_slot(node, idx_off, &ty_int), stage_index[k], &ty_int);
        Node* bnode = safe_binop(node, OP_LE, safe_num(node, 0), idx_assign);
        Node* c = safe_binop(node, OP_LT, safe_slot(node, idx_off, &ty_int),
                             safe_slot(node, len_off, &ty_int));
        Node* conj = safe_binop(node, OP_AND, safe_binop(node, OP_AND, a, bnode), c);
        guard = guard ? safe_binop(node, OP_AND, guard, conj) : conj;
        Node* access = safe_elem(node, safe_slot(node, cont_off, ct), ct,
                                 safe_slot(node, idx_off, &ty_int));
        if (k == ns - 1) {
            *value_out = access;
        } else {
            prev_access = access;
        }
    }
    *guard_out = guard;
    free(idx_nodes);
    free(idx_types);
    free(stage_cont);
    free(stage_index);
    return elem_type;
}

// Pos 添字の糖衣（step 77）: 配列への `a[p]`（p は Pos2 / Pos3）を
// `a[p[0]][p[1]]`（Pos3 は3段）へ書き換える。深さは Pos の次元で
// 決め、配列の入れ子の深さと一致しなければエラーにする。p は一度
// だけ評価するため、パース時に確保された一時置き場（node->offset）
// へ退避する代入を、最初の要素取り出しのレシーバとして埋め込む
// （代入式の値が p 自身になる）。添字の p 自体は検査済みなので、
// 書き換え後のノードは再検査せず、手で型を確定させる
// （check_chain と同じ方針）
static Type* check_pos_index(Node* node, Type* lt, Type* it) {
    int dim = pos_dim(it->klass);
    int depth = 0;
    Type* elem = lt;
    while (elem->kind == TY_ARRAY) {
        depth++;
        elem = elem->elem;
    }
    // 末尾の str はもう1段と数える（step 81）: [str] への Pos2 は
    // 「行を取り、行の中の文字を取る」2段として扱う
    if (is_str_type(elem)) {
        depth++;
    }
    if (depth != dim) {
        error_node(node, "座標`%s`は%d次元ですが、配列の入れ子は%d段です", type_text(it), dim, depth);
        return &ty_unknown;
    }
    if (node->offset == 0) {
        // パースで一時置き場を確保していない添字（コンパイラ内部で
        // 合成されたノード）は対象外。通常の利用者の添字は全て
        // primary の後置解析で確保される
        error_node(node, "座標`%s`での添え字はここでは使えません", type_text(it));
        return &ty_unknown;
    }
    Class* pklass = class_of(it);
    Function* getitem = find_callable(pklass, "__getitem__", 11);
    if (!getitem) {
        error_node(node, "クラス`%.*s`に`__getitem__`は定義されていません", pklass->len, pklass->name);
        return &ty_unknown;
    }
    // 一時置き場の型を記録し、退避代入（左辺スロット＋検査済みの p）を組む
    local_types[node->offset / 8] = it;
    Node* slot = create_node(ND_LVAR);
    slot->offset = node->offset;
    slot->tok = node->tok;
    slot->type = it;
    Node* assign = create_node(ND_ASSIGN);
    assign->tok = node->tok;
    assign->left_child = slot;
    assign->right_child = node->right_child;
    assign->type = it;
    Node* cur = node->left_child;
    Type* cur_elem = lt;
    for (int i = 0; i < dim; i++) {
        // p[i] の取り出し。最初の段は退避代入をレシーバにする
        Node* recv;
        if (i == 0) {
            recv = assign;
        } else {
            recv = create_node(ND_LVAR);
            recv->offset = node->offset;
            recv->tok = node->tok;
            recv->type = it;
        }
        Node* idx = create_node(ND_INDEX);
        idx->tok = node->tok;
        idx->left_child = recv;
        idx->right_child = create_number_node(i);
        rewrite_to_mcall(idx, it, &ty_int, "__getitem__", 11, getitem);
        idx->type = &ty_int;
        Node* step = create_node(ND_INDEX);
        step->tok = node->tok;
        step->left_child = cur;
        step->right_child = idx;
        if (is_str_type(cur_elem)) {
            // 文字列への段（step 81）: 通常の `s[i]` と同じ
            // __getitem__ 呼び出しにする。cur_elem は要素ではなく
            // str そのものなので、その戻り型が次の要素型になる
            Class* sklass = class_of(cur_elem);
            Function* sgetitem = find_callable(sklass, "__getitem__", 11);
            if (!sgetitem) {
                error_node(node, "クラス`%.*s`に`__getitem__`は定義されていません", sklass->len, sklass->name);
                return &ty_unknown;
            }
            cur_elem = rewrite_to_mcall(step, cur_elem, &ty_int, "__getitem__", 11, sgetitem);
            cur = step;
            cur->type = cur_elem;
        } else {
            cur = step;
            cur_elem = cur_elem->elem;
            cur->type = cur_elem;
        }
    }
    Node* nxt = node->next;
    *node = *cur;
    node->next = nxt;
    node->type = cur_elem;
    return cur_elem;
}

static Type* check_index(Node* node) {
    Type* lt = check_node(node->left_child);
    Type* it = check_node(node->right_child);
    if (lt->kind == TY_ARRAY && it->kind == TY_CLASS && pos_dim(it->klass) > 0) {
        return check_pos_index(node, lt, it);
    }
    if (lt->kind == TY_CLASS) {
        Function* method = find_callable(class_of(lt), "__getitem__", 11);
        if (!method) {
            error_node(node, "`%s`型の値に添え字は使えません", type_text(lt));
            return &ty_unknown;
        }
        return rewrite_to_mcall(node, lt, it, "__getitem__", 11, method);
    }
    if (lt->kind != TY_UNKNOWN && lt->kind != TY_ARRAY) {
        error_node(node, "`%s`型の値に添え字は使えません", type_text(lt));
    }
    if (it->kind != TY_UNKNOWN && it->kind != TY_INT) {
        error_node(node->right_child, "添え字には`int`が必要ですが、`%s`が指定されています", type_text(it));
    }
    return lt->kind == TY_ARRAY ? lt->elem : &ty_unknown;
}

static Type* check_return(Node* node) {
    expected_type = current_return;
    Type* rt = check_node(node->left_child);
    expected_type = NULL;
    if (rt->kind != TY_UNKNOWN) {
        if (!current_return) {
            current_fn->return_type = rt;
            current_return = rt;
        } else if (!type_equals(current_return, rt)) {
            error_node(node, "関数`%.*s`の戻り値は`%s`ですが、`%s`が返されています", current_fn->len, current_fn->name, type_text(current_return), type_text(rt));
        }
    }
    return rt;
}

static Node* iter_slot_node(Node* for_node) {
    Node* slot = create_node(ND_LVAR);
    slot->offset = for_node->offset;
    return slot;
}

static Node* make_mcall(Node* receiver, Class* klass, Function* fn, const char* name, int len) {
    Node* call = create_node(ND_MCALL);
    call->left_child = receiver;
    call->klass = klass;
    call->func = fn;
    call->name = (char*)name;
    call->len = len;
    return call;
}

// 条件位置の真理値解釈（step 79）: 検査済みの式 expr（型 t）を、条件
// として使える形へ書き換えて返す。bool と int はそのまま使う。
// クラス・配列は `__bool__() -> bool` が定義されていれば、その呼び
// 出しへ書き換える。定義が無ければ位置つきエラーにする（以前は
// ポインタの真偽で常に真として黙って通っていた）
static Node* boolify(Node* expr, Type* t) {
    if (!t || t->kind == TY_UNKNOWN || t->kind == TY_INT) {
        return expr;
    }
    if (t->kind == TY_CLASS && t->klass && t->klass->len == 4
            && !memcmp(t->klass->name, "bool", 4)) {
        return expr;
    }
    if (t->kind == TY_ARRAY || t->kind == TY_CLASS) {
        Class* klass = class_of(t);
        Function* method = klass ? find_callable(klass, "__bool__", 8) : NULL;
        if (!method) {
            if (klass) {
                error_node(expr, "クラス`%.*s`の値は条件に使えません（`__bool__`が定義されていません）", klass->len, klass->name);
            } else {
                error_node(expr, "`%s`型の値は条件に使えません", type_text(t));
            }
            return expr;
        }
        if (method->nparams != 1) {
            error_node(expr, "クラス`%.*s`のメソッド`__bool__`は引数を取れませんが、%d個が定義されています", klass->len, klass->name, method->nparams - 1);
            return expr;
        }
        if (method->return_type && !type_equals(method->return_type, bool_type())) {
            error_node(expr, "クラス`%.*s`のメソッド`__bool__`の戻り値は`bool`でなければなりません", klass->len, klass->name);
            return expr;
        }
        Node* call = make_mcall(expr, klass, method, "__bool__", 8);
        call->tok = expr->tok;
        call->type = bool_type();
        return call;
    }
    error_node(expr, "`%s`型の値は条件に使えません", type_text(t));
    return expr;
}

// ---- 未定義変数の検出（step 79） ----
// パースは「代入で変数が生まれる」言語仕様のため、読み取りでも暗黙に
// 変数を登録してしまう。そこで型検査の前に、代入される名前を関数
// ごとに集めておき、どこでも代入されていない名前の読み取りをエラー
// にする。文同士の前後関係は見ない（関数全体・プログラム全体で
// 代入が存在すれば定義済みとする。前方参照を許すため）

typedef struct DefName DefName;
struct DefName {
    char* name;
    int len;
    int offset;   // ローカル用。グローバルでは -1
    Token* tok;   // 定義位置（タイポ候補の行表示用）
    DefName* next;
};

typedef struct FnDefs FnDefs;
struct FnDefs {
    Function* fn;
    DefName* locals;
    FnDefs* next;
};

static FnDefs* fn_defs_list;
// プログラム全体で代入されるグローバルの名前
static DefName* global_defs;

static void def_add(DefName** list, char* name, int len, int offset, Token* tok) {
    for (DefName* d = *list; d; d = d->next) {
        if (d->len == len && d->offset == offset && !memcmp(d->name, name, len)) {
            return;
        }
    }
    DefName* d = calloc(1, sizeof(DefName));
    d->name = name;
    d->len = len;
    d->offset = offset;
    d->tok = tok;
    d->next = *list;
    *list = d;
}

static DefName* def_find(DefName* list, char* name, int len) {
    for (DefName* d = list; d; d = d->next) {
        if (d->len == len && !memcmp(d->name, name, len)) {
            return d;
        }
    }
    return NULL;
}

static DefName* def_find_offset(DefName* list, int offset) {
    for (DefName* d = list; d; d = d->next) {
        if (d->offset == offset) {
            return d;
        }
    }
    return NULL;
}

// 代入先になりうる場所に直接現れた変数を、定義として記録する
static void collect_def_target(Node* target, DefName** locals) {
    if (!target || !target->name) {
        return;
    }
    if (target->kind == ND_LVAR) {
        def_add(locals, target->name, target->len, target->offset, target->tok);
    } else if (target->kind == ND_GVAR) {
        def_add(&global_defs, target->name, target->len, -1, target->tok);
    }
}

static void collect_defs_node(Node* node, DefName** locals) {
    if (!node) {
        return;
    }
    if (node->kind == ND_LAMBDA) {
        // ラムダ本体は別の関数として個別に集める。中には入らない
        collect_defs_node(node->next, locals);
        return;
    }
    if (node->kind == ND_ASSIGN) {
        collect_def_target(node->left_child, locals);
    } else if (node->kind == ND_MULTIASSIGN) {
        for (Node* t = node->body; t; t = t->next) {
            collect_def_target(t, locals);
        }
    } else if (node->kind == ND_FOR) {
        collect_def_target(node->left_child, locals);
    } else if (node->kind == ND_ARROW_L || node->kind == ND_ARROW_R) {
        // 矢印は向きと型で代入先が決まる。収集の段階では判別できない
        // ため、両辺の直接の変数を定義として拾う（安全側）
        collect_def_target(node->left_child, locals);
        collect_def_target(node->right_child, locals);
    }
    collect_defs_node(node->left_child, locals);
    collect_defs_node(node->right_child, locals);
    collect_defs_node(node->cond, locals);
    collect_defs_node(node->then, locals);
    collect_defs_node(node->els, locals);
    collect_defs_node(node->init, locals);
    collect_defs_node(node->inc, locals);
    collect_defs_node(node->body, locals);
    collect_defs_node(node->args, locals);
    collect_defs_node(node->next, locals);
}

// 関数の定義済みローカルを（まだなら）集める。グローバルの定義も
// この関数の分を全体の集合へ足す（実体化で後から現れる関数の分も
// 拾えるようにするため）
static void ensure_defs(Function* fn) {
    if (!fn) {
        return;
    }
    for (FnDefs* fd = fn_defs_list; fd; fd = fd->next) {
        if (fd->fn == fn) {
            return;
        }
    }
    FnDefs* fd = calloc(1, sizeof(FnDefs));
    fd->fn = fn;
    fd->next = fn_defs_list;
    fn_defs_list = fd;
    collect_defs_node(fn->body, &fd->locals);
}

static DefName* current_local_defs() {
    for (FnDefs* fd = fn_defs_list; fd; fd = fd->next) {
        if (fd->fn == current_fn) {
            return fd->locals;
        }
    }
    return NULL;
}

// トークンが現在のソース（利用者のファイル）の範囲内にある場合に
// その行番号（1始まり）を返し、範囲外なら 0 を返す
static long token_line_in_src(Token* tok) {
    if (!tok || !tok->str || !src) {
        return 0;
    }
    uintptr_t p = (uintptr_t)tok->str;
    uintptr_t lo = (uintptr_t)src;
    uintptr_t hi = lo + (uintptr_t)strlen(src);
    if (p < lo || p >= hi) {
        return 0;
    }
    long line = 1;
    for (char* c = src; c < tok->str; c++) {
        if (*c == '\n') {
            line++;
        }
    }
    return line;
}

// 編集距離（レーベンシュタイン、バイト単位）
static int edit_distance(const char* a, int alen, const char* b, int blen) {
    int prev[256];
    int cur[256];
    if (alen > 255 || blen > 255) {
        return 1 << 20;
    }
    for (int j = 0; j <= blen; j++) {
        prev[j] = j;
    }
    for (int i = 1; i <= alen; i++) {
        cur[0] = i;
        for (int j = 1; j <= blen; j++) {
            int cost = a[i - 1] == b[j - 1] ? 0 : 1;
            int del = prev[j] + 1;
            int ins = cur[j - 1] + 1;
            int sub = prev[j - 1] + cost;
            int m = del < ins ? del : ins;
            cur[j] = m < sub ? m : sub;
        }
        memcpy(prev, cur, (blen + 1) * sizeof(int));
    }
    return prev[blen];
}

// 未定義変数の読み取りを報告する。設計メモの規定に従い、参照できる
// 変数の中に編集距離の近いものが1つだけ突出してあれば、ヒントとして
// typo の候補を添える（同率・僅差なら出さない。最大1件）
static void error_undef(Node* node) {
    const char* best_name = NULL;
    int best_len = 0;
    long best_line = 0;
    int best_dist = 1 << 20;
    int second_dist = 1 << 20;
    if (node->len >= 2) {
        DefName* lists[2] = { current_local_defs(), global_defs };
        for (int li = 0; li < 2; li++) {
            for (DefName* d = lists[li]; d; d = d->next) {
                if (d->len == node->len && !memcmp(d->name, node->name, node->len)) {
                    continue;
                }
                long line = token_line_in_src(d->tok);
                if (line == 0) {
                    continue;
                }
                int dist = edit_distance(node->name, node->len, d->name, d->len);
                if (dist < best_dist) {
                    second_dist = best_dist;
                    best_dist = dist;
                    best_name = d->name;
                    best_len = d->len;
                    best_line = line;
                } else if (dist < second_dist) {
                    second_dist = dist;
                }
            }
        }
        // 短い名前ほど厳しく: 許容距離は名前の長さで段階を付ける
        int longer = node->len > best_len ? node->len : best_len;
        int limit = longer <= 2 ? 1 : longer <= 5 ? 2 : 3;
        if (best_name && (best_dist > limit || second_dist - best_dist <= 1)) {
            best_name = NULL;
        }
    }
    if (best_name) {
        error_node(node, "変数`%.*s`は定義されていません ヒント: もしかして `%.*s` ですか（定義は %ld 行目）", node->len, node->name, best_len, best_name, best_line);
    } else {
        error_node(node, "変数`%.*s`は定義されていません", node->len, node->name);
    }
}

static bool is_loop_var(Node* target, Node* var) {
    if (!target || !var) {
        return false;
    }
    if (var->kind == ND_LVAR) {
        return target->kind == ND_LVAR && target->offset == var->offset;
    }
    if (var->kind == ND_GVAR) {
        return target->kind == ND_GVAR && target->len == var->len && !memcmp(target->name, var->name, var->len);
    }
    return false;
}

// ループ本体の中から、ループ変数への代入（複合代入と後置 ++/-- は
// desugar 済みで ND_ASSIGN になっている）を探し、最初に見つけた
// 代入先ノードを返す。ラムダ本体の中は探さない（捕捉は値渡しで、
// 中の代入は ND_CAPVAR 宛てになるため、書き戻しの対象ではない）
static Node* find_var_assign(Node* node, Node* var) {
    if (!node || node->kind == ND_LAMBDA) {
        return NULL;
    }
    if (node->kind == ND_ASSIGN && is_loop_var(node->left_child, var)) {
        return node->left_child;
    }
    if (node->kind == ND_MULTIASSIGN) {
        for (Node* t = node->body; t; t = t->next) {
            if (is_loop_var(t, var)) {
                return t;
            }
        }
    }
    Node* found;
    if ((found = find_var_assign(node->left_child, var))) return found;
    if ((found = find_var_assign(node->right_child, var))) return found;
    if ((found = find_var_assign(node->cond, var))) return found;
    if ((found = find_var_assign(node->then, var))) return found;
    if ((found = find_var_assign(node->els, var))) return found;
    if ((found = find_var_assign(node->init, var))) return found;
    if ((found = find_var_assign(node->inc, var))) return found;
    if ((found = find_var_assign(node->body, var))) return found;
    if ((found = find_var_assign(node->args, var))) return found;
    if ((found = find_var_assign(node->next, var))) return found;
    return NULL;
}

static Node* make_set_current_call(Node* for_node, Class* iklass, Function* set_current_fn) {
    Node* var = for_node->left_child;
    Node* arg = create_node(var->kind);
    arg->offset = var->offset;
    arg->name = var->name;
    arg->len = var->len;
    arg->tok = var->tok;
    Node* call = make_mcall(iter_slot_node(for_node), iklass, set_current_fn, "__set_current__", 15);
    call->args = arg;
    return call;
}

// このループ宛の break / continue を「書き戻してから break / continue」に
// 書き換える。入れ子のループ本体には入らない（その break は入れ子側の
// もの）が、入れ子の for の else 節はこのループの外側なので対象に含める
// （rewrite_breaks_stmt と同じ歩き方）
static void wrap_loop_exits(Node* node, Node* for_node, Class* iklass, Function* set_current_fn) {
    if (!node) {
        return;
    }
    switch (node->kind) {
        case ND_BREAK:
        case ND_CONTINUE: {
            Node* call = make_set_current_call(for_node, iklass, set_current_fn);
            Node* orig = create_node(node->kind);
            orig->tok = node->tok;
            call->next = orig;
            node->kind = ND_BLOCK;
            node->body = call;
            return;
        }
        case ND_WHILE:
            return;
        case ND_FOR:
            wrap_loop_exits(node->els, for_node, iklass, set_current_fn);
            return;
        case ND_BLOCK:
            for (Node* s = node->body; s; s = s->next) {
                wrap_loop_exits(s, for_node, iklass, set_current_fn);
            }
            return;
        case ND_IF:
            wrap_loop_exits(node->then, for_node, iklass, set_current_fn);
            wrap_loop_exits(node->els, for_node, iklass, set_current_fn);
            return;
        default:
            return;
    }
}

// for 文を「イテレータ取得 → while (__has_next__) { 変数 = __next__(); 本体 }」に書き換える。
// プロトコルは Java 式（__iter__ / __has_next__ / __next__）。
// for...else 用: この for 宛の break を「フラグを倒してから break」に書き換える。
// 入れ子のループ本体には入らない（その break は入れ子側のもの）が、
// 入れ子の for の else 節は外側のループの外側にあるので対象に含める
static void rewrite_breaks_stmt(Node* node, int flag_offset) {
    if (!node) {
        return;
    }
    switch (node->kind) {
        case ND_BREAK: {
            Node* flag = create_node(ND_LVAR);
            flag->offset = flag_offset;
            Node* clear = create_node(ND_ASSIGN);
            clear->left_child = flag;
            Node* zero = create_node(ND_NUM);
            zero->val = 0;
            clear->right_child = zero;
            clear->next = create_node(ND_BREAK);
            node->kind = ND_BLOCK;
            node->body = clear;
            return;
        }
        case ND_WHILE:
            return;
        case ND_FOR:
            rewrite_breaks_stmt(node->els, flag_offset);
            return;
        case ND_BLOCK:
            for (Node* s = node->body; s; s = s->next) {
                rewrite_breaks_stmt(s, flag_offset);
            }
            return;
        case ND_IF:
            rewrite_breaks_stmt(node->then, flag_offset);
            rewrite_breaks_stmt(node->els, flag_offset);
            return;
        default:
            return;
    }
}

static Type* check_for(Node* node) {
    Type* iterable = check_node(node->init);
    // プロトコル検査。壊れている場合は protocol_ok を倒し、while への
    // 書き換えはしない。ただしループ変数の型記録と本体の検査は続け、
    // 本体側の独立したエラーを拾う（要素型は不明として扱う）
    Class* klass = NULL;
    Class* iklass = NULL;
    Function* iter_fn = NULL;
    Function* has_next_fn = NULL;
    Function* next_fn = NULL;
    Function* set_current_fn = NULL;
    Type* elem = &ty_unknown;
    bool protocol_ok = true;
    if (iterable->kind == TY_UNKNOWN) {
        error_node(node->init, "型が分からない値は反復できません");
        protocol_ok = false;
    }
    if (protocol_ok) {
        klass = class_of(iterable);
        iter_fn = klass ? find_callable(klass, "__iter__", 8) : NULL;
        if (!iter_fn) {
            error_node(node->init, "`%s`型の値は反復できません", type_text(iterable));
            protocol_ok = false;
        }
    }
    if (protocol_ok) {
        if (iter_fn->nparams != 1) {
            error_node(node, "クラス`%.*s`の`__iter__`は引数を取れません", klass->len, klass->name);
        }
        Type* iter_type = iter_fn->return_type;
        if (!iter_type || iter_type->kind != TY_CLASS || !(iklass = class_of(iter_type))) {
            error_node(node, "クラス`%.*s`の`__iter__`はイテレータを返す必要があります", klass->len, klass->name);
            protocol_ok = false;
        }
    }
    if (protocol_ok) {
        has_next_fn = find_callable(iklass, "__has_next__", 12);
        next_fn = find_callable(iklass, "__next__", 8);
        if (!has_next_fn || !next_fn) {
            error_node(node, "クラス`%.*s`には`__has_next__`と`__next__`が必要です", iklass->len, iklass->name);
            protocol_ok = false;
        }
    }
    if (protocol_ok) {
        if (has_next_fn->nparams != 1) {
            error_node(node, "クラス`%.*s`の`__has_next__`は引数を取れません", iklass->len, iklass->name);
        }
        if (next_fn->nparams != 1) {
            error_node(node, "クラス`%.*s`の`__next__`は引数を取れません", iklass->len, iklass->name);
        }
        if (has_next_fn->return_type && !type_equals(has_next_fn->return_type, bool_type())) {
            error_node(node, "クラス`%.*s`の`__has_next__`は`bool`を返す必要があります", iklass->len, iklass->name);
        }
        elem = next_fn->return_type ? next_fn->return_type : &ty_unknown;
        // __set_current__ は任意メソッド（書き戻し用）。あれば署名だけ検査する
        set_current_fn = find_callable(iklass, "__set_current__", 15);
        if (set_current_fn) {
            if (set_current_fn->nparams != 2) {
                error_node(node, "クラス`%.*s`の`__set_current__`は引数を1つ取る必要があります", iklass->len, iklass->name);
            }
            Type* pt = set_current_fn->param_types[1];
            if (pt && elem->kind != TY_UNKNOWN && !type_equals(pt, elem)) {
                error_node(node, "クラス`%.*s`の`__set_current__`の引数は`%s`である必要があります", iklass->len, iklass->name, type_text(elem));
            }
        }
    }

    Node* get_iter = create_node(ND_ASSIGN);
    get_iter->left_child = iter_slot_node(node);
    get_iter->right_child = make_mcall(node->init, klass, iter_fn, "__iter__", 8);

    Node* set_var = create_node(ND_ASSIGN);
    set_var->left_child = node->left_child;
    set_var->right_child = make_mcall(iter_slot_node(node), iklass, next_fn, "__next__", 8);
    set_var->next = node->then;

    Node* loop_body = create_node(ND_BLOCK);
    loop_body->body = set_var;

    Node* loop = create_node(ND_WHILE);
    loop->cond = make_mcall(iter_slot_node(node), iklass, has_next_fn, "__has_next__", 12);
    loop->then = loop_body;

    Node* var = node->left_child;
    if (var->kind == ND_LVAR) {
        Type* recorded = local_types[var->offset / 8];
        check_assign_type(var, recorded, elem);
        if (!recorded && elem->kind != TY_UNKNOWN) {
            local_types[var->offset / 8] = elem;
        }
        var->type = local_types[var->offset / 8];
    } else if (var->kind == ND_GVAR) {
        Type* recorded = find_global_type(var);
        check_assign_type(var, recorded, elem);
        if (recorded->kind == TY_UNKNOWN) {
            record_global_type(var, elem);
        }
        var->type = find_global_type(var);
    }
    // ループ変数への代入があれば、その結果を現在の要素へ書き戻す（step 58）。
    // 書き戻しはイテレータの任意メソッド __set_current__ で行い、本体の末尾と、
    // このループ宛の break / continue の直前に呼び出しを差し込む。
    // ponytail: return で抜ける場合は書き戻さない（要素はもう使われない）
    if (protocol_ok && (var->kind == ND_LVAR || var->kind == ND_GVAR)) {
        Node* assign = find_var_assign(node->then, var);
        if (assign && !set_current_fn) {
            error_node(assign, "`%s`型の反復ではループ変数への代入は要素に書き戻されないため、代入できません", type_text(iterable));
        } else if (assign) {
            local_types[node->offset / 8] = iter_fn->return_type;
            wrap_loop_exits(node->then, node, iklass, set_current_fn);
            Node* tail = node->then;
            while (tail->next) {
                tail = tail->next;
            }
            tail->next = make_set_current_call(node, iklass, set_current_fn);
        }
    }
    if (node->els) {
        rewrite_breaks_stmt(node->then, (int)node->val);
    }
    check_node(node->then);
    if (node->els) {
        check_node(node->els);
    }

    if (!protocol_ok) {
        // プロトコルが壊れている場合は while への書き換えをしない
        // （診断があるので codegen は走らない）
        return &ty_unknown;
    }
    node->kind = ND_BLOCK;
    if (node->els) {
        // break しなかったときだけ else 節が動くよう、フラグつきでつなぐ
        Node* flag_init = create_node(ND_ASSIGN);
        Node* flag_var = create_node(ND_LVAR);
        flag_var->offset = (int)node->val;
        flag_init->left_child = flag_var;
        Node* one = create_node(ND_NUM);
        one->val = 1;
        flag_init->right_child = one;
        Node* if_node = create_node(ND_IF);
        Node* cond = create_node(ND_LVAR);
        cond->offset = (int)node->val;
        if_node->cond = cond;
        if_node->then = node->els;
        node->body = flag_init;
        flag_init->next = get_iter;
        get_iter->next = loop;
        loop->next = if_node;
    } else {
        node->body = get_iter;
        get_iter->next = loop;
    }
    return &ty_unknown;
}

// 複数代入: 値を全部評価して一時置き場へ退避し、それから対象へ1つずつ代入する
static Type* check_multiassign(Node* node) {
    int i = 0;
    for (Node* v = node->args; v; v = v->next, i++) {
        Type* vt = check_node(v);
        local_types[(node->offset + i * 8) / 8] = vt;
    }
    Node* head = NULL;
    Node** tail = &head;
    i = 0;
    for (Node* v = node->args; v; v = v->next, i++) {
        Node* slot = create_node(ND_LVAR);
        slot->offset = node->offset + i * 8;
        Node* a = create_node(ND_ASSIGN);
        a->left_child = slot;
        a->right_child = v;
        *tail = a;
        tail = &a->next;
    }
    i = 0;
    for (Node* target = node->body; target; target = target->next, i++) {
        Node* slot = create_node(ND_LVAR);
        slot->offset = node->offset + i * 8;
        Node* a = create_node(ND_ASSIGN);
        a->left_child = target;
        a->right_child = slot;
        check_node(a);
        *tail = a;
        tail = &a->next;
    }
    node->kind = ND_BLOCK;
    node->body = head;
    return &ty_unknown;
}

// 連鎖比較: 中間値を2つの一時置き場に交互へ退避しながら、比較を && でつなぐ
static Type* check_chain(Node* node) {
    // 安全添え字は連鎖比較の中では扱わない（step 81）。被演算子の
    // 位置にあると、範囲チェックの無い bool として静かに比較されて
    // しまうため、位置つきエラーにする
    if (node->left_child->kind == ND_SAFEIDX) {
        error_node(node->left_child, "連鎖比較の中では安全添え字は使えません");
    }
    for (Node* pair = node->body; pair; pair = pair->next) {
        if (pair->left_child->kind == ND_SAFEIDX) {
            error_node(pair->left_child, "連鎖比較の中では安全添え字は使えません");
        }
    }
    Node* prev_operand = node->left_child;
    Node* combined = NULL;
    int i = 0;
    for (Node* pair = node->body; pair; pair = pair->next, i++) {
        int cur_offset = node->offset + (i % 2) * 8;
        Node* slot = create_node(ND_LVAR);
        slot->offset = cur_offset;
        Node* store = create_node(ND_ASSIGN);
        store->left_child = slot;
        store->right_child = pair->left_child;
        Node* comp = create_node(ND_BINOP);
        comp->op.binop = pair->op.binop;
        comp->tok = pair->tok;
        if (pair->val) {
            comp->left_child = store;
            comp->right_child = prev_operand;
        } else {
            comp->left_child = prev_operand;
            comp->right_child = store;
        }
        check_node(comp);
        if (combined) {
            // && の検査は両辺の再検査と bool を返すだけなので、済んだ両辺は
            // 再検査せず、型だけをここで確定させる（再検査による二重の書き換えを避ける）
            Node* and_node = create_node(ND_BINOP);
            and_node->op.binop = OP_AND;
            and_node->left_child = combined;
            and_node->right_child = comp;
            and_node->type = bool_type();
            combined = and_node;
        } else {
            combined = comp;
        }
        Node* prev_slot = create_node(ND_LVAR);
        prev_slot->offset = cur_offset;
        prev_operand = prev_slot;
    }
    Node* nxt = node->next;
    *node = *combined;
    node->next = nxt;
    return bool_type();
}

// 入力構文: 形に応じて prelude の読み取り関数への呼び出しに置き換える
static Type* check_input(Node* node) {
    int nargs = 0;
    for (Node* a = node->args; a; a = a->next) {
        nargs++;
    }
    const char* fname;
    if (node->val == 0) {
        fname = nargs == 0 ? "read_int" : nargs == 1 ? "read_ints" : "read_ints2";
    } else if (node->val == 1) {
        fname = nargs == 0 ? "read_token" : "read_tokens";
    } else {
        fname = "read_line";
    }
    node->kind = ND_FUNCALL;
    node->name = (char*)fname;
    node->len = (int)strlen(fname);
    return check_funcall(node, NULL);
}

// put 文: 各値を __str__ で文字列化し、空白区切りで出して改行する
static Type* check_put(Node* node) {
    Node* head = NULL;
    Node** tail = &head;
    int i = 0;
    for (Node* arg = node->args; arg; arg = arg->next, i++) {
        if (i > 0) {
            Node* sep = create_node(ND_FUNCALL);
            sep->name = "putchar";
            sep->len = 7;
            Node* sp = create_node(ND_NUM);
            sp->val = 32;
            sep->args = sp;
            *tail = sep;
            tail = &sep->next;
        }
        Type* at = check_node(arg);
        Class* klass = NULL;
        Function* method = NULL;
        if (at->kind == TY_UNKNOWN) {
            error_node(arg, "型が分からない値は put できません");
        } else if (!(klass = class_of(at)) || !(method = find_callable(klass, "__str__", 7))) {
            error_node(arg, "`%s`型の値は put できません", type_text(at));
        } else if (method->nparams != 1 || (method->return_type && method->return_type->kind != TY_UNKNOWN && !(method->return_type->kind == TY_CLASS && method->return_type->klass == find_class("str", 3)))) {
            error_node(arg, "クラス`%.*s`の`__str__`は引数を取らず、`str`を返す必要があります", klass->len, klass->name);
        }
        Node* conv = create_node(ND_MCALL);
        conv->name = "__str__";
        conv->len = 7;
        conv->left_child = arg;
        conv->klass = klass;
        conv->func = method;
        Node* out = create_node(ND_FUNCALL);
        out->name = "print_str";
        out->len = 9;
        out->args = conv;
        *tail = out;
        tail = &out->next;
    }
    Node* nl = create_node(ND_FUNCALL);
    nl->name = "putchar";
    nl->len = 7;
    Node* lf = create_node(ND_NUM);
    lf->val = 10;
    nl->args = lf;
    *tail = nl;
    node->kind = ND_BLOCK;
    node->body = head;
    return &ty_unknown;
}

static Type* check_node(Node* node) {
    if (!node) {
        return &ty_unknown;
    }
    // 期待型は直属のラムダ・配列リテラル・構築（関数呼び出し）だけが
    // 受け取り、それ以外へは漏らさない
    Type* my_expected = NULL;
    if (node->kind == ND_LAMBDA || node->kind == ND_ARRAY || node->kind == ND_FUNCALL) {
        my_expected = expected_type;
    }
    expected_type = NULL;
    Type* result = &ty_unknown;
    switch (node->kind) {
        case ND_NUM:
            result = &ty_int;
            break;
        case ND_BOOL:
            result = bool_type();
            break;
        case ND_LVAR:
            result = local_types[node->offset / 8];
            if (!result) {
                // 型が付いていない読み取りは、定義済みかを確認する
                // （step 79）。内部スロット（名前なし）と引数
                // （offset が引数の範囲）は対象外
                int slot = node->offset / 8;
                bool is_param = current_fn && slot >= 1 && slot <= current_fn->nparams;
                if (node->name && current_fn && !is_param
                        && !def_find_offset(current_local_defs(), node->offset)) {
                    if (def_find(global_defs, node->name, node->len)) {
                        // パース時点では後方のトップレベル代入が見えず
                        // ローカルとして解釈された、定義済みグローバル
                        // の前方参照。グローバル変数として読み直す
                        node->kind = ND_GVAR;
                        result = find_global_type(node);
                        break;
                    }
                    error_undef(node);
                }
                result = &ty_unknown;
            }
            break;
        case ND_CAPVAR: {
            result = &ty_unknown;
            for (Capture* c = current_captures; c; c = c->next) {
                if (c->env_offset == node->offset) {
                    result = c->type ? c->type : &ty_unknown;
                    break;
                }
            }
            break;
        }
        case ND_FUNCREF:
            result = check_funcref(node);
            break;
        case ND_LAMBDA:
            result = check_lambda(node, my_expected);
            break;
        case ND_VCALL:
            result = check_vcall(node);
            break;
        case ND_BINOP:
            result = check_binop(node);
            break;
        case ND_ARROW_L:
        case ND_ARROW_R:
            result = check_arrow(node);
            break;
        case ND_ASSIGN:
            result = check_assign(node);
            break;
        case ND_GVAR:
            // どこでも代入されていない名前の読み取りはエラー（step 79）。
            // 合成ノード（名前なし）は対象外
            if (node->name && !def_find(global_defs, node->name, node->len)) {
                error_undef(node);
            }
            result = find_global_type(node);
            break;
        case ND_UNARY: {
            Type* operand = check_node(node->left_child);
            if (node->op.unop == OP_NOT) {
                // `!` の被演算子は真理値として解釈する（step 79）
                node->left_child = boolify(node->left_child, operand);
                result = bool_type();
            }
            break;
        }
        case ND_FUNCALL:
            result = check_funcall(node, my_expected);
            break;
        case ND_ARRAY:
            result = check_arraylit(node, my_expected);
            break;
        case ND_NEWARRAY: {
            Type* fill = check_node(node->left_child);
            Type* len = check_node(node->right_child);
            if (len->kind != TY_UNKNOWN && len->kind != TY_INT) {
                error_node(node->right_child, "長さには`int`が必要ですが、`%s`が指定されています", type_text(len));
            }
            result = array_type(fill);
            break;
        }
        case ND_INDEX:
            result = check_index(node);
            break;
        case ND_SAFEIDX: {
            // 安全添え字の単独形は「全段とも範囲内か」の bool
            // （step 81）。比較の被演算子の形は check_binop が
            // 先回りして check_safe_cmp で処理する
            Node* guard;
            Node* value;
            build_safe_parts(node, &guard, &value);
            Node* replacement = guard ? guard : value;
            Type* rt = guard ? bool_type() : replacement->type;
            Node* nxt = node->next;
            *node = *replacement;
            node->next = nxt;
            result = rt;
            break;
        }
        case ND_LEN:
            check_node(node->left_child);
            result = &ty_int;
            break;
        case ND_BLOCK:
            for (Node* cur = node->body; cur; cur = cur->next) {
                check_node(cur);
            }
            break;
        case ND_IF:
            check_node(node->cond);
            // 条件は真理値として解釈する（step 79）
            node->cond = boolify(node->cond, node->cond->type);
            check_node(node->then);
            check_node(node->els);
            break;
        case ND_WHILE:
            check_node(node->cond);
            node->cond = boolify(node->cond, node->cond->type);
            check_node(node->then);
            break;
        case ND_FOR:
            result = check_for(node);
            break;
        case ND_MULTIASSIGN:
            result = check_multiassign(node);
            break;
        case ND_CHAIN:
            result = check_chain(node);
            break;
        case ND_INPUT:
            result = check_input(node);
            break;
        case ND_PUT:
            result = check_put(node);
            break;
        case ND_RETURN:
            result = check_return(node);
            break;
        case ND_MEMBER:
            result = check_member(node);
            break;
        case ND_APPEND: {
            // 通常は check_mcall の書き換えで生まれ、ここへは来ない。
            // 再検査の経路でも、対象と引数の検証はやり直す
            Type* recv_ty = check_node(node->left_child);
            if (recv_ty->kind == TY_ARRAY) {
                result = check_append(node, recv_ty);
            } else {
                check_node(node->args);
                result = &ty_int;
            }
            break;
        }
        case ND_MCALL:
            result = check_mcall(node);
            break;
        default:
            break;
    }
    node->type = result;
    return result;
}

static void check_init_complete(Class* klass, bool* assigned) {
    for (Field* f = klass->fields; f; f = f->next) {
        if (!assigned[f->offset / 8]) {
            error_plain("クラス`%.*s`の`__init__`で、フィールド`%.*s`が初期化されていません", klass->len, klass->name, f->len, f->name);
        }
    }
}

static bool* copy_assigned(bool* assigned, int nfields) {
    bool* copy = malloc(nfields * sizeof(bool));
    memcpy(copy, assigned, nfields * sizeof(bool));
    return copy;
}

static bool check_init_stmts(Node* stmt, Class* klass, bool* assigned, int nfields);

static bool check_init_if(Node* n, Class* klass, bool* assigned, int nfields) {
    bool* then_set = copy_assigned(assigned, nfields);
    bool* else_set = copy_assigned(assigned, nfields);
    bool then_ends = check_init_stmts(n->then, klass, then_set, nfields);
    bool else_ends = n->els ? check_init_stmts(n->els, klass, else_set, nfields) : false;
    if (!then_ends && !else_ends) {
        for (int i = 0; i < nfields; i++) {
            assigned[i] = then_set[i] && else_set[i];
        }
    } else if (then_ends && !else_ends) {
        memcpy(assigned, else_set, nfields * sizeof(bool));
    } else if (!then_ends && else_ends) {
        memcpy(assigned, then_set, nfields * sizeof(bool));
    }
    free(then_set);
    free(else_set);
    return then_ends && else_ends;
}

static bool check_init_stmt(Node* n, Class* klass, bool* assigned, int nfields) {
    switch (n->kind) {
        case ND_ASSIGN: {
            Node* lhs = n->left_child;
            if (lhs->kind == ND_MEMBER && lhs->left_child->kind == ND_LVAR && lhs->left_child->offset == 8) {
                assigned[lhs->offset / 8] = true;
            }
            return false;
        }
        case ND_BLOCK:
            return check_init_stmts(n->body, klass, assigned, nfields);
        case ND_RETURN:
            check_init_complete(klass, assigned);
            return true;
        case ND_BREAK:
        case ND_CONTINUE:
            return true;
        case ND_IF:
            return check_init_if(n, klass, assigned, nfields);
        case ND_WHILE: {
            bool* body_set = copy_assigned(assigned, nfields);
            check_init_stmts(n->body, klass, body_set, nfields);
            free(body_set);
            return false;
        }
        default:
            return false;
    }
}

static void check_init(Function* fn) {
    Class* klass = fn->klass;
    int nfields = 0;
    for (Field* f = klass->fields; f; f = f->next) {
        nfields++;
    }
    if (nfields == 0) {
        return;
    }
    bool* assigned = calloc(nfields, sizeof(bool));
    if (!check_init_stmts(fn->body, klass, assigned, nfields)) {
        check_init_complete(klass, assigned);
    }
    free(assigned);
}

static bool check_init_stmts(Node* stmt, Class* klass, bool* assigned, int nfields) {
    for (Node* n = stmt; n; n = n->next) {
        if (check_init_stmt(n, klass, assigned, nfields)) {
            return true;
        }
    }
    return false;
}

// 関数本体を検査する（typecheck の各段で共通の手順）
static void check_function_body(Function* fn) {
    memset(local_types, 0, sizeof(local_types));
    ensure_defs(fn);
    current_fn = fn;
    current_return = fn->return_type;
    for (int i = 0; i < fn->nparams && i < 6; i++) {
        if (fn->param_types[i]) {
            local_types[i + 1] = fn->param_types[i];
        }
    }
    for (Node* stmt = fn->body; stmt; stmt = stmt->next) {
        check_node(stmt);
    }
    fn->tc_done = true;
}

// 検査済みの本体を辿り、実体化メソッドへの呼び出しに印（tc_wanted）を
// 付ける。型検査の書き換え（for の反復プロトコル、dunder 書き換え、
// print の変換など）は全て func を設定した MCALL を本体へ残すため、
// 検査後の本体を辿れば呼び出しは漏れなく拾える。ラムダの本体は、
// そのラムダが検査済みなら中まで辿る
static void collect_wanted(Node* n) {
    if (!n) {
        return;
    }
    if (n->func && n->func->klass && n->func->klass->origin) {
        n->func->tc_wanted = true;
    }
    if (n->kind == ND_LAMBDA && n->func) {
        collect_wanted(n->func->body);
    }
    collect_wanted(n->left_child);
    collect_wanted(n->right_child);
    collect_wanted(n->cond);
    collect_wanted(n->then);
    collect_wanted(n->els);
    collect_wanted(n->init);
    collect_wanted(n->inc);
    collect_wanted(n->body);
    collect_wanted(n->args);
    collect_wanted(n->next);
}

void typecheck(Function* prog) {
    functions = prog;
    // 定義収集の状態はコンパイル単位ごとに作り直す（同一プロセスで
    // 複数回コンパイルされても、前のソースの定義が残らないように）
    while (fn_defs_list) {
        FnDefs* fd = fn_defs_list;
        fn_defs_list = fd->next;
        while (fd->locals) {
            DefName* d = fd->locals;
            fd->locals = d->next;
            free(d);
        }
        free(fd);
    }
    while (global_defs) {
        DefName* d = global_defs;
        global_defs = d->next;
        free(d);
    }
    // 未定義変数の検出（step 79）に備え、先に全関数の代入を集める。
    // グローバルはプログラム全体が対象（前方参照を許すため）
    for (Function* fn = prog; fn; fn = fn->next) {
        ensure_defs(fn);
    }
    for (Function* fn = prog; fn; fn = fn->next) {
        if (fn->klass && fn->klass->tparam_count > 0) {
            continue;
        }
        if (fn->is_lambda) {
            // ラムダ本体は、使われている場所で期待型と一緒に検査する
            continue;
        }
        if (fn->klass && fn->klass->origin) {
            // ジェネリクスの実体化メソッドは後段で、呼び出されたもの
            // だけを検査する（step 83）。実体化したクラスの全メソッドを
            // 一律に検査すると、使わないメソッドが要求する dunder
            // （例: 配列の sort が要求する要素型の `__lt__`）の不足が、
            // 無関係なメソッドの呼び出しまで失敗させてしまうため
            continue;
        }
        check_function_body(fn);
        collect_wanted(fn->body);
    }
    // 実体化メソッドの遅延検査。検査が進むと新たな呼び出しが見つかる
    // ため、進展が無くなるまで繰り返す
    bool progress = true;
    while (progress) {
        progress = false;
        for (Function* fn = prog; fn; fn = fn->next) {
            if (!fn->klass || !fn->klass->origin || fn->is_lambda) {
                continue;
            }
            if (!fn->tc_wanted || fn->tc_done) {
                continue;
            }
            check_function_body(fn);
            collect_wanted(fn->body);
            progress = true;
        }
    }

    for (Function* fn = prog; fn; fn = fn->next) {
        if (fn->klass && fn->klass->tparam_count > 0) {
            continue;
        }
        if (fn->klass && fn->len == 8 && !memcmp(fn->name, "__init__", 8)) {
            check_init(fn);
        }
    }
}
