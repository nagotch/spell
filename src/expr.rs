// 式と型の構文解析（expr.c の移植）
use crate::parse::{tok_is_char, tok_is_symbol};
use crate::{
    BinOp, Cx, Flow, Keyword, Name, NodeData, NodeId, TokenId, Type, TypeId, TypeKind, UnaryOp,
    MAX_TPARAMS,
};

impl Cx {
    // 深さ0で、次のトークンが前のトークンより後の行にあれば、式はそこで終わり
    fn at_expr_line_break(&self) -> bool {
        if self.expr_depth != 0 {
            return false;
        }
        let t = self.tok();
        match t.prev {
            Some(prev) => t.line > self.tokens[prev as usize].line,
            None => false,
        }
    }

    fn parse_args(&mut self) -> Result<Option<NodeId>, Flow> {
        self.expr_depth += 1;
        let result;
        if self.consume(")") {
            result = None;
        } else {
            let saved = self.suppress_multi;
            self.suppress_multi = true;
            let mut head: Option<NodeId> = None;
            let mut tail: Option<NodeId> = None;
            loop {
                let a = self.expr()?;
                match tail {
                    Some(t) => self.nodes[t as usize].next = Some(a),
                    None => head = Some(a),
                }
                tail = Some(a);
                if !self.consume(",") {
                    break;
                }
            }
            self.suppress_multi = saved;
            self.expect(")")?;
            result = head;
        }
        self.expr_depth -= 1;
        Ok(result)
    }

    // put 文の引数列（括弧なし、カンマ区切り）を読む
    pub fn parse_put_args(&mut self) -> Result<NodeId, Flow> {
        let saved = self.suppress_multi;
        self.suppress_multi = true;
        let mut head: Option<NodeId> = None;
        let mut tail: Option<NodeId> = None;
        loop {
            let a = self.expr()?;
            match tail {
                Some(t) => self.nodes[t as usize].next = Some(a),
                None => head = Some(a),
            }
            tail = Some(a);
            if !self.consume(",") {
                break;
            }
        }
        self.suppress_multi = saved;
        Ok(head.unwrap())
    }

    fn primary(&mut self) -> Result<NodeId, Flow> {
        {
            let t = self.tok().clone();
            if t.kind == crate::TokenKind::Keyword && t.keyword == Keyword::Fn {
                let kw = self.cur_tok_id();
                return self.parse_lambda(false, kw);
            }
            if t.kind == crate::TokenKind::Keyword
                && (t.keyword == Keyword::In
                    || t.keyword == Keyword::InStr
                    || t.keyword == Keyword::InLine)
            {
                // 入力構文: in / in[n] / in[h][w]、in_str / in_str[n]、in_line
                let kw = self.cur_tok_id();
                let mode: i64 = match t.keyword {
                    Keyword::In => 0,
                    Keyword::InStr => 1,
                    _ => 2,
                };
                self.advance();
                let mut args: Vec<NodeId> = Vec::new();
                while self.consume("[") {
                    self.expr_depth += 1;
                    let e = self.expr()?;
                    self.expr_depth -= 1;
                    self.expect("]")?;
                    args.push(e);
                }
                let nb = args.len() as i64;
                if mode == 2 && nb > 0 {
                    self.err_tok(
                        kw,
                        "`in_line`に添え字は付けられません".as_bytes().to_vec(),
                    )?;
                }
                if (mode == 0 && nb > 2) || (mode == 1 && nb > 1) {
                    let kwtext = self.token_bytes(&t);
                    let mut m = Vec::new();
                    m.push(b'`');
                    m.extend_from_slice(&kwtext);
                    m.extend_from_slice("`の添え字は".as_bytes());
                    m.extend_from_slice(if mode == 0 { b"2" } else { b"1" });
                    m.extend_from_slice("個までです".as_bytes());
                    self.err_tok(kw, m)?;
                }
                let mut head: Option<NodeId> = None;
                let mut tail: Option<NodeId> = None;
                for a in args {
                    match tail {
                        Some(x) => self.nodes[x as usize].next = Some(a),
                        None => head = Some(a),
                    }
                    tail = Some(a);
                }
                let id = self.new_node(NodeData::Input {
                    args: head,
                    mode,
                });
                self.nodes[id as usize].tok = Some(kw);
                return Ok(id);
            }
        }
        let ident_token = self.consume_ident();
        let mut node: NodeId;
        if self.tok().kind == crate::TokenKind::Str {
            let str_tok = self.cur_tok_id();
            let arr = self.char_array();
            let name = self.intern_str("str");
            node = self.new_node(NodeData::Funcall {
                name,
                args: Some(arr),
                klass: None,
                func: None,
            });
            self.nodes[node as usize].tok = Some(str_tok);
        } else if ident_token.is_none() {
            let t = self.tok().clone();
            if tok_is_char(self, &t, b'[') {
                let bracket = self.cur_tok_id();
                self.advance();
                node = self.array_literal()?;
                self.nodes[node as usize].tok = Some(bracket);
            } else if self.consume("(") {
                self.expr_depth += 1;
                node = self.expr()?;
                self.expr_depth -= 1;
                self.expect(")")?;
            } else {
                let num_tok = self.cur_tok_id();
                let num = self.expect_number()?;
                node = self.create_number_node(num);
                self.nodes[node as usize].tok = Some(num_tok);
            }
        } else if self.tok().kind == crate::TokenKind::Symbol
            && tok_is_char(self, &self.tok().clone(), b'(')
            && self.ident_is_value_call(ident_token.unwrap())
        {
            // 変数に収まった関数値の呼び出しは値経由にする
            node = self.create_ident_node(ident_token.unwrap())?;
        } else if self.consume("(") {
            let itok = ident_token.unwrap();
            let name = self.name_from_token(itok);
            let args = self.parse_args()?;
            node = self.new_node(NodeData::Funcall {
                name,
                args,
                klass: None,
                func: None,
            });
            self.nodes[node as usize].tok = Some(itok);
        } else {
            node = self.create_ident_node(ident_token.unwrap())?;
        }
        // 後置演算子
        loop {
            let postfix_tok = self.cur_tok_id();
            if self.at_expr_line_break() {
                break;
            }
            if self.consume("(") {
                let args = self.parse_args()?;
                let call = self.new_node(NodeData::VCall {
                    left: node,
                    args,
                });
                self.nodes[call as usize].tok = Some(postfix_tok);
                node = call;
                continue;
            }
            let inc = self.consume("++");
            if inc || self.consume("--") {
                let one = self.create_number_node(1);
                let an = self.new_node(NodeData::OpAssign {
                    op: if inc { BinOp::Add } else { BinOp::Sub },
                    left: node,
                    right: one,
                });
                self.nodes[an as usize].tok = Some(postfix_tok);
                node = an;
                continue;
            }
            if self.consume("[") {
                self.expr_depth += 1;
                let right = self.expr()?;
                self.expr_depth -= 1;
                self.expect("]")?;
                let nn = self.new_node(NodeData::Index {
                    left: node,
                    right,
                });
                self.nodes[nn as usize].tok = Some(postfix_tok);
                node = nn;
                continue;
            }
            if !self.consume(".") {
                break;
            }
            let member_token = match self.consume_ident() {
                Some(m) => m,
                None => {
                    let tid = self.cur_tok_id();
                    let tok = self.tok().clone();
                    match self.token_text(&tok) {
                        Some(actual) => {
                            let mut m = Vec::new();
                            m.extend_from_slice("フィールド名が必要ですが、".as_bytes());
                            m.extend_from_slice(&actual);
                            m.extend_from_slice("が指定されています".as_bytes());
                            self.err_tok(tid, m)?;
                        }
                        None => {
                            self.err_tok(
                                tid,
                                "フィールド名が必要ですが、ファイルの末尾に達しました"
                                    .as_bytes()
                                    .to_vec(),
                            )?;
                        }
                    }
                    unreachable!()
                }
            };
            let mname = self.name_from_token(member_token);
            let nn = self.new_node(NodeData::Member {
                left: node,
                name: mname,
                offset: 0,
                func: None,
                klass: None,
            });
            self.nodes[nn as usize].tok = Some(member_token);
            node = nn;
            if !self.at_expr_line_break() && self.consume("(") {
                let args = self.parse_args()?;
                let (left, name) = match &self.nodes[node as usize].data {
                    NodeData::Member { left, name, .. } => (*left, *name),
                    _ => unreachable!(),
                };
                let tok = self.nodes[node as usize].tok;
                let mc = self.new_node(NodeData::MCall {
                    left,
                    args,
                    name,
                    func: None,
                    klass: None,
                });
                self.nodes[mc as usize].tok = tok;
                // C では同じノードの kind を書き換える。ノード自体を
                // 作り直すと参照がずれるため、スロットへ直接書き込む
                self.nodes[node as usize] = self.nodes[mc as usize].clone();
                node = node;
            }
        }
        Ok(node)
    }

    fn unary(&mut self) -> Result<NodeId, Flow> {
        let op_tok = self.cur_tok_id();
        if self.consume("+") {
            return self.primary();
        } else if self.consume("-") {
            let zero = self.create_number_node(0);
            let p = self.primary()?;
            let node = self.create_binary_node(BinOp::Sub, zero, p);
            self.nodes[node as usize].tok = Some(op_tok);
            return Ok(node);
        } else if self.consume("*") {
            let left = self.unary()?;
            let node = self.new_node(NodeData::Unary {
                op: UnaryOp::Deref,
                left,
            });
            self.nodes[node as usize].tok = Some(op_tok);
            return Ok(node);
        } else if self.consume("&") {
            let left = self.unary()?;
            let node = self.new_node(NodeData::Unary {
                op: UnaryOp::Addr,
                left,
            });
            self.nodes[node as usize].tok = Some(op_tok);
            return Ok(node);
        } else if self.consume("!") {
            let left = self.unary()?;
            let node = self.new_node(NodeData::Unary {
                op: UnaryOp::Not,
                left,
            });
            self.nodes[node as usize].tok = Some(op_tok);
            return Ok(node);
        }
        self.primary()
    }

    fn mul(&mut self) -> Result<NodeId, Flow> {
        let mut node = self.unary()?;
        loop {
            let op_tok = self.cur_tok_id();
            if self.at_expr_line_break() {
                break;
            }
            if self.consume("*") {
                let r = self.unary()?;
                node = self.create_binary_node(BinOp::Mul, node, r);
            } else if self.consume("/") {
                let r = self.unary()?;
                node = self.create_binary_node(BinOp::Div, node, r);
            } else if self.consume("%") {
                let r = self.unary()?;
                node = self.create_binary_node(BinOp::Mod, node, r);
            } else {
                break;
            }
            self.nodes[node as usize].tok = Some(op_tok);
        }
        Ok(node)
    }

    fn add(&mut self) -> Result<NodeId, Flow> {
        let mut node = self.mul()?;
        loop {
            let op_tok = self.cur_tok_id();
            if self.at_expr_line_break() {
                break;
            }
            if self.consume("+") {
                let r = self.mul()?;
                node = self.create_binary_node(BinOp::Add, node, r);
            } else if self.consume("-") {
                let r = self.mul()?;
                node = self.create_binary_node(BinOp::Sub, node, r);
            } else {
                break;
            }
            self.nodes[node as usize].tok = Some(op_tok);
        }
        Ok(node)
    }

    // 関係演算子を1つ読む。> / >= は左右を入れ替えて < / <= にそろえる
    fn read_rel_op(&mut self) -> Option<(BinOp, bool)> {
        if self.at_expr_line_break() {
            return None;
        }
        if self.consume("<=") {
            Some((BinOp::Le, false))
        } else if self.consume(">=") {
            Some((BinOp::Le, true))
        } else if self.consume("<") {
            Some((BinOp::Lt, false))
        } else if self.consume(">") {
            Some((BinOp::Lt, true))
        } else {
            None
        }
    }

    fn make_cmp_chain(
        &mut self,
        first: NodeId,
        pairs: Vec<NodeId>,
    ) -> NodeId {
        if pairs.len() == 1 {
            let pair = pairs[0];
            let (op, operand, val, ptok) = match &self.nodes[pair as usize].data {
                NodeData::Binop {
                    op,
                    left,
                    val,
                    ..
                } => (*op, *left, *val, self.nodes[pair as usize].tok),
                _ => unreachable!(),
            };
            let single = if val != 0 {
                self.create_binary_node(op, operand, first)
            } else {
                self.create_binary_node(op, first, operand)
            };
            self.nodes[single as usize].tok = ptok;
            return single;
        }
        let ptok = self.nodes[pairs[0] as usize].tok;
        // ペアを next で連結する
        for i in 0..pairs.len() {
            let next = if i + 1 < pairs.len() {
                Some(pairs[i + 1])
            } else {
                None
            };
            self.nodes[pairs[i] as usize].next = next;
        }
        self.register_any();
        let offset = self.locals.last().unwrap().offset;
        self.register_any();
        let id = self.new_node(NodeData::Chain {
            first,
            pairs: Some(pairs[0]),
            offset,
        });
        self.nodes[id as usize].tok = ptok;
        id
    }

    fn relational(&mut self) -> Result<NodeId, Flow> {
        let first = self.add()?;
        let mut op_tok = self.cur_tok_id();
        let (mut op, mut swapped) = match self.read_rel_op() {
            Some(x) => x,
            None => return Ok(first),
        };
        let mut pairs: Vec<NodeId> = Vec::new();
        loop {
            let operand = self.add()?;
            let pair = self.new_node(NodeData::Binop {
                op,
                left: operand,
                right: None,
                val: if swapped { 1 } else { 0 },
            });
            // right は使わない（C でもペアの right_child は NULL）
            self.nodes[pair as usize].tok = Some(op_tok);
            pairs.push(pair);
            op_tok = self.cur_tok_id();
            match self.read_rel_op() {
                Some((o, s)) => {
                    op = o;
                    swapped = s;
                }
                None => break,
            }
        }
        Ok(self.make_cmp_chain(first, pairs))
    }

    fn read_eq_op(&mut self) -> Option<BinOp> {
        if self.at_expr_line_break() {
            return None;
        }
        if self.consume("==") {
            Some(BinOp::Eq)
        } else if self.consume("!=") {
            Some(BinOp::Ne)
        } else {
            None
        }
    }

    fn equality(&mut self) -> Result<NodeId, Flow> {
        let first = self.relational()?;
        let mut op_tok = self.cur_tok_id();
        let mut op = match self.read_eq_op() {
            Some(o) => o,
            None => return Ok(first),
        };
        let mut pairs: Vec<NodeId> = Vec::new();
        loop {
            let operand = self.relational()?;
            let pair = self.new_node(NodeData::Binop {
                op,
                left: operand,
                right: None,
                val: 0,
            });
            self.nodes[pair as usize].tok = Some(op_tok);
            pairs.push(pair);
            op_tok = self.cur_tok_id();
            match self.read_eq_op() {
                Some(o) => op = o,
                None => break,
            }
        }
        Ok(self.make_cmp_chain(first, pairs))
    }

    fn and(&mut self) -> Result<NodeId, Flow> {
        let mut node = self.equality()?;
        let op_tok = self.cur_tok_id();
        if !self.at_expr_line_break() && self.consume("&&") {
            let r = self.and()?;
            node = self.create_binary_node(BinOp::And, node, r);
            self.nodes[node as usize].tok = Some(op_tok);
        }
        Ok(node)
    }

    fn or(&mut self) -> Result<NodeId, Flow> {
        let mut node = self.and()?;
        let op_tok = self.cur_tok_id();
        if !self.at_expr_line_break() && self.consume("||") {
            let r = self.or()?;
            node = self.create_binary_node(BinOp::Or, node, r);
            self.nodes[node as usize].tok = Some(op_tok);
        }
        Ok(node)
    }

    fn range(&mut self) -> Result<NodeId, Flow> {
        let node = self.or()?;
        let op_tok = self.cur_tok_id();
        if self.at_expr_line_break() {
            return Ok(node);
        }
        let incl: i64 = if self.consume("..=") {
            1
        } else if self.consume("..") {
            0
        } else {
            return Ok(node);
        };
        let rhs = self.or()?;
        let incl_node = self.create_number_node(incl);
        let name = self.intern_str("Range");
        let call = self.new_node(NodeData::Funcall {
            name,
            args: Some(node),
            klass: None,
            func: None,
        });
        self.nodes[call as usize].tok = Some(op_tok);
        self.nodes[node as usize].next = Some(rhs);
        self.nodes[rhs as usize].next = Some(incl_node);
        Ok(call)
    }

    fn assign(&mut self) -> Result<NodeId, Flow> {
        if self.lambda_arrow_ahead() {
            let kw = self.cur_tok_id();
            return self.parse_lambda(true, kw);
        }
        let first_token = self.cur_tok_id();
        let mut node = self.range()?;
        let error_token = self.cur_tok_id();
        if !self.suppress_multi {
            let t = self.tok().clone();
            if tok_is_char(self, &t, b',') {
                // 複数代入
                let mut targets = vec![node];
                while self.consume(",") {
                    targets.push(self.range()?);
                }
                self.expect("=")?;
                let saved = self.suppress_multi;
                self.suppress_multi = true;
                let mut values = vec![self.assign()?];
                while self.consume(",") {
                    values.push(self.assign()?);
                }
                self.suppress_multi = saved;
                let ntargets = targets.len() as i32;
                let mut nvalues = values.len() as i32;
                // a, b = in の形: 対象の数だけ読み取りを並べる
                if nvalues == 1 && ntargets > 1 {
                    let v = values[0];
                    if let NodeData::Input { args: None, mode } = self.nodes[v as usize].data {
                        if mode != 2 {
                            let mut chain = Vec::new();
                            for _ in 0..ntargets {
                                chain.push(self.new_node(NodeData::Input {
                                    args: None,
                                    mode,
                                }));
                            }
                            values = chain;
                            nvalues = ntargets;
                        }
                    }
                }
                if ntargets != nvalues {
                    let m = format!(
                        "代入先と値の個数が一致しません（代入先{}個、値{}個）",
                        ntargets, nvalues
                    )
                    .into_bytes();
                    self.err_tok(first_token, m)?;
                }
                for &t in &targets {
                    let bound = self.bind_target_node(t);
                    if bound != t {
                        let saved_next = self.nodes[t as usize].next;
                        let bval = self.nodes[bound as usize].clone();
                        self.nodes[t as usize] = bval;
                        self.nodes[t as usize].next = saved_next;
                    }
                }
                // targets / values を next で連結
                for i in 0..targets.len() {
                    let next = if i + 1 < targets.len() {
                        Some(targets[i + 1])
                    } else {
                        None
                    };
                    self.nodes[targets[i] as usize].next = next;
                }
                for i in 0..values.len() {
                    let next = if i + 1 < values.len() {
                        Some(values[i + 1])
                    } else {
                        None
                    };
                    self.nodes[values[i] as usize].next = next;
                }
                let multi = self.new_node(NodeData::MultiAssign {
                    targets: Some(targets[0]),
                    values: Some(values[0]),
                    count: nvalues,
                    offset: 0,
                });
                self.nodes[multi as usize].tok = Some(first_token);
                let mut off = 0;
                for i in 0..nvalues {
                    self.register_any();
                    if i == 0 {
                        off = self.locals.last().unwrap().offset;
                    }
                }
                if let NodeData::MultiAssign { offset, .. } = &mut self.nodes[multi as usize].data {
                    *offset = off;
                }
                return Ok(multi);
            }
        }
        if self.consume(":") {
            node = self.bind_target_node(node);
            let kind = self.nodes[node as usize].kind();
            if kind != crate::NodeKind::LVar && kind != crate::NodeKind::GVar {
                self.err_tok(
                    error_token,
                    "型注釈は変数にだけ付けられます".as_bytes().to_vec(),
                )?;
            }
            let anno = self.parse_type()?;
            self.expect("=")?;
            let ntok = self.nodes[node as usize].tok;
            let right = self.assign()?;
            let an = self.new_node(NodeData::Assign {
                left: node,
                right,
                anno: Some(anno),
            });
            self.nodes[an as usize].tok = ntok;
            node = an;
        } else if self.consume("=") {
            let ntok = self.nodes[node as usize].tok;
            let left = self.bind_target_node(node);
            let right = self.assign()?;
            let an = self.new_node(NodeData::Assign {
                left,
                right,
                anno: None,
            });
            self.nodes[an as usize].tok = ntok;
            node = an;
        } else if self.consume("+=") {
            let ntok = self.nodes[node as usize].tok;
            let right = self.assign()?;
            let an = self.new_node(NodeData::OpAssign {
                op: BinOp::Add,
                left: node,
                right,
            });
            self.nodes[an as usize].tok = ntok;
            node = an;
        } else if self.consume("-=") {
            let ntok = self.nodes[node as usize].tok;
            let right = self.assign()?;
            let an = self.new_node(NodeData::OpAssign {
                op: BinOp::Sub,
                left: node,
                right,
            });
            self.nodes[an as usize].tok = ntok;
            node = an;
        } else if self.consume("*=") {
            let ntok = self.nodes[node as usize].tok;
            let right = self.assign()?;
            let an = self.new_node(NodeData::OpAssign {
                op: BinOp::Mul,
                left: node,
                right,
            });
            self.nodes[an as usize].tok = ntok;
            node = an;
        } else if self.consume("/=") {
            let ntok = self.nodes[node as usize].tok;
            let right = self.assign()?;
            let an = self.new_node(NodeData::OpAssign {
                op: BinOp::Div,
                left: node,
                right,
            });
            self.nodes[an as usize].tok = ntok;
            node = an;
        } else if self.consume("%=") {
            let ntok = self.nodes[node as usize].tok;
            let right = self.assign()?;
            let an = self.new_node(NodeData::OpAssign {
                op: BinOp::Mod,
                left: node,
                right,
            });
            self.nodes[an as usize].tok = ntok;
            node = an;
        }
        Ok(node)
    }

    pub fn expr(&mut self) -> Result<NodeId, Flow> {
        self.assign()
    }

    fn array_literal(&mut self) -> Result<NodeId, Flow> {
        self.expr_depth += 1;
        if self.consume("]") {
            self.register_array(0);
            let offset = self.locals.last().unwrap().offset;
            let id = self.new_node(NodeData::Array {
                body: None,
                array_len: 0,
                offset,
            });
            self.expr_depth -= 1;
            return Ok(id);
        }
        let saved = self.suppress_multi;
        self.suppress_multi = true;
        let first_expr = self.expr()?;
        let node;
        if self.consume(";") {
            let len_expr = self.expr()?;
            node = self.new_node(NodeData::NewArray {
                fill: first_expr,
                len: len_expr,
            });
        } else {
            let mut elems = vec![first_expr];
            while self.consume(",") {
                elems.push(self.expr()?);
            }
            for i in 0..elems.len() {
                let next = if i + 1 < elems.len() {
                    Some(elems[i + 1])
                } else {
                    None
                };
                self.nodes[elems[i] as usize].next = next;
            }
            self.register_array(elems.len() as i32);
            let offset = self.locals.last().unwrap().offset;
            node = self.new_node(NodeData::Array {
                body: Some(elems[0]),
                array_len: elems.len() as i32,
                offset,
            });
        }
        self.suppress_multi = saved;
        self.expect("]")?;
        self.expr_depth -= 1;
        Ok(node)
    }

    // 型引数の閉じ括弧。入れ子では >> と1トークンになるため、
    // >> の前半だけを消費して後半を > として残す
    fn expect_type_end(&mut self) -> Result<(), Flow> {
        let t = self.tok().clone();
        if tok_is_symbol(self, &t, ">>") {
            let cur = self.tpos;
            self.tokens[cur].start += 1;
            self.tokens[cur].len = 1;
            return Ok(());
        }
        self.expect(">")
    }

    pub fn parse_type(&mut self) -> Result<TypeId, Flow> {
        {
            let t = self.tok().clone();
            if t.kind == crate::TokenKind::Keyword && t.keyword == Keyword::Fn {
                self.advance();
                self.expect("(")?;
                let mut params: [Option<TypeId>; 6] = [None; 6];
                let mut nparams: i32 = 0;
                if !self.consume(")") {
                    loop {
                        let pt = self.parse_type()?;
                        if nparams < 6 {
                            params[nparams as usize] = Some(pt);
                        }
                        nparams += 1;
                        if !self.consume(",") {
                            break;
                        }
                    }
                    self.expect(")")?;
                }
                if nparams > 6 {
                    let tid = self.cur_tok_id();
                    let m = format!("関数型の引数は6個までですが、{}個が指定されています", nparams)
                        .into_bytes();
                    self.err_tok(tid, m)?;
                }
                self.expect("->")?;
                let ret = self.parse_type()?;
                return Ok(self.func_type(&params, nparams, Some(ret)));
            }
        }
        if self.consume("[") {
            let elem = self.parse_type()?;
            self.expect("]")?;
            return Ok(self.array_type(elem));
        }
        let ident = match self.consume_ident() {
            Some(i) => i,
            None => {
                let tid = self.cur_tok_id();
                if self.at_eof() {
                    self.err_tok(
                        tid,
                        "型名が必要ですが、ファイルの末尾に達しました"
                            .as_bytes()
                            .to_vec(),
                    )?;
                } else {
                    let actual = self.tt(tid);
                    let mut m = Vec::new();
                    m.extend_from_slice("型名が必要ですが、".as_bytes());
                    m.extend_from_slice(&actual);
                    m.extend_from_slice("が見つかりました".as_bytes());
                    self.err_tok(tid, m)?;
                }
                unreachable!()
            }
        };
        let name = self.name_from_token(ident);
        if let Some(pk) = self.parsing_klass {
            let (tparams, tcount) = {
                let k = &self.classes[pk as usize];
                (k.tparams, k.tparam_count)
            };
            for i in 0..(tcount as usize).min(MAX_TPARAMS) {
                if let Some(pn) = tparams[i] {
                    if self.name_eq(name, pn) {
                        let mut t = Type::new(TypeKind::Var);
                        t.name = Some(name);
                        return Ok(self.new_type(t));
                    }
                }
            }
        }
        if self.name_eq_lit(name, "int") {
            return Ok(crate::TY_INT_ID);
        }
        if let Some(klass) = self.find_class(name) {
            if self.consume("<") {
                let mut applied = Type::new(TypeKind::Class);
                applied.klass = Some(klass);
                let mut count: i32 = 0;
                loop {
                    let pt = self.parse_type()?;
                    if (count as usize) < MAX_TPARAMS {
                        applied.targs[count as usize] = Some(pt);
                    }
                    count += 1;
                    if !self.consume(",") {
                        break;
                    }
                }
                applied.targ_count = count;
                let applied_id = self.new_type(applied);
                self.expect_type_end()?;
                let expected = self.classes[klass as usize].tparam_count;
                if count != expected {
                    let kname = self.classes[klass as usize].name;
                    let mut m = Vec::new();
                    m.extend_from_slice("クラス`".as_bytes());
                    m.extend_from_slice(&self.name_bytes(kname).to_vec());
                    m.extend_from_slice(
                        format!("`の型引数は{}個ですが、{}個が指定されています", expected, count)
                            .as_bytes(),
                    );
                    self.err_tok(ident, m)?;
                }
                return Ok(applied_id);
            }
            return Ok(self.classes[klass as usize].ty);
        }
        let mut m = Vec::new();
        m.extend_from_slice("型`".as_bytes());
        m.extend_from_slice(&self.name_bytes(name).to_vec());
        m.extend_from_slice("`は定義されていません".as_bytes());
        self.err_tok(ident, m)?;
        Ok(crate::TY_UNKNOWN_ID)
    }
}
