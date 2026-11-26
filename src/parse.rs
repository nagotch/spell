// 構文解析（parse.c の移植）。文・関数・クラス・ラムダと、
// キャプチャ有効時の文単位リカバリを担う。式は expr.rs。
use crate::{
    Capture, Class, ClassId, Cx, Flow, Function, Keyword, LVar, Name, NodeData, NodeId, Token,
    TokenId, TokenKind, TypeId, MAX_TPARAMS,
};

pub fn tok_is_symbol(cx: &Cx, t: &Token, symbol: &str) -> bool {
    t.kind == TokenKind::Symbol && cx.token_bytes(t) == symbol.as_bytes()
}

pub fn tok_is_char(cx: &Cx, t: &Token, c: u8) -> bool {
    t.kind == TokenKind::Symbol && t.len == 1 && cx.token_bytes(t) == [c]
}

pub struct CaptureHit {
    pub level: usize,
    pub lvar: Option<LVar>,
    pub cap: Option<crate::CaptureId>,
}

impl Cx {
    // ---- トークン操作 ----
    pub fn consume(&mut self, symbol: &str) -> bool {
        let t = self.tok().clone();
        if tok_is_symbol(self, &t, symbol) {
            self.advance();
            true
        } else {
            false
        }
    }

    pub fn consume_keyword(&mut self, keyword: Keyword) -> bool {
        let t = self.tok();
        if t.kind == TokenKind::Keyword && t.keyword == keyword {
            self.advance();
            true
        } else {
            false
        }
    }

    pub fn consume_ident(&mut self) -> Option<TokenId> {
        if self.tok().kind == TokenKind::Ident {
            let id = self.tpos as TokenId;
            self.advance();
            Some(id)
        } else {
            None
        }
    }

    pub fn at_eof(&self) -> bool {
        self.tok().kind == TokenKind::Eof
    }

    pub fn cur_tok_id(&self) -> TokenId {
        self.tpos as TokenId
    }

    // token_text（EOF では "(null)"、glibc の %s と同じ）
    pub fn tt(&self, t: TokenId) -> Vec<u8> {
        let tok = self.tokens[t as usize].clone();
        self.token_text(&tok)
            .unwrap_or_else(|| b"(null)".to_vec())
    }

    pub fn err_tok(&mut self, t: TokenId, message: Vec<u8>) -> Result<(), Flow> {
        let tok = self.tokens[t as usize].clone();
        let (s, e) = (self.token_start(&tok), self.token_end(&tok));
        self.err_at(s, e, message)
    }

    pub fn expect(&mut self, symbol: &str) -> Result<(), Flow> {
        if self.consume(symbol) {
            return Ok(());
        }
        let t = self.cur_tok_id();
        let tok = self.tok().clone();
        match self.token_text(&tok) {
            Some(actual) => {
                let mut m = Vec::new();
                m.push(b'`');
                m.extend_from_slice(symbol.as_bytes());
                m.extend_from_slice("`が必要ですが、".as_bytes());
                m.extend_from_slice(&actual);
                m.extend_from_slice("が見つかりました".as_bytes());
                self.err_tok(t, m)
            }
            None => {
                let mut m = Vec::new();
                m.push(b'`');
                m.extend_from_slice(symbol.as_bytes());
                m.extend_from_slice("`が必要ですが、ファイルの末尾に達しました".as_bytes());
                self.err_tok(t, m)
            }
        }
    }

    pub fn expect_number(&mut self) -> Result<i64, Flow> {
        if self.tok().kind == TokenKind::Num {
            let v = self.tok().val;
            self.advance();
            return Ok(v);
        }
        let t = self.cur_tok_id();
        let tok = self.tok().clone();
        match self.token_text(&tok) {
            Some(actual) => {
                let mut m = Vec::new();
                m.extend_from_slice("数値が必要ですが、".as_bytes());
                m.extend_from_slice(&actual);
                m.extend_from_slice("が見つかりました".as_bytes());
                self.err_tok(t, m)?;
            }
            None => {
                self.err_tok(
                    t,
                    "数値が必要ですが、ファイルの末尾に達しました"
                        .as_bytes()
                        .to_vec(),
                )?;
            }
        }
        Ok(0)
    }

    // ---- 変数の登録と検索 ----
    pub fn note_top_used(&mut self, offset: i32, array_len: i32) {
        if self.is_top_level {
            let used = offset + 8 * array_len;
            if self.top_max_offset < used {
                self.top_max_offset = used;
            }
        }
    }

    pub fn register_any(&mut self) {
        let offset = match self.locals.last() {
            Some(head) => head.offset + 8 * (head.array_len + 1),
            None => 8,
        };
        self.locals.push(LVar {
            name: Name::default(),
            offset,
            array_len: 0,
        });
        self.note_top_used(offset, 0);
    }

    pub fn register_lvar(&mut self, tok: TokenId) {
        self.register_any();
        let name = self.name_from_token(tok);
        self.locals.last_mut().unwrap().name = name;
    }

    pub fn register_gvar(&mut self, tok: TokenId) {
        let name = self.name_from_token(tok);
        self.globals.push(LVar {
            name,
            offset: 0,
            array_len: 0,
        });
    }

    pub fn register_array(&mut self, array_len: i32) {
        self.register_any();
        let offset = {
            let head = self.locals.last_mut().unwrap();
            head.array_len = array_len;
            head.offset
        };
        self.note_top_used(offset, array_len);
    }

    pub fn find_lvar(&self, name: Name) -> Option<LVar> {
        for v in self.locals.iter().rev() {
            if self.name_eq(v.name, name) {
                return Some(v.clone());
            }
        }
        None
    }

    pub fn find_gvar(&self, name: Name) -> Option<LVar> {
        for v in self.globals.iter().rev() {
            if self.name_eq(v.name, name) {
                return Some(v.clone());
            }
        }
        None
    }

    pub fn find_class(&self, name: Name) -> Option<ClassId> {
        let mut cur = self.classes_head;
        while let Some(id) = cur {
            let c = &self.classes[id as usize];
            if self.name_eq(c.name, name) {
                return Some(id);
            }
            cur = c.next;
        }
        None
    }

    fn register_func_name(&mut self, name: Name) {
        if self.func_names.iter().any(|f| self.name_eq(*f, name)) {
            return;
        }
        self.func_names.push(name);
    }

    fn is_func_name(&self, name: Name) -> bool {
        self.func_names.iter().any(|f| self.name_eq(*f, name))
    }

    // ---- ラムダの捕捉 ----
    pub fn find_outer(&self, name: Name) -> Option<CaptureHit> {
        let n = self.lambda_stack.len();
        for li in (0..n).rev() {
            let level = &self.lambda_stack[li];
            for v in level.outer_locals.iter().rev() {
                if self.name_eq(v.name, name) {
                    return Some(CaptureHit {
                        level: li,
                        lvar: Some(v.clone()),
                        cap: None,
                    });
                }
            }
            if li > 0 {
                // level->outer（外側ラムダ）の捕捉から探す
                let mut cur = self.lambda_stack[li - 1].captures;
                while let Some(cid) = cur {
                    let c = &self.captures[cid as usize];
                    if self.name_eq(c.name, name) {
                        return Some(CaptureHit {
                            level: li,
                            lvar: None,
                            cap: Some(cid),
                        });
                    }
                    cur = c.next;
                }
            }
        }
        None
    }

    fn add_capture(
        &mut self,
        level: usize,
        name: Name,
        src_offset: i32,
        src_is_env: bool,
        src_cap: Option<crate::CaptureId>,
    ) -> crate::CaptureId {
        let mut cur = self.lambda_stack[level].captures;
        while let Some(cid) = cur {
            let c = &self.captures[cid as usize];
            if self.name_eq(c.name, name) {
                return cid;
            }
            cur = c.next;
        }
        let env_offset = self.lambda_stack[level].ncaptures * 8;
        self.lambda_stack[level].ncaptures += 1;
        let head = self.lambda_stack[level].captures;
        let cid = self.new_capture(Capture {
            name,
            src_offset,
            src_is_env,
            src_cap,
            env_offset,
            ty: None,
            next: head,
        });
        self.lambda_stack[level].captures = Some(cid);
        cid
    }

    fn resolve_capture(&mut self, tok: TokenId) -> Result<Option<crate::CaptureId>, Flow> {
        let name = self.name_from_token(tok);
        let hit = match self.find_outer(name) {
            Some(h) => h,
            None => return Ok(None),
        };
        let mut cap = if let Some(lvar) = &hit.lvar {
            self.add_capture(hit.level, name, lvar.offset, false, None)
        } else {
            let src = hit.cap.unwrap();
            let src_off = self.captures[src as usize].env_offset;
            self.add_capture(hit.level, name, src_off, true, Some(src))
        };
        // 現在のラムダから、見つけた階層の手前まで捕捉を積む
        let top = self.lambda_stack.len() - 1;
        let mut chain: Vec<usize> = Vec::new();
        let mut li = top;
        while li > hit.level {
            if chain.len() >= 64 {
                let t = tok;
                let m = "ラムダの入れ子が深すぎます（64段まで）".as_bytes().to_vec();
                return self.err_tok(t, m).map(|_| None);
            }
            chain.push(li);
            li -= 1;
        }
        for &lvl in chain.iter().rev() {
            let src_off = self.captures[cap as usize].env_offset;
            cap = self.add_capture(lvl, name, src_off, true, Some(cap));
        }
        Ok(Some(cap))
    }

    // 識別子＋`(` を値経由の呼び出しに振り分けるかの判定
    pub fn ident_is_value_call(&self, tok: TokenId) -> bool {
        let t = &self.tokens[tok as usize];
        let bytes = self.token_bytes(t);
        self.ident_is_value_call_bytes(&bytes)
    }

    fn ident_is_value_call_bytes(&self, bytes: &[u8]) -> bool {
        if !self.is_top_level {
            for v in self.locals.iter().rev() {
                if self.name_bytes(v.name) == bytes {
                    return true;
                }
            }
            if !self.lambda_stack.is_empty() {
                // find_outer と同じ探索をバイト列で行う
                let n = self.lambda_stack.len();
                for li in (0..n).rev() {
                    let level = &self.lambda_stack[li];
                    for v in level.outer_locals.iter().rev() {
                        if self.name_bytes(v.name) == bytes {
                            return true;
                        }
                    }
                    if li > 0 {
                        let mut cur = self.lambda_stack[li - 1].captures;
                        while let Some(cid) = cur {
                            let c = &self.captures[cid as usize];
                            if self.name_bytes(c.name) == bytes {
                                return true;
                            }
                            cur = c.next;
                        }
                    }
                }
            }
        }
        for v in self.globals.iter().rev() {
            if self.name_bytes(v.name) == bytes {
                return true;
            }
        }
        false
    }

    pub fn create_ident_node(&mut self, tok: TokenId) -> Result<NodeId, Flow> {
        let name = self.name_from_token(tok);
        if !self.is_top_level {
            if let Some(lvar) = self.find_lvar(name) {
                let id = self.new_node(NodeData::LVar {
                    name,
                    offset: lvar.offset,
                });
                self.nodes[id as usize].tok = Some(tok);
                return Ok(id);
            }
            if !self.lambda_stack.is_empty() {
                if let Some(cap) = self.resolve_capture(tok)? {
                    let env_offset = self.captures[cap as usize].env_offset;
                    let id = self.new_node(NodeData::CapVar {
                        name,
                        offset: env_offset,
                    });
                    self.nodes[id as usize].tok = Some(tok);
                    return Ok(id);
                }
            }
        }
        if self.find_gvar(name).is_none() {
            if self.is_func_name(name) {
                let id = self.new_node(NodeData::FuncRef { name, func: None });
                self.nodes[id as usize].tok = Some(tok);
                return Ok(id);
            }
            if !self.is_top_level {
                self.register_lvar(tok);
                let offset = self.locals.last().unwrap().offset;
                let id = self.new_node(NodeData::LVar { name, offset });
                self.nodes[id as usize].tok = Some(tok);
                return Ok(id);
            }
            self.register_gvar(tok);
        }
        let id = self.new_node(NodeData::GVar { name });
        self.nodes[id as usize].tok = Some(tok);
        Ok(id)
    }

    // 代入先として使われた関数参照ノードを、新しい変数への束縛に作り替える
    pub fn bind_target_node(&mut self, node: NodeId) -> NodeId {
        let (is_funcref, tok) = match &self.nodes[node as usize].data {
            NodeData::FuncRef { .. } => (true, self.nodes[node as usize].tok),
            _ => (false, None),
        };
        if !is_funcref {
            return node;
        }
        let tok = match tok {
            Some(t) => t,
            None => return node,
        };
        let name = self.name_from_token(tok);
        if self.is_top_level {
            self.register_gvar(tok);
            let id = self.new_node(NodeData::GVar { name });
            self.nodes[id as usize].tok = Some(tok);
            return id;
        }
        self.register_lvar(tok);
        let offset = self.locals.last().unwrap().offset;
        let id = self.new_node(NodeData::LVar { name, offset });
        self.nodes[id as usize].tok = Some(tok);
        id
    }

    pub fn create_number_node(&mut self, num: i64) -> NodeId {
        self.new_node(NodeData::Num { val: num })
    }

    pub fn create_binary_node(&mut self, op: crate::BinOp, left: NodeId, right: NodeId) -> NodeId {
        self.new_node(NodeData::Binop {
            op,
            left,
            right: Some(right),
            val: 0,
        })
    }

    // 文字列リテラルを文字コードの配列リテラルへ（char の符号つきで読む）
    pub fn char_array(&mut self) -> NodeId {
        let str_tok = self.cur_tok_id();
        let tok = self.tokens[str_tok as usize].clone();
        self.advance();
        let decoded = tok.str_buf.clone().unwrap_or_default();
        let mut head: Option<NodeId> = None;
        let mut tail: Option<NodeId> = None;
        for &b in &decoded {
            let n = self.create_number_node((b as i8) as i64);
            match tail {
                Some(t) => self.nodes[t as usize].next = Some(n),
                None => head = Some(n),
            }
            tail = Some(n);
        }
        self.register_array(decoded.len() as i32);
        let offset = self.locals.last().unwrap().offset;
        let id = self.new_node(NodeData::Array {
            body: head,
            array_len: decoded.len() as i32,
            offset,
        });
        self.nodes[id as usize].tok = Some(str_tok);
        id
    }

    // ---- リカバリ（LSP キャプチャ有効時） ----
    // 失敗したら状態を巻き戻し、Ok(None) を返す。呼び出し側が境界まで
    // 読み飛ばす。キャプチャ無効時は素通し（エラーは exit する）
    pub fn recover_point<T>(
        &mut self,
        f: impl FnOnce(&mut Self) -> Result<T, Flow>,
    ) -> Result<Option<T>, Flow> {
        if !self.sink.capture {
            return f(self).map(Some);
        }
        let saved_locals = self.locals.len();
        let saved_globals = self.globals.len();
        let saved_lambda = self.lambda_stack.len();
        let saved_depth = self.expr_depth;
        let saved_armed = self.sink.recover_armed;
        self.sink.recover_armed = true;
        match f(self) {
            Ok(v) => {
                self.sink.recover_armed = saved_armed;
                Ok(Some(v))
            }
            Err(Flow::Recover) => {
                self.sink.recover_armed = saved_armed;
                self.locals.truncate(saved_locals);
                self.globals.truncate(saved_globals);
                self.lambda_stack.truncate(saved_lambda);
                self.expr_depth = saved_depth;
                Ok(None)
            }
            Err(e) => {
                self.sink.recover_armed = saved_armed;
                Err(e)
            }
        }
    }

    pub fn skip_to_stmt_boundary(&mut self, consume_close: bool) {
        let mut depth = 0i32;
        while !self.at_eof() {
            let t = self.tok().clone();
            if tok_is_char(self, &t, b'{') {
                depth += 1;
                self.advance();
                continue;
            }
            if tok_is_char(self, &t, b'}') {
                if depth == 0 {
                    if consume_close {
                        self.advance();
                    }
                    return;
                }
                depth -= 1;
                self.advance();
                if depth == 0 {
                    return;
                }
                continue;
            }
            if depth == 0 && tok_is_char(self, &t, b';') {
                self.advance();
                return;
            }
            self.advance();
        }
    }

    pub fn expect_stmt_end(&mut self, subject: &str) -> Result<(), Flow> {
        if self.consume(";") {
            return Ok(());
        }
        let t = self.tok().clone();
        if t.kind == TokenKind::Eof || tok_is_char(self, &t, b'}') {
            return Ok(());
        }
        if let Some(prev) = t.prev {
            let prev_line = self.tokens[prev as usize].line;
            if t.line > prev_line {
                return Ok(());
            }
        }
        let tid = self.cur_tok_id();
        let actual = self.tt(tid);
        let mut m = Vec::new();
        m.extend_from_slice(subject.as_bytes());
        m.extend_from_slice("の終わりには`;`か改行が必要ですが、".as_bytes());
        m.extend_from_slice(&actual);
        m.extend_from_slice("が見つかりました".as_bytes());
        self.err_tok(tid, m)
    }
}

// ---- 文・関数・クラス・プログラム（parse.c 後半の移植） ----
use crate::{BinOp, Field, FunctionId, LambdaCtx};

impl Cx {
    fn stmt(&mut self) -> Result<NodeId, Flow> {
        let start = self.cur_tok_id();
        // 空文（余分な `;`）は読み飛ばす
        let mut skipped_semi = false;
        while self.consume(";") {
            skipped_semi = true;
        }
        if skipped_semi && !self.is_top_level {
            let t = self.tok().clone();
            if tok_is_char(self, &t, b'}') {
                let id = self.new_node(NodeData::Block { body: None });
                self.nodes[id as usize].tok = Some(start);
                return Ok(id);
            }
        }
        if self.consume("{") {
            return self.stmt_block();
        } else if self.consume_keyword(Keyword::Return) {
            return self.stmt_return(start);
        } else if self.consume_keyword(Keyword::If) {
            return self.stmt_if(start);
        } else if self.consume_keyword(Keyword::While) {
            return self.stmt_while(start);
        } else if self.consume_keyword(Keyword::For) {
            return self.stmt_for(start);
        }
        // `!` の直後が文の終わりなら break の省略形
        {
            let t = self.tok().clone();
            if tok_is_char(self, &t, b'!') {
                let n = self.peek_tok(1).clone();
                let is_end = n.kind == TokenKind::Eof
                    || tok_is_char(self, &n, b';')
                    || tok_is_char(self, &n, b'}')
                    || n.line > t.line;
                if is_end {
                    self.consume("!");
                    self.consume(";");
                    let id = self.new_node(NodeData::Break);
                    self.nodes[id as usize].tok = Some(start);
                    return Ok(id);
                }
            }
        }
        if self.consume("^") {
            let id = self.new_node(NodeData::Continue);
            self.nodes[id as usize].tok = Some(start);
            self.expect_stmt_end("文")?;
            return Ok(id);
        } else if self.consume("@") {
            let t = self.tok().clone();
            if tok_is_char(self, &t, b'{') {
                // `@ {}` は while true {} の省略形
                let cond = self.create_number_node(1);
                self.nodes[cond as usize].tok = Some(start);
                let then = self.stmt_branch()?;
                let id = self.new_node(NodeData::While {
                    cond,
                    then,
                });
                self.nodes[id as usize].tok = Some(start);
                return Ok(id);
            }
            let count = self.expr()?;
            return self.make_for_count(count, start);
        } else if self.tok().kind == TokenKind::Ident {
            let n = self.peek_tok(1).clone();
            if tok_is_char(self, &n, b'@') {
                let var = self.consume_ident().unwrap();
                self.consume("@");
                let init = self.expr()?;
                return self.make_for_iter(var, init, start);
            }
        }
        if self.consume("<<") || self.consume("<") {
            // 出力の省略形: << 式 / < 式（print と同じ）
            let name = self.intern_str("print");
            let args = self.expr()?;
            let id = self.new_node(NodeData::Funcall {
                name,
                args: Some(args),
                klass: None,
                func: None,
            });
            self.nodes[id as usize].tok = Some(start);
            self.expect_stmt_end("文")?;
            return Ok(id);
        } else if self.consume(">>") {
            // 入力の省略形: >> 対象
            let target_expr = self.expr()?;
            let target = self.bind_target_node(target_expr);
            self.expect_stmt_end("文")?;
            let input = self.new_node(NodeData::Input {
                args: None,
                mode: 0,
            });
            self.nodes[input as usize].tok = Some(start);
            let ttok = self.nodes[target as usize].tok;
            let id = self.new_node(NodeData::Assign {
                left: target,
                right: input,
                anno: None,
            });
            self.nodes[id as usize].tok = ttok;
            return Ok(id);
        } else if self.consume_keyword(Keyword::Put) {
            let args = self.parse_put_args()?;
            let id = self.new_node(NodeData::Put { args: Some(args) });
            self.nodes[id as usize].tok = Some(start);
            self.expect_stmt_end("文")?;
            return Ok(id);
        } else if self.consume_keyword(Keyword::Yn) {
            let name = self.intern_str("print_yn");
            let args = self.expr()?;
            let id = self.new_node(NodeData::Funcall {
                name,
                args: Some(args),
                klass: None,
                func: None,
            });
            self.nodes[id as usize].tok = Some(start);
            self.expect_stmt_end("文")?;
            return Ok(id);
        }
        // 入力修飾子: a- = in[n]
        {
            let t = self.tok().clone();
            if t.kind == TokenKind::Ident {
                let n1 = self.peek_tok(1).clone();
                let n2 = self.peek_tok(2).clone();
                if tok_is_char(self, &n1, b'-') && tok_is_char(self, &n2, b'=') {
                    let var = self.consume_ident().unwrap();
                    self.consume("-");
                    self.consume("=");
                    let cur = self.tok().clone();
                    if cur.kind != TokenKind::Keyword || cur.keyword != Keyword::In {
                        let tid = self.cur_tok_id();
                        let actual = self.tt(tid);
                        let mut m = Vec::new();
                        m.extend_from_slice("`in`が必要ですが、".as_bytes());
                        m.extend_from_slice(&actual);
                        m.extend_from_slice("が見つかりました".as_bytes());
                        self.err_tok(tid, m)?;
                    }
                    self.advance();
                    self.expect("[")?;
                    let n = self.expr()?;
                    self.expect("]")?;
                    self.expect_stmt_end("文")?;
                    let name = self.intern_str("read_ints_dec");
                    let call = self.new_node(NodeData::Funcall {
                        name,
                        args: Some(n),
                        klass: None,
                        func: None,
                    });
                    let target_node = self.create_ident_node(var)?;
                    let target = self.bind_target_node(target_node);
                    let id = self.new_node(NodeData::Assign {
                        left: target,
                        right: call,
                        anno: None,
                    });
                    self.nodes[id as usize].tok = Some(var);
                    return Ok(id);
                }
            }
        }
        if self.consume_keyword(Keyword::Break) {
            let id = self.new_node(NodeData::Break);
            self.nodes[id as usize].tok = Some(start);
            self.expect_stmt_end("文")?;
            return Ok(id);
        } else if self.consume_keyword(Keyword::Continue) {
            let id = self.new_node(NodeData::Continue);
            self.nodes[id as usize].tok = Some(start);
            self.expect_stmt_end("文")?;
            return Ok(id);
        } else {
            let node = self.expr()?;
            self.expect_stmt_end("文")?;
            return Ok(node);
        }
    }

    // 分岐位置専用の文読み取り。区切りだけがあって本体が空なら空ブロック
    fn stmt_branch(&mut self) -> Result<NodeId, Flow> {
        let mut t = self.tpos;
        let mut skipped_semi = false;
        while tok_is_char(self, &self.tokens[t], b';') {
            t += 1;
            skipped_semi = true;
        }
        if skipped_semi {
            let tk = &self.tokens[t];
            let at_boundary = tk.kind == TokenKind::Eof
                || tok_is_char(self, tk, b'}')
                || (tk.kind == TokenKind::Keyword
                    && (tk.keyword == Keyword::Else || tk.keyword == Keyword::Elif));
            if at_boundary {
                self.tpos = t;
                let id = self.new_node(NodeData::Block { body: None });
                self.nodes[id as usize].tok = Some(t as TokenId);
                return Ok(id);
            }
        }
        self.stmt()
    }

    fn parse_stmt_list(&mut self) -> Result<Option<NodeId>, Flow> {
        let mut head: Option<NodeId> = None;
        let mut tail: Option<NodeId> = None;
        while !self.consume("}") {
            let s = if self.sink.capture {
                match self.recover_point(|cx| cx.stmt())? {
                    Some(s) => s,
                    None => {
                        if self.at_eof() {
                            break;
                        }
                        self.skip_to_stmt_boundary(false);
                        continue;
                    }
                }
            } else {
                self.stmt()?
            };
            match tail {
                Some(t) => self.nodes[t as usize].next = Some(s),
                None => head = Some(s),
            }
            tail = Some(s);
        }
        Ok(head)
    }

    fn stmt_block(&mut self) -> Result<NodeId, Flow> {
        let body = self.parse_stmt_list()?;
        Ok(self.new_node(NodeData::Block { body }))
    }

    fn stmt_return(&mut self, kw: TokenId) -> Result<NodeId, Flow> {
        let left = self.expr()?;
        let id = self.new_node(NodeData::Return { left });
        self.nodes[id as usize].tok = Some(kw);
        self.expect_stmt_end("文")?;
        Ok(id)
    }

    fn stmt_if(&mut self, kw: TokenId) -> Result<NodeId, Flow> {
        self.expect("(")?;
        self.expr_depth += 1;
        let cond = self.expr()?;
        self.expr_depth -= 1;
        self.expect(")")?;
        let then = self.stmt_branch()?;
        let mut els = None;
        if self.consume_keyword(Keyword::Else) {
            els = Some(self.stmt_branch()?);
        } else {
            let t = self.tok().clone();
            if t.kind == TokenKind::Keyword && t.keyword == Keyword::Elif {
                let elif_tok = self.cur_tok_id();
                self.advance();
                els = Some(self.stmt_if(elif_tok)?);
            }
        }
        let id = self.new_node(NodeData::If { cond, then, els });
        self.nodes[id as usize].tok = Some(kw);
        Ok(id)
    }

    fn stmt_while(&mut self, kw: TokenId) -> Result<NodeId, Flow> {
        self.expect("(")?;
        self.expr_depth += 1;
        let cond = self.expr()?;
        self.expr_depth -= 1;
        self.expect(")")?;
        let then = self.stmt_branch()?;
        let id = self.new_node(NodeData::While { cond, then });
        self.nodes[id as usize].tok = Some(kw);
        Ok(id)
    }

    fn make_for_iter(&mut self, var: TokenId, init: NodeId, kw: TokenId) -> Result<NodeId, Flow> {
        self.register_any();
        let iter_offset = self.locals.last().unwrap().offset;
        let var_node = self.create_ident_node(var)?;
        let var_bound = self.bind_target_node(var_node);
        let then = self.stmt_branch()?;
        let mut els = None;
        let mut flag: i64 = 0;
        if self.consume_keyword(Keyword::Else) {
            self.register_any();
            flag = self.locals.last().unwrap().offset as i64;
            els = Some(self.stmt_branch()?);
        }
        let id = self.new_node(NodeData::For {
            init,
            var: var_bound,
            then,
            els,
            iter_offset,
            flag,
        });
        self.nodes[id as usize].tok = Some(kw);
        Ok(id)
    }

    fn make_for_count(&mut self, count: NodeId, kw: TokenId) -> Result<NodeId, Flow> {
        // 束縛なしの回数リピートは Range(0, n) の要素走査として扱う
        let name = self.intern_str("__count_range");
        let range_call = self.new_node(NodeData::Funcall {
            name,
            args: Some(count),
            klass: None,
            func: None,
        });
        self.register_any();
        let iter_offset = self.locals.last().unwrap().offset;
        self.register_any();
        let var_offset = self.locals.last().unwrap().offset;
        let var = self.new_node(NodeData::LVar {
            name: Name::default(),
            offset: var_offset,
        });
        let then = self.stmt_branch()?;
        let mut els = None;
        let mut flag: i64 = 0;
        if self.consume_keyword(Keyword::Else) {
            self.register_any();
            flag = self.locals.last().unwrap().offset as i64;
            els = Some(self.stmt_branch()?);
        }
        let id = self.new_node(NodeData::For {
            init: range_call,
            var,
            then,
            els,
            iter_offset,
            flag,
        });
        self.nodes[id as usize].tok = Some(kw);
        Ok(id)
    }

    fn stmt_for(&mut self, kw: TokenId) -> Result<NodeId, Flow> {
        let t = self.tok().clone();
        if t.kind == TokenKind::Ident {
            let n = self.peek_tok(1).clone();
            if n.kind == TokenKind::Keyword && n.keyword == Keyword::In {
                let var = self.consume_ident().unwrap();
                self.consume_keyword(Keyword::In);
                let init = self.expr()?;
                return self.make_for_iter(var, init, kw);
            }
        }
        let count = self.expr()?;
        self.make_for_count(count, kw)
    }

    // 「(引数, ...)」の中身を読む。開き括弧は消費済み。引数の数を返す
    fn parse_params(
        &mut self,
        param_types: &mut [Option<TypeId>; 6],
    ) -> Result<i32, Flow> {
        let mut nparams: i32 = 0;
        if !self.consume(")") {
            loop {
                let tok = match self.consume_ident() {
                    Some(t) => t,
                    None => {
                        // C では NULL 参照で落ちる場所。診断を出して止める
                        let tid = self.cur_tok_id();
                        let actual = self.tt(tid);
                        let mut m = Vec::new();
                        m.extend_from_slice("引数名が必要ですが、".as_bytes());
                        m.extend_from_slice(&actual);
                        m.extend_from_slice("が見つかりました".as_bytes());
                        self.err_tok(tid, m)?;
                        return Ok(nparams);
                    }
                };
                self.register_lvar(tok);
                if self.consume(":") {
                    let pt = self.parse_type()?;
                    if nparams < 6 {
                        param_types[nparams as usize] = Some(pt);
                    }
                }
                nparams += 1;
                if !self.consume(",") {
                    break;
                }
            }
            self.expect(")")?;
        }
        Ok(nparams)
    }

    fn function(&mut self, klass: Option<ClassId>) -> Result<FunctionId, Flow> {
        self.locals.clear();
        self.consume_keyword(Keyword::Def);
        let tok = match self.consume_ident() {
            Some(t) => t,
            None => {
                let tid = self.cur_tok_id();
                let actual = self.tt(tid);
                let mut m = Vec::new();
                m.extend_from_slice("関数名が必要ですが、".as_bytes());
                m.extend_from_slice(&actual);
                m.extend_from_slice("が見つかりました".as_bytes());
                self.err_tok(tid, m)?;
                // ここには来ない（err_tok は exit / Err を返す）が、
                // Ok の場合は先へ進めないのでダミーを返す
                return Ok(self.new_func(Function::new()));
            }
        };
        let name = self.name_from_token(tok);
        self.expect("(")?;
        let mut param_types: [Option<TypeId>; 6] = [None; 6];
        let nparams = self.parse_params(&mut param_types)?;
        if nparams > 6 {
            let m = format!("引数は6個までですが、{}個が指定されています", nparams)
                .into_bytes();
            self.err_tok(tok, m)?;
        }
        let mut return_type = None;
        if self.consume("->") {
            return_type = Some(self.parse_type()?);
        }
        self.expect("{")?;
        let body = self.parse_stmt_list()?;
        let mut func = Function::new();
        func.name = name;
        func.body = body;
        func.nparams = nparams;
        func.return_type = return_type;
        func.param_types = param_types;
        func.klass = klass;
        if let Some(k) = klass {
            if nparams == 0 {
                let m = "メソッドには最初の引数`self`が必要です".as_bytes().to_vec();
                self.err_tok(tok, m)?;
            }
            if nparams > 0 {
                func.param_types[0] = Some(self.classes[k as usize].ty);
            }
        }
        let mut max_offset = 0;
        for v in &self.locals {
            let offset = v.offset + 8 * v.array_len;
            if max_offset < offset {
                max_offset = offset;
            }
        }
        func.stack_size = (max_offset + 15) / 16 * 16;
        Ok(self.new_func(func))
    }

    // 矢印形ラムダの先読み判定
    pub fn lambda_arrow_ahead(&self) -> bool {
        let is_arrow = |t: &Token| tok_is_symbol(self, t, "->");
        let t = self.tok().clone();
        if t.kind == TokenKind::Ident {
            return is_arrow(self.peek_tok(1));
        }
        if tok_is_char(self, &t, b'(') {
            let mut i = self.tpos + 1;
            let at = |cx: &Self, i: usize| -> Token {
                if i > cx.trun_eof {
                    cx.tokens[cx.trun_eof].clone()
                } else {
                    cx.tokens[i].clone()
                }
            };
            let t1 = at(self, i);
            if tok_is_char(self, &t1, b')') {
                return is_arrow(&at(self, i + 1));
            }
            loop {
                let ti = at(self, i);
                if ti.kind != TokenKind::Ident {
                    return false;
                }
                i += 1;
                let tn = at(self, i);
                if tok_is_char(self, &tn, b',') {
                    i += 1;
                    continue;
                }
                if tok_is_char(self, &tn, b')') {
                    return is_arrow(&at(self, i + 1));
                }
                return false;
            }
        }
        false
    }

    pub fn parse_lambda(&mut self, arrow: bool, kw: TokenId) -> Result<NodeId, Flow> {
        let ctx = LambdaCtx {
            captures: None,
            ncaptures: 0,
            outer_locals: self.locals.clone(),
        };
        self.lambda_stack.push(ctx);
        let saved_locals = std::mem::take(&mut self.locals);
        let saved_top = self.is_top_level;
        self.is_top_level = false;

        let mut param_types: [Option<TypeId>; 6] = [None; 6];
        let mut nparams: i32 = 0;
        let mut return_type = None;
        let body: Option<NodeId>;
        if arrow {
            if self.consume("(") {
                if !self.consume(")") {
                    loop {
                        let p = match self.consume_ident() {
                            Some(p) => p,
                            None => {
                                let tid = self.cur_tok_id();
                                let actual = self.tt(tid);
                                let mut m = Vec::new();
                                m.extend_from_slice("引数名が必要ですが、".as_bytes());
                                m.extend_from_slice(&actual);
                                m.extend_from_slice("が見つかりました".as_bytes());
                                self.err_tok(tid, m)?;
                                break;
                            }
                        };
                        self.register_lvar(p);
                        nparams += 1;
                        if !self.consume(",") {
                            break;
                        }
                    }
                    self.expect(")")?;
                }
            } else {
                let p = self.consume_ident().unwrap_or(self.cur_tok_id());
                self.register_lvar(p);
                nparams = 1;
            }
            if nparams > 6 {
                let m = format!("引数は6個までですが、{}個が指定されています", nparams)
                    .into_bytes();
                self.err_tok(kw, m)?;
            }
            self.expect("->")?;
            let ret_expr = self.expr()?;
            let ret = self.new_node(NodeData::Return { left: ret_expr });
            body = Some(ret);
        } else {
            self.advance(); // fn キーワードを消費
            self.expect("(")?;
            nparams = self.parse_params(&mut param_types)?;
            if nparams > 6 {
                let m = format!("引数は6個までですが、{}個が指定されています", nparams)
                    .into_bytes();
                self.err_tok(kw, m)?;
            }
            if self.consume("->") {
                return_type = Some(self.parse_type()?);
            }
            self.expect("{")?;
            body = self.parse_stmt_list()?;
        }

        let name_str = format!("__lambda_{}", self.lambda_counter);
        self.lambda_counter += 1;
        let name = self.intern_str(&name_str);
        let mut func = Function::new();
        func.name = name;
        func.body = body;
        func.nparams = nparams;
        func.return_type = return_type;
        func.param_types = param_types;
        func.is_lambda = true;
        let ctx = self.lambda_stack.pop().unwrap();
        func.captures = ctx.captures;
        let mut max_offset = 0;
        for v in &self.locals {
            let off = v.offset + 8 * v.array_len;
            if max_offset < off {
                max_offset = off;
            }
        }
        func.env_offset = max_offset + 8;
        func.stack_size = (func.env_offset + 15) / 16 * 16;
        let func_id = self.new_func(func);
        self.funcs[func_id as usize].next = None;
        self.pending_lambdas.push(func_id);

        self.locals = saved_locals;
        self.is_top_level = saved_top;

        let id = self.new_node(NodeData::Lambda {
            func: Some(func_id),
            name,
            captures: ctx.captures,
        });
        self.nodes[id as usize].tok = Some(kw);
        Ok(id)
    }

    pub fn register_class(&mut self, name: Name) -> ClassId {
        if let Some(id) = self.find_class(name) {
            return id;
        }
        let id = self.classes.len() as ClassId;
        let ty = if self.name_eq_lit(name, "int") {
            crate::TY_INT_ID
        } else if self.name_eq_lit(name, "array") {
            self.array_type(crate::TY_UNKNOWN_ID)
        } else {
            let mut t = crate::Type::new(crate::TypeKind::Class);
            t.klass = Some(id);
            self.new_type(t)
        };
        let class = Class {
            next: self.classes_head,
            name,
            fields: None,
            ty,
            tparams: [None; MAX_TPARAMS],
            tparam_count: 0,
            origin: None,
            targs: [None; MAX_TPARAMS],
            targ_count: 0,
        };
        self.classes.push(class);
        self.classes_head = Some(id);
        id
    }

    fn class_decl(&mut self, seq: &mut Vec<FunctionId>) -> Result<(), Flow> {
        self.consume_keyword(Keyword::Class);
        let tok = match self.consume_ident() {
            Some(t) => t,
            None => {
                let tid = self.cur_tok_id();
                let actual = self.tt(tid);
                let mut m = Vec::new();
                m.extend_from_slice("クラス名が必要ですが、".as_bytes());
                m.extend_from_slice(&actual);
                m.extend_from_slice("が見つかりました".as_bytes());
                self.err_tok(tid, m)?;
                return Ok(());
            }
        };
        let name = self.name_from_token(tok);
        let klass = self.register_class(name);
        if self.consume("<") {
            let mut count: i32 = 0;
            let mut extra: Option<TokenId> = None;
            loop {
                let tparam = match self.consume_ident() {
                    Some(t) => t,
                    None => {
                        let tid = self.cur_tok_id();
                        let actual = self.tt(tid);
                        let mut m = Vec::new();
                        m.extend_from_slice("型パラメータ名が必要ですが、".as_bytes());
                        m.extend_from_slice(&actual);
                        m.extend_from_slice("が見つかりました".as_bytes());
                        self.err_tok(tid, m)?;
                        return Ok(());
                    }
                };
                let tp_name = self.name_from_token(tparam);
                if (count as usize) < MAX_TPARAMS {
                    self.classes[klass as usize].tparams[count as usize] = Some(tp_name);
                } else if extra.is_none() {
                    extra = Some(tparam);
                }
                count += 1;
                if !self.consume(",") {
                    break;
                }
            }
            self.classes[klass as usize].tparam_count =
                if count < MAX_TPARAMS as i32 { count } else { MAX_TPARAMS as i32 };
            if count > MAX_TPARAMS as i32 {
                let m = format!(
                    "型パラメータは{}個までですが、{}個が指定されています",
                    MAX_TPARAMS, count
                )
                .into_bytes();
                self.err_tok(extra.unwrap(), m)?;
            }
            self.expect(">")?;
        }
        self.expect("{")?;
        let mut offset: i32 = 0;
        self.parsing_klass = Some(klass);
        while !self.consume("}") {
            if self.sink.capture {
                match self.recover_point(|cx| cx.class_member(&mut *seq, klass, &mut offset))? {
                    Some(()) => {}
                    None => {
                        if self.at_eof() {
                            break;
                        }
                        self.skip_to_stmt_boundary(false);
                    }
                }
            } else {
                self.class_member(seq, klass, &mut offset)?;
            }
        }
        self.parsing_klass = None;
        Ok(())
    }

    fn class_member(
        &mut self,
        seq: &mut Vec<FunctionId>,
        klass: ClassId,
        offset: &mut i32,
    ) -> Result<(), Flow> {
        while self.consume(";") {}
        let t = self.tok().clone();
        if tok_is_char(self, &t, b'}') {
            return Ok(());
        }
        if t.kind == TokenKind::Keyword && t.keyword == Keyword::Def {
            let f = self.function(Some(klass))?;
            seq.push(f);
            return Ok(());
        }
        let field_tok = match self.consume_ident() {
            Some(t) => t,
            None => {
                let tid = self.cur_tok_id();
                let actual = self.tt(tid);
                let mut m = Vec::new();
                m.extend_from_slice("フィールド名が必要ですが、".as_bytes());
                m.extend_from_slice(&actual);
                m.extend_from_slice("が見つかりました".as_bytes());
                self.err_tok(tid, m)?;
                return Ok(());
            }
        };
        let fname = self.name_from_token(field_tok);
        // 重複チェック
        {
            let mut cur = self.classes[klass as usize].fields;
            while let Some(fid) = cur {
                let f = &self.fields[fid as usize];
                if self.name_eq(f.name, fname) {
                    let mut m = Vec::new();
                    m.extend_from_slice("フィールド`".as_bytes());
                    m.extend_from_slice(&self.name_bytes(fname).to_vec());
                    m.extend_from_slice("`は既に定義されています".as_bytes());
                    self.err_tok(field_tok, m)?;
                    return Ok(());
                }
                cur = f.next;
            }
        }
        self.expect(":")?;
        let fty = self.parse_type()?;
        let head = self.classes[klass as usize].fields;
        let field = Field {
            next: head,
            name: fname,
            ty: fty,
            offset: *offset,
        };
        *offset += 8;
        let fid = self.new_field(field);
        self.classes[klass as usize].fields = Some(fid);
        self.expect_stmt_end("フィールド")?;
        Ok(())
    }

    fn parse_top_item(
        &mut self,
        seq: &mut Vec<FunctionId>,
        stmt_seq: &mut Vec<NodeId>,
    ) -> Result<(), Flow> {
        let t = self.tok().clone();
        if t.kind == TokenKind::Keyword && t.keyword == Keyword::Class {
            self.class_decl(seq)?;
        } else if (t.kind == TokenKind::Keyword && t.keyword == Keyword::Def) || self.consume(":") {
            let f = self.function(None)?;
            seq.push(f);
        } else {
            self.is_top_level = true;
            let s = self.stmt()?;
            stmt_seq.push(s);
            self.is_top_level = false;
        }
        Ok(())
    }

    pub fn program(&mut self) -> Result<Option<FunctionId>, Flow> {
        self.top_max_offset = 0;
        let run_start = self.tpos;
        let eof = self.trun_eof;
        // 事前走査1: クラス名と型パラメータを先に登録する
        {
            let mut i = run_start;
            while i < eof {
                let t = self.tokens[i].clone();
                if t.kind == TokenKind::Keyword && t.keyword == Keyword::Class {
                    let nt = self.tokens[i + 1].clone();
                    if nt.kind == TokenKind::Ident {
                        let cname = self.name_from_token((i + 1) as TokenId);
                        let klass = self.register_class(cname);
                        let mut u = i + 2;
                        if u <= eof && tok_is_symbol(self, &self.tokens[u], "<") {
                            let mut count: i32 = 0;
                            u += 1;
                            while u <= eof && self.tokens[u].kind == TokenKind::Ident {
                                if (count as usize) < MAX_TPARAMS {
                                    let pn = self.name_from_token(u as TokenId);
                                    self.classes[klass as usize].tparams[count as usize] =
                                        Some(pn);
                                }
                                count += 1;
                                u += 1;
                                if u > eof || !tok_is_symbol(self, &self.tokens[u], ",") {
                                    break;
                                }
                                u += 1;
                            }
                            // 個数は丸めずに記録する
                            self.classes[klass as usize].tparam_count = count;
                        }
                    }
                }
                i += 1;
            }
        }
        // 事前走査2: トップレベル関数の名前を先に登録する
        {
            let mut depth = 0i32;
            let mut i = run_start;
            while i < eof {
                let t = self.tokens[i].clone();
                if tok_is_symbol(self, &t, "{") {
                    depth += 1;
                    i += 1;
                    continue;
                }
                if tok_is_symbol(self, &t, "}") {
                    depth -= 1;
                    i += 1;
                    continue;
                }
                if depth == 0 {
                    if t.kind == TokenKind::Keyword && t.keyword == Keyword::Def {
                        let nt = self.tokens[i + 1].clone();
                        if nt.kind == TokenKind::Ident {
                            let fname = self.name_from_token((i + 1) as TokenId);
                            self.register_func_name(fname);
                        }
                    } else if tok_is_symbol(self, &t, ":") {
                        let mut u = i + 1;
                        if u <= eof {
                            let ut = self.tokens[u].clone();
                            if ut.kind == TokenKind::Keyword && ut.keyword == Keyword::Def {
                                u += 1;
                            }
                            if u <= eof {
                                let ut = self.tokens[u].clone();
                                if ut.kind == TokenKind::Ident
                                    && u + 1 <= eof
                                    && tok_is_symbol(self, &self.tokens[u + 1], "(")
                                {
                                    let fname = self.name_from_token(u as TokenId);
                                    self.register_func_name(fname);
                                }
                            }
                        }
                    }
                }
                i += 1;
            }
        }
        self.tpos = run_start;

        let mut seq: Vec<FunctionId> = Vec::new();
        let mut stmt_seq: Vec<NodeId> = Vec::new();
        while !self.at_eof() {
            if self.sink.capture {
                match self.recover_point(|cx| cx.parse_top_item(&mut seq, &mut stmt_seq))? {
                    Some(()) => {}
                    None => {
                        self.is_top_level = false;
                        if self.at_eof() {
                            break;
                        }
                        self.skip_to_stmt_boundary(true);
                    }
                }
            } else {
                self.parse_top_item(&mut seq, &mut stmt_seq)?;
            }
        }
        // 解析中に作られたラムダを関数チェーンの末尾へ足す
        // （pending は末尾が最新の積み方なので、pop 順が C の
        // リスト先頭からの取り出し順と一致する）
        while let Some(f) = self.pending_lambdas.pop() {
            seq.push(f);
        }
        // next 鎖を張る
        for i in 0..seq.len() {
            let next = if i + 1 < seq.len() {
                Some(seq[i + 1])
            } else {
                None
            };
            self.funcs[seq[i] as usize].next = next;
        }
        let head = seq.first().copied();
        if stmt_seq.is_empty() {
            return Ok(head);
        }
        // トップレベル文を連結する
        for i in 0..stmt_seq.len() {
            let next = if i + 1 < stmt_seq.len() {
                Some(stmt_seq[i + 1])
            } else {
                None
            };
            self.nodes[stmt_seq[i] as usize].next = next;
        }
        let zero = self.create_number_node(0);
        let ret = self.new_node(NodeData::Return { left: zero });
        self.nodes[*stmt_seq.last().unwrap() as usize].next = Some(ret);
        let block = self.new_node(NodeData::Block {
            body: Some(stmt_seq[0]),
        });
        let mut init = Function::new();
        init.body = Some(block);
        init.stack_size = (self.top_max_offset + 15) / 16 * 16;
        let mut has_main = false;
        for &f in &seq {
            let func = &self.funcs[f as usize];
            if self.name_eq_lit(func.name, "main") {
                has_main = true;
            }
        }
        if has_main {
            init.name = self.intern_str("__spell_init");
        } else {
            init.name = self.intern_str("main");
        }
        init.next = head;
        let init_id = self.new_func(init);
        Ok(Some(init_id))
    }
}
