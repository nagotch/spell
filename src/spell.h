#pragma once

#include <stdio.h>
#include <stdlib.h>
#include <ctype.h>
#include <stdarg.h>
#include <stdbool.h>
#include <stdint.h>
#include <string.h>
#include <setjmp.h>

typedef struct Field Field;
typedef struct Class Class;
typedef struct Function Function;

typedef enum {
    KW_RETURN,
    KW_IF,
    KW_ELSE,
    KW_WHILE,
    KW_FOR,
    KW_BREAK,
    KW_CONTINUE,
    KW_DEF,
    KW_CLASS,
    KW_IN,
    KW_ELIF,
    KW_IN_STR,
    KW_IN_LINE,
    KW_PUT,
    KW_YN,
    KW_FN,
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
    int line;
    Token* prev;
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

// ジェネリックなクラスの型パラメータ・型引数の上限
#define MAX_TPARAMS 4

typedef enum {
    TY_UNKNOWN,
    TY_INT,
    TY_ARRAY,
    TY_CLASS,
    TY_VAR,
    TY_FUNC,
} TypeKind;
typedef struct Type Type;
struct Type {
    TypeKind kind;
    Type* elem;
    Class* klass;
    Type* targs[MAX_TPARAMS];
    int targ_count;
    char* name;
    int len;
    // TY_FUNC のときだけ使う: 引数の型と戻り値の型
    Type* func_params[6];
    int func_nparams;
    Type* func_ret;
};

// ラムダが外側の変数を捕まえた記録。環境ブロック内のスロットと、
// 捕まえ元の場所（外側関数のローカル、または外側ラムダの環境スロット）を結ぶ
typedef struct Capture Capture;
struct Capture {
    char* name;
    int len;
    int src_offset;
    bool src_is_env;
    Capture* src_cap;
    int env_offset;
    Type* type;
    Capture* next;
};

// コンパイラ組み込みプリミティブ（step 59）。セルフホスト向けに、
// ドライバ層の機能（argv・ファイル入出力・stderr・終了）を [int] の
// 文字コード配列と int だけで扱える形で用意する。呼び出しは typecheck が
// この列挙で印を付け、codegen がランタイムヘルパ（__rt_spell_*）への
// 呼び出しとして出す。名前は予約済みの `__spell_` 名前空間に置く
typedef enum {
    BUILTIN_NONE = 0,
    BUILTIN_ARGC,
    BUILTIN_ARGV_CHARS,
    BUILTIN_READ_FILE_CHARS,
    BUILTIN_IO_OK,
    BUILTIN_WRITE_FILE_CHARS,
    BUILTIN_STDERR_CHARS,
    BUILTIN_EXIT,
} SpellBuiltin;

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
    ND_MEMBER,
    ND_MCALL,
    ND_MULTIASSIGN,
    ND_CHAIN,
    ND_INPUT,
    ND_PUT,
    ND_FUNCREF,
    ND_LAMBDA,
    ND_VCALL,
    ND_CAPVAR,
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
    Class* klass;
    Function* func;
    Token* tok;
    // ND_LAMBDA のときだけ使う: このラムダの捕捉一覧
    Capture* captures;
    // ND_FUNCALL のときだけ使う: 組み込みプリミティブならその種類
    int builtin;
};

struct Function {
    Function* next;
    char* name;
    int len;
    Node* body;
    int nparams;
    int stack_size;
    Type* param_types[6];
    Type* return_type;
    Class* klass;
    // ラムダのときだけ使う
    bool is_lambda;
    int env_offset;
    Capture* captures;
    bool lambda_checked;
    Type* func_type;
};

typedef struct LVar LVar;
struct LVar {
    char* name;
    int len;
    int offset;
    LVar* next;
    int array_len;
};

struct Field {
    Field* next;
    char* name;
    int len;
    Type* type;
    int offset;
};

struct Class {
    Class* next;
    char* name;
    int len;
    Field* fields;
    Type* type;
    char* tparams[MAX_TPARAMS];
    int tparam_lens[MAX_TPARAMS];
    int tparam_count;
    Class* origin;
    Type* targs[MAX_TPARAMS];
    int targ_count;
};

extern Token* token;
extern LVar* locals;
extern LVar* globals;
extern char* src;
extern char* src_name;

extern Class* parsing_klass;

extern Type ty_int;
extern Type ty_array;
extern Type ty_unknown;

long token_start(Token* tok);
long token_end(Token* tok);
char* token_text(Token* tok);

Node* create_node(NodeKind kind);
Class* find_class(char* name, int len);

Type* array_type(Type* elem);
Type* func_type(Type* params[], int nparams, Type* ret);

void error_at(long start, long end, char* fmt, ...);
void error_at_v(long start, long end, char* fmt, va_list ap);
void error_plain(char* fmt, ...);
void error_plain_v(char* fmt, va_list ap);
void error_internal(char* fmt, ...);

// エラーの回収（LSP サーバー用）。キャプチャ有効中のエラーは表示も
// exit もせず、下の診断リストへ記録される。記録したあとの振る舞いは
// 状況で変わる: parse 中は文単位のリカバリ先（error_capture_set_recover）
// へ longjmp し、typecheck 等で継続モード（error_capture_set_continue）
// の間は呼び出し側へ戻って検査を続け、それ以外（字句解析中や上限到達、
// 内部エラー）は error_capture_begin の jmp_buf へ longjmp して
// チェック全体を打ち切る。通常モードでは無効で、従来どおり表示して
// exit(1) する
#define ERROR_DIAG_MAX 50
typedef struct {
    char message[4096];
    long start;
    long end;
    bool has_pos;
    char* src;
} ErrorDiag;
extern ErrorDiag error_diags[ERROR_DIAG_MAX];
extern int error_diag_count;

jmp_buf* error_capture_begin(void);
void error_capture_end(void);
bool error_capture_enabled(void);
void error_capture_set_continue(bool enabled);
jmp_buf* error_capture_set_recover(jmp_buf* jb);

// コンパイルのパイプライン（main.c）。LSP サーバーからも使う
Function* load_program(char* path);
Function* load_program_text(char* text, char* name);
void spell_prelude_path(char* buf, long size);

Token* tokenize(char* s);
Function* program();
void desugar(Function* prog);
void typecheck(Function* prog);
void codegen(Function* prog);
