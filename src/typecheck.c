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
        error_node(node, "クラス`%.*s`のメソッド`%.*s`の引数は1個ですが、%d個が定義されています", klass->len, klass->name, name, method->nparams - 1);
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

static Type* check_binop(Node* node) {
    Type* lt = check_node(node->left_child);
    Type* rt = check_node(node->right_child);
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

static Type* check_assign(Node* node) {
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
    }
    return rhs;
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
    node->kind = ND_APPEND;
    return &ty_int;
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

static Type* check_index(Node* node) {
    Type* lt = check_node(node->left_child);
    Type* it = check_node(node->right_child);
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
        case ND_LVAR:
            result = local_types[node->offset / 8];
            if (!result) {
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
        case ND_ASSIGN:
            result = check_assign(node);
            break;
        case ND_GVAR:
            result = find_global_type(node);
            break;
        case ND_UNARY:
            check_node(node->left_child);
            if (node->op.unop == OP_NOT) {
                result = bool_type();
            }
            break;
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
            check_node(node->then);
            check_node(node->els);
            break;
        case ND_WHILE:
            check_node(node->cond);
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
        case ND_APPEND:
            // 通常は check_mcall の書き換えで生まれ、ここへは来ない。
            // 再検査の経路でも壊れないよう、子だけ検査して int とする
            check_node(node->left_child);
            check_node(node->args);
            result = &ty_int;
            break;
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

void typecheck(Function* prog) {
    functions = prog;
    for (Function* fn = prog; fn; fn = fn->next) {
        if (fn->klass && fn->klass->tparam_count > 0) {
            continue;
        }
        if (fn->is_lambda) {
            // ラムダ本体は、使われている場所で期待型と一緒に検査する
            continue;
        }
        memset(local_types, 0, sizeof(local_types));
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
