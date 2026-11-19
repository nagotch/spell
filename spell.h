#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

typedef enum { TK_NUM, TK_SYMBOL, TK_EOF, TK_IDENT } TokenKind;
typedef struct Token Token;
struct Token {
    TokenKind kind;
    long val;
    char* str;
    int len;
    Token *next;
};

typedef enum {ND_ADD, ND_SUB, ND_MUL, ND_DIV, ND_NUM, ND_EQ, ND_NE, ND_LT, ND_LE} NodeKind;
typedef struct Node Node;
struct Node {
    NodeKind kind;
    Node* left_child;
    Node* right_child;
    long val;
};

extern Token* token;

Token* new_token(TokenKind token_kind, Token* pre_token_ptr);
Token* tokenize(char* s);
bool consume(char* symbol);
void expect(char* symbol);
long expect_number();
bool at_eof();
Node* create_binary_operation_node(NodeKind kind, Node* left_child, Node* right_child);
Node* create_number_node(long num);
Node* expr();
Node* primary();
Node* unary();
Node* mul();
Node* add();
Node* relational();
Node* equality();
void gen(Node* node);
