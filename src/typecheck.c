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

static bool type_equals(Type* a, Type* b) {
    if (a->kind != b->kind) {
        return false;
    }
    if (a->kind == TY_ARRAY) {
        return type_equals(a->elem, b->elem);
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

static Type* array_type(Type* elem) {
    Type* t = calloc(1, sizeof(Type));
    t->kind = TY_ARRAY;
    t->elem = elem;
    return t;
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
                if (!recorded) {
                    local_types[lhs->offset / 8] = node->anno ? node->anno : rhs;
                }
                lhs->type = local_types[lhs->offset / 8];
            } else if (lhs->kind == ND_GVAR) {
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
                if (!recorded) {
                    record_global_type(lhs, node->anno ? node->anno : rhs);
                }
                lhs->type = find_global_type(lhs);
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
            for (Node* arg = node->args; arg; arg = arg->next) {
                check_node(arg);
            }
            result = &ty_int;
            break;
        case ND_ARRAY:
            ;
            Type* elem = &ty_unknown;
            for (Node* cur = node->body; cur; cur = cur->next) {
                Type* t = check_node(cur);
                if (cur == node->body) {
                    elem = t;
                }
            }
            result = array_type(elem);
            break;
        case ND_NEWARRAY:
            ;
            Type* fill = check_node(node->left_child);
            check_node(node->right_child);
            result = array_type(fill);
            break;
        case ND_INDEX:
            ;
            Type* arr = check_node(node->left_child);
            check_node(node->right_child);
            if (arr->kind == TY_ARRAY) {
                result = arr->elem;
            }
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
            check_node(node->left_child);
            break;
        default:
            break;
    }
    node->type = result;
    return result;
}

void typecheck(Function* prog) {
    for (Function* fn = prog; fn; fn = fn->next) {
        memset(local_types, 0, sizeof(local_types));
        for (Node* stmt = fn->body; stmt; stmt = stmt->next) {
            check_node(stmt);
        }
    }
}
