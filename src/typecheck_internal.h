#include "spell.h"

// typecheck.c
extern Function* functions;

// parse.c（typecheck の書き換えで数値ノードを作るために使う）
Node* create_number_node(long num);

// types.c
bool type_equals(Type* a, Type* b);
bool type_strict_equals(Type* a, Type* b);
char* type_text(Type* type);
Type* find_global_type(Node* node);
void record_global_type(Node* node, Type* type);
Type* subst_type(Type* t, Class* tmpl, Type* targs[]);
bool type_has_unknown(Type* t);
Type* adopt_type(Type* anno, Type* rhs);

// generics.c
Class* class_of(Type* t);
Class* instantiate(Class* tmpl, Type* targs[], int targ_count);
void unify_type(Type* pat, Type* arg, Class* tmpl, Type* gtargs[]);
