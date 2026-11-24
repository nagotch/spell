#include "spell.h"

Type ty_int = { TY_INT, NULL };
Type ty_bool = { TY_BOOL, NULL };
Type ty_char = { TY_CHAR, NULL };
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

static Function* find_method(Class* klass, char* name, int len) {
    for (Function* fn = functions; fn; fn = fn->next) {
        if (fn->klass == klass && fn->len == len && !memcmp(fn->name, name, len)) {
            return fn;
        }
    }
    return NULL;
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
        case TY_BOOL:
            p += sprintf(p, "bool");
            break;
        case TY_CHAR:
            p += sprintf(p, "char");
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
            check_node(node->left_child);
            check_node(node->right_child);
            if (node->op.binop == OP_EQ || node->op.binop == OP_NE
             || node->op.binop == OP_LT || node->op.binop == OP_LE
             || node->op.binop == OP_OR || node->op.binop == OP_AND) {
                result = &ty_bool;
            } else {
                result = &ty_int;
            }
            break;
        case ND_ASSIGN:
            ;
            Type* rhs = check_node(node->right_child);
            if (node->anno) {
                rhs = adopt_type(node->anno, rhs);
            }
            Node* lhs = node->left_child;
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
                Type* elem = check_node(lhs);
                if (elem->kind != TY_UNKNOWN && rhs->kind != TY_UNKNOWN && !type_equals(elem, rhs)) {
                    char expected[256];
                    strcpy(expected, type_text(elem));
                    error_plain("配列の要素は`%s`ですが、`%s`が代入されています", expected, type_text(rhs));
                }
            } else {
                check_node(lhs);
            }
            result = rhs;
            break;
        case ND_GVAR:
            result = find_global_type(node);
            break;
        case ND_UNARY:
            if (node->op.unop == OP_NOT) {
                result = &ty_bool;
            }
            break;
        case ND_FUNCALL:
            ;
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
        case ND_MEMBER:
            ;
            Type* obj = check_node(node->left_child);
            if (obj->kind == TY_UNKNOWN) {
                result = &ty_unknown;
                break;
            }
            if (obj->kind != TY_CLASS) {
                error_plain("`%s`型の値にフィールドは使えません", type_text(obj));
            }
            Class* klass = obj->klass;
            Field* field = NULL;
            for (Field* f = klass->fields; f; f = f->next) {
                if (f->len == node->len && !memcmp(f->name, node->name, f->len)) {
                    field = f;
                    break;
                }
            }
            if (!field) {
                error_plain("クラス`%.*s`にフィールド`%.*s`は定義されていません", klass->len, klass->name, node->len, node->name);
            }
            node->offset = field->offset;
            node->klass = klass;
            result = field->type;
            break;
        default:
            break;
    }
    node->type = result;
    return result;
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
}
