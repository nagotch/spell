#include "spell.h"

static Node* desugar_node(Node* node) {
    if (!node) {
        return NULL;
    }
    node->left_child = desugar_node(node->left_child);
    node->right_child = desugar_node(node->right_child);
    node->cond = desugar_node(node->cond);
    node->then = desugar_node(node->then);
    node->els = desugar_node(node->els);
    node->init = desugar_node(node->init);
    node->inc = desugar_node(node->inc);
    node->body = desugar_node(node->body);
    node->next = desugar_node(node->next);
    node->args = desugar_node(node->args);

    if (node->kind == ND_OPASSIGN) {
        Node* combined = create_node(ND_BINOP);
        combined->op = node->op;
        combined->tok = node->tok;
        combined->left_child = node->left_child;
        combined->right_child = node->right_child;
        node->kind = ND_ASSIGN;
        node->right_child = combined;
    }

    return node;
}

void desugar(Function* prog) {
    for (Function* fn = prog; fn; fn = fn->next) {
        fn->body = desugar_node(fn->body);
    }

}
