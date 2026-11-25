#include "spell.h"
#include "typecheck_internal.h"

// 型そのものの道具: 型の比較・表示・変換と、グローバル変数の型表。

Type ty_int = { TY_INT, NULL };
Type ty_array = { TY_ARRAY, NULL };
Type ty_unknown = { TY_UNKNOWN, NULL };

// 型の比較。strict でない間は「不明な型は全部と一致する」緩い比較、
// strict では不明な型同士だけを一致とする厳密な比較（実体化のキャッシュ用）
static bool type_cmp(Type* a, Type* b, bool strict) {
    if (!strict) {
        // 緩い比較では型変数同士は名前が違っても一致とする（元の type_equals の挙動）
        if (a->kind == TY_VAR && b->kind == TY_VAR) {
            return true;
        }
        if (a->kind == TY_UNKNOWN || b->kind == TY_UNKNOWN) {
            return true;
        }
    }
    if (a->kind != b->kind) {
        return false;
    }
    if (a->kind == TY_ARRAY) {
        return type_cmp(a->elem, b->elem, strict);
    }
    if (a->kind == TY_CLASS) {
        Class* ka = a->klass->origin ? a->klass->origin : a->klass;
        Class* kb = b->klass->origin ? b->klass->origin : b->klass;
        if (ka != kb) {
            return false;
        }
        if (!a->targ && !b->targ) {
            return true;
        }
        if (!a->targ || !b->targ) {
            return false;
        }
        return type_cmp(a->targ, b->targ, strict);
    }
    if (a->kind == TY_VAR) {
        return a->len == b->len && !memcmp(a->name, b->name, a->len);
    }
    return true;
}

bool type_equals(Type* a, Type* b) {
    return type_cmp(a, b, false);
}

bool type_strict_equals(Type* a, Type* b) {
    return type_cmp(a, b, true);
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
            if (type->targ) {
                p += sprintf(p, "<");
                p = type_text_write(type->targ, p);
                p += sprintf(p, ">");
            }
            break;
        case TY_VAR:
            p += sprintf(p, "%.*s", type->len, type->name);
            break;
    }
    return p;
}

char* type_text(Type* type) {
    // 1つのエラーメッセージの中で複数回呼ばれても上書きし合わないよう、
    // バッファを4つ使い回す（呼び出し側で strcpy し直す手間をなくすため）
    static char bufs[4][256];
    static int next = 0;
    char* buf = bufs[next];
    next = (next + 1) % 4;
    type_text_write(type, buf);
    return buf;
}

typedef struct GVarType GVarType;
struct GVarType {
    char* name;
    int len;
    Type* type;
    GVarType* next;
};
static GVarType* global_types;

Type* find_global_type(Node* node) {
    for (GVarType* g = global_types; g; g = g->next) {
        if (g->len == node->len && !memcmp(g->name, node->name, g->len)) {
            return g->type;
        }
    }
    return &ty_unknown;
}

void record_global_type(Node* node, Type* type) {
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

Type* subst_type(Type* t, Type* targ) {
    if (!t) {
        return NULL;
    }
    switch (t->kind) {
        case TY_VAR:
            return targ;
        case TY_ARRAY:
            return array_type(subst_type(t->elem, targ));
        case TY_CLASS:
            if (t->targ) {
                Type* nt = calloc(1, sizeof(Type));
                nt->kind = TY_CLASS;
                nt->klass = t->klass;
                nt->targ = subst_type(t->targ, targ);
                return nt;
            }
            return t;
        default:
            return t;
    }
}

bool type_has_unknown(Type* t) {
    if (!t) {
        return false;
    }
    switch (t->kind) {
        case TY_UNKNOWN:
            return true;
        case TY_ARRAY:
            return type_has_unknown(t->elem);
        case TY_CLASS:
            return type_has_unknown(t->targ);
        default:
            return false;
    }
}

Type* adopt_type(Type* anno, Type* rhs) {
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
