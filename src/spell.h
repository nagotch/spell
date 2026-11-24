#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdbool.h>
#include <string.h>

typedef enum {
    KW_RETURN,
    KW_IF,
    KW_ELSE,
    KW_WHILE,
    KW_FOR,
    KW_BREAK,
    KW_CONTINUE,
    KW_DEF,
} Keyword;

typedef enum {
    TK_NUM,
    TK_SYMBOL,
    TK_EOF,
    TK_IDENT,
    TK_KEYWORD,
    TK_STR,
} TokenKind;
typedef struct Token Token;
struct Token {
    TokenKind kind;
    Keyword keyword;
    long val;
    char* str;
    int len;
    Token *next;
};

typedef enum {
    OP_ADD,
    OP_SUB,
    OP_MUL,
    OP_DIV,
    OP_EQ,
    OP_NE,
    OP_LT,
    OP_LE,
    OP_MOD,
    OP_AND,
    OP_OR,
} BinOp;
typedef enum {
    OP_ADDR,
    OP_DEREF,
    OP_NOT,
} UnaryOp;
typedef union {
    BinOp binop;
    UnaryOp unop;
} Op;

typedef enum {
    TY_UNKNOWN,
    TY_INT,
    TY_BOOL,
    TY_CHAR,
    TY_ARRAY,
} TypeKind;
typedef struct Type Type;
struct Type {
    TypeKind kind;
    Type* elem;
};

typedef enum {
    ND_NUM,
    ND_ASSIGN, 
    ND_LVAR,
    ND_GVAR,
    ND_RETURN,
    ND_IF,
    ND_WHILE,
    ND_FOR,
    ND_BLOCK,
    ND_FUNCALL,
    ND_ARRAY,
    ND_INDEX,
    ND_LEN,
    ND_NEWARRAY,
    ND_BINOP,
    ND_UNARY,
    ND_OPASSIGN,
    ND_BREAK,
    ND_CONTINUE,
} NodeKind;
typedef struct Node Node;
struct Node {
    NodeKind kind;
    Op op;
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
    int array_len;
    Type* type;
    Type* anno;
};

typedef struct Function Function;
struct Function {
    Function* next;
    char* name;
    int len;
    Node* body;
    int nparams;
    int stack_size;
    Type* param_types[6];
    Type* return_type;
};

typedef struct LVar LVar;
struct LVar {
    char* name;
    int len;
    int offset;
    LVar* next;
    int array_len;
};

extern Token* token;
extern LVar* locals;
extern LVar* globals;
extern char* src;
extern char* src_name;

extern Type ty_int;
extern Type ty_bool;
extern Type ty_char;
extern Type ty_array;
extern Type ty_unknown;

long token_start(Token* tok);
long token_end(Token* tok);
char* token_text(Token* tok);

Node* create_node(NodeKind kind);

Type* array_type(Type* elem);

void error_at(long start, long end, char* fmt, ...);
void error_plain(char* fmt, ...);
void error_internal(char* fmt, ...);

Token* tokenize(char* s);
Function* program();
void desugar(Function* prog);
void typecheck(Function* prog);
void codegen(Function* prog);
