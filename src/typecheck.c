#include "spell.h"

static Type ty_int = { TY_INT, NULL };
static Type ty_bool = { TY_BOOL, NULL };
static Type ty_unknown = { TY_UNKNOWN, NULL };

static Type* local_types[1024];
static Type* global_types[1024];

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
                local_types[lhs->offset / 8] = rhs;
                lhs->type = rhs;
            } else if (lhs->kind == ND_GVAR) {
                lhs->type = rhs;
            } else {
                check_node(lhs);
            }
            result = rhs;
            break;
        case ND_GVAR:
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
        check_node(fn->body);
    }
}
