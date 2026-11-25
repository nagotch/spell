#include "spell.h"

// parse.c
bool consume(char* symbol);
void expect(char* symbol);
Token* consume_ident();
Node* char_array();
Node* create_ident_node(Token* tok);
Node* bind_target_node(Node* node);
long expect_number();
Node* create_number_node(long num);
Node* create_binary_operation_node(BinOp kind, Node* left_child, Node* right_child);
void register_array(int array_len);
void register_any();

// expr.c
Node* expr();
Node* parse_put_args();
Type* parse_type();
// 式の括弧・角括弧の入れ子深さ。0 の場所では改行をまたいで式を
// 継続しない（step 55）。parse.c の if/while 条件括弧からも増減する
extern int expr_depth;

// parse.c（ラムダ）
Node* parse_lambda(bool arrow, Token* kw);
bool lambda_arrow_ahead();
bool ident_is_value_call(Token* tok);
