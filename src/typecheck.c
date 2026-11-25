#include "spell.h"

Type ty_int = { TY_INT, NULL };
Type ty_array = { TY_ARRAY, NULL };
Type ty_unknown = { TY_UNKNOWN, NULL };

static Type* local_types[1024];

typedef struct GVarType GVarType;
struct GVarType {
    char* name;
    int len;
    Type* type;
    GVarType* next;
};
static GVarType* global_types;
static Function* current_fn;
static Type* current_return;
static Function* functions;

static Class* class_of(Type* t) {
    switch (t->kind) {
        case TY_CLASS: return t->klass;
        case TY_INT: return find_class("int", 3);
        case TY_ARRAY: return find_class("array", 5);
        default: return NULL;
    }
}

static Function* find_method(Class* klass, char* name, int len) {
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
        Function* method = find_method(klass, (char*)dunder, 6);
        if (method && method->return_type) {
            return method->return_type;
        }
    }
    return &ty_int;
}

static Function* find_function(Node* node) {
    for (Function* fn = functions; fn; fn = fn->next) {
        if (!fn->klass && fn->len == node->len && !memcmp(fn->name, node->name, fn->len)) {
            return fn;
        }
    }
    return NULL;
}

static bool type_equals(Type* a, Type* b) {
    if (a->kind == TY_UNKNOWN || b->kind == TY_UNKNOWN) {
        return true;
    }
    if (a->kind != b->kind) {
        return false;
    }
    if (a->kind == TY_ARRAY) {
        return type_equals(a->elem, b->elem);
    }
    if (a->kind == TY_CLASS) {
        return a->klass == b->klass;
    }
    return true;
}

static char* type_text_write(Type* type, char* p) {
    switch (type->kind) {
        case TY_INT:
            p += sprintf(p, "int");
            break;
        case TY_ARRAY:
            p += sprintf(p, "[");
            p = type_text_write(type->elem, p);
            p += sprintf(p, "]");
            break;
        case TY_UNKNOWN:
            p += sprintf(p, "unknown");
            break;
        case TY_CLASS:
            p += sprintf(p, "%.*s", type->klass->len, type->klass->name);
            break;
    }
    return p;
}

char* type_text(Type* type) {
    static char buf[256];
    type_text_write(type, buf);
    return buf;
}

static Type* find_global_type(Node* node) {
    for (GVarType* g = global_types; g; g = g->next) {
        if (g->len == node->len && !memcmp(g->name, node->name, g->len)) {
            return g->type;
        }
    }
    return &ty_unknown;
}

static void record_global_type(Node* node, Type* type) {
    for (GVarType* g = global_types; g; g = g->next) {
        if (g->len == node->len && !memcmp(g->name, node->name, g->len)) {
            g->type = type;
            return;
        }
    }
    GVarType* g = calloc(1, sizeof(GVarType));
    g->name = node->name;
    g->len = node->len;
    g->type = type;
    g->next = global_types;
    global_types = g;
}

Type* array_type(Type* elem) {
    Type* t = calloc(1, sizeof(Type));
    t->kind = TY_ARRAY;
    t->elem = elem;
    return t;
}

static Type* adopt_type(Type* anno, Type* rhs) {
    if (rhs->kind == TY_UNKNOWN) {
        return anno;
    }
    if (anno->kind == TY_ARRAY && rhs->kind == TY_ARRAY) {
        Type* elem = adopt_type(anno->elem, rhs->elem);
        if (elem == rhs->elem) {
            return rhs;
        }
        return array_type(elem);
    }
    return rhs;
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

static bool scalar_compatible(Type* a, Type* b) {
    return a->kind == b->kind;
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
        char buf[256];
        strcpy(buf, type_text(method->param_types[1]));
        error_plain("クラス`%.*s`のメソッド`%.*s`の引数には`%s`が必要ですが、`%s`が指定されています", klass->len, klass->name, name, buf, type_text(rt));
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
    Function* method = find_method(klass, "__setitem__", 11);
    if (!method) {
        error_plain("クラス`%.*s`に`__setitem__`は定義されていません", klass->len, klass->name);
    }
    if (method->nparams != 3) {
        error_plain("クラス`%.*s`のメソッド`__setitem__`の引数は2個ですが、%d個が定義されています", klass->len, klass->name, method->nparams - 1);
    }
    if (method->param_types[1] && it->kind != TY_UNKNOWN && !type_equals(method->param_types[1], it)) {
        char buf[256];
        strcpy(buf, type_text(method->param_types[1]));
        error_plain("クラス`%.*s`のメソッド`__setitem__`の引数には`%s`が必要ですが、`%s`が指定されています", klass->len, klass->name, buf, type_text(it));
    }
    if (method->param_types[2] && rhs->kind != TY_UNKNOWN && !type_equals(method->param_types[2], rhs)) {
        char buf[256];
        strcpy(buf, type_text(method->param_types[2]));
        error_plain("クラス`%.*s`のメソッド`__setitem__`の引数には`%s`が必要ですが、`%s`が指定されています", klass->len, klass->name, buf, type_text(rhs));
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

static Type* rewrite_to_funcall(Node* node, const char* name, int len, Type* result, bool swap) {
    Node* left = node->left_child;
    Node* right = node->right_child;
    if (swap) {
        right->next = left;
        left->next = NULL;
        node->args = right;
    } else {
        left->next = right;
        right->next = NULL;
        node->args = left;
    }
    node->kind = ND_FUNCALL;
    node->name = (char*)name;
    node->len = len;
    return result;
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
        case ND_BINOP: {
            Type* lt = check_node(node->left_child);
            Type* rt = check_node(node->right_child);
            const char* dunder = arith_dunder(node->op.binop);
            if (dunder) {
                if (lt->kind == TY_CLASS) {
                    Function* method = find_method(lt->klass, (char*)dunder, 7);
                    if (!method) {
                        error_plain("クラス`%.*s`に演算子`%s`は定義されていません", lt->klass->len, lt->klass->name, op_text(node->op.binop));
                    }
                    result = rewrite_to_mcall(node, lt, rt, dunder, 7, method);
                    break;
                }
                if (lt->kind == TY_ARRAY || rt->kind == TY_ARRAY) {
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
                        Function* method = find_method(class_of(lt), "__add__", 7);
                        result = rewrite_to_mcall(node, lt, rt, "__add__", 7, method);
                        if (result->kind == TY_UNKNOWN) { result = lt; }
                        break;
                    }
                    if (node->op.binop == OP_MUL) {
                        if (ot->kind != TY_INT && ot->kind != TY_UNKNOWN) {
                            error_plain("`%s`型の値に演算子`%s`は使えません", type_text(lt), op_text(node->op.binop));
                        }
                        Function* method = find_method(class_of(lt), "__mul__", 7);
                        result = rewrite_to_mcall(node, lt, rt, "__mul__", 7, method);
                        if (result->kind == TY_UNKNOWN) { result = lt; }
                        break;
                    }
                    error_plain("`%s`型の値に演算子`%s`は使えません", type_text(lt), op_text(node->op.binop));
                }
                if (lt->kind != TY_UNKNOWN && rt->kind != TY_UNKNOWN && !scalar_compatible(lt, rt)) {
                    char buf[256];
                    strcpy(buf, type_text(lt));
                    error_plain("`%s`型と`%s`型の値に演算子`%s`は使えません", buf, type_text(rt), op_text(node->op.binop));
                }
                result = &ty_int;
                break;
            }
            const char* cdunder = cmp_dunder(node->op.binop);
            if (cdunder) {
                if (lt->kind == TY_CLASS) {
                    Function* method = find_method(lt->klass, (char*)cdunder, 6);
                    if (method) {
                        result = rewrite_to_mcall(node, lt, rt, cdunder, 6, method);
                        break;
                    }
                    if (node->op.binop == OP_LT || node->op.binop == OP_LE) {
                        error_plain("クラス`%.*s`に演算子`%s`は定義されていません", lt->klass->len, lt->klass->name, op_text(node->op.binop));
                    }
                    result = bool_type();
                    break;
                }
                if (lt->kind == TY_ARRAY || rt->kind == TY_ARRAY) {
                    if (node->op.binop == OP_LT || node->op.binop == OP_LE) {
                        Type* at = lt->kind == TY_ARRAY ? lt : rt;
                        error_plain("`%s`型の値に演算子`%s`は使えません", type_text(at), op_text(node->op.binop));
                    }
                    Type* ot = lt->kind == TY_ARRAY ? rt : lt;
                    if (ot->kind == TY_ARRAY || ot->kind == TY_UNKNOWN) {
                        if (node->op.binop == OP_EQ) {
                            Function* method = find_method(class_of(lt), "__eq__", 6);
                            result = rewrite_to_mcall(node, lt, rt, "__eq__", 6, method);
                        } else {
                            Function* method = find_method(class_of(lt), "__ne__", 6);
                            result = rewrite_to_mcall(node, lt, rt, "__ne__", 6, method);
                        }
                        if (result->kind == TY_UNKNOWN) { result = bool_type(); }
                        break;
                    }
                    result = bool_type();
                    break;
                }
                if ((node->op.binop == OP_LT || node->op.binop == OP_LE)
                        && lt->kind != TY_UNKNOWN && rt->kind != TY_UNKNOWN && !scalar_compatible(lt, rt)) {
                    char buf[256];
                    strcpy(buf, type_text(lt));
                    error_plain("`%s`型と`%s`型の値に演算子`%s`は使えません", buf, type_text(rt), op_text(node->op.binop));
                }
                result = scalar_cmp_result(cdunder);
                break;
            }
            result = bool_type();
            break;
        }
        case ND_ASSIGN: {
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
                result = rhs;
                break;
            }
            if (lhs->kind == ND_LVAR) {
                Type* recorded = local_types[lhs->offset / 8];
                if (node->anno && rhs->kind != TY_UNKNOWN && !type_equals(node->anno, rhs)) {
                    char expected[256];
                    strcpy(expected, type_text(node->anno));
                    error_plain("変数`%.*s`の型は`%s`ですが、`%s`が代入されています", lhs->len, lhs->name, expected, type_text(rhs));
                }
                if (recorded && rhs->kind != TY_UNKNOWN && !type_equals(recorded, rhs)) {
                    char expected[256];
                    strcpy(expected, type_text(recorded));
                    error_plain("変数`%.*s`の型は`%s`ですが、`%s`が代入されています", lhs->len, lhs->name, expected, type_text(rhs));
                }
                if (!recorded && (node->anno || rhs->kind != TY_UNKNOWN)) {
                    local_types[lhs->offset / 8] = node->anno ? node->anno : rhs;
                }
                lhs->type = local_types[lhs->offset / 8];
            } else if (lhs->kind == ND_GVAR) {
                Type* recorded = find_global_type(lhs);
                if (node->anno && rhs->kind != TY_UNKNOWN && !type_equals(node->anno, rhs)) {
                    char expected[256];
                    strcpy(expected, type_text(node->anno));
                    error_plain("変数`%.*s`の型は`%s`ですが、`%s`が代入されています", lhs->len, lhs->name, expected, type_text(rhs));
                }
                if (recorded->kind != TY_UNKNOWN && rhs->kind != TY_UNKNOWN && !type_equals(recorded, rhs)) {
                    char expected[256];
                    strcpy(expected, type_text(recorded));
                    error_plain("変数`%.*s`の型は`%s`ですが、`%s`が代入されています", lhs->len, lhs->name, expected, type_text(rhs));
                }
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
                        char expected[256];
                        strcpy(expected, type_text(elem));
                        error_plain("配列の要素は`%s`ですが、`%s`が代入されています", expected, type_text(rhs));
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
            result = rhs;
            break;
        }
        case ND_GVAR:
            result = find_global_type(node);
            break;
        case ND_UNARY:
            if (node->op.unop == OP_NOT) {
                result = bool_type();
            }
            break;
        case ND_FUNCALL: {
            Type* arg_types[6];
            int nargs = 0;
            for (Node* arg = node->args; arg; arg = arg->next) {
                Type* at = check_node(arg);
                if (nargs < 6) {
                    arg_types[nargs] = at;
                }
                nargs++;
            }
            Function* callee = find_function(node);
            if (!callee) {
                Class* klass = find_class(node->name, node->len);
                if (klass) {
                    node->klass = klass;
                    Function* init = find_method(klass, "__init__", 8);
                    node->func = init;
                    int expected = init ? init->nparams - 1 : 0;
                    if (nargs != expected) {
                        error_plain("クラス`%.*s`の`__init__`の引数は%d個ですが、%d個が指定されています", klass->len, klass->name, expected, nargs);
                    }
                    for (int i = 0; i < nargs; i++) {
                        Type* pt = init->param_types[i + 1];
                        if (pt && arg_types[i]->kind != TY_UNKNOWN && !type_equals(pt, arg_types[i])) {
                            char buf[256];
                            strcpy(buf, type_text(pt));
                            error_plain("クラス`%.*s`の`__init__`の引数には`%s`が必要ですが、`%s`が指定されています", klass->len, klass->name, buf, type_text(arg_types[i]));
                        }
                    }
                    result = klass->type;
                    break;
                }
                if (node->len == 7 && (!memcmp(node->name, "getchar", 7) || !memcmp(node->name, "putchar", 7))) {
                    result = &ty_int;
                }
                break;
            }
            if (nargs != callee->nparams) {
                error_plain("関数`%.*s`の引数は%d個ですが、%d個が指定されています", node->len, node->name, callee->nparams, nargs);
            }
            for (int i = 0; i < nargs && i < callee->nparams && i < 6; i++) {
                Type* pt = callee->param_types[i];
                if (pt && arg_types[i]->kind != TY_UNKNOWN && !type_equals(pt, arg_types[i])) {
                    char expected[256];
                    strcpy(expected, type_text(pt));
                    error_plain("関数`%.*s`の引数には`%s`が必要ですが、`%s`が指定されています", node->len, node->name, expected, type_text(arg_types[i]));
                }
            }
            result = callee->return_type ? callee->return_type : &ty_unknown;
            break;
         }
        case ND_ARRAY:
            ;
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
                    char expected[256];
                    strcpy(expected, type_text(elem));
                    error_plain("配列の要素は`%s`ですが、`%s`が含まれています", expected, type_text(et));
                }
            }
            result = array_type(elem);
            break;
        case ND_NEWARRAY:
            ;
            Type* fill = check_node(node->left_child);
            Type* len = check_node(node->right_child);
            if (len->kind != TY_UNKNOWN && len->kind != TY_INT) {
                error_plain("長さには`int`が必要ですが、`%s`が指定されています", type_text(len));
            }
            result = array_type(fill);
            break;
        case ND_INDEX:
            ;
            Type* lt = check_node(node->left_child);
            Type* it = check_node(node->right_child);
            if (lt->kind == TY_CLASS) {
                Function* method = find_method(lt->klass, "__getitem__", 11);
                if (!method) {
                    error_plain("`%s`型の値に添え字は使えません", type_text(lt));
                }
                result = rewrite_to_mcall(node, lt, it, "__getitem__", 11, method);
                break;
            }
            if (lt->kind != TY_UNKNOWN && lt->kind != TY_ARRAY) {
                error_plain("`%s`型の値に添え字は使えません", type_text(lt));
            }
            if (it->kind != TY_UNKNOWN && it->kind != TY_INT) {
                error_plain("添え字には`int`が必要ですが、`%s`が指定されています", type_text(it));
            }
            result = lt->kind == TY_ARRAY ? lt->elem : &ty_unknown;
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
            check_node(node->init);
            check_node(node->cond);
            check_node(node->then);
            check_node(node->inc);
            break;
        case ND_RETURN:
            ;
            Type* rt = check_node(node->left_child);
            if (rt->kind != TY_UNKNOWN) {
                if (!current_return) {
                    current_fn->return_type = rt;
                    current_return = rt;
                } else if (!type_equals(current_return, rt)) {
                    char expected[256];
                    strcpy(expected, type_text(current_return));
                    error_plain("関数`%.*s`の戻り値は`%s`ですが、`%s`が返されています", current_fn->len, current_fn->name, expected, type_text(rt));
                }
            }
            result = rt;
            break;
        case ND_MEMBER: {
            Type* obj = check_node(node->left_child);
            if (obj->kind == TY_UNKNOWN) {
                if (node->len == 3 && !memcmp(node->name, "len", 3)) {
                    node->kind = ND_LEN;
                    result = &ty_int;
                } else {
                    result = &ty_unknown;
                }
                break;
            }
            if (obj->kind == TY_ARRAY && node->len == 3 && !memcmp(node->name, "len", 3)) {
                node->kind = ND_LEN;
                result = &ty_int;
                break;
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
                result = field->type;
                break;
            }
            Function* method = find_method(klass, node->name, node->len);
            if (method) {
                if (method->nparams > 1) {
                    error_plain("メソッド`%.*s`には引数が必要ですが、括弧なしで使われています", node->len, node->name);
                }
                node->func = method;
                result = method->return_type ? method->return_type : &ty_unknown;
                break;
            }
            error_plain("クラス`%.*s`にフィールド`%.*s`は定義されていません", klass->len, klass->name, node->len, node->name);
            break;
        }
        case ND_MCALL:
            ;
            Type* obj = check_node(node->left_child);
            Type* arg_types[6];
            int nargs = 0;
            for (Node* arg = node->args; arg; arg = arg->next) {
                Type* at = check_node(arg);
                if (nargs < 6) {
                    arg_types[nargs] = at;
                }
                nargs++;
            }
            if (node->len == 3 && !memcmp(node->name, "len", 3) && (obj->kind == TY_ARRAY || obj->kind == TY_UNKNOWN)) {
                if (nargs != 0) {
                    error_plain("`len`の引数は0個ですが、%d個が指定されています", nargs);
                }
                node->kind = ND_LEN;
                result = &ty_int;
                break;
            }
            if (obj->kind == TY_UNKNOWN) {
                result = &ty_unknown;
                break;
            }
            Class* klass = class_of(obj);
            if (!klass) {
                error_plain("`%s`型の値にメソッドは使えません", type_text(obj));
            }
            Function* method = find_method(klass, node->name, node->len);
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
                    char buf[256];
                    strcpy(buf, type_text(pt));
                    error_plain("クラス`%.*s`のメソッド`%.*s`の引数には`%s`が必要ですが、`%s`が指定されています", klass->len, klass->name, node->len, node->name, buf, type_text(arg_types[i]));
                }
            }
            result = method->return_type ? method->return_type : &ty_unknown;
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
        case ND_IF: {
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
        case ND_WHILE: {
            bool* body_set = copy_assigned(assigned, nfields);
            check_init_stmts(n->body, klass, body_set, nfields);
            free(body_set);
            return false;
        }
        case ND_FOR: {
            if (n->init) {
                check_init_stmt(n->init, klass, assigned, nfields);
            }
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
        if (fn->klass && fn->len == 8 && !memcmp(fn->name, "__init__", 8)) {
            check_init(fn);
        }
    }
}
