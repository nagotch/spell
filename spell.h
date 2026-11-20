#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

typedef enum {
    TK_NUM,
    TK_SYMBOL,
    TK_EOF,
    TK_IDENT,
    TK_RETURN, 
    TK_IF,
    TK_ELSE,
    TK_WHILE,
    TK_FOR,
} TokenKind;
typedef struct Token Token;
struct Token {
    TokenKind kind;
    long val;
    char* str;
    int len;
    Token *next;
};

typedef enum {
    ND_ADD,
    ND_SUB,
    ND_MUL,
    ND_DIV,
    ND_NUM,
    ND_EQ,
    ND_NE,
    ND_LT,
    ND_LE,
    ND_ASSIGN, 
    ND_LVAR,
    ND_RETURN,
    ND_IF,
    ND_WHILE,
    ND_FOR,
    ND_BLOCK,
    ND_FUNCALL,
} NodeKind;
typedef struct Node Node;
struct Node {
    NodeKind kind;
    Node* left_child;
    Node* right_child;
    long val;
    int offset;
    Node* cond;
    Node* then;
    Node* els;
    Node* init;
    Node* inc;
    Node* body;
    Node* next;
    char* name;
    int len;
    Node* args;
};

typedef struct Function Function;
struct Function {
    Function* next;
    char* name;
    Node* body;
    int nparams;
    int stack_size;
};

typedef struct LVar LVar;
struct LVar {
    char* name;
    int len;
    int offset;
    LVar* next;
};

extern Token* token;
extern LVar* locals;

Token* new_token(TokenKind token_kind, Token* pre_token_ptr);
Token* tokenize(char* s);
bool consume(char* symbol);
void expect(char* symbol);
long expect_number();
bool at_eof();
Node* create_binary_operation_node(NodeKind kind, Node* left_child, Node* right_child);
Node* create_number_node(long num);
Node* primary();
Node* unary();
Node* mul();
Node* add();
Node* relational();
Node* equality();
Node* assign();
Node* expr();
Node* stmt();
Function* program();
void gen(Node* node);
void gen_lval(Node* node);
LVar* find_lvar(Token* tok);
