#include "spell.h"
#include "typecheck_internal.h"

// ジェネリクス: 型からクラスを引く class_of と、単相化（instantiate）。

Class* class_of(Type* t) {
    switch (t->kind) {
        case TY_CLASS:
            if (t->klass->tparam_count > 0) {
                if (t->targ_count == 0) {
                    error_plain("クラス`%.*s`には型引数が必要です", t->klass->len, t->klass->name);
                }
                return instantiate(t->klass, t->targs, t->targ_count);
            }
            return t->klass;
        case TY_INT: return find_class("int", 3);
        case TY_ARRAY: {
            Class* arr = find_class("array", 5);
            if (arr && arr->tparam_count > 0) {
                // 要素型が不明な配列は空リテラル由来で中身を持たないため、
                // int の実体化で代用する（不明な型引数では実体化できない）
                Type* elem = type_has_unknown(t->elem) ? &ty_int : t->elem;
                Type* targs[1] = { elem };
                return instantiate(arr, targs, 1);
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

static Node* clone_node(Node* n, Class* tmpl, Type* targs[]) {
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
    c->anno = subst_type(n->anno, tmpl, targs);
    c->left_child = clone_node(n->left_child, tmpl, targs);
    c->right_child = clone_node(n->right_child, tmpl, targs);
    c->cond = clone_node(n->cond, tmpl, targs);
    c->then = clone_node(n->then, tmpl, targs);
    c->els = clone_node(n->els, tmpl, targs);
    c->init = clone_node(n->init, tmpl, targs);
    c->inc = clone_node(n->inc, tmpl, targs);
    c->body = clone_node(n->body, tmpl, targs);
    c->args = clone_node(n->args, tmpl, targs);
    c->next = clone_node(n->next, tmpl, targs);
    return c;
}

static Class* instances = NULL;

Class* instantiate(Class* tmpl, Type* targs[], int targ_count) {
    for (Class* c = instances; c; c = c->next) {
        if (c->origin != tmpl || c->targ_count != targ_count) {
            continue;
        }
        bool same = true;
        for (int i = 0; i < targ_count; i++) {
            if (!type_strict_equals(c->targs[i], targs[i])) {
                same = false;
                break;
            }
        }
        if (same) {
            return c;
        }
    }
    Class* inst = calloc(1, sizeof(Class));
    inst->name = tmpl->name;
    inst->len = tmpl->len;
    inst->origin = tmpl;
    inst->targ_count = targ_count;
    for (int i = 0; i < targ_count; i++) {
        inst->targs[i] = targs[i];
    }
    if (tmpl->type->kind == TY_ARRAY) {
        inst->type = array_type(targs[0]);
    } else {
        inst->type = calloc(1, sizeof(Type));
        inst->type->kind = TY_CLASS;
        inst->type->klass = inst;
        inst->type->targ_count = targ_count;
        for (int i = 0; i < targ_count; i++) {
            inst->type->targs[i] = targs[i];
        }
    }
    Field* ftail = NULL;
    for (Field* f = tmpl->fields; f; f = f->next) {
        Field* nf = calloc(1, sizeof(Field));
        nf->name = f->name;
        nf->len = f->len;
        nf->offset = f->offset;
        nf->type = subst_type(f->type, tmpl, targs);
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
            nf->param_types[i] = subst_type(fn->param_types[i], tmpl, targs);
        }
        nf->param_types[0] = inst->type;
        nf->return_type = subst_type(fn->return_type, tmpl, targs);
        memo_n = 0;
        nf->body = clone_node(fn->body, tmpl, targs);
        tail->next = nf;
        tail = nf;
    }
    return inst;
}

// pat の中の型変数を tmpl のパラメータ位置で引き、実引数 arg から gtargs を埋める
void unify_type(Type* pat, Type* arg, Class* tmpl, Type* gtargs[]) {
    if (!pat || !arg) {
        return;
    }
    if (pat->kind == TY_VAR) {
        for (int i = 0; i < tmpl->tparam_count; i++) {
            if (pat->len != tmpl->tparam_lens[i] || memcmp(pat->name, tmpl->tparams[i], pat->len)) {
                continue;
            }
            Type* cur = gtargs[i];
            if (!cur) {
                gtargs[i] = arg;
                return;
            }
            if (type_strict_equals(cur, arg)) {
                return;
            }
            if (type_has_unknown(cur) && !type_has_unknown(arg)) {
                gtargs[i] = arg;
                return;
            }
            if (type_has_unknown(arg)) {
                return;
            }
            error_plain("型引数を推論できません");
        }
        return;
    }
    if (pat->kind == TY_ARRAY && arg->kind == TY_ARRAY) {
        unify_type(pat->elem, arg->elem, tmpl, gtargs);
        return;
    }
    if (pat->kind == TY_CLASS && arg->kind == TY_CLASS && pat->targ_count == arg->targ_count) {
        // 型引数への再帰は、同じクラス（実体化なら同じ origin）のときだけ。
        // 違うクラスの型引数から推論してはいけない
        Class* pk = pat->klass->origin ? pat->klass->origin : pat->klass;
        Class* ak = arg->klass->origin ? arg->klass->origin : arg->klass;
        if (pk == ak) {
            for (int i = 0; i < pat->targ_count; i++) {
                unify_type(pat->targs[i], arg->targs[i], tmpl, gtargs);
            }
        }
    }
}

