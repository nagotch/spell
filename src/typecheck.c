#include "spell.h"
#include "typecheck_internal.h"

// 型検査: AST を歩いて各ノードに型を付け、演算子を dunder メソッド呼び出しへ
// 書き換える。main からコード生成の直前に呼ばれる。
// 型の道具は types.c、ジェネリクスの実体化は generics.c、ここは検査本体。

static Type* local_types[1024];
static Function* current_fn;
static Type* current_return;
Function* functions;

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
        error_plain("クラス`%.*s`のメソッド`%.*s`の引数は1個ですが、%d個が定義されています", klass->len, klass->name, name, method->nparams - 1);
    }
    if (method->param_types[1] && rt->kind != TY_UNKNOWN && !type_equals(method->param_types[1], rt)) {
        error_plain("クラス`%.*s`のメソッド`%.*s`の引数には`%s`が必要ですが、`%s`が指定されています", klass->len, klass->name, len, name, type_text(method->param_types[1]), type_text(rt));
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
        error_plain("クラス`%.*s`に`__setitem__`は定義されていません", klass->len, klass->name);
    }
    if (method->nparams != 3) {
        error_plain("クラス`%.*s`のメソッド`__setitem__`の引数は2個ですが、%d個が定義されています", klass->len, klass->name, method->nparams - 1);
    }
    if (method->param_types[1] && it->kind != TY_UNKNOWN && !type_equals(method->param_types[1], it)) {
        error_plain("クラス`%.*s`のメソッド`__setitem__`の引数には`%s`が必要ですが、`%s`が指定されています", klass->len, klass->name, type_text(method->param_types[1]), type_text(it));
    }
    if (method->param_types[2] && rhs->kind != TY_UNKNOWN && !type_equals(method->param_types[2], rhs)) {
        error_plain("クラス`%.*s`のメソッド`__setitem__`の引数には`%s`が必要ですが、`%s`が指定されています", klass->len, klass->name, type_text(method->param_types[2]), type_text(rhs));
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

// 引数を全部検査し、型を最大 6 個まで記録する。引数の数を返す
static int check_args(Node* args, Type* arg_types[6]) {
    int nargs = 0;
    for (Node* arg = args; arg; arg = arg->next) {
        Type* at = check_node(arg);
        if (nargs < 6) {
            arg_types[nargs] = at;
        }
        nargs++;
    }
    return nargs;
}

// 変数の型（注釈または記録済み）と代入する値の型が食い違えばエラー
static void check_assign_type(Node* lhs, Type* expected, Type* rhs) {
    if (expected && expected->kind != TY_UNKNOWN && rhs->kind != TY_UNKNOWN && !type_equals(expected, rhs)) {
        error_plain("変数`%.*s`の型は`%s`ですが、`%s`が代入されています", lhs->len, lhs->name, type_text(expected), type_text(rhs));
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
            error_plain("`%s`型の値に演算子`%s`は使えません", type_text(lt), op_text(node->op.binop));
        }
        Function* method = find_callable(class_of(lt), "__add__", 7);
        Type* result = rewrite_to_mcall(node, lt, rt, "__add__", 7, method);
        if (result->kind == TY_UNKNOWN) { result = lt; }
        return result;
    }
    if (node->op.binop == OP_MUL) {
        if (ot->kind != TY_INT && ot->kind != TY_UNKNOWN) {
            error_plain("`%s`型の値に演算子`%s`は使えません", type_text(lt), op_text(node->op.binop));
        }
        Function* method = find_callable(class_of(lt), "__mul__", 7);
        Type* result = rewrite_to_mcall(node, lt, rt, "__mul__", 7, method);
        if (result->kind == TY_UNKNOWN) { result = lt; }
        return result;
    }
    error_plain("`%s`型の値に演算子`%s`は使えません", type_text(lt), op_text(node->op.binop));
    return &ty_unknown;
}

static Type* check_array_cmp(Node* node, Type* lt, Type* rt) {
    if (node->op.binop == OP_LT || node->op.binop == OP_LE) {
        Type* at = lt->kind == TY_ARRAY ? lt : rt;
        error_plain("`%s`型の値に演算子`%s`は使えません", type_text(at), op_text(node->op.binop));
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
                error_plain("クラス`%.*s`に演算子`%s`は定義されていません", lt->klass->len, lt->klass->name, op_text(node->op.binop));
            }
            return rewrite_to_mcall(node, lt, rt, dunder, 7, method);
        }
        if (lt->kind == TY_ARRAY || rt->kind == TY_ARRAY) {
            return check_array_arith(node, lt, rt);
        }
        if (lt->kind != TY_UNKNOWN && rt->kind != TY_UNKNOWN && lt->kind != rt->kind) {
            error_plain("`%s`型と`%s`型の値に演算子`%s`は使えません", type_text(lt), type_text(rt), op_text(node->op.binop));
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
                error_plain("クラス`%.*s`に演算子`%s`は定義されていません", lt->klass->len, lt->klass->name, op_text(node->op.binop));
            }
            return bool_type();
        }
        if (lt->kind == TY_ARRAY || rt->kind == TY_ARRAY) {
            return check_array_cmp(node, lt, rt);
        }
        if ((node->op.binop == OP_LT || node->op.binop == OP_LE)
                && lt->kind != TY_UNKNOWN && rt->kind != TY_UNKNOWN && lt->kind != rt->kind) {
            error_plain("`%s`型と`%s`型の値に演算子`%s`は使えません", type_text(lt), type_text(rt), op_text(node->op.binop));
        }
        return scalar_cmp_result(cdunder);
    }
    return bool_type();
}

static Type* check_assign(Node* node) {
    Type* rhs = check_node(node->right_child);
    if (node->anno) {
        rhs = adopt_type(node->anno, rhs);
    }
    Node* lhs = node->left_child;
    if (lhs->kind == ND_MCALL && node->right_child->kind == ND_BINOP && node->right_child->left_child == lhs) {
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
            rewrite_to_setitem(node, bt->klass, lhs->left_child, lhs->right_child, it, rhs, node->right_child);
        } else {
            Type* elem = check_node(lhs);
            if (elem->kind != TY_UNKNOWN && rhs->kind != TY_UNKNOWN && !type_equals(elem, rhs)) {
                error_plain("配列の要素は`%s`ですが、`%s`が代入されています", type_text(elem), type_text(rhs));
            }
        }
    } else {
        check_node(lhs);
        if (lhs->kind == ND_LEN) {
            error_plain("`len`には代入できません");
        }
        if (lhs->kind == ND_MEMBER && lhs->func) {
            error_plain("メソッド`%.*s`には代入できません", lhs->len, lhs->name);
        }
        if (lhs->kind == ND_MCALL || lhs->kind == ND_FUNCALL) {
            error_plain("呼び出しの結果には代入できません");
        }
    }
    return rhs;
}

static Type* check_construct(Node* node, Class* klass, Type* arg_types[6], int nargs) {
    if (klass->type->kind != TY_CLASS) {
        error_plain("`%.*s`はクラスではありません", klass->len, klass->name);
    }
    if (klass->tparam) {
        Function* ginit = find_callable(klass, "__init__", 8);
        Type* gtarg = NULL;
        if (ginit) {
            for (int i = 0; i < nargs && i + 1 < ginit->nparams; i++) {
                gtarg = unify_type(ginit->param_types[i + 1], arg_types[i], gtarg);
            }
        }
        if (!gtarg) {
            error_plain("クラス`%.*s`の型引数を推論できません", klass->len, klass->name);
        }
        klass = instantiate(klass, gtarg);
    }
    node->klass = klass;
    Function* init = find_callable(klass, "__init__", 8);
    node->func = init;
    int expected = init ? init->nparams - 1 : 0;
    if (nargs != expected) {
        error_plain("クラス`%.*s`の`__init__`の引数は%d個ですが、%d個が指定されています", klass->len, klass->name, expected, nargs);
    }
    for (int i = 0; i < nargs; i++) {
        Type* pt = init->param_types[i + 1];
        if (pt && arg_types[i]->kind != TY_UNKNOWN && !type_equals(pt, arg_types[i])) {
            error_plain("クラス`%.*s`の`__init__`の引数には`%s`が必要ですが、`%s`が指定されています", klass->len, klass->name, type_text(pt), type_text(arg_types[i]));
        }
    }
    return klass->type;
}

static Type* check_funcall(Node* node) {
    Type* arg_types[6];
    int nargs = check_args(node->args, arg_types);
    Function* callee = find_callable(NULL, node->name, node->len);
    if (node->len == 5 && !memcmp(node->name, "print", 5) && !callee && !find_class(node->name, node->len)) {
        if (nargs != 1) {
            error_plain("`print`の引数は1個ですが、%d個が指定されています", nargs);
        }
        Type* at = arg_types[0];
        if (at->kind == TY_UNKNOWN) {
            error_plain("型が分からない値は print できません");
        }
        Class* klass = class_of(at);
        Function* method = klass ? find_callable(klass, "__str__", 7) : NULL;
        if (!method) {
            error_plain("`%s`型の値は print できません", type_text(at));
        }
        if (method->nparams != 1 || (method->return_type && method->return_type->kind != TY_UNKNOWN && !(method->return_type->kind == TY_CLASS && method->return_type->klass == find_class("str", 3)))) {
            error_plain("クラス`%.*s`の`__str__`は引数を取らず、`str`を返す必要があります", klass->len, klass->name);
        }
        Node* conv = calloc(1, sizeof(Node));
        conv->kind = ND_MCALL;
        conv->name = "__str__";
        conv->len = 7;
        conv->left_child = node->args;
        conv->klass = klass;
        conv->func = method;
        node->args = conv;
        node->name = "__println";
        node->len = 9;
        return &ty_unknown;
    }
    if (!callee) {
        Class* klass = find_class(node->name, node->len);
        if (klass) {
            return check_construct(node, klass, arg_types, nargs);
        }
        if (node->len == 7 && (!memcmp(node->name, "getchar", 7) || !memcmp(node->name, "putchar", 7))) {
            return &ty_int;
        }
        return &ty_unknown;
    }
    if (nargs != callee->nparams) {
        error_plain("関数`%.*s`の引数は%d個ですが、%d個が指定されています", node->len, node->name, callee->nparams, nargs);
    }
    for (int i = 0; i < nargs && i < callee->nparams && i < 6; i++) {
        Type* pt = callee->param_types[i];
        if (pt && arg_types[i]->kind != TY_UNKNOWN && !type_equals(pt, arg_types[i])) {
            error_plain("関数`%.*s`の引数には`%s`が必要ですが、`%s`が指定されています", node->len, node->name, type_text(pt), type_text(arg_types[i]));
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
        error_plain("`%s`型の値にフィールドは使えません", type_text(obj));
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
            error_plain("メソッド`%.*s`には引数が必要ですが、括弧なしで使われています", node->len, node->name);
        }
        node->func = method;
        return method->return_type ? method->return_type : &ty_unknown;
    }
    error_plain("クラス`%.*s`にフィールド`%.*s`は定義されていません", klass->len, klass->name, node->len, node->name);
    return &ty_unknown;
}

static Type* check_mcall(Node* node) {
    Type* obj = check_node(node->left_child);
    Type* arg_types[6];
    int nargs = check_args(node->args, arg_types);
    if (node->len == 3 && !memcmp(node->name, "len", 3) && (obj->kind == TY_ARRAY || obj->kind == TY_UNKNOWN)) {
        if (nargs != 0) {
            error_plain("`len`の引数は0個ですが、%d個が指定されています", nargs);
        }
        node->kind = ND_LEN;
        return &ty_int;
    }
    if (obj->kind == TY_UNKNOWN) {
        return &ty_unknown;
    }
    Class* klass = class_of(obj);
    if (!klass) {
        error_plain("`%s`型の値にメソッドは使えません", type_text(obj));
    }
    Function* method = find_callable(klass, node->name, node->len);
    if (!method) {
        error_plain("クラス`%.*s`にメソッド`%.*s`は定義されていません", klass->len, klass->name, node->len, node->name);
    }
    node->func = method;
    node->klass = klass;
    if (nargs != method->nparams - 1) {
        error_plain("クラス`%.*s`のメソッド`%.*s`の引数は%d個ですが、%d個が指定されています", klass->len, klass->name, node->len, node->name, method->nparams - 1, nargs);
    }
    for (int i = 0; i < nargs && i + 1 < method->nparams; i++) {
        Type* pt = method->param_types[i + 1];
        if (pt && arg_types[i]->kind != TY_UNKNOWN && !type_equals(pt, arg_types[i])) {
            error_plain("クラス`%.*s`のメソッド`%.*s`の引数には`%s`が必要ですが、`%s`が指定されています", klass->len, klass->name, node->len, node->name, type_text(pt), type_text(arg_types[i]));
        }
    }
    return method->return_type ? method->return_type : &ty_unknown;
}

static Type* check_arraylit(Node* node) {
    Type* elem = &ty_unknown;
    for (Node* e = node->body; e; e = e->next) {
        Type* et = check_node(e);
        if (et->kind == TY_UNKNOWN) {
            continue;
        }
        if (elem->kind == TY_UNKNOWN) {
            elem = et;
            continue;
        }
        if (!type_equals(elem, et)) {
            error_plain("配列の要素は`%s`ですが、`%s`が含まれています", type_text(elem), type_text(et));
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
            error_plain("`%s`型の値に添え字は使えません", type_text(lt));
        }
        return rewrite_to_mcall(node, lt, it, "__getitem__", 11, method);
    }
    if (lt->kind != TY_UNKNOWN && lt->kind != TY_ARRAY) {
        error_plain("`%s`型の値に添え字は使えません", type_text(lt));
    }
    if (it->kind != TY_UNKNOWN && it->kind != TY_INT) {
        error_plain("添え字には`int`が必要ですが、`%s`が指定されています", type_text(it));
    }
    return lt->kind == TY_ARRAY ? lt->elem : &ty_unknown;
}

static Type* check_return(Node* node) {
    Type* rt = check_node(node->left_child);
    if (rt->kind != TY_UNKNOWN) {
        if (!current_return) {
            current_fn->return_type = rt;
            current_return = rt;
        } else if (!type_equals(current_return, rt)) {
            error_plain("関数`%.*s`の戻り値は`%s`ですが、`%s`が返されています", current_fn->len, current_fn->name, type_text(current_return), type_text(rt));
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
    if (iterable->kind == TY_UNKNOWN) {
        error_plain("型が分からない値は反復できません");
    }
    Class* klass = class_of(iterable);
    Function* iter_fn = find_callable(klass, "__iter__", 8);
    if (!iter_fn) {
        error_plain("`%s`型の値は反復できません", type_text(iterable));
    }
    if (iter_fn->nparams != 1) {
        error_plain("クラス`%.*s`の`__iter__`は引数を取れません", klass->len, klass->name);
    }
    Type* iter_type = iter_fn->return_type;
    if (!iter_type || iter_type->kind != TY_CLASS) {
        error_plain("クラス`%.*s`の`__iter__`はイテレータを返す必要があります", klass->len, klass->name);
    }
    Class* iklass = class_of(iter_type);
    Function* has_next_fn = find_callable(iklass, "__has_next__", 12);
    Function* next_fn = find_callable(iklass, "__next__", 8);
    if (!has_next_fn || !next_fn) {
        error_plain("クラス`%.*s`には`__has_next__`と`__next__`が必要です", iklass->len, iklass->name);
    }
    if (has_next_fn->nparams != 1) {
        error_plain("クラス`%.*s`の`__has_next__`は引数を取れません", iklass->len, iklass->name);
    }
    if (next_fn->nparams != 1) {
        error_plain("クラス`%.*s`の`__next__`は引数を取れません", iklass->len, iklass->name);
    }
    if (has_next_fn->return_type && !type_equals(has_next_fn->return_type, bool_type())) {
        error_plain("クラス`%.*s`の`__has_next__`は`bool`を返す必要があります", iklass->len, iklass->name);
    }
    Type* elem = next_fn->return_type ? next_fn->return_type : &ty_unknown;

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
    if (node->els) {
        rewrite_breaks_stmt(node->then, (int)node->val);
    }
    check_node(node->then);
    if (node->els) {
        check_node(node->els);
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
    return check_funcall(node);
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
        if (at->kind == TY_UNKNOWN) {
            error_plain("型が分からない値は put できません");
        }
        Class* klass = class_of(at);
        Function* method = klass ? find_callable(klass, "__str__", 7) : NULL;
        if (!method) {
            error_plain("`%s`型の値は put できません", type_text(at));
        }
        if (method->nparams != 1 || (method->return_type && method->return_type->kind != TY_UNKNOWN && !(method->return_type->kind == TY_CLASS && method->return_type->klass == find_class("str", 3)))) {
            error_plain("クラス`%.*s`の`__str__`は引数を取らず、`str`を返す必要があります", klass->len, klass->name);
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
            result = check_funcall(node);
            break;
        case ND_ARRAY:
            result = check_arraylit(node);
            break;
        case ND_NEWARRAY: {
            Type* fill = check_node(node->left_child);
            Type* len = check_node(node->right_child);
            if (len->kind != TY_UNKNOWN && len->kind != TY_INT) {
                error_plain("長さには`int`が必要ですが、`%s`が指定されています", type_text(len));
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
        if (fn->klass && fn->klass->tparam) {
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
        if (fn->klass && fn->klass->tparam) {
            continue;
        }
        if (fn->klass && fn->len == 8 && !memcmp(fn->name, "__init__", 8)) {
            check_init(fn);
        }
    }
}
