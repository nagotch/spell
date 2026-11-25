#include "spell.h"
#include "typecheck_internal.h"

// ジェネリクス: 型からクラスを引く class_of と、単相化（instantiate）。

Class* class_of(Type* t) {
    switch (t->kind) {
        case TY_CLASS:
            if (t->klass->tparam) {
                if (!t->targ) {
                    error_plain("クラス`%.*s`には型引数が必要です", t->klass->len, t->klass->name);
                }
                return instantiate(t->klass, t->targ);
            }
            return t->klass;
        case TY_INT: return find_class("int", 3);
        case TY_ARRAY: {
            Class* arr = find_class("array", 5);
            if (arr && arr->tparam) {
                // 要素型が不明な配列は空リテラル由来で中身を持たないため、
                // int の実体化で代用する（不明な型引数では実体化できない）
                if (type_has_unknown(t->elem)) {
                    return instantiate(arr, &ty_int);
                }
                return instantiate(arr, t->elem);
            }
            return arr;
        }
        default: return NULL;
    }
}

#define CLONE_MEMO_MAX 8192
static Node* memo_orig[CLONE_MEMO_MAX];
static Node* memo_copy[CLONE_MEMO_MAX];
static int memo_n = 0;

static Node* clone_node(Node* n, Type* targ) {
    if (!n) {
        return NULL;
    }
    for (int i = 0; i < memo_n; i++) {
        if (memo_orig[i] == n) {
            return memo_copy[i];
        }
    }
    if (memo_n >= CLONE_MEMO_MAX) {
        error_internal("ジェネリックなメソッドが大きすぎます");
    }
    Node* c = calloc(1, sizeof(Node));
    *c = *n;
    memo_orig[memo_n] = n;
    memo_copy[memo_n] = c;
    memo_n++;
    c->type = NULL;
    c->func = NULL;
    c->klass = NULL;
    c->anno = subst_type(n->anno, targ);
    c->left_child = clone_node(n->left_child, targ);
    c->right_child = clone_node(n->right_child, targ);
    c->cond = clone_node(n->cond, targ);
    c->then = clone_node(n->then, targ);
    c->els = clone_node(n->els, targ);
    c->init = clone_node(n->init, targ);
    c->inc = clone_node(n->inc, targ);
    c->body = clone_node(n->body, targ);
    c->args = clone_node(n->args, targ);
    c->next = clone_node(n->next, targ);
    return c;
}

static Class* instances = NULL;

Class* instantiate(Class* tmpl, Type* targ) {
    for (Class* c = instances; c; c = c->next) {
        if (c->origin == tmpl && type_strict_equals(c->targ, targ)) {
            return c;
        }
    }
    Class* inst = calloc(1, sizeof(Class));
    inst->name = tmpl->name;
    inst->len = tmpl->len;
    inst->origin = tmpl;
    inst->targ = targ;
    if (tmpl->type->kind == TY_ARRAY) {
        inst->type = array_type(targ);
    } else {
        inst->type = calloc(1, sizeof(Type));
        inst->type->kind = TY_CLASS;
        inst->type->klass = inst;
        inst->type->targ = targ;
    }
    Field* ftail = NULL;
    for (Field* f = tmpl->fields; f; f = f->next) {
        Field* nf = calloc(1, sizeof(Field));
        nf->name = f->name;
        nf->len = f->len;
        nf->offset = f->offset;
        nf->type = subst_type(f->type, targ);
        if (ftail) {
            ftail->next = nf;
        } else {
            inst->fields = nf;
        }
        ftail = nf;
    }
    inst->next = instances;
    instances = inst;
    Function* tail = functions;
    while (tail->next) {
        tail = tail->next;
    }
    for (Function* fn = functions; fn; fn = fn->next) {
        if (fn->klass != tmpl) {
            continue;
        }
        Function* nf = calloc(1, sizeof(Function));
        *nf = *fn;
        nf->klass = inst;
        nf->next = NULL;
        for (int i = 0; i < 6; i++) {
            nf->param_types[i] = subst_type(fn->param_types[i], targ);
        }
        nf->param_types[0] = inst->type;
        nf->return_type = subst_type(fn->return_type, targ);
        memo_n = 0;
        nf->body = clone_node(fn->body, targ);
        tail->next = nf;
        tail = nf;
    }
    return inst;
}

Type* unify_type(Type* pat, Type* arg, Type* cur) {
    if (!pat || !arg) {
        return cur;
    }
    if (pat->kind == TY_VAR) {
        if (!cur) {
            return arg;
        }
        if (type_strict_equals(cur, arg)) {
            return cur;
        }
        if (type_has_unknown(cur) && !type_has_unknown(arg)) {
            return arg;
        }
        if (type_has_unknown(arg)) {
            return cur;
        }
        error_plain("型引数を推論できません");
    }
    if (pat->kind == TY_ARRAY && arg->kind == TY_ARRAY) {
        return unify_type(pat->elem, arg->elem, cur);
    }
    if (pat->kind == TY_CLASS && pat->targ && arg->kind == TY_CLASS && arg->targ) {
        return unify_type(pat->targ, arg->targ, cur);
    }
    return cur;
}

