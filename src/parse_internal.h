#include "spell.h"

// parse.c
bool consume(char* symbol);
void expect(char* symbol);
Token* consume_ident();
Node* char_array();
Node* create_ident_node(Token* tok);
long expect_number();
Node* create_number_node(long num);
Node* create_binary_operation_node(BinOp kind, Node* left_child, Node* right_child);
void register_array(int array_len);

// expr.c
Node* expr();
Type* parse_type();
