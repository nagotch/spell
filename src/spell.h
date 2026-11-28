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
    KW_AND,
    KW_OR,
    KW_NOT,
} Keyword;

typedef enum {
    TK_NUM,
    TK_SYMBOL,
    TK_EOF,
    TK_IDENT,
    TK_KEYWORD,
    TK_STR,
    // f-string リテラル（step 93）。TK_STR と違い、str はソース内の
    // `f` の位置を指し、len はリテラル全体（`f` と引用符を含む）の
    // 長さ。内側の分割とデコードはパーサが行う
    TK_FSTR,
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
    // 配列への append（step 65）。typecheck が配列型レシーバへの
    // `append` メソッド呼び出しをこの組み込みノードへ書き換える。
    // left_child = 代入可能な対象（スロット）、args = 追加する値（1個）
    ND_APPEND,
    // 矢印演算子（step 66）。`A <- B` が ND_ARROW_L、`A -> B` が
    // ND_ARROW_R。パース時点では型が分からないためこの形で運び、
    // typecheck がオペランドの型で dunder 呼び出し・代入・int の
    // 比較へ書き換える（書き換え後は残らない）
    ND_ARROW_L,
    ND_ARROW_R,
    // 真理値リテラル（step 70）。式の位置の `true` / `false` / `T` / `F`
    // をパーサがこの種別に変換する。val が 1（真）か 0（偽）で、型は
    // prelude の bool クラス。型の位置やメンバ名は対象外
    ND_BOOL,
    // 安全添え字（step 81）。`対象?[添え字][添え字]...` をパーサが
    // この種別で運ぶ。left_child が対象、args が添え字の並び、
    // array_len が角括弧の個数、offset が型検査の書き換えで使う
    // 一時スロット列の先頭。typecheck が範囲チェック（bool）と
    // 通常の添え字・論理演算へ書き換えるため、codegen には残らない
    ND_SAFEIDX,
    // スライス（step 85）。`対象[lo:hi]`（端は省略可）をパーサが
    // この種別で運ぶ。left_child が対象、right_child が下限の式
    // （省略時は NULL）、args が上限の式（省略時は NULL）、offset が
    // 型検査の書き換えで使う一時スロット列の先頭（対象・下限・
    // 上限の3枠）。typecheck が境界を正規化して prelude の
    // `__slice__(lo, hi)` 呼び出しへ書き換えるため、codegen には
    // 残らない
    ND_SLICE,
    // f-string（step 93）。`f"....{式}...."` をパーサがこの種別で
    // 運ぶ。args が部品（文字列リテラルのノードと式ノード）の並び。
    // typecheck が各式部品を print と同じ `__str__` 変換で包み、
    // str の `+` の連鎖へ書き換えるため、codegen には残らない
    ND_FSTR,
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
    // 構築時の明示的な型引数（step 76）。`名前<型, ...>(引数...)` の
    // 形で書かれた場合だけ parse が詰める。ctor_targ_count == 0 は
    // 指定なし（推論・期待型・既定に委ねる）。呼び出しなしで式の位置に
    // `名前<型, ...>` と書く形は無い
    Type* ctor_targs[MAX_TPARAMS];
    int ctor_targ_count;
    // ND_FUNCREF のときだけ使う（step 91）: オーバーロードされた名前の
    // 関数参照で、期待型が無く解決を保留した印。保留したまま検査が
    // 終わらないよう、保留した呼び出し側が勝者を決めた後に再検査し、
    // 残っていれば曖昧エラーにする
    bool funcref_deferred;
    // 型を引数に渡す構築（step 77）。`Deque(int)` のように構築の引数が
    // 型1つだけの場合に parse が立てる。要素型は ctor_targs[0] に入り、
    // 引数そのものは作らない（nargs は 0）。typecheck が要素型の
    // ゼロ値ファクトリを合成して引数へ差し込む
    bool ctor_type_arg;
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
    // 到達可能性（step 63）: codegen が入口から辿って印を付ける。
    // 印の無い関数はアセンブリへ出さない
    bool reachable;
    // ジェネリクス実体化メソッドの遅延検査（step 83）: 実体化された
    // メソッドは、検査済みの本体から呼び出されているもの（tc_wanted）
    // だけを typecheck が検査する。tc_done は検査済みの印
    bool tc_wanted;
    bool tc_done;
    // 定義した名前のトークン（step 91）。再定義エラーの位置と行番号に
    // 使う。合成された関数（ラムダ・入口・ゼロ値ファクトリ等）は
    // NULL のまま
    Token* name_tok;
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
    // クラス宣言を既に読んだか（step 91）。program() の先行スキャンは
    // register_class で仮登録だけをするため、宣言済みかどうかは
    // class_decl がこの印で管理し、同名クラスの再定義を止める。
    // decl_tok は最初の宣言の名前トークン（行番号の併記用）
    bool declared;
    Token* decl_tok;
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
// f-string のリテラル部分 [start, end) をデコードして malloc した
// バッファを返す（step 93）。`{{` は `{`、`}}` は `}` になり、
// バックスラッシュエスケープは通常の文字列と同じ対応表で解釈する。
// first は f-string の `f` の位置、quote は引用符（エラー表示用）
char* decode_fstr_literal(char* start, char* end, int* out_len, char* first, char quote);
// リテラル1つ分（通常の文字列、または `f` の位置から始まる
// f-string）を読み飛ばし、閉じ引用符の直後を返す（step 93）。
// 構造検査済みの前提で、字句解析と同じ境界で読み飛ばす
char* fstr_skip_literal(char* p);
Function* program();
Function* ensure_entry_point(Function* prog);
void desugar(Function* prog);
void typecheck(Function* prog);
void codegen(Function* prog);
