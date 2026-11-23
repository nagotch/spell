#include "spell.h"

static Node* desugar_node(Node* node);

void desugar(Function* prog) {
    for (Function* fn = prog; fn; fn = fn->next) {
        fn->body = desugar_node(fn->body);
    }
}

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
    return node;
}
