// spell コンパイラの Rust 移植。C 版（main 以外の各 .c ファイル）と
// モジュール構成をそろえ、挙動（診断文・位置・終了コード・アセンブリ）を
// 一致させることが目的。
//
// 設計メモ:
// - AST は arena（Vec<Node>）＋ NodeId インデックス。next 鎖で文・引数
//   リストを保つ。書き換えは NodeId を返すのではなく C と同じく
//   その場でノードを書き換える（typecheck の dunder 書き換えが前提）。
// - 型・クラス・関数・捕捉もそれぞれ append-only の arena に置く。
//   型は intern しない（C の calloc した Type と同じく、作るたびに新しい）。
//   予約 ID: 0 = TY_UNKNOWN、1 = TY_INT、2 = 要素なし TY_ARRAY
//   （C の静的単例 ty_unknown / ty_int / ty_array に対応）。
// - 名前は専用バイト列 arena へ intern した (off, len) で表す。
//   ソース断片・リテラル・合成名（__lambda_N 等）を同じ仕組みで扱う。
// - ソースは SourceMap に生バイト列で保持する。C が NUL 終端文字列として
//   扱う都合上、有効長は最初の NUL まで（eff_len）。

pub mod codegen;
pub mod desugar;
pub mod driver;
pub mod error;
pub mod expr;
pub mod generics;
pub mod parse;
pub mod tokenize;
pub mod typecheck;
pub mod types;

pub const MAX_TPARAMS: usize = 4;

pub type NodeId = u32;
pub type TypeId = u32;
pub type ClassId = u32;
pub type FunctionId = u32;
pub type CaptureId = u32;
pub type FieldId = u32;
pub type TokenId = u32;

pub const TY_UNKNOWN_ID: TypeId = 0;
pub const TY_INT_ID: TypeId = 1;
pub const TY_ARRAY_NONE_ID: TypeId = 2;

// 名前（intern 済みバイト列への参照）
#[derive(Clone, Copy, PartialEq, Eq, Debug, Default)]
pub struct Name {
    pub off: u32,
    pub len: u32,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Keyword {
    Return,
    If,
    Else,
    While,
    For,
    Break,
    Continue,
    Def,
    Class,
    In,
    Elif,
    InStr,
    InLine,
    Put,
    Yn,
    Fn,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum TokenKind {
    Num,
    Symbol,
    Eof,
    Ident,
    Keyword,
    Str,
}

#[derive(Clone)]
pub struct Token {
    pub kind: TokenKind,
    pub keyword: Keyword,
    pub val: i64,
    // このトークンが属するソース
    pub src: u32,
    // ソース内のバイト位置。TK_STR では開き引用符の位置（C では str が
    // ヒープ上の復号バッファを指し、token_start は意味を持たないため、
    // 直接 error_at に渡される場合の予備として引用符位置を入れる）
    pub start: u32,
    // ソース上のバイト長。TK_STR では復号後のバイト数（C の len と同じ）
    pub len: u32,
    pub line: i32,
    pub prev: Option<TokenId>,
    // TK_STR の復号済みバイト列
    pub str_buf: Option<Vec<u8>>,
    // true なら位置はソースバッファを指していない（TK_STR）。
    // error_node の範囲判定で「範囲外」として扱う（C のポインタ比較と同じ）
    pub heap_span: bool,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum BinOp {
    Add,
    Sub,
    Mul,
    Div,
    Eq,
    Ne,
    Lt,
    Le,
    Mod,
    And,
    Or,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum UnaryOp {
    Addr,
    Deref,
    Not,
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum NodeKind {
    Num,
    Assign,
    LVar,
    GVar,
    Return,
    If,
    While,
    For,
    Block,
    Funcall,
    Array,
    Index,
    Len,
    NewArray,
    Binop,
    Unary,
    OpAssign,
    Break,
    Continue,
    Member,
    MCall,
    MultiAssign,
    Chain,
    Input,
    Put,
    FuncRef,
    Lambda,
    VCall,
    CapVar,
}

// ノードの種類ごとのペイロード。共通の next / tok / ty は Node 側に置く
#[derive(Clone)]
pub enum NodeData {
    Num {
        val: i64,
    },
    Assign {
        left: NodeId,
        right: NodeId,
        anno: Option<TypeId>,
    },
    OpAssign {
        op: BinOp,
        left: NodeId,
        right: NodeId,
    },
    LVar {
        name: Name,
        offset: i32,
    },
    GVar {
        name: Name,
    },
    CapVar {
        name: Name,
        offset: i32,
    },
    Return {
        left: NodeId,
    },
    If {
        cond: NodeId,
        then: NodeId,
        els: Option<NodeId>,
    },
    While {
        cond: NodeId,
        then: NodeId,
    },
    For {
        init: NodeId,
        var: NodeId,
        then: NodeId,
        els: Option<NodeId>,
        // イテレータ退避スロットのオフセット（C の Node.offset）
        iter_offset: i32,
        // for...else 用フラグスロットのオフセット（C の Node.val）
        flag: i64,
    },
    Block {
        body: Option<NodeId>,
    },
    Funcall {
        name: Name,
        args: Option<NodeId>,
        // 構築式として解決されたクラス（あれば codegen は構築を出す）
        klass: Option<ClassId>,
        // 構築式の __init__、または print 書き換え先の情報
        func: Option<FunctionId>,
    },
    Array {
        body: Option<NodeId>,
        array_len: i32,
        offset: i32,
    },
    Index {
        left: NodeId,
        right: NodeId,
    },
    Len {
        left: NodeId,
    },
    NewArray {
        fill: NodeId,
        len: NodeId,
    },
    Binop {
        op: BinOp,
        left: NodeId,
        // 連鎖比較のペアでは右辺を持たない（C の right_child == NULL）
        right: Option<NodeId>,
        // 連鎖比較のペアでのみ使う左右入替フラグ（C の Node.val）
        val: i64,
    },
    Unary {
        op: UnaryOp,
        left: NodeId,
    },
    Break,
    Continue,
    Member {
        left: NodeId,
        name: Name,
        offset: i32,
        func: Option<FunctionId>,
        klass: Option<ClassId>,
    },
    MCall {
        left: NodeId,
        args: Option<NodeId>,
        name: Name,
        func: Option<FunctionId>,
        klass: Option<ClassId>,
    },
    MultiAssign {
        targets: Option<NodeId>,
        values: Option<NodeId>,
        count: i32,
        offset: i32,
    },
    Chain {
        first: NodeId,
        pairs: Option<NodeId>,
        offset: i32,
    },
    Input {
        args: Option<NodeId>,
        mode: i64,
    },
    Put {
        args: Option<NodeId>,
    },
    FuncRef {
        name: Name,
        func: Option<FunctionId>,
    },
    Lambda {
        func: Option<FunctionId>,
        name: Name,
        captures: Option<CaptureId>,
    },
    VCall {
        left: NodeId,
        args: Option<NodeId>,
    },
}

#[derive(Clone)]
pub struct Node {
    pub next: Option<NodeId>,
    pub tok: Option<TokenId>,
    pub ty: Option<TypeId>,
    pub data: NodeData,
}

impl Node {
    pub fn kind(&self) -> NodeKind {
        match &self.data {
            NodeData::Num { .. } => NodeKind::Num,
            NodeData::Assign { .. } => NodeKind::Assign,
            NodeData::OpAssign { .. } => NodeKind::OpAssign,
            NodeData::LVar { .. } => NodeKind::LVar,
            NodeData::GVar { .. } => NodeKind::GVar,
            NodeData::CapVar { .. } => NodeKind::CapVar,
            NodeData::Return { .. } => NodeKind::Return,
            NodeData::If { .. } => NodeKind::If,
            NodeData::While { .. } => NodeKind::While,
            NodeData::For { .. } => NodeKind::For,
            NodeData::Block { .. } => NodeKind::Block,
            NodeData::Funcall { .. } => NodeKind::Funcall,
            NodeData::Array { .. } => NodeKind::Array,
            NodeData::Index { .. } => NodeKind::Index,
            NodeData::Len { .. } => NodeKind::Len,
            NodeData::NewArray { .. } => NodeKind::NewArray,
            NodeData::Binop { .. } => NodeKind::Binop,
            NodeData::Unary { .. } => NodeKind::Unary,
            NodeData::Break => NodeKind::Break,
            NodeData::Continue => NodeKind::Continue,
            NodeData::Member { .. } => NodeKind::Member,
            NodeData::MCall { .. } => NodeKind::MCall,
            NodeData::MultiAssign { .. } => NodeKind::MultiAssign,
            NodeData::Chain { .. } => NodeKind::Chain,
            NodeData::Input { .. } => NodeKind::Input,
            NodeData::Put { .. } => NodeKind::Put,
            NodeData::FuncRef { .. } => NodeKind::FuncRef,
            NodeData::Lambda { .. } => NodeKind::Lambda,
            NodeData::VCall { .. } => NodeKind::VCall,
        }
    }
}

#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum TypeKind {
    Unknown,
    Int,
    Array,
    Class,
    Var,
    Func,
}

#[derive(Clone)]
pub struct Type {
    pub kind: TypeKind,
    pub elem: Option<TypeId>,
    pub klass: Option<ClassId>,
    pub targs: [Option<TypeId>; MAX_TPARAMS],
    pub targ_count: i32,
    // TY_VAR の名前
    pub name: Option<Name>,
    // TY_FUNC のときだけ使う
    pub func_params: [Option<TypeId>; 6],
    pub func_nparams: i32,
    pub func_ret: Option<TypeId>,
}

impl Type {
    pub fn new(kind: TypeKind) -> Type {
        Type {
            kind,
            elem: None,
            klass: None,
            targs: [None; MAX_TPARAMS],
            targ_count: 0,
            name: None,
            func_params: [None; 6],
            func_nparams: 0,
            func_ret: None,
        }
    }
}

#[derive(Clone)]
pub struct Capture {
    pub name: Name,
    pub src_offset: i32,
    pub src_is_env: bool,
    pub src_cap: Option<CaptureId>,
    pub env_offset: i32,
    pub ty: Option<TypeId>,
    pub next: Option<CaptureId>,
}

#[derive(Clone)]
pub struct Function {
    pub next: Option<FunctionId>,
    pub name: Name,
    pub body: Option<NodeId>,
    pub nparams: i32,
    pub stack_size: i32,
    pub param_types: [Option<TypeId>; 6],
    pub return_type: Option<TypeId>,
    pub klass: Option<ClassId>,
    // ラムダのときだけ使う
    pub is_lambda: bool,
    pub env_offset: i32,
    pub captures: Option<CaptureId>,
    pub lambda_checked: bool,
    pub func_type: Option<TypeId>,
}

impl Function {
    pub fn new() -> Function {
        Function {
            next: None,
            name: Name::default(),
            body: None,
            nparams: 0,
            stack_size: 0,
            param_types: [None; 6],
            return_type: None,
            klass: None,
            is_lambda: false,
            env_offset: 0,
            captures: None,
            lambda_checked: false,
            func_type: None,
        }
    }
}

#[derive(Clone)]
pub struct Field {
    pub next: Option<FieldId>,
    pub name: Name,
    pub ty: TypeId,
    pub offset: i32,
}

#[derive(Clone)]
pub struct Class {
    pub next: Option<ClassId>,
    pub name: Name,
    pub fields: Option<FieldId>,
    pub ty: TypeId,
    pub tparams: [Option<Name>; MAX_TPARAMS],
    pub tparam_count: i32,
    pub origin: Option<ClassId>,
    pub targs: [Option<TypeId>; MAX_TPARAMS],
    pub targ_count: i32,
}

#[derive(Clone)]
pub struct LVar {
    pub name: Name,
    pub offset: i32,
    pub array_len: i32,
}

// ソース1つ分。bytes は読み込んだ生バイト列で、C の NUL 終端文字列と
// 同じく最初の NUL までが有効範囲
pub struct Source {
    pub name: Vec<u8>,
    pub bytes: Vec<u8>,
}

impl Source {
    pub fn eff_len(&self) -> usize {
        self.bytes.iter().position(|&b| b == 0).unwrap_or(self.bytes.len())
    }
    pub fn text(&self) -> &[u8] {
        &self.bytes[..self.eff_len()]
    }
    // C の src[i] 読み（範囲外は NUL として扱う）
    pub fn byte_at(&self, i: usize) -> u8 {
        let t = self.text();
        if i < t.len() {
            t[i]
        } else {
            0
        }
    }
}

// 診断1件（LSP キャプチャ用、C の ErrorDiag）
#[derive(Clone)]
pub struct Diag {
    pub message: Vec<u8>,
    pub start: i64,
    pub end: i64,
    pub has_pos: bool,
    pub src: u32,
}

// キャプチャモードでのエラーの伝播先（C の longjmp の行き先）
// Abort = チェック全体の打ち切り（error_capture_begin の jmp_buf）、
// Recover = 文単位のリカバリ先
#[derive(Clone, Copy, PartialEq, Eq, Debug)]
pub enum Flow {
    Abort,
    Recover,
}

pub struct Sink {
    pub capture: bool,
    // 継続モード（typecheck 等）: 記録したら呼び出し側へ戻る
    pub cont: bool,
    // 文単位のリカバリ先が有効か（parse 中の境界ハンドラが立てる）
    pub recover_armed: bool,
    pub diags: Vec<Diag>,
}

impl Sink {
    pub fn new() -> Sink {
        Sink {
            capture: false,
            cont: false,
            recover_armed: false,
            diags: Vec::new(),
        }
    }
}

// ラムダ解析中の文脈（C の LambdaParse）。outer_locals はラムダへ入った
// 時点の locals のスナップショット（先頭追加のみで変わらないため凍結扱い）
pub struct LambdaCtx {
    pub captures: Option<CaptureId>,
    pub ncaptures: i32,
    pub outer_locals: Vec<LVar>,
}

// コンパイラ全体の状態（C のファイルスコープ global 群の置き場）
pub struct Cx {
    pub sources: Vec<Source>,
    // 現在処理中のソース（C の src / src_name）
    pub cur_src: u32,
    pub sink: Sink,
    // 名前 arena
    pub names: Vec<u8>,
    // トークン arena（字句解析のたびに追記される）
    pub tokens: Vec<Token>,
    // 現在の字句解析ランとカーソル
    pub tpos: usize,
    pub trun_eof: usize,
    // arena 群
    pub nodes: Vec<Node>,
    pub types: Vec<Type>,
    pub classes: Vec<Class>,
    pub funcs: Vec<Function>,
    pub captures: Vec<Capture>,
    pub fields: Vec<Field>,
    // クラス表（next 鎖の先頭）
    pub classes_head: Option<ClassId>,
    // ジェネリクス実体化の一覧（末尾が最新。探索は末尾から = C の先頭から）
    pub instances: Vec<ClassId>,
    // typecheck 中の関数一覧（C の typecheck.c の functions）
    pub functions_head: Option<FunctionId>,
    // parse 中の状態
    pub locals: Vec<LVar>,
    pub globals: Vec<LVar>,
    pub func_names: Vec<Name>,
    pub lambda_stack: Vec<LambdaCtx>,
    pub pending_lambdas: Vec<FunctionId>,
    pub parsing_klass: Option<ClassId>,
    pub is_top_level: bool,
    pub top_max_offset: i32,
    pub expr_depth: i32,
    pub suppress_multi: bool,
    pub lambda_counter: i32,
    pub clone_counter: i32,
    // typecheck 中の状態
    pub local_types: Vec<Option<TypeId>>,
    pub current_fn: Option<FunctionId>,
    pub current_return: Option<TypeId>,
    pub expected_type: Option<TypeId>,
    pub current_captures: Option<CaptureId>,
    // グローバル変数の型表（types.c の GVarType）
    pub global_types: Vec<(Name, TypeId)>,
    // codegen 中の状態
    pub max_label: i32,
    pub break_label: i32,
    pub continue_label: i32,
    pub current_env_offset: i32,
    // CLI のアセンブリ出力先（main が freopen する stdout の代わり）。
    // エラーで exit する前に flush するためにここに置く
    pub asm_out: Option<std::io::BufWriter<std::fs::File>>,
}

pub const NO_SRC: u32 = u32::MAX;

impl Cx {
    pub fn new() -> Cx {
        let mut cx = Cx {
            sources: Vec::new(),
            cur_src: NO_SRC,
            sink: Sink::new(),
            names: Vec::new(),
            tokens: Vec::new(),
            tpos: 0,
            trun_eof: 0,
            nodes: Vec::new(),
            types: Vec::new(),
            classes: Vec::new(),
            funcs: Vec::new(),
            captures: Vec::new(),
            fields: Vec::new(),
            classes_head: None,
            instances: Vec::new(),
            functions_head: None,
            locals: Vec::new(),
            globals: Vec::new(),
            func_names: Vec::new(),
            lambda_stack: Vec::new(),
            pending_lambdas: Vec::new(),
            parsing_klass: None,
            is_top_level: false,
            top_max_offset: 0,
            expr_depth: 0,
            suppress_multi: false,
            lambda_counter: 0,
            clone_counter: 0,
            local_types: vec![None; 1024],
            current_fn: None,
            current_return: None,
            expected_type: None,
            current_captures: None,
            global_types: Vec::new(),
            max_label: 0,
            break_label: -1,
            continue_label: -1,
            current_env_offset: 0,
            asm_out: None,
        };
        // 予約 ID の型単例（types.c の ty_unknown / ty_int / ty_array）
        cx.types.push(Type::new(TypeKind::Unknown));
        cx.types.push(Type::new(TypeKind::Int));
        let mut arr = Type::new(TypeKind::Array);
        arr.elem = None;
        cx.types.push(arr);
        cx
    }

    // ---- 名前 arena ----
    pub fn intern(&mut self, bytes: &[u8]) -> Name {
        let off = self.names.len() as u32;
        self.names.extend_from_slice(bytes);
        Name {
            off,
            len: bytes.len() as u32,
        }
    }
    pub fn intern_str(&mut self, s: &str) -> Name {
        self.intern(s.as_bytes())
    }
    pub fn name_bytes(&self, n: Name) -> &[u8] {
        &self.names[n.off as usize..(n.off + n.len) as usize]
    }
    pub fn name_eq(&self, a: Name, b: Name) -> bool {
        self.name_bytes(a) == self.name_bytes(b)
    }
    pub fn name_eq_lit(&self, a: Name, lit: &str) -> bool {
        self.name_bytes(a) == lit.as_bytes()
    }

    // ---- ソース ----
    pub fn add_source(&mut self, name: Vec<u8>, bytes: Vec<u8>) -> u32 {
        let id = self.sources.len() as u32;
        self.sources.push(Source { name, bytes });
        id
    }
    pub fn cur_text(&self) -> &[u8] {
        if self.cur_src == NO_SRC {
            return &[];
        }
        self.sources[self.cur_src as usize].text()
    }

    // ---- arena への追加 ----
    pub fn new_node(&mut self, data: NodeData) -> NodeId {
        let id = self.nodes.len() as NodeId;
        self.nodes.push(Node {
            next: None,
            tok: None,
            ty: None,
            data,
        });
        id
    }
    pub fn new_type(&mut self, t: Type) -> TypeId {
        let id = self.types.len() as TypeId;
        self.types.push(t);
        id
    }
    pub fn new_func(&mut self, f: Function) -> FunctionId {
        let id = self.funcs.len() as FunctionId;
        self.funcs.push(f);
        id
    }
    pub fn new_capture(&mut self, c: Capture) -> CaptureId {
        let id = self.captures.len() as CaptureId;
        self.captures.push(c);
        id
    }
    pub fn new_field(&mut self, f: Field) -> FieldId {
        let id = self.fields.len() as FieldId;
        self.fields.push(f);
        id
    }

    // ---- トークン参照 ----
    pub fn tok(&self) -> &Token {
        &self.tokens[self.tpos]
    }
    pub fn tok_at(&self, i: usize) -> &Token {
        &self.tokens[i]
    }
    pub fn peek_tok(&self, n: usize) -> &Token {
        let i = self.tpos + n;
        if i > self.trun_eof {
            &self.tokens[self.trun_eof]
        } else {
            &self.tokens[i]
        }
    }
    pub fn advance(&mut self) {
        if self.tpos < self.trun_eof {
            self.tpos += 1;
        }
    }
    // トークンのソース上の文字列（TK_STR では復号済みバイト列）
    pub fn token_bytes(&self, t: &Token) -> Vec<u8> {
        if t.kind == TokenKind::Str {
            return t.str_buf.clone().unwrap_or_default();
        }
        let src = &self.sources[t.src as usize];
        let text = src.text();
        let s = t.start as usize;
        let e = s + t.len as usize;
        if s >= text.len() {
            return Vec::new();
        }
        text[s..e.min(text.len())].to_vec()
    }
    pub fn token_start(&self, t: &Token) -> i64 {
        t.start as i64
    }
    pub fn token_end(&self, t: &Token) -> i64 {
        t.start as i64 + t.len as i64
    }
    // token_text（error.c の token_text）: EOF では None
    pub fn token_text(&self, t: &Token) -> Option<Vec<u8>> {
        let mut out = Vec::new();
        match t.kind {
            TokenKind::Num => {
                out.extend_from_slice("数値`".as_bytes());
                out.extend_from_slice(t.val.to_string().as_bytes());
                out.push(b'`');
            }
            TokenKind::Eof => return None,
            TokenKind::Ident => {
                out.extend_from_slice("名前`".as_bytes());
                out.extend_from_slice(&self.token_bytes(t));
                out.push(b'`');
            }
            TokenKind::Keyword | TokenKind::Symbol => {
                out.push(b'`');
                out.extend_from_slice(&self.token_bytes(t));
                out.push(b'`');
            }
            TokenKind::Str => {
                out.extend_from_slice("文字列".as_bytes());
            }
        }
        Some(out)
    }
    pub fn name_from_token(&mut self, t: TokenId) -> Name {
        let tok = self.tokens[t as usize].clone();
        let bytes = self.token_bytes(&tok);
        self.intern(&bytes)
    }
}

// メッセージ組み立て用: C の printf 書式 (%.*s / %d / %ld / %s) の
// 代わりに、バイト列をそのまま連結するための小さな道具
pub fn msg(parts: &[&[u8]]) -> Vec<u8> {
    let mut out = Vec::new();
    for p in parts {
        out.extend_from_slice(p);
    }
    out
}

pub fn msg_fmt1(fmt: &str, a: &[u8]) -> Vec<u8> {
    // "%s" が1つだけの単純な書式用（型名やファイル名の差し込み）
    let bytes = fmt.as_bytes();
    let mut out = Vec::new();
    let mut i = 0;
    while i < bytes.len() {
        if bytes[i] == b'%' && i + 1 < bytes.len() && bytes[i + 1] == b's' {
            out.extend_from_slice(a);
            i += 2;
        } else {
            out.push(bytes[i]);
            i += 1;
        }
    }
    out
}
