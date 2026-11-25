#include "spell.h"

// typecheck.c
extern Function* functions;

// types.c
bool type_equals(Type* a, Type* b);
bool type_strict_equals(Type* a, Type* b);
char* type_text(Type* type);
Type* find_global_type(Node* node);
void record_global_type(Node* node, Type* type);
Type* subst_type(Type* t, Type* targ);
bool type_has_unknown(Type* t);
Type* adopt_type(Type* anno, Type* rhs);

// generics.c
Class* class_of(Type* t);
Class* instantiate(Class* tmpl, Type* targ);
Type* unify_type(Type* pat, Type* arg, Type* cur);
