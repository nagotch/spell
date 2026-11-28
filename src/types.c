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
        // アドレスの実体は int なので、緩い比較では int と互換とする
        // （step 97）。`int` の変数へアドレスを代入する・その逆を許し、
        // 判定は変数に記録された型の側で行う
        if ((a->kind == TY_ADDR && b->kind == TY_INT)
                || (a->kind == TY_INT && b->kind == TY_ADDR)) {
            return true;
        }
    }
    if (a->kind != b->kind) {
        return false;
    }
    if (a->kind == TY_ARRAY) {
        return type_cmp(a->elem, b->elem, strict);
    }
    if (a->kind == TY_ADDR) {
        Type* ea = a->elem ? a->elem : &ty_unknown;
        Type* eb = b->elem ? b->elem : &ty_unknown;
        return type_cmp(ea, eb, strict);
    }
    if (a->kind == TY_CLASS) {
        Class* ka = a->klass->origin ? a->klass->origin : a->klass;
        Class* kb = b->klass->origin ? b->klass->origin : b->klass;
        if (ka != kb) {
            return false;
        }
        if (a->targ_count != b->targ_count) {
            return false;
        }
        for (int i = 0; i < a->targ_count; i++) {
            if (!type_cmp(a->targs[i], b->targs[i], strict)) {
                return false;
            }
        }
        return true;
    }
    if (a->kind == TY_VAR) {
        return a->len == b->len && !memcmp(a->name, b->name, a->len);
    }
    if (a->kind == TY_FUNC) {
        if (a->func_nparams != b->func_nparams) {
            return false;
        }
        for (int i = 0; i < a->func_nparams; i++) {
            Type* pa = a->func_params[i] ? a->func_params[i] : &ty_unknown;
            Type* pb = b->func_params[i] ? b->func_params[i] : &ty_unknown;
            if (!type_cmp(pa, pb, strict)) {
                return false;
            }
        }
        Type* ra = a->func_ret ? a->func_ret : &ty_unknown;
        Type* rb = b->func_ret ? b->func_ret : &ty_unknown;
        return type_cmp(ra, rb, strict);
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
            if (type->targ_count > 0) {
                p += sprintf(p, "<");
                for (int i = 0; i < type->targ_count; i++) {
                    if (i > 0) {
                        p += sprintf(p, ", ");
                    }
                    p = type_text_write(type->targs[i], p);
                }
                p += sprintf(p, ">");
            }
            break;
        case TY_VAR:
            p += sprintf(p, "%.*s", type->len, type->name);
            break;
        case TY_ADDR:
            p += sprintf(p, "&");
            p = type_text_write(type->elem ? type->elem : &ty_unknown, p);
            break;
        case TY_FUNC:
            p += sprintf(p, "fn(");
            for (int i = 0; i < type->func_nparams; i++) {
                if (i > 0) {
                    p += sprintf(p, ", ");
                }
                p = type_text_write(type->func_params[i] ? type->func_params[i] : &ty_unknown, p);
            }
            p += sprintf(p, ") -> ");
            p = type_text_write(type->func_ret ? type->func_ret : &ty_unknown, p);
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

// `&` の結果の型（step 97）。elem は指す先の型、不明なら NULL
Type* addr_type(Type* elem) {
    Type* t = calloc(1, sizeof(Type));
    t->kind = TY_ADDR;
    t->elem = elem;
    return t;
}

Type* func_type(Type* params[], int nparams, Type* ret) {
    Type* t = calloc(1, sizeof(Type));
    t->kind = TY_FUNC;
    t->func_nparams = nparams;
    for (int i = 0; i < nparams && i < 6; i++) {
        t->func_params[i] = params[i];
    }
    t->func_ret = ret;
    return t;
}

// tmpl の型パラメータ名と targs を位置で対応させて置換する
Type* subst_type(Type* t, Class* tmpl, Type* targs[]) {
    if (!t) {
        return NULL;
    }
    switch (t->kind) {
        case TY_VAR:
            for (int i = 0; i < tmpl->tparam_count; i++) {
                if (t->len == tmpl->tparam_lens[i] && !memcmp(t->name, tmpl->tparams[i], t->len)) {
                    return targs[i];
                }
            }
            return t;
        case TY_ARRAY:
            return array_type(subst_type(t->elem, tmpl, targs));
        case TY_ADDR:
            return addr_type(subst_type(t->elem, tmpl, targs));
        case TY_CLASS:
            if (t->targ_count > 0) {
                Type* nt = calloc(1, sizeof(Type));
                nt->kind = TY_CLASS;
                nt->klass = t->klass;
                nt->targ_count = t->targ_count;
                for (int i = 0; i < t->targ_count; i++) {
                    nt->targs[i] = subst_type(t->targs[i], tmpl, targs);
                }
                return nt;
            }
            return t;
        case TY_FUNC: {
            Type* nt = calloc(1, sizeof(Type));
            nt->kind = TY_FUNC;
            nt->func_nparams = t->func_nparams;
            for (int i = 0; i < t->func_nparams; i++) {
                nt->func_params[i] = subst_type(t->func_params[i], tmpl, targs);
            }
            nt->func_ret = subst_type(t->func_ret, tmpl, targs);
            return nt;
        }
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
        case TY_ADDR:
            return !t->elem || type_has_unknown(t->elem);
        case TY_CLASS:
            for (int i = 0; i < t->targ_count; i++) {
                if (type_has_unknown(t->targs[i])) {
                    return true;
                }
            }
            return false;
        case TY_FUNC:
            for (int i = 0; i < t->func_nparams; i++) {
                if (type_has_unknown(t->func_params[i])) {
                    return true;
                }
            }
            return type_has_unknown(t->func_ret);
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
