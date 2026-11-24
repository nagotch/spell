#include "spell.h"

static Type ty_int = { TY_INT, NULL };
static Type ty_bool = { TY_BOOL, NULL };
static Type ty_unknown = { TY_UNKNOWN, NULL };

static Type* local_types[1024];
static Type* global_types[1024];

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
            check_node(node->right_child);
            if (node->left_child->kind == ND_LVAR || node->left_child->kind == ND_GVAR) {
            }
            break;
        case ND_GVAR:
            break;
        case ND_UNARY:
            if (node->op.unop == OP_NOT) {
                result = &ty_bool;
            }
            break;
        case ND_FUNCALL:
            break;
        case ND_ARRAY:
            break;
        case ND_NEWARRAY:
            break;
        case ND_INDEX:
            break;
        case ND_LEN:
            break;
        case ND_BLOCK:
        case ND_IF:
        case ND_WHILE:
        case ND_FOR:
        case ND_RETURN:
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
