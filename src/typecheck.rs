// 型検査（typecheck.c の移植）: AST を歩いて各ノードに型を付け、
// 演算子を dunder メソッド呼び出しへ書き換える。
// C の Type* の NULL は Option<TypeId> の None に対応する。
use crate::{
    BinOp, ClassId, Cx, Flow, FunctionId, Name, NodeData, NodeId, TypeId, TypeKind, UnaryOp,
    TY_INT_ID, TY_UNKNOWN_ID,
};

pub const LOCAL_TYPES_N: usize = 1024;

impl Cx {
    // ---- local_types（C の static Type* local_types[1024]） ----
    pub fn lt_get(&self, idx: usize) -> Option<TypeId> {
        self.local_types.get(idx).copied().flatten()
    }
    pub fn lt_set(&mut self, idx: usize, v: Option<TypeId>) {
        if idx >= self.local_types.len() {
            self.local_types.resize(idx + 1, None);
        }
        self.local_types[idx] = v;
    }
    fn lt_clear(&mut self) {
        for v in self.local_types.iter_mut() {
            *v = None;
        }
    }

    // 関数・メソッドを探す。klass が None なら通常の関数
    pub fn find_callable(&self, klass: Option<ClassId>, name: Name) -> Option<FunctionId> {
        let mut cur = self.functions_head;
        while let Some(f) = cur {
            let func = &self.funcs[f as usize];
            if func.klass == klass && self.name_eq(func.name, name) {
                return Some(f);
            }
            cur = func.next;
        }
        None
    }

    pub fn find_callable_lit(&self, klass: Option<ClassId>, name: &str) -> Option<FunctionId> {
        let mut cur = self.functions_head;
        while let Some(f) = cur {
            let func = &self.funcs[f as usize];
            if func.klass == klass && self.name_eq_lit(func.name, name) {
                return Some(f);
            }
            cur = func.next;
        }
        None
    }

    fn bool_type(&mut self) -> TypeId {
        let n = self.intern_str("bool");
        match self.find_class(n) {
            Some(k) => self.classes[k as usize].ty,
            None => TY_INT_ID,
        }
    }

    fn scalar_cmp_result(&mut self, dunder: &str) -> TypeId {
        let n = self.intern_str("int");
        if let Some(k) = self.find_class(n) {
            if let Some(m) = self.find_callable_lit(Some(k), dunder) {
                if let Some(rt) = self.funcs[m as usize].return_type {
                    return rt;
                }
            }
        }
        TY_INT_ID
    }

    pub fn node_name(&self, id: NodeId) -> Name {
        match &self.nodes[id as usize].data {
            NodeData::LVar { name, .. }
            | NodeData::GVar { name }
            | NodeData::CapVar { name, .. }
            | NodeData::Member { name, .. }
            | NodeData::MCall { name, .. }
            | NodeData::Funcall { name, .. }
            | NodeData::FuncRef { name, .. }
            | NodeData::Lambda { name, .. } => *name,
            _ => Name::default(),
        }
    }

    fn type_text_of(&self, t: TypeId) -> Vec<u8> {
        self.type_text(t)
    }

    // 二項演算子・添え字を ND_MCALL へ書き換える（rewrite_to_mcall）
    fn rewrite_to_mcall(
        &mut self,
        node: NodeId,
        lt: TypeId,
        rt: TypeId,
        name: &str,
        method: Option<FunctionId>,
    ) -> Result<TypeId, Flow> {
        let method = match method {
            Some(m) => m,
            // C では NULL のメソッドを参照して落ちる場所
            None => return Ok(TY_UNKNOWN_ID),
        };
        let (left, right) = match &self.nodes[node as usize].data {
            NodeData::Binop {
                left,
                right: Some(r),
                ..
            } => (*left, *r),
            NodeData::Index { left, right } => (*left, *right),
            _ => return Ok(TY_UNKNOWN_ID),
        };
        let klass = self.class_of(lt)?;
        if let Some(k) = klass {
            let (m_nparams, m_pt1) = {
                let mfunc = &self.funcs[method as usize];
                (mfunc.nparams, mfunc.param_types[1])
            };
            if m_nparams != 2 {
                let msg = crate::msg(&[
                    "クラス`".as_bytes(),
                    &self.name_bytes(self.classes[k as usize].name).to_vec(),
                    "`のメソッド`".as_bytes(),
                    name.as_bytes(),
                    "`の引数は1個ですが、".as_bytes(),
                    (m_nparams - 1).to_string().as_bytes(),
                    "個が定義されています".as_bytes(),
                ]);
                self.err_node(Some(node), msg)?;
            }
            let pt = m_pt1;
            if let Some(pt) = pt {
                if self.types[rt as usize].kind != TypeKind::Unknown && !self.type_equals(pt, rt) {
                    let msg = crate::msg(&[
                        "クラス`".as_bytes(),
                        &self.name_bytes(self.classes[k as usize].name).to_vec(),
                        "`のメソッド`".as_bytes(),
                        name.as_bytes(),
                        "`の引数には`".as_bytes(),
                        &self.type_text_of(pt),
                        "`が必要ですが、`".as_bytes(),
                        &self.type_text_of(rt),
                        "`が指定されています".as_bytes(),
                    ]);
                    self.err_node(Some(node), msg)?;
                }
            }
        }
        self.nodes[right as usize].next = None;
        let name_id = self.intern_str(name);
        self.nodes[node as usize].data = NodeData::MCall {
            left,
            args: Some(right),
            name: name_id,
            func: Some(method),
            klass,
        };
        Ok(self.funcs[method as usize]
            .return_type
            .unwrap_or(TY_UNKNOWN_ID))
    }

    // 代入先の添え字を __setitem__ 呼び出しへ書き換える（rewrite_to_setitem）
    fn rewrite_to_setitem(
        &mut self,
        node: NodeId,
        klass: Option<ClassId>,
        base: NodeId,
        index: NodeId,
        it: TypeId,
        rhs: TypeId,
        value: NodeId,
    ) -> Result<(), Flow> {
        let klass = match klass {
            Some(k) => k,
            None => return Ok(()),
        };
        let method = self.find_callable_lit(Some(klass), "__setitem__");
        let method = match method {
            Some(m) => m,
            None => {
                let msg = crate::msg(&[
                    "クラス`".as_bytes(),
                    &self.name_bytes(self.classes[klass as usize].name).to_vec(),
                    "`に`__setitem__`は定義されていません".as_bytes(),
                ]);
                self.err_node(Some(node), msg)?;
                // メソッドなしでは書き換えを続けられない
                return Ok(());
            }
        };
        let mfunc = self.funcs[method as usize].clone();
        if mfunc.nparams != 3 {
            let msg = crate::msg(&[
                "クラス`".as_bytes(),
                &self.name_bytes(self.classes[klass as usize].name).to_vec(),
                "`のメソッド`__setitem__`の引数は2個ですが、".as_bytes(),
                (mfunc.nparams - 1).to_string().as_bytes(),
                "個が定義されています".as_bytes(),
            ]);
            self.err_node(Some(node), msg)?;
        }
        for (slot, arg_ty) in [(1usize, it), (2usize, rhs)] {
            if let Some(pt) = mfunc.param_types[slot] {
                if self.types[arg_ty as usize].kind != TypeKind::Unknown
                    && !self.type_equals(pt, arg_ty)
                {
                    let msg = crate::msg(&[
                        "クラス`".as_bytes(),
                        &self.name_bytes(self.classes[klass as usize].name).to_vec(),
                        "`のメソッド`__setitem__`の引数には`".as_bytes(),
                        &self.type_text_of(pt),
                        "`が必要ですが、`".as_bytes(),
                        &self.type_text_of(arg_ty),
                        "`が指定されています".as_bytes(),
                    ]);
                    self.err_node(Some(node), msg)?;
                }
            }
        }
        self.nodes[index as usize].next = Some(value);
        let name_id = self.intern_str("__setitem__");
        self.nodes[node as usize].data = NodeData::MCall {
            left: base,
            args: Some(index),
            name: name_id,
            func: Some(method),
            klass: Some(klass),
        };
        Ok(())
    }

    // 引数を全部検査し、型を最大 6 個まで記録する。引数の数を返す
    fn check_args_expected(
        &mut self,
        args: Option<NodeId>,
        expected_list: &[Option<TypeId>; 6],
        use_expected: bool,
    ) -> Result<(i32, [TypeId; 6]), Flow> {
        let mut arg_types = [TY_UNKNOWN_ID; 6];
        let mut nargs: i32 = 0;
        let mut cur = args;
        while let Some(arg) = cur {
            self.expected_type = if use_expected && nargs < 6 {
                expected_list[nargs as usize]
            } else {
                None
            };
            let at = self.check_node(Some(arg))?;
            self.expected_type = None;
            if nargs < 6 {
                arg_types[nargs as usize] = at;
            }
            nargs += 1;
            cur = self.nodes[arg as usize].next;
        }
        Ok((nargs, arg_types))
    }

    fn check_args(
        &mut self,
        args: Option<NodeId>,
    ) -> Result<(i32, [TypeId; 6]), Flow> {
        self.check_args_expected(args, &[None; 6], false)
    }

    // 変数の型（注釈または記録済み）と代入する値の型が食い違えばエラー
    fn check_assign_type(
        &mut self,
        lhs: NodeId,
        expected: Option<TypeId>,
        rhs: TypeId,
    ) -> Result<(), Flow> {
        if let Some(expected) = expected {
            if self.types[expected as usize].kind != TypeKind::Unknown
                && self.types[rhs as usize].kind != TypeKind::Unknown
                && !self.type_equals(expected, rhs)
            {
                let name = self.node_name(lhs);
                let msg = crate::msg(&[
                    "変数`".as_bytes(),
                    &self.name_bytes(name).to_vec(),
                    "`の型は`".as_bytes(),
                    &self.type_text_of(expected),
                    "`ですが、`".as_bytes(),
                    &self.type_text_of(rhs),
                    "`が代入されています".as_bytes(),
                ]);
                self.err_node(Some(lhs), msg)?;
            }
        }
        Ok(())
    }

    fn check_array_arith(
        &mut self,
        node: NodeId,
        lt: TypeId,
        rt: TypeId,
    ) -> Result<TypeId, Flow> {
        let (op, left, right) = match &self.nodes[node as usize].data {
            NodeData::Binop {
                op,
                left,
                right: Some(r),
                ..
            } => (*op, *left, *r),
            _ => return Ok(TY_UNKNOWN_ID),
        };
        let (mut lt, mut rt) = (lt, rt);
        if self.types[lt as usize].kind != TypeKind::Array {
            // 配列側が左に来るよう、ノードの左右と型を入れ替える
            if let NodeData::Binop {
                left: nl,
                right: nr,
                ..
            } = &mut self.nodes[node as usize].data
            {
                *nl = right;
                *nr = Some(left);
            }
            std::mem::swap(&mut lt, &mut rt);
        }
        // 入れ替え後は lt が配列側、ot（もう一方）は rt
        let ot = rt;
        match op {
            crate::BinOp::Add => {
                if self.types[ot as usize].kind != TypeKind::Array
                    && self.types[ot as usize].kind != TypeKind::Unknown
                {
                    let msg = crate::msg(&[
                        "`".as_bytes(),
                        &self.type_text_of(lt),
                        "`型の値に演算子`+`は使えません".as_bytes(),
                    ]);
                    self.err_node(Some(node), msg)?;
                }
                let k = self.class_of(lt)?;
                let method = self.find_callable_lit(k, "__add__");
                let result = self.rewrite_to_mcall(node, lt, rt, "__add__", method)?;
                if self.types[result as usize].kind == TypeKind::Unknown {
                    return Ok(lt);
                }
                Ok(result)
            }
            crate::BinOp::Mul => {
                if self.types[ot as usize].kind != TypeKind::Int
                    && self.types[ot as usize].kind != TypeKind::Unknown
                {
                    let msg = crate::msg(&[
                        "`".as_bytes(),
                        &self.type_text_of(lt),
                        "`型の値に演算子`*`は使えません".as_bytes(),
                    ]);
                    self.err_node(Some(node), msg)?;
                }
                let k = self.class_of(lt)?;
                let method = self.find_callable_lit(k, "__mul__");
                let result = self.rewrite_to_mcall(node, lt, rt, "__mul__", method)?;
                if self.types[result as usize].kind == TypeKind::Unknown {
                    return Ok(lt);
                }
                Ok(result)
            }
            _ => {
                let msg = crate::msg(&[
                    "`".as_bytes(),
                    &self.type_text_of(lt),
                    "`型の値に演算子`".as_bytes(),
                    op_text(op).as_bytes(),
                    "`は使えません".as_bytes(),
                ]);
                self.err_node(Some(node), msg)?;
                Ok(TY_UNKNOWN_ID)
            }
        }
    }

    fn check_array_cmp(
        &mut self,
        node: NodeId,
        lt: TypeId,
        rt: TypeId,
    ) -> Result<TypeId, Flow> {
        let op = match &self.nodes[node as usize].data {
            NodeData::Binop { op, .. } => *op,
            _ => return Ok(TY_UNKNOWN_ID),
        };
        if op == crate::BinOp::Lt || op == crate::BinOp::Le {
            let at = if self.types[lt as usize].kind == TypeKind::Array {
                lt
            } else {
                rt
            };
            let msg = crate::msg(&[
                "`".as_bytes(),
                &self.type_text_of(at),
                "`型の値に演算子`".as_bytes(),
                op_text(op).as_bytes(),
                "`は使えません".as_bytes(),
            ]);
            self.err_node(Some(node), msg)?;
        }
        let ot = if self.types[lt as usize].kind == TypeKind::Array {
            rt
        } else {
            lt
        };
        if self.types[ot as usize].kind == TypeKind::Array
            || self.types[ot as usize].kind == TypeKind::Unknown
        {
            let k = self.class_of(lt)?;
            if op == crate::BinOp::Eq {
                let method = self.find_callable_lit(k, "__eq__");
                self.rewrite_to_mcall(node, lt, rt, "__eq__", method)?;
            } else {
                let method = self.find_callable_lit(k, "__ne__");
                self.rewrite_to_mcall(node, lt, rt, "__ne__", method)?;
            }
        }
        Ok(self.bool_type())
    }

    fn check_binop(&mut self, node: NodeId) -> Result<TypeId, Flow> {
        let (op, left, right) = match &self.nodes[node as usize].data {
            NodeData::Binop {
                op,
                left,
                right: Some(r),
                ..
            } => (*op, *left, *r),
            _ => return Ok(TY_UNKNOWN_ID),
        };
        let lt = self.check_node(Some(left))?;
        let rt = self.check_node(Some(right))?;
        if let Some(dunder) = arith_dunder(op) {
            if self.types[lt as usize].kind == TypeKind::Class {
                let k = self.class_of(lt)?;
                let method = self.find_callable_lit(k, dunder);
                if method.is_none() {
                    let kname = self.types[lt as usize].klass.unwrap();
                    let msg = crate::msg(&[
                        "クラス`".as_bytes(),
                        &self.name_bytes(self.classes[kname as usize].name).to_vec(),
                        "`に演算子`".as_bytes(),
                        op_text(op).as_bytes(),
                        "`は定義されていません".as_bytes(),
                    ]);
                    self.err_node(Some(node), msg)?;
                    return Ok(TY_UNKNOWN_ID);
                }
                return self.rewrite_to_mcall(node, lt, rt, dunder, method);
            }
            if self.types[lt as usize].kind == TypeKind::Array
                || self.types[rt as usize].kind == TypeKind::Array
            {
                return self.check_array_arith(node, lt, rt);
            }
            if self.types[lt as usize].kind != TypeKind::Unknown
                && self.types[rt as usize].kind != TypeKind::Unknown
                && self.types[lt as usize].kind != self.types[rt as usize].kind
            {
                let msg = crate::msg(&[
                    "`".as_bytes(),
                    &self.type_text_of(lt),
                    "`型と`".as_bytes(),
                    &self.type_text_of(rt),
                    "`型の値に演算子`".as_bytes(),
                    op_text(op).as_bytes(),
                    "`は使えません".as_bytes(),
                ]);
                self.err_node(Some(node), msg)?;
            }
            return Ok(TY_INT_ID);
        }
        if let Some(cdunder) = cmp_dunder(op) {
            if self.types[lt as usize].kind == TypeKind::Class {
                let k = self.class_of(lt)?;
                let method = self.find_callable_lit(k, cdunder);
                if method.is_some() {
                    return self.rewrite_to_mcall(node, lt, rt, cdunder, method);
                }
                if op == crate::BinOp::Lt || op == crate::BinOp::Le {
                    let kname = self.types[lt as usize].klass.unwrap();
                    let msg = crate::msg(&[
                        "クラス`".as_bytes(),
                        &self.name_bytes(self.classes[kname as usize].name).to_vec(),
                        "`に演算子`".as_bytes(),
                        op_text(op).as_bytes(),
                        "`は定義されていません".as_bytes(),
                    ]);
                    self.err_node(Some(node), msg)?;
                }
                return Ok(self.bool_type());
            }
            if self.types[lt as usize].kind == TypeKind::Array
                || self.types[rt as usize].kind == TypeKind::Array
            {
                return self.check_array_cmp(node, lt, rt);
            }
            if (op == crate::BinOp::Lt || op == crate::BinOp::Le)
                && self.types[lt as usize].kind != TypeKind::Unknown
                && self.types[rt as usize].kind != TypeKind::Unknown
                && self.types[lt as usize].kind != self.types[rt as usize].kind
            {
                let msg = crate::msg(&[
                    "`".as_bytes(),
                    &self.type_text_of(lt),
                    "`型と`".as_bytes(),
                    &self.type_text_of(rt),
                    "`型の値に演算子`".as_bytes(),
                    op_text(op).as_bytes(),
                    "`は使えません".as_bytes(),
                ]);
                self.err_node(Some(node), msg)?;
            }
            return Ok(self.scalar_cmp_result(cdunder));
        }
        Ok(self.bool_type())
    }
}

pub fn arith_dunder(op: BinOp) -> Option<&'static str> {
    match op {
        BinOp::Add => Some("__add__"),
        BinOp::Sub => Some("__sub__"),
        BinOp::Mul => Some("__mul__"),
        BinOp::Div => Some("__div__"),
        BinOp::Mod => Some("__mod__"),
        _ => None,
    }
}

pub fn cmp_dunder(op: BinOp) -> Option<&'static str> {
    match op {
        BinOp::Eq => Some("__eq__"),
        BinOp::Ne => Some("__ne__"),
        BinOp::Lt => Some("__lt__"),
        BinOp::Le => Some("__le__"),
        _ => None,
    }
}

pub fn op_text(op: BinOp) -> &'static str {
    match op {
        BinOp::Add => "+",
        BinOp::Sub => "-",
        BinOp::Mul => "*",
        BinOp::Div => "/",
        BinOp::Mod => "%",
        BinOp::Eq => "==",
        BinOp::Ne => "!=",
        BinOp::Lt => "<",
        BinOp::Le => "<=",
        _ => "?",
    }
}

impl Cx {
    fn check_assign(&mut self, node: NodeId) -> Result<TypeId, Flow> {
        let (left, right, anno) = match &self.nodes[node as usize].data {
            NodeData::Assign { left, right, anno } => (*left, *right, *anno),
            _ => return Ok(TY_UNKNOWN_ID),
        };
        // 右辺への期待型: 注釈があれば注釈、なければ代入先の記録済みの型
        if let Some(a) = anno {
            self.expected_type = Some(a);
        } else {
            match &self.nodes[left as usize].data {
                NodeData::LVar { offset, .. } => {
                    self.expected_type = self.lt_get((*offset / 8) as usize);
                }
                NodeData::GVar { name } => {
                    let gt = self.find_global_type(*name);
                    self.expected_type = if self.types[gt as usize].kind != TypeKind::Unknown {
                        Some(gt)
                    } else {
                        None
                    };
                }
                _ => {}
            }
        }
        let mut rhs = self.check_node(Some(right))?;
        self.expected_type = None;
        if let Some(a) = anno {
            rhs = self.adopt_type(a, rhs);
        }
        let lhs = left;
        // 右辺の検査で添字が __getitem__ 呼び出しへ書き換え済みの場合の
        // __setitem__ 化（desugar 済みの複合代入が対象）
        {
            let mcall_info = match &self.nodes[lhs as usize].data {
                NodeData::MCall {
                    func: Some(_),
                    left: mleft,
                    args,
                    klass,
                    ..
                } => Some((*mleft, *args, *klass)),
                _ => None,
            };
            if let Some((mleft, margs, mklass)) = mcall_info {
                let is_binop_sharing = match &self.nodes[right as usize].data {
                    NodeData::Binop { left: bl, .. } => *bl == lhs,
                    _ => false,
                };
                if is_binop_sharing {
                    let arg_node = margs.unwrap();
                    let mut idx_node = self.nodes[arg_node as usize].clone();
                    idx_node.next = None;
                    let idx = self.new_node(idx_node.data.clone());
                    self.nodes[idx as usize].tok = idx_node.tok;
                    self.nodes[idx as usize].ty = idx_node.ty;
                    let it = self.nodes[arg_node as usize].ty.unwrap_or(TY_UNKNOWN_ID);
                    self.rewrite_to_setitem(node, mklass, mleft, idx, it, rhs, right)?;
                    return Ok(rhs);
                }
            }
        }
        match &self.nodes[lhs as usize].data.clone() {
            NodeData::LVar { offset, .. } => {
                let off = *offset;
                let recorded = self.lt_get((off / 8) as usize);
                self.check_assign_type(lhs, anno, rhs)?;
                self.check_assign_type(lhs, recorded, rhs)?;
                if recorded.is_none()
                    && (anno.is_some() || self.types[rhs as usize].kind != TypeKind::Unknown)
                {
                    self.lt_set((off / 8) as usize, anno.or(Some(rhs)));
                }
                let v = self.lt_get((off / 8) as usize);
                self.nodes[lhs as usize].ty = v;
            }
            NodeData::GVar { name } => {
                let name = *name;
                let recorded = self.find_global_type(name);
                self.check_assign_type(lhs, anno, rhs)?;
                self.check_assign_type(lhs, Some(recorded), rhs)?;
                if self.types[recorded as usize].kind == TypeKind::Unknown {
                    self.record_global_type(name, anno.unwrap_or(rhs));
                }
                let v = self.find_global_type(name);
                self.nodes[lhs as usize].ty = Some(v);
            }
            NodeData::Index {
                left: ileft,
                right: iright,
            } => {
                let (ileft, iright) = (*ileft, *iright);
                let bt = self.check_node(Some(ileft))?;
                if self.types[bt as usize].kind == TypeKind::Class {
                    let it = self.check_node(Some(iright))?;
                    let k = self.class_of(bt)?;
                    self.rewrite_to_setitem(node, k, ileft, iright, it, rhs, right)?;
                } else {
                    let elem = self.check_node(Some(lhs))?;
                    if self.types[elem as usize].kind != TypeKind::Unknown
                        && self.types[rhs as usize].kind != TypeKind::Unknown
                        && !self.type_equals(elem, rhs)
                    {
                        let msg = crate::msg(&[
                            "配列の要素は`".as_bytes(),
                            &self.type_text_of(elem),
                            "`ですが、`".as_bytes(),
                            &self.type_text_of(rhs),
                            "`が代入されています".as_bytes(),
                        ]);
                        self.err_node(Some(node), msg)?;
                    }
                }
            }
            NodeData::CapVar { .. } => {
                let cap_type = self.check_node(Some(lhs))?;
                if self.types[cap_type as usize].kind != TypeKind::Unknown
                    && self.types[rhs as usize].kind != TypeKind::Unknown
                    && !self.type_equals(cap_type, rhs)
                {
                    let name = self.node_name(lhs);
                    let msg = crate::msg(&[
                        "変数`".as_bytes(),
                        &self.name_bytes(name).to_vec(),
                        "`の型は`".as_bytes(),
                        &self.type_text_of(cap_type),
                        "`ですが、`".as_bytes(),
                        &self.type_text_of(rhs),
                        "`が代入されています".as_bytes(),
                    ]);
                    self.err_node(Some(lhs), msg)?;
                }
                self.nodes[lhs as usize].ty = Some(cap_type);
            }
            _ => {
                self.check_node(Some(lhs))?;
                let kind = self.nodes[lhs as usize].kind();
                let name = self.node_name(lhs);
                match kind {
                    crate::NodeKind::Len => {
                        self.err_node(Some(lhs), "`len`には代入できません".as_bytes().to_vec())?;
                    }
                    crate::NodeKind::Member => {
                        let has_func = matches!(
                            &self.nodes[lhs as usize].data,
                            NodeData::Member { func: Some(_), .. }
                        );
                        if has_func {
                            let msg = crate::msg(&[
                                "メソッド`".as_bytes(),
                                &self.name_bytes(name).to_vec(),
                                "`には代入できません".as_bytes(),
                            ]);
                            self.err_node(Some(lhs), msg)?;
                        }
                    }
                    crate::NodeKind::FuncRef => {
                        let msg = crate::msg(&[
                            "関数`".as_bytes(),
                            &self.name_bytes(name).to_vec(),
                            "`には代入できません".as_bytes(),
                        ]);
                        self.err_node(Some(lhs), msg)?;
                    }
                    crate::NodeKind::MCall
                    | crate::NodeKind::Funcall
                    | crate::NodeKind::VCall => {
                        self.err_node(
                            Some(lhs),
                            "呼び出しの結果には代入できません".as_bytes().to_vec(),
                        )?;
                    }
                    crate::NodeKind::Lambda => {
                        self.err_node(Some(lhs), "ラムダには代入できません".as_bytes().to_vec())?;
                    }
                    _ => {}
                }
            }
        }
        Ok(rhs)
    }

    fn check_construct(
        &mut self,
        node: NodeId,
        klass: ClassId,
        arg_types: &[TypeId; 6],
        nargs: i32,
        expected_ty: Option<TypeId>,
    ) -> Result<TypeId, Flow> {
        let mut klass = klass;
        if self.types[self.classes[klass as usize].ty as usize].kind != TypeKind::Class {
            let msg = crate::msg(&[
                "`".as_bytes(),
                &self.name_bytes(self.classes[klass as usize].name).to_vec(),
                "`はクラスではありません".as_bytes(),
            ]);
            self.err_node(Some(node), msg)?;
        }
        if self.classes[klass as usize].tparam_count > 0 {
            let ginit = self.find_callable_lit(Some(klass), "__init__");
            let mut gtargs: [Option<TypeId>; 4] = [None; 4];
            if let Some(g) = ginit {
                let gnparams = self.funcs[g as usize].nparams;
                let gparams = self.funcs[g as usize].param_types;
                for i in 0..nargs as usize {
                    if (i as i32) + 1 < gnparams {
                        self.unify_type(gparams[i + 1], Some(arg_types[i]), klass, &mut gtargs)?;
                    }
                }
            }
            // 引数から決まらなかった型引数は期待型から補う
            if let Some(ety) = expected_ty {
                if self.types[ety as usize].kind == TypeKind::Class
                    && self.types[ety as usize].targ_count
                        == self.classes[klass as usize].tparam_count
                {
                    let ek = self.types[ety as usize].klass.unwrap();
                    let ek_origin = self.classes[ek as usize].origin.unwrap_or(ek);
                    let tk_origin = self.classes[klass as usize].origin.unwrap_or(klass);
                    if ek_origin == tk_origin {
                        let etargs = self.types[ety as usize].targs;
                        for i in 0..self.classes[klass as usize].tparam_count as usize {
                            if gtargs[i].is_none() {
                                gtargs[i] = etargs[i];
                            }
                        }
                    }
                }
            }
            let mut gtargs_ok = true;
            for i in 0..self.classes[klass as usize].tparam_count as usize {
                if gtargs[i].is_none() {
                    let msg = crate::msg(&[
                        "クラス`".as_bytes(),
                        &self.name_bytes(self.classes[klass as usize].name).to_vec(),
                        "`の型引数を推論できません".as_bytes(),
                    ]);
                    self.err_node(Some(node), msg)?;
                    gtargs_ok = false;
                }
            }
            if !gtargs_ok {
                return Ok(TY_UNKNOWN_ID);
            }
            let tpc = self.classes[klass as usize].tparam_count;
            klass = self.instantiate(klass, &gtargs, tpc)?;
        }
        if let NodeData::Funcall { klass: k, .. } = &mut self.nodes[node as usize].data {
            *k = Some(klass);
        }
        let init = self.find_callable_lit(Some(klass), "__init__");
        if let NodeData::Funcall { func, .. } = &mut self.nodes[node as usize].data {
            *func = init;
        }
        let expected = init.map(|f| self.funcs[f as usize].nparams - 1).unwrap_or(0);
        if nargs != expected {
            let msg = crate::msg(&[
                "クラス`".as_bytes(),
                &self.name_bytes(self.classes[klass as usize].name).to_vec(),
                "`の`__init__`の引数は".as_bytes(),
                expected.to_string().as_bytes(),
                "個ですが、".as_bytes(),
                nargs.to_string().as_bytes(),
                "個が指定されています".as_bytes(),
            ]);
            self.err_node(Some(node), msg)?;
        }
        if let Some(f) = init {
            let fparams = self.funcs[f as usize].param_types;
            let fnparams = self.funcs[f as usize].nparams;
            for i in 0..nargs as usize {
                if (i as i32) + 1 < fnparams {
                    if let Some(pt) = fparams[i + 1] {
                        if self.types[arg_types[i] as usize].kind != TypeKind::Unknown
                            && !self.type_equals(pt, arg_types[i])
                        {
                            let msg = crate::msg(&[
                                "クラス`".as_bytes(),
                                &self.name_bytes(self.classes[klass as usize].name).to_vec(),
                                "`の`__init__`の引数には`".as_bytes(),
                                &self.type_text_of(pt),
                                "`が必要ですが、`".as_bytes(),
                                &self.type_text_of(arg_types[i]),
                                "`が指定されています".as_bytes(),
                            ]);
                            self.err_node(Some(node), msg)?;
                        }
                    }
                }
            }
        }
        Ok(self.classes[klass as usize].ty)
    }

    // 関数参照: 関数のシグネチャから関数型を組み立てる
    fn check_funcref(&mut self, node: NodeId) -> Result<TypeId, Flow> {
        let name = self.node_name(node);
        let func = self.find_callable(None, name);
        let func = match func {
            Some(f) => f,
            None => return Ok(TY_UNKNOWN_ID),
        };
        if let NodeData::FuncRef { func: ff, .. } = &mut self.nodes[node as usize].data {
            *ff = Some(func);
        }
        let f = self.funcs[func as usize].clone();
        let mut params: [Option<TypeId>; 6] = [None; 6];
        for i in 0..(f.nparams as usize).min(6) {
            params[i] = Some(f.param_types[i].unwrap_or(TY_UNKNOWN_ID));
        }
        Ok(self.func_type(
            &params,
            f.nparams,
            Some(f.return_type.unwrap_or(TY_UNKNOWN_ID)),
        ))
    }

    // ラムダ本体のオンデマンド検査（check_lambda）
    fn check_lambda(
        &mut self,
        node: NodeId,
        expected: Option<TypeId>,
    ) -> Result<TypeId, Flow> {
        let (mut func, name) = match &self.nodes[node as usize].data {
            NodeData::Lambda { func, name, .. } => (*func, *name),
            _ => return Ok(TY_UNKNOWN_ID),
        };
        if func.is_none() {
            // ジェネリクスのクローンでは func がリセットされるため名前で引く
            let mut cur = self.functions_head;
            while let Some(f) = cur {
                let ff = &self.funcs[f as usize];
                if ff.is_lambda && self.name_eq(ff.name, name) {
                    func = Some(f);
                    break;
                }
                cur = ff.next;
            }
            if let NodeData::Lambda { func: ff, .. } = &mut self.nodes[node as usize].data {
                *ff = func;
            }
        }
        let func = match func {
            Some(f) => f,
            None => return Ok(TY_UNKNOWN_ID),
        };
        if self.funcs[func as usize].lambda_checked {
            if let Some(e) = expected {
                if self.types[e as usize].kind == TypeKind::Func {
                    if let Some(ft) = self.funcs[func as usize].func_type {
                        if !self.type_equals(e, ft) {
                            let msg = crate::msg(&[
                                "ラムダの型は`".as_bytes(),
                                &self.type_text_of(ft),
                                "`ですが、`".as_bytes(),
                                &self.type_text_of(e),
                                "`が期待されています".as_bytes(),
                            ]);
                            self.err_node(Some(node), msg)?;
                        }
                    }
                }
            }
            return Ok(self.funcs[func as usize].func_type.unwrap_or(TY_UNKNOWN_ID));
        }
        if let Some(e) = expected {
            if self.types[e as usize].kind == TypeKind::Func {
                let et = self.types[e as usize].clone();
                if et.func_nparams != self.funcs[func as usize].nparams {
                    let msg = crate::msg(&[
                        "ラムダの引数は".as_bytes(),
                        self.funcs[func as usize].nparams.to_string().as_bytes(),
                        "個ですが、".as_bytes(),
                        et.func_nparams.to_string().as_bytes(),
                        "個が期待されています".as_bytes(),
                    ]);
                    self.err_node(Some(node), msg)?;
                }
                for i in 0..self.funcs[func as usize].nparams as usize {
                    let ep = et.func_params[i];
                    let ep = match ep {
                        Some(p) if self.types[p as usize].kind != TypeKind::Unknown => p,
                        _ => continue,
                    };
                    let cur = self.funcs[func as usize].param_types[i];
                    if let Some(cp) = cur {
                        if !self.type_equals(cp, ep) {
                            let msg = crate::msg(&[
                                "ラムダの引数の型は`".as_bytes(),
                                &self.type_text_of(cp),
                                "`ですが、`".as_bytes(),
                                &self.type_text_of(ep),
                                "`が期待されています".as_bytes(),
                            ]);
                            self.err_node(Some(node), msg)?;
                        }
                    }
                    if cur.is_none() {
                        self.funcs[func as usize].param_types[i] = Some(ep);
                    }
                }
                if self.funcs[func as usize].return_type.is_none() {
                    if let Some(r) = et.func_ret {
                        if self.types[r as usize].kind != TypeKind::Unknown {
                            self.funcs[func as usize].return_type = Some(r);
                        }
                    }
                }
            }
        }
        for i in 0..self.funcs[func as usize].nparams as usize {
            if self.funcs[func as usize].param_types[i].is_none() {
                self.err_node(
                    Some(node),
                    "ラムダの引数の型が分かりません（型注釈を付けるか、関数型が期待される場所で使ってください）"
                        .as_bytes()
                        .to_vec(),
                )?;
            }
        }
        // 捕捉の型を確定する
        {
            let mut cur = self.funcs[func as usize].captures;
            while let Some(c) = cur {
                let (src_is_env, src_cap, src_offset, next) = {
                    let cc = &self.captures[c as usize];
                    (cc.src_is_env, cc.src_cap, cc.src_offset, cc.next)
                };
                let ty = if src_is_env {
                    src_cap
                        .and_then(|s| self.captures[s as usize].ty)
                        .unwrap_or(TY_UNKNOWN_ID)
                } else {
                    self.lt_get((src_offset / 8) as usize)
                        .unwrap_or(TY_UNKNOWN_ID)
                };
                self.captures[c as usize].ty = Some(ty);
                cur = next;
            }
        }
        // 検査状態を退避して本体を歩く
        let saved_types = self.local_types.clone();
        let saved_fn = self.current_fn;
        let saved_return = self.current_return;
        let saved_captures = self.current_captures;
        self.lt_clear();
        self.current_fn = Some(func);
        self.current_return = self.funcs[func as usize].return_type;
        self.current_captures = self.funcs[func as usize].captures;
        {
            let f = self.funcs[func as usize].clone();
            for i in 0..(f.nparams as usize).min(6) {
                if let Some(pt) = f.param_types[i] {
                    self.lt_set(i + 1, Some(pt));
                }
            }
            let mut cur = f.body;
            while let Some(stmt) = cur {
                self.check_node(Some(stmt))?;
                cur = self.nodes[stmt as usize].next;
            }
        }
        self.funcs[func as usize].lambda_checked = true;
        let (params, nparams, ret) = {
            let f = &self.funcs[func as usize];
            let mut params: [Option<TypeId>; 6] = [None; 6];
            for i in 0..(f.nparams as usize).min(6) {
                params[i] = Some(f.param_types[i].unwrap_or(TY_UNKNOWN_ID));
            }
            (
                params,
                f.nparams,
                Some(f.return_type.unwrap_or(TY_UNKNOWN_ID)),
            )
        };
        let ft = self.func_type(&params, nparams, ret);
        self.funcs[func as usize].func_type = Some(ft);
        self.local_types = saved_types;
        self.current_fn = saved_fn;
        self.current_return = saved_return;
        self.current_captures = saved_captures;
        Ok(ft)
    }

    // 値経由の呼び出し（check_vcall）
    fn check_vcall(&mut self, node: NodeId) -> Result<TypeId, Flow> {
        let (left, args) = match &self.nodes[node as usize].data {
            NodeData::VCall { left, args } => (*left, *args),
            _ => return Ok(TY_UNKNOWN_ID),
        };
        let lt = self.check_node(Some(left))?;
        let mut expected_list: [Option<TypeId>; 6] = [None; 6];
        let mut use_expected = false;
        if self.types[lt as usize].kind == TypeKind::Func {
            use_expected = true;
            let t = self.types[lt as usize].clone();
            for i in 0..(t.func_nparams as usize).min(6) {
                expected_list[i] = t.func_params[i];
            }
        }
        let (nargs, arg_types) = self.check_args_expected(args, &expected_list, use_expected)?;
        if self.types[lt as usize].kind == TypeKind::Unknown {
            return Ok(TY_UNKNOWN_ID);
        }
        if self.types[lt as usize].kind != TypeKind::Func {
            let msg = crate::msg(&[
                "`".as_bytes(),
                &self.type_text_of(lt),
                "`型の値は呼び出せません".as_bytes(),
            ]);
            self.err_node(Some(node), msg)?;
            return Ok(TY_UNKNOWN_ID);
        }
        let t = self.types[lt as usize].clone();
        if nargs != t.func_nparams {
            let msg = crate::msg(&[
                "関数値の引数は".as_bytes(),
                t.func_nparams.to_string().as_bytes(),
                "個ですが、".as_bytes(),
                nargs.to_string().as_bytes(),
                "個が指定されています".as_bytes(),
            ]);
            self.err_node(Some(node), msg)?;
        }
        for i in 0..nargs as usize {
            if (i as i32) < t.func_nparams && i < 6 {
                if let Some(pt) = t.func_params[i] {
                    if self.types[arg_types[i] as usize].kind != TypeKind::Unknown
                        && !self.type_equals(pt, arg_types[i])
                    {
                        let msg = crate::msg(&[
                            "関数値の引数には`".as_bytes(),
                            &self.type_text_of(pt),
                            "`が必要ですが、`".as_bytes(),
                            &self.type_text_of(arg_types[i]),
                            "`が指定されています".as_bytes(),
                        ]);
                        self.err_node(Some(node), msg)?;
                    }
                }
            }
        }
        Ok(t.func_ret.unwrap_or(TY_UNKNOWN_ID))
    }

    fn check_funcall(
        &mut self,
        node: NodeId,
        expected: Option<TypeId>,
    ) -> Result<TypeId, Flow> {
        let (name, args) = match &self.nodes[node as usize].data {
            NodeData::Funcall { name, args, .. } => (*name, *args),
            _ => return Ok(TY_UNKNOWN_ID),
        };
        let callee = self.find_callable(None, name);
        let klass = if callee.is_none() {
            self.find_class(name)
        } else {
            None
        };
        // 引数の期待型を先に用意する（引数位置のラムダの型を決めるため）
        let mut expected_list: [Option<TypeId>; 6] = [None; 6];
        let mut use_expected = false;
        if let Some(f) = callee {
            use_expected = true;
            let fp = self.funcs[f as usize].param_types;
            for i in 0..(self.funcs[f as usize].nparams as usize).min(6) {
                expected_list[i] = fp[i];
            }
        } else if let Some(k) = klass {
            if let Some(init) = self.find_callable_lit(Some(k), "__init__") {
                use_expected = true;
                let ip = self.funcs[init as usize].param_types;
                let inp = self.funcs[init as usize].nparams;
                for i in 0..6 {
                    if (i as i32) + 1 < inp {
                        expected_list[i] = ip[i + 1];
                    }
                }
            }
        }
        let (nargs, arg_types) = self.check_args_expected(args, &expected_list, use_expected)?;
        if self.name_eq_lit(name, "print") && callee.is_none() && klass.is_none() {
            if nargs != 1 {
                let msg = crate::msg(&[
                    "`print`の引数は1個ですが、".as_bytes(),
                    nargs.to_string().as_bytes(),
                    "個が指定されています".as_bytes(),
                ]);
                self.err_node(Some(node), msg)?;
                return Ok(TY_UNKNOWN_ID);
            }
            let at = arg_types[0];
            if self.types[at as usize].kind == TypeKind::Unknown {
                self.err_node(
                    Some(node),
                    "型が分からない値は print できません".as_bytes().to_vec(),
                )?;
                return Ok(TY_UNKNOWN_ID);
            }
            let k2 = self.class_of(at)?;
            let method = k2.and_then(|k| self.find_callable_lit(Some(k), "__str__"));
            let method = match method {
                Some(m) => m,
                None => {
                    let msg = crate::msg(&[
                        "`".as_bytes(),
                        &self.type_text_of(at),
                        "`型の値は print できません".as_bytes(),
                    ]);
                    self.err_node(Some(node), msg)?;
                    return Ok(TY_UNKNOWN_ID);
                }
            };
            let k2 = k2.unwrap();
            {
                let (m_nparams, m_ret) = {
                    let m = &self.funcs[method as usize];
                    (m.nparams, m.return_type)
                };
                let str_class = {
                    let n = self.intern_str("str");
                    self.find_class(n)
                };
                let bad_ret = match m_ret {
                    Some(rt) => {
                        self.types[rt as usize].kind != TypeKind::Unknown
                            && !(self.types[rt as usize].kind == TypeKind::Class
                                && self.types[rt as usize].klass == str_class)
                    }
                    None => false,
                };
                if m_nparams != 1 || bad_ret {
                    let msg = crate::msg(&[
                        "クラス`".as_bytes(),
                        &self.name_bytes(self.classes[k2 as usize].name).to_vec(),
                        "`の`__str__`は引数を取らず、`str`を返す必要があります".as_bytes(),
                    ]);
                    self.err_node(Some(node), msg)?;
                }
            }
            // print(x) を __println(x.__str__()) へ書き換える
            let str_name = self.intern_str("__str__");
            let conv = self.new_node(NodeData::MCall {
                left: args.unwrap(),
                args: None,
                name: str_name,
                func: Some(method),
                klass: Some(k2),
            });
            let println_name = self.intern_str("__println");
            if let NodeData::Funcall {
                name: nm,
                args: ag,
                ..
            } = &mut self.nodes[node as usize].data
            {
                *ag = Some(conv);
                *nm = println_name;
            }
            return Ok(TY_UNKNOWN_ID);
        }
        if callee.is_none() {
            if let Some(k) = klass {
                return self.check_construct(node, k, &arg_types, nargs, expected);
            }
            if self.name_eq_lit(name, "getchar") || self.name_eq_lit(name, "putchar") {
                return Ok(TY_INT_ID);
            }
            return Ok(TY_UNKNOWN_ID);
        }
        let callee = callee.unwrap();
        let cf = self.funcs[callee as usize].clone();
        if nargs != cf.nparams {
            let msg = crate::msg(&[
                "関数`".as_bytes(),
                &self.name_bytes(name).to_vec(),
                "`の引数は".as_bytes(),
                cf.nparams.to_string().as_bytes(),
                "個ですが、".as_bytes(),
                nargs.to_string().as_bytes(),
                "個が指定されています".as_bytes(),
            ]);
            self.err_node(Some(node), msg)?;
        }
        for i in 0..nargs as usize {
            if (i as i32) < cf.nparams && i < 6 {
                if let Some(pt) = cf.param_types[i] {
                    if self.types[arg_types[i] as usize].kind != TypeKind::Unknown
                        && !self.type_equals(pt, arg_types[i])
                    {
                        let msg = crate::msg(&[
                            "関数`".as_bytes(),
                            &self.name_bytes(name).to_vec(),
                            "`の引数には`".as_bytes(),
                            &self.type_text_of(pt),
                            "`が必要ですが、`".as_bytes(),
                            &self.type_text_of(arg_types[i]),
                            "`が指定されています".as_bytes(),
                        ]);
                        self.err_node(Some(node), msg)?;
                    }
                }
            }
        }
        Ok(cf.return_type.unwrap_or(TY_UNKNOWN_ID))
    }
}

impl Cx {
    fn check_member(&mut self, node: NodeId) -> Result<TypeId, Flow> {
        let (left, name) = match &self.nodes[node as usize].data {
            NodeData::Member { left, name, .. } => (*left, *name),
            _ => return Ok(TY_UNKNOWN_ID),
        };
        let obj = self.check_node(Some(left))?;
        if self.types[obj as usize].kind == TypeKind::Unknown {
            if self.name_eq_lit(name, "len") {
                self.nodes[node as usize].data = NodeData::Len { left };
                return Ok(TY_INT_ID);
            }
            return Ok(TY_UNKNOWN_ID);
        }
        if self.types[obj as usize].kind == TypeKind::Array && self.name_eq_lit(name, "len") {
            self.nodes[node as usize].data = NodeData::Len { left };
            return Ok(TY_INT_ID);
        }
        let klass = self.class_of(obj)?;
        let klass = match klass {
            Some(k) => k,
            None => {
                let msg = crate::msg(&[
                    "`".as_bytes(),
                    &self.type_text_of(obj),
                    "`型の値にフィールドは使えません".as_bytes(),
                ]);
                self.err_node(Some(node), msg)?;
                return Ok(TY_UNKNOWN_ID);
            }
        };
        if let NodeData::Member { klass: k, .. } = &mut self.nodes[node as usize].data {
            *k = Some(klass);
        }
        // フィールド優先
        let mut fcur = self.classes[klass as usize].fields;
        while let Some(f) = fcur {
            let ff = &self.fields[f as usize];
            if self.name_eq(ff.name, name) {
                let (foff, fty, _) = (ff.offset, ff.ty, ff.next);
                if let NodeData::Member { offset, .. } = &mut self.nodes[node as usize].data {
                    *offset = foff;
                }
                return Ok(fty);
            }
            fcur = ff.next;
        }
        let method = self.find_callable(Some(klass), name);
        if let Some(m) = method {
            let mf = self.funcs[m as usize].clone();
            if mf.nparams > 1 {
                let msg = crate::msg(&[
                    "メソッド`".as_bytes(),
                    &self.name_bytes(name).to_vec(),
                    "`には引数が必要ですが、括弧なしで使われています".as_bytes(),
                ]);
                self.err_node(Some(node), msg)?;
            }
            if let NodeData::Member { func, .. } = &mut self.nodes[node as usize].data {
                *func = Some(m);
            }
            return Ok(mf.return_type.unwrap_or(TY_UNKNOWN_ID));
        }
        let msg = crate::msg(&[
            "クラス`".as_bytes(),
            &self.name_bytes(self.classes[klass as usize].name).to_vec(),
            "`にフィールド`".as_bytes(),
            &self.name_bytes(name).to_vec(),
            "`は定義されていません".as_bytes(),
        ]);
        self.err_node(Some(node), msg)?;
        Ok(TY_UNKNOWN_ID)
    }

    fn check_mcall(&mut self, node: NodeId) -> Result<TypeId, Flow> {
        let (left, args, name) = match &self.nodes[node as usize].data {
            NodeData::MCall {
                left, args, name, ..
            } => (*left, *args, *name),
            _ => return Ok(TY_UNKNOWN_ID),
        };
        let obj = self.check_node(Some(left))?;
        if self.name_eq_lit(name, "len")
            && (self.types[obj as usize].kind == TypeKind::Array
                || self.types[obj as usize].kind == TypeKind::Unknown)
        {
            let (nargs, _) = self.check_args(args)?;
            if nargs != 0 {
                let msg = crate::msg(&[
                    "`len`の引数は0個ですが、".as_bytes(),
                    nargs.to_string().as_bytes(),
                    "個が指定されています".as_bytes(),
                ]);
                self.err_node(Some(node), msg)?;
            }
            self.nodes[node as usize].data = NodeData::Len { left };
            return Ok(TY_INT_ID);
        }
        if self.types[obj as usize].kind == TypeKind::Unknown {
            self.check_args(args)?;
            return Ok(TY_UNKNOWN_ID);
        }
        let klass = self.class_of(obj)?;
        let klass = match klass {
            Some(k) => k,
            None => {
                let msg = crate::msg(&[
                    "`".as_bytes(),
                    &self.type_text_of(obj),
                    "`型の値にメソッドは使えません".as_bytes(),
                ]);
                self.err_node(Some(node), msg)?;
                return Ok(TY_UNKNOWN_ID);
            }
        };
        let method = self.find_callable(Some(klass), name);
        if method.is_none() {
            // フィールドなら関数型フィールドの呼び出し（ND_VCALL）へ書き換える
            let mut fcur = self.classes[klass as usize].fields;
            let mut found: Option<(i32, TypeId)> = None;
            while let Some(f) = fcur {
                let ff = &self.fields[f as usize];
                if self.name_eq(ff.name, name) {
                    found = Some((ff.offset, ff.ty));
                    break;
                }
                fcur = ff.next;
            }
            if let Some((foff, fty)) = found {
                let tok = self.nodes[node as usize].tok;
                let member = self.new_node(NodeData::Member {
                    left,
                    name,
                    offset: foff,
                    func: None,
                    klass: Some(klass),
                });
                self.nodes[member as usize].tok = tok;
                self.nodes[member as usize].ty = Some(fty);
                self.nodes[node as usize].data = NodeData::VCall {
                    left: member,
                    args,
                };
                return self.check_vcall(node);
            }
            let msg = crate::msg(&[
                "クラス`".as_bytes(),
                &self.name_bytes(self.classes[klass as usize].name).to_vec(),
                "`にメソッド`".as_bytes(),
                &self.name_bytes(name).to_vec(),
                "`は定義されていません".as_bytes(),
            ]);
            self.err_node(Some(node), msg)?;
            return Ok(TY_UNKNOWN_ID);
        }
        let method = method.unwrap();
        if let NodeData::MCall { func, klass: k, .. } = &mut self.nodes[node as usize].data {
            *func = Some(method);
            *k = Some(klass);
        }
        let mf = self.funcs[method as usize].clone();
        let mut expected_list: [Option<TypeId>; 6] = [None; 6];
        for i in 0..6 {
            if (i as i32) + 1 < mf.nparams {
                expected_list[i] = mf.param_types[i + 1];
            }
        }
        let (nargs, arg_types) = self.check_args_expected(args, &expected_list, true)?;
        if nargs != mf.nparams - 1 {
            let msg = crate::msg(&[
                "クラス`".as_bytes(),
                &self.name_bytes(self.classes[klass as usize].name).to_vec(),
                "`のメソッド`".as_bytes(),
                &self.name_bytes(name).to_vec(),
                "`の引数は".as_bytes(),
                (mf.nparams - 1).to_string().as_bytes(),
                "個ですが、".as_bytes(),
                nargs.to_string().as_bytes(),
                "個が指定されています".as_bytes(),
            ]);
            self.err_node(Some(node), msg)?;
        }
        for i in 0..nargs as usize {
            if (i as i32) + 1 < mf.nparams {
                if let Some(pt) = mf.param_types[i + 1] {
                    if self.types[arg_types[i] as usize].kind != TypeKind::Unknown
                        && !self.type_equals(pt, arg_types[i])
                    {
                        let msg = crate::msg(&[
                            "クラス`".as_bytes(),
                            &self.name_bytes(self.classes[klass as usize].name).to_vec(),
                            "`のメソッド`".as_bytes(),
                            &self.name_bytes(name).to_vec(),
                            "`の引数には`".as_bytes(),
                            &self.type_text_of(pt),
                            "`が必要ですが、`".as_bytes(),
                            &self.type_text_of(arg_types[i]),
                            "`が指定されています".as_bytes(),
                        ]);
                        self.err_node(Some(node), msg)?;
                    }
                }
            }
        }
        Ok(mf.return_type.unwrap_or(TY_UNKNOWN_ID))
    }

    fn check_arraylit(
        &mut self,
        node: NodeId,
        expected: Option<TypeId>,
    ) -> Result<TypeId, Flow> {
        let body = match &self.nodes[node as usize].data {
            NodeData::Array { body, .. } => *body,
            _ => return Ok(TY_UNKNOWN_ID),
        };
        let mut elem = TY_UNKNOWN_ID;
        let elem_expected = expected.and_then(|e| {
            if self.types[e as usize].kind == TypeKind::Array {
                self.types[e as usize].elem
            } else {
                None
            }
        });
        let mut cur = body;
        while let Some(e) = cur {
            self.expected_type = elem_expected;
            let et = self.check_node(Some(e))?;
            self.expected_type = None;
            cur = self.nodes[e as usize].next;
            if self.types[et as usize].kind == TypeKind::Unknown {
                continue;
            }
            if self.types[elem as usize].kind == TypeKind::Unknown {
                elem = et;
                continue;
            }
            if !self.type_equals(elem, et) {
                let msg = crate::msg(&[
                    "配列の要素は`".as_bytes(),
                    &self.type_text_of(elem),
                    "`ですが、`".as_bytes(),
                    &self.type_text_of(et),
                    "`が含まれています".as_bytes(),
                ]);
                self.err_node(Some(e), msg)?;
            }
        }
        Ok(self.array_type(elem))
    }

    fn check_index(&mut self, node: NodeId) -> Result<TypeId, Flow> {
        let (left, right) = match &self.nodes[node as usize].data {
            NodeData::Index { left, right } => (*left, *right),
            _ => return Ok(TY_UNKNOWN_ID),
        };
        let lt = self.check_node(Some(left))?;
        let it = self.check_node(Some(right))?;
        if self.types[lt as usize].kind == TypeKind::Class {
            let k = self.class_of(lt)?;
            let method = self.find_callable_lit(k, "__getitem__");
            if method.is_none() {
                let msg = crate::msg(&[
                    "`".as_bytes(),
                    &self.type_text_of(lt),
                    "`型の値に添え字は使えません".as_bytes(),
                ]);
                self.err_node(Some(node), msg)?;
                return Ok(TY_UNKNOWN_ID);
            }
            return self.rewrite_to_mcall(node, lt, it, "__getitem__", method);
        }
        if self.types[lt as usize].kind != TypeKind::Unknown
            && self.types[lt as usize].kind != TypeKind::Array
        {
            let msg = crate::msg(&[
                "`".as_bytes(),
                &self.type_text_of(lt),
                "`型の値に添え字は使えません".as_bytes(),
            ]);
            self.err_node(Some(node), msg)?;
        }
        if self.types[it as usize].kind != TypeKind::Unknown
            && self.types[it as usize].kind != TypeKind::Int
        {
            let msg = crate::msg(&[
                "添え字には`int`が必要ですが、`".as_bytes(),
                &self.type_text_of(it),
                "`が指定されています".as_bytes(),
            ]);
            self.err_node(Some(right), msg)?;
        }
        if self.types[lt as usize].kind == TypeKind::Array {
            Ok(self.types[lt as usize].elem.unwrap_or(TY_UNKNOWN_ID))
        } else {
            Ok(TY_UNKNOWN_ID)
        }
    }

    fn check_return(&mut self, node: NodeId) -> Result<TypeId, Flow> {
        let left = match &self.nodes[node as usize].data {
            NodeData::Return { left } => *left,
            _ => return Ok(TY_UNKNOWN_ID),
        };
        self.expected_type = self.current_return;
        let rt = self.check_node(Some(left))?;
        self.expected_type = None;
        if self.types[rt as usize].kind != TypeKind::Unknown {
            match self.current_return {
                None => {
                    if let Some(f) = self.current_fn {
                        self.funcs[f as usize].return_type = Some(rt);
                    }
                    self.current_return = Some(rt);
                }
                Some(cr) => {
                    if !self.type_equals(cr, rt) {
                        let fname = self
                            .current_fn
                            .map(|f| self.funcs[f as usize].name)
                            .unwrap_or_default();
                        let msg = crate::msg(&[
                            "関数`".as_bytes(),
                            &self.name_bytes(fname).to_vec(),
                            "`の戻り値は`".as_bytes(),
                            &self.type_text_of(cr),
                            "`ですが、`".as_bytes(),
                            &self.type_text_of(rt),
                            "`が返されています".as_bytes(),
                        ]);
                        self.err_node(Some(node), msg)?;
                    }
                }
            }
        }
        Ok(rt)
    }
}

impl Cx {
    fn iter_slot_node(&mut self, iter_offset: i32) -> NodeId {
        self.new_node(NodeData::LVar {
            name: Name::default(),
            offset: iter_offset,
        })
    }

    fn make_mcall(
        &mut self,
        receiver: NodeId,
        klass: Option<ClassId>,
        func: Option<FunctionId>,
        name: &str,
    ) -> NodeId {
        let name_id = self.intern_str(name);
        self.new_node(NodeData::MCall {
            left: receiver,
            args: None,
            name: name_id,
            func,
            klass,
        })
    }

    fn is_loop_var(&self, target: NodeId, var: NodeId) -> bool {
        match (&self.nodes[var as usize].data, &self.nodes[target as usize].data) {
            (NodeData::LVar { offset: vo, .. }, NodeData::LVar { offset: to, .. }) => vo == to,
            (NodeData::GVar { name: vn }, NodeData::GVar { name: tn }) => {
                self.name_eq(*vn, *tn)
            }
            _ => false,
        }
    }

    // C のフィールド順（left, right, cond, then, els, init, body,
    // args, next）で子を列挙する（find_var_assign 用）
    fn ordered_children(&self, id: NodeId) -> Vec<NodeId> {
        let n = &self.nodes[id as usize];
        let mut out: Vec<NodeId> = Vec::new();
        match &n.data {
            NodeData::Assign { left, right, .. } | NodeData::OpAssign { left, right, .. } => {
                out.push(*left);
                out.push(*right);
            }
            NodeData::Return { left }
            | NodeData::Len { left }
            | NodeData::Unary { left, .. }
            | NodeData::Member { left, .. } => out.push(*left),
            NodeData::If { cond, then, els } => {
                out.push(*cond);
                out.push(*then);
                if let Some(e) = els {
                    out.push(*e);
                }
            }
            NodeData::While { cond, then } => {
                out.push(*cond);
                out.push(*then);
            }
            NodeData::For {
                init,
                var,
                then,
                els,
                ..
            } => {
                out.push(*var);
                out.push(*then);
                if let Some(e) = els {
                    out.push(*e);
                }
                out.push(*init);
            }
            NodeData::Block { body } | NodeData::Array { body, .. } => {
                if let Some(b) = body {
                    out.push(*b);
                }
            }
            NodeData::Index { left, right } | NodeData::NewArray { fill: left, len: right } => {
                out.push(*left);
                out.push(*right);
            }
            NodeData::Binop { left, right, .. } => {
                out.push(*left);
                if let Some(r) = right {
                    out.push(*r);
                }
            }
            NodeData::MCall { left, args, .. } | NodeData::VCall { left, args } => {
                out.push(*left);
                if let Some(a) = args {
                    out.push(*a);
                }
            }
            NodeData::MultiAssign {
                targets, values, ..
            } => {
                if let Some(t) = targets {
                    out.push(*t);
                }
                if let Some(v) = values {
                    out.push(*v);
                }
            }
            NodeData::Chain { first, pairs, .. } => {
                out.push(*first);
                if let Some(p) = pairs {
                    out.push(*p);
                }
            }
            NodeData::Funcall { args, .. }
            | NodeData::Input { args, .. }
            | NodeData::Put { args } => {
                if let Some(a) = args {
                    out.push(*a);
                }
            }
            _ => {}
        }
        if let Some(next) = n.next {
            out.push(next);
        }
        out
    }

    // ループ本体の中からループ変数への代入を探す（ラムダ本体には入らない）
    fn find_var_assign(&self, node: Option<NodeId>, var: NodeId) -> Option<NodeId> {
        let node = node?;
        match &self.nodes[node as usize].data {
            NodeData::Lambda { .. } => return None,
            NodeData::Assign { left, .. } => {
                if self.is_loop_var(*left, var) {
                    return Some(*left);
                }
            }
            NodeData::MultiAssign { targets, .. } => {
                let mut cur = *targets;
                while let Some(t) = cur {
                    if self.is_loop_var(t, var) {
                        return Some(t);
                    }
                    cur = self.nodes[t as usize].next;
                }
            }
            _ => {}
        }
        for child in self.ordered_children(node) {
            if let Some(found) = self.find_var_assign(Some(child), var) {
                return Some(found);
            }
        }
        None
    }

    fn make_set_current_call(
        &mut self,
        iter_offset: i32,
        var: NodeId,
        iklass: Option<ClassId>,
        set_current_fn: Option<FunctionId>,
    ) -> NodeId {
        let arg = {
            let v = &self.nodes[var as usize];
            let tok = v.tok;
            let data = match &v.data {
                NodeData::LVar { name, offset } => NodeData::LVar {
                    name: *name,
                    offset: *offset,
                },
                NodeData::GVar { name } => NodeData::GVar { name: *name },
                _ => NodeData::LVar {
                    name: Name::default(),
                    offset: 0,
                },
            };
            let id = self.new_node(data);
            self.nodes[id as usize].tok = tok;
            id
        };
        let slot = self.iter_slot_node(iter_offset);
        let call = self.make_mcall(slot, iklass, set_current_fn, "__set_current__");
        if let NodeData::MCall { args, .. } = &mut self.nodes[call as usize].data {
            *args = Some(arg);
        }
        call
    }

    // このループ宛の break / continue を「書き戻してから」に書き換える
    fn wrap_loop_exits(
        &mut self,
        node: Option<NodeId>,
        iter_offset: i32,
        var: NodeId,
        iklass: Option<ClassId>,
        set_current_fn: Option<FunctionId>,
    ) {
        let node = match node {
            Some(n) => n,
            None => return,
        };
        match &self.nodes[node as usize].data.clone() {
            NodeData::Break | NodeData::Continue => {
                let is_break = matches!(self.nodes[node as usize].data, NodeData::Break);
                let tok = self.nodes[node as usize].tok;
                let call = self.make_set_current_call(iter_offset, var, iklass, set_current_fn);
                let orig = self.new_node(if is_break {
                    NodeData::Break
                } else {
                    NodeData::Continue
                });
                self.nodes[orig as usize].tok = tok;
                self.nodes[call as usize].next = Some(orig);
                self.nodes[node as usize].data = NodeData::Block { body: Some(call) };
            }
            NodeData::While { .. } => {}
            NodeData::For { els, .. } => {
                let els = *els;
                self.wrap_loop_exits(els, iter_offset, var, iklass, set_current_fn);
            }
            NodeData::Block { body } => {
                let mut cur = *body;
                while let Some(s) = cur {
                    self.wrap_loop_exits(Some(s), iter_offset, var, iklass, set_current_fn);
                    cur = self.nodes[s as usize].next;
                }
            }
            NodeData::If { then, els, .. } => {
                let (then, els) = (*then, *els);
                self.wrap_loop_exits(Some(then), iter_offset, var, iklass, set_current_fn);
                self.wrap_loop_exits(els, iter_offset, var, iklass, set_current_fn);
            }
            _ => {}
        }
    }

    // for...else 用: この for 宛の break を「フラグを倒してから break」に
    fn rewrite_breaks_stmt(&mut self, node: Option<NodeId>, flag_offset: i32) {
        let node = match node {
            Some(n) => n,
            None => return,
        };
        match &self.nodes[node as usize].data.clone() {
            NodeData::Break => {
                let flag = self.new_node(NodeData::LVar {
                    name: Name::default(),
                    offset: flag_offset,
                });
                let zero = self.new_node(NodeData::Num { val: 0 });
                let brk = self.new_node(NodeData::Break);
                let clear = self.new_node(NodeData::Assign {
                    left: flag,
                    right: zero,
                    anno: None,
                });
                self.nodes[clear as usize].next = Some(brk);
                self.nodes[node as usize].data = NodeData::Block { body: Some(clear) };
            }
            NodeData::While { .. } => {}
            NodeData::For { els, .. } => {
                let els = *els;
                self.rewrite_breaks_stmt(els, flag_offset);
            }
            NodeData::Block { body } => {
                let mut cur = *body;
                while let Some(s) = cur {
                    self.rewrite_breaks_stmt(Some(s), flag_offset);
                    cur = self.nodes[s as usize].next;
                }
            }
            NodeData::If { then, els, .. } => {
                let (then, els) = (*then, *els);
                self.rewrite_breaks_stmt(Some(then), flag_offset);
                self.rewrite_breaks_stmt(els, flag_offset);
            }
            _ => {}
        }
    }

    fn check_for(&mut self, node: NodeId) -> Result<TypeId, Flow> {
        let (init, var, then, els, iter_offset, flag) = match &self.nodes[node as usize].data {
            NodeData::For {
                init,
                var,
                then,
                els,
                iter_offset,
                flag,
            } => (*init, *var, *then, *els, *iter_offset, *flag),
            _ => return Ok(TY_UNKNOWN_ID),
        };
        let iterable = self.check_node(Some(init))?;
        // プロトコル検査。壊れている場合は while への書き換えはしない
        let mut klass: Option<ClassId> = None;
        let mut iklass: Option<ClassId> = None;
        let mut iter_fn: Option<FunctionId> = None;
        let mut has_next_fn: Option<FunctionId> = None;
        let mut next_fn: Option<FunctionId> = None;
        let mut set_current_fn: Option<FunctionId> = None;
        let mut elem = TY_UNKNOWN_ID;
        let mut protocol_ok = true;
        if self.types[iterable as usize].kind == TypeKind::Unknown {
            self.err_node(
                Some(init),
                "型が分からない値は反復できません".as_bytes().to_vec(),
            )?;
            protocol_ok = false;
        }
        if protocol_ok {
            klass = self.class_of(iterable)?;
            iter_fn = klass.and_then(|k| self.find_callable_lit(Some(k), "__iter__"));
            if iter_fn.is_none() {
                let msg = crate::msg(&[
                    "`".as_bytes(),
                    &self.type_text_of(iterable),
                    "`型の値は反復できません".as_bytes(),
                ]);
                self.err_node(Some(init), msg)?;
                protocol_ok = false;
            }
        }
        if protocol_ok {
            let k = klass.unwrap();
            let f = iter_fn.unwrap();
            if self.funcs[f as usize].nparams != 1 {
                let msg = crate::msg(&[
                    "クラス`".as_bytes(),
                    &self.name_bytes(self.classes[k as usize].name).to_vec(),
                    "`の`__iter__`は引数を取れません".as_bytes(),
                ]);
                self.err_node(Some(node), msg)?;
            }
            let iter_type = self.funcs[f as usize].return_type;
            let ik = match iter_type {
                Some(t) if self.types[t as usize].kind == TypeKind::Class => {
                    self.class_of(t)?
                }
                _ => None,
            };
            match ik {
                Some(k2) => iklass = Some(k2),
                None => {
                    let msg = crate::msg(&[
                        "クラス`".as_bytes(),
                        &self.name_bytes(self.classes[k as usize].name).to_vec(),
                        "`の`__iter__`はイテレータを返す必要があります".as_bytes(),
                    ]);
                    self.err_node(Some(node), msg)?;
                    protocol_ok = false;
                }
            }
        }
        if protocol_ok {
            let ik = iklass.unwrap();
            has_next_fn = self.find_callable_lit(Some(ik), "__has_next__");
            next_fn = self.find_callable_lit(Some(ik), "__next__");
            if has_next_fn.is_none() || next_fn.is_none() {
                let msg = crate::msg(&[
                    "クラス`".as_bytes(),
                    &self.name_bytes(self.classes[ik as usize].name).to_vec(),
                    "`には`__has_next__`と`__next__`が必要です".as_bytes(),
                ]);
                self.err_node(Some(node), msg)?;
                protocol_ok = false;
            }
        }
        if protocol_ok {
            let ik = iklass.unwrap();
            let hn = has_next_fn.unwrap();
            let nf = next_fn.unwrap();
            if self.funcs[hn as usize].nparams != 1 {
                let msg = crate::msg(&[
                    "クラス`".as_bytes(),
                    &self.name_bytes(self.classes[ik as usize].name).to_vec(),
                    "`の`__has_next__`は引数を取れません".as_bytes(),
                ]);
                self.err_node(Some(node), msg)?;
            }
            if self.funcs[nf as usize].nparams != 1 {
                let msg = crate::msg(&[
                    "クラス`".as_bytes(),
                    &self.name_bytes(self.classes[ik as usize].name).to_vec(),
                    "`の`__next__`は引数を取れません".as_bytes(),
                ]);
                self.err_node(Some(node), msg)?;
            }
            if let Some(rt) = self.funcs[hn as usize].return_type {
                let bt = self.bool_type();
                if !self.type_equals(rt, bt) {
                    let msg = crate::msg(&[
                        "クラス`".as_bytes(),
                        &self.name_bytes(self.classes[ik as usize].name).to_vec(),
                        "`の`__has_next__`は`bool`を返す必要があります".as_bytes(),
                    ]);
                    self.err_node(Some(node), msg)?;
                }
            }
            elem = self.funcs[nf as usize].return_type.unwrap_or(TY_UNKNOWN_ID);
            set_current_fn = self.find_callable_lit(Some(ik), "__set_current__");
            if let Some(sf) = set_current_fn {
                if self.funcs[sf as usize].nparams != 2 {
                    let msg = crate::msg(&[
                        "クラス`".as_bytes(),
                        &self.name_bytes(self.classes[ik as usize].name).to_vec(),
                        "`の`__set_current__`は引数を1つ取る必要があります".as_bytes(),
                    ]);
                    self.err_node(Some(node), msg)?;
                }
                if let Some(pt) = self.funcs[sf as usize].param_types[1] {
                    if self.types[elem as usize].kind != TypeKind::Unknown
                        && !self.type_equals(pt, elem)
                    {
                        let msg = crate::msg(&[
                            "クラス`".as_bytes(),
                            &self.name_bytes(self.classes[ik as usize].name).to_vec(),
                            "`の`__set_current__`の引数は`".as_bytes(),
                            &self.type_text_of(elem),
                            "`である必要があります".as_bytes(),
                        ]);
                        self.err_node(Some(node), msg)?;
                    }
                }
            }
        }

        // while への書き換え部品（プロトコルが壊れていても組み立てる）
        let slot1 = self.iter_slot_node(iter_offset);
        let iter_call = self.make_mcall(init, klass, iter_fn, "__iter__");
        let get_iter = self.new_node(NodeData::Assign {
            left: slot1,
            right: iter_call,
            anno: None,
        });
        let slot2 = self.iter_slot_node(iter_offset);
        let next_call = self.make_mcall(slot2, iklass, next_fn, "__next__");
        let set_var = self.new_node(NodeData::Assign {
            left: var,
            right: next_call,
            anno: None,
        });
        self.nodes[set_var as usize].next = Some(then);
        let loop_body = self.new_node(NodeData::Block { body: Some(set_var) });
        let slot3 = self.iter_slot_node(iter_offset);
        let cond_call = self.make_mcall(slot3, iklass, has_next_fn, "__has_next__");
        let loop_node = self.new_node(NodeData::While {
            cond: cond_call,
            then: loop_body,
        });

        // ループ変数の型記録
        match &self.nodes[var as usize].data.clone() {
            NodeData::LVar { offset, .. } => {
                let off = *offset;
                let recorded = self.lt_get((off / 8) as usize);
                self.check_assign_type(var, recorded, elem)?;
                if recorded.is_none() && self.types[elem as usize].kind != TypeKind::Unknown {
                    self.lt_set((off / 8) as usize, Some(elem));
                }
                let v = self.lt_get((off / 8) as usize);
                self.nodes[var as usize].ty = v;
            }
            NodeData::GVar { name } => {
                let name = *name;
                let recorded = self.find_global_type(name);
                self.check_assign_type(var, Some(recorded), elem)?;
                if self.types[recorded as usize].kind == TypeKind::Unknown {
                    self.record_global_type(name, elem);
                }
                let v = self.find_global_type(name);
                self.nodes[var as usize].ty = Some(v);
            }
            _ => {}
        }
        // ループ変数への代入があれば書き戻す（step 58）
        // ponytail: return で抜ける場合は書き戻さない（C と同じ）
        let var_is_var = matches!(
            self.nodes[var as usize].data,
            NodeData::LVar { .. } | NodeData::GVar { .. }
        );
        if protocol_ok && var_is_var {
            let assign = self.find_var_assign(Some(then), var);
            if let Some(a) = assign {
                if set_current_fn.is_none() {
                    let msg = crate::msg(&[
                        "`".as_bytes(),
                        &self.type_text_of(iterable),
                        "`型の反復ではループ変数への代入は要素に書き戻されないため、代入できません"
                            .as_bytes(),
                    ]);
                    self.err_node(Some(a), msg)?;
                } else {
                    let iter_ret = iter_fn.and_then(|f| self.funcs[f as usize].return_type);
                    self.lt_set((iter_offset / 8) as usize, iter_ret);
                    self.wrap_loop_exits(Some(then), iter_offset, var, iklass, set_current_fn);
                    let mut tail = then;
                    while let Some(nx) = self.nodes[tail as usize].next {
                        tail = nx;
                    }
                    let call =
                        self.make_set_current_call(iter_offset, var, iklass, set_current_fn);
                    self.nodes[tail as usize].next = Some(call);
                }
            }
        }
        if els.is_some() {
            self.rewrite_breaks_stmt(Some(then), flag as i32);
        }
        self.check_node(Some(then))?;
        if let Some(e) = els {
            self.check_node(Some(e))?;
        }

        if !protocol_ok {
            // プロトコルが壊れている場合は while への書き換えをしない
            return Ok(TY_UNKNOWN_ID);
        }
        if let Some(e) = els {
            let flag_var = self.new_node(NodeData::LVar {
                name: Name::default(),
                offset: flag as i32,
            });
            let one = self.new_node(NodeData::Num { val: 1 });
            let flag_init = self.new_node(NodeData::Assign {
                left: flag_var,
                right: one,
                anno: None,
            });
            let cond_var = self.new_node(NodeData::LVar {
                name: Name::default(),
                offset: flag as i32,
            });
            let if_node = self.new_node(NodeData::If {
                cond: cond_var,
                then: e,
                els: None,
            });
            self.nodes[flag_init as usize].next = Some(get_iter);
            self.nodes[get_iter as usize].next = Some(loop_node);
            self.nodes[loop_node as usize].next = Some(if_node);
            self.nodes[node as usize].data = NodeData::Block {
                body: Some(flag_init),
            };
        } else {
            self.nodes[get_iter as usize].next = Some(loop_node);
            self.nodes[node as usize].data = NodeData::Block {
                body: Some(get_iter),
            };
        }
        Ok(TY_UNKNOWN_ID)
    }
}

impl Cx {
    // 複数代入: 値を全部評価して一時置き場へ退避し、それから対象へ代入する
    fn check_multiassign(&mut self, node: NodeId) -> Result<TypeId, Flow> {
        let (targets, values, offset) = match &self.nodes[node as usize].data {
            NodeData::MultiAssign {
                targets,
                values,
                offset,
                ..
            } => (*targets, *values, *offset),
            _ => return Ok(TY_UNKNOWN_ID),
        };
        let mut i: i32 = 0;
        let mut cur = values;
        while let Some(v) = cur {
            let vt = self.check_node(Some(v))?;
            self.lt_set(((offset + i * 8) / 8) as usize, Some(vt));
            i += 1;
            cur = self.nodes[v as usize].next;
        }
        let mut head: Option<NodeId> = None;
        let mut tail: Option<NodeId> = None;
        let mut append = |cx: &mut Cx, head: &mut Option<NodeId>, tail: &mut Option<NodeId>, a: NodeId| {
            match tail {
                Some(t) => cx.nodes[*t as usize].next = Some(a),
                None => *head = Some(a),
            }
            *tail = Some(a);
        };
        i = 0;
        let mut cur = values;
        while let Some(v) = cur {
            let slot = self.new_node(NodeData::LVar {
                name: Name::default(),
                offset: offset + i * 8,
            });
            let a = self.new_node(NodeData::Assign {
                left: slot,
                right: v,
                anno: None,
            });
            append(self, &mut head, &mut tail, a);
            i += 1;
            cur = self.nodes[v as usize].next;
        }
        i = 0;
        let mut cur = targets;
        while let Some(t) = cur {
            let slot = self.new_node(NodeData::LVar {
                name: Name::default(),
                offset: offset + i * 8,
            });
            let a = self.new_node(NodeData::Assign {
                left: t,
                right: slot,
                anno: None,
            });
            self.check_node(Some(a))?;
            append(self, &mut head, &mut tail, a);
            i += 1;
            cur = self.nodes[t as usize].next;
        }
        self.nodes[node as usize].data = NodeData::Block { body: head };
        Ok(TY_UNKNOWN_ID)
    }

    // 連鎖比較: 中間値を一時置き場へ退避しながら比較を && でつなぐ
    fn check_chain(&mut self, node: NodeId) -> Result<TypeId, Flow> {
        let (first, pairs, offset) = match &self.nodes[node as usize].data {
            NodeData::Chain {
                first,
                pairs,
                offset,
            } => (*first, *pairs, *offset),
            _ => return Ok(TY_UNKNOWN_ID),
        };
        let mut prev_operand = first;
        let mut combined: Option<NodeId> = None;
        let mut i: i32 = 0;
        let mut cur = pairs;
        while let Some(pair) = cur {
            let (pop, pleft, pval, ptok) = match &self.nodes[pair as usize].data {
                NodeData::Binop {
                    op,
                    left,
                    val,
                    ..
                } => (*op, *left, *val, self.nodes[pair as usize].tok),
                _ => return Ok(TY_UNKNOWN_ID),
            };
            let cur_offset = offset + (i % 2) * 8;
            let slot = self.new_node(NodeData::LVar {
                name: Name::default(),
                offset: cur_offset,
            });
            let store = self.new_node(NodeData::Assign {
                left: slot,
                right: pleft,
                anno: None,
            });
            let comp = self.new_node(NodeData::Binop {
                op: pop,
                left: if pval != 0 { store } else { prev_operand },
                right: Some(if pval != 0 { prev_operand } else { store }),
                val: 0,
            });
            self.nodes[comp as usize].tok = ptok;
            self.check_node(Some(comp))?;
            match combined {
                Some(prev) => {
                    let and_node = self.new_node(NodeData::Binop {
                        op: BinOp::And,
                        left: prev,
                        right: Some(comp),
                        val: 0,
                    });
                    let bt = self.bool_type();
                    self.nodes[and_node as usize].ty = Some(bt);
                    combined = Some(and_node);
                }
                None => combined = Some(comp),
            }
            let prev_slot = self.new_node(NodeData::LVar {
                name: Name::default(),
                offset: cur_offset,
            });
            prev_operand = prev_slot;
            i += 1;
            cur = self.nodes[pair as usize].next;
        }
        let combined = combined.unwrap();
        let nxt = self.nodes[node as usize].next;
        let comb = self.nodes[combined as usize].clone();
        self.nodes[node as usize] = comb;
        self.nodes[node as usize].next = nxt;
        Ok(self.bool_type())
    }

    // 入力構文: prelude の読み取り関数への呼び出しに置き換える
    fn check_input(&mut self, node: NodeId) -> Result<TypeId, Flow> {
        let (args, mode) = match &self.nodes[node as usize].data {
            NodeData::Input { args, mode } => (*args, *mode),
            _ => return Ok(TY_UNKNOWN_ID),
        };
        let mut nargs = 0;
        let mut cur = args;
        while let Some(a) = cur {
            nargs += 1;
            cur = self.nodes[a as usize].next;
        }
        let fname = if mode == 0 {
            match nargs {
                0 => "read_int",
                1 => "read_ints",
                _ => "read_ints2",
            }
        } else if mode == 1 {
            if nargs == 0 {
                "read_token"
            } else {
                "read_tokens"
            }
        } else {
            "read_line"
        };
        let name = self.intern_str(fname);
        self.nodes[node as usize].data = NodeData::Funcall {
            name,
            args,
            klass: None,
            func: None,
        };
        self.check_funcall(node, None)
    }

    // put 文: 各値を __str__ で文字列化し、空白区切りで出して改行する
    fn check_put(&mut self, node: NodeId) -> Result<TypeId, Flow> {
        let args = match &self.nodes[node as usize].data {
            NodeData::Put { args } => *args,
            _ => return Ok(TY_UNKNOWN_ID),
        };
        let str_class = {
            let n = self.intern_str("str");
            self.find_class(n)
        };
        let mut head: Option<NodeId> = None;
        let mut tail: Option<NodeId> = None;
        let mut i: i32 = 0;
        let mut cur = args;
        while let Some(arg) = cur {
            let next = self.nodes[arg as usize].next;
            if i > 0 {
                let sep_name = self.intern_str("putchar");
                let sp = self.new_node(NodeData::Num { val: 32 });
                let sep = self.new_node(NodeData::Funcall {
                    name: sep_name,
                    args: Some(sp),
                    klass: None,
                    func: None,
                });
                match tail {
                    Some(t) => self.nodes[t as usize].next = Some(sep),
                    None => head = Some(sep),
                }
                tail = Some(sep);
            }
            let at = self.check_node(Some(arg))?;
            let mut klass: Option<ClassId> = None;
            let mut method: Option<FunctionId> = None;
            if self.types[at as usize].kind == TypeKind::Unknown {
                self.err_node(
                    Some(arg),
                    "型が分からない値は put できません".as_bytes().to_vec(),
                )?;
            } else {
                klass = self.class_of(at)?;
                method = klass.and_then(|k| self.find_callable_lit(Some(k), "__str__"));
                if klass.is_none() || method.is_none() {
                    let msg = crate::msg(&[
                        "`".as_bytes(),
                        &self.type_text_of(at),
                        "`型の値は put できません".as_bytes(),
                    ]);
                    self.err_node(Some(arg), msg)?;
                } else {
                    let m = self.funcs[method.unwrap() as usize].clone();
                    let bad_ret = match m.return_type {
                        Some(rt) => {
                            self.types[rt as usize].kind != TypeKind::Unknown
                                && !(self.types[rt as usize].kind == TypeKind::Class
                                    && self.types[rt as usize].klass == str_class)
                        }
                        None => false,
                    };
                    if m.nparams != 1 || bad_ret {
                        let msg = crate::msg(&[
                            "クラス`".as_bytes(),
                            &self.name_bytes(self.classes[klass.unwrap() as usize].name)
                                .to_vec(),
                            "`の`__str__`は引数を取らず、`str`を返す必要があります".as_bytes(),
                        ]);
                        self.err_node(Some(arg), msg)?;
                    }
                }
            }
            let conv_name = self.intern_str("__str__");
            let conv = self.new_node(NodeData::MCall {
                left: arg,
                args: None,
                name: conv_name,
                func: method,
                klass,
            });
            let out_name = self.intern_str("print_str");
            let out = self.new_node(NodeData::Funcall {
                name: out_name,
                args: Some(conv),
                klass: None,
                func: None,
            });
            match tail {
                Some(t) => self.nodes[t as usize].next = Some(out),
                None => head = Some(out),
            }
            tail = Some(out);
            i += 1;
            cur = next;
        }
        let nl_name = self.intern_str("putchar");
        let lf = self.new_node(NodeData::Num { val: 10 });
        let nl = self.new_node(NodeData::Funcall {
            name: nl_name,
            args: Some(lf),
            klass: None,
            func: None,
        });
        match tail {
            Some(t) => self.nodes[t as usize].next = Some(nl),
            None => head = Some(nl),
        }
        self.nodes[node as usize].data = NodeData::Block { body: head };
        Ok(TY_UNKNOWN_ID)
    }

    pub fn check_node(&mut self, node: Option<NodeId>) -> Result<TypeId, Flow> {
        let node = match node {
            Some(n) => n,
            None => return Ok(TY_UNKNOWN_ID),
        };
        // 期待型は直属のラムダ・配列リテラル・構築だけが受け取る
        let kind = self.nodes[node as usize].kind();
        let my_expected = if matches!(
            kind,
            crate::NodeKind::Lambda | crate::NodeKind::Array | crate::NodeKind::Funcall
        ) {
            self.expected_type
        } else {
            None
        };
        self.expected_type = None;
        let mut result = TY_UNKNOWN_ID;
        match kind {
            crate::NodeKind::Num => result = TY_INT_ID,
            crate::NodeKind::LVar => {
                if let NodeData::LVar { offset, .. } = &self.nodes[node as usize].data {
                    let off = *offset;
                    result = self.lt_get((off / 8) as usize).unwrap_or(TY_UNKNOWN_ID);
                }
            }
            crate::NodeKind::CapVar => {
                let off = match &self.nodes[node as usize].data {
                    NodeData::CapVar { offset, .. } => *offset,
                    _ => 0,
                };
                result = TY_UNKNOWN_ID;
                let mut cur = self.current_captures;
                while let Some(c) = cur {
                    let cc = &self.captures[c as usize];
                    if cc.env_offset == off {
                        result = cc.ty.unwrap_or(TY_UNKNOWN_ID);
                        break;
                    }
                    cur = cc.next;
                }
            }
            crate::NodeKind::FuncRef => result = self.check_funcref(node)?,
            crate::NodeKind::Lambda => result = self.check_lambda(node, my_expected)?,
            crate::NodeKind::VCall => result = self.check_vcall(node)?,
            crate::NodeKind::Binop => result = self.check_binop(node)?,
            crate::NodeKind::Assign => result = self.check_assign(node)?,
            crate::NodeKind::GVar => {
                let name = self.node_name(node);
                result = self.find_global_type(name);
            }
            crate::NodeKind::Unary => {
                let (op, left) = match &self.nodes[node as usize].data {
                    NodeData::Unary { op, left } => (*op, *left),
                    _ => (UnaryOp::Not, node),
                };
                self.check_node(Some(left))?;
                if op == UnaryOp::Not {
                    result = self.bool_type();
                }
            }
            crate::NodeKind::Funcall => result = self.check_funcall(node, my_expected)?,
            crate::NodeKind::Array => result = self.check_arraylit(node, my_expected)?,
            crate::NodeKind::NewArray => {
                let (fill, len) = match &self.nodes[node as usize].data {
                    NodeData::NewArray { fill, len } => (*fill, *len),
                    _ => (node, node),
                };
                let fill_t = self.check_node(Some(fill))?;
                let len_t = self.check_node(Some(len))?;
                if self.types[len_t as usize].kind != TypeKind::Unknown
                    && self.types[len_t as usize].kind != TypeKind::Int
                {
                    let msg = crate::msg(&[
                        "長さには`int`が必要ですが、`".as_bytes(),
                        &self.type_text_of(len_t),
                        "`が指定されています".as_bytes(),
                    ]);
                    self.err_node(Some(len), msg)?;
                }
                result = self.array_type(fill_t);
            }
            crate::NodeKind::Index => result = self.check_index(node)?,
            crate::NodeKind::Len => {
                let left = match &self.nodes[node as usize].data {
                    NodeData::Len { left } => *left,
                    _ => node,
                };
                self.check_node(Some(left))?;
                result = TY_INT_ID;
            }
            crate::NodeKind::Block => {
                let body = match &self.nodes[node as usize].data {
                    NodeData::Block { body } => *body,
                    _ => None,
                };
                let mut cur = body;
                while let Some(s) = cur {
                    self.check_node(Some(s))?;
                    cur = self.nodes[s as usize].next;
                }
            }
            crate::NodeKind::If => {
                let (cond, then, els) = match &self.nodes[node as usize].data {
                    NodeData::If { cond, then, els } => (*cond, *then, *els),
                    _ => (node, node, None),
                };
                self.check_node(Some(cond))?;
                self.check_node(Some(then))?;
                self.check_node(els)?;
            }
            crate::NodeKind::While => {
                let (cond, then) = match &self.nodes[node as usize].data {
                    NodeData::While { cond, then } => (*cond, *then),
                    _ => (node, node),
                };
                self.check_node(Some(cond))?;
                self.check_node(Some(then))?;
            }
            crate::NodeKind::For => result = self.check_for(node)?,
            crate::NodeKind::MultiAssign => result = self.check_multiassign(node)?,
            crate::NodeKind::Chain => result = self.check_chain(node)?,
            crate::NodeKind::Input => result = self.check_input(node)?,
            crate::NodeKind::Put => result = self.check_put(node)?,
            crate::NodeKind::Return => result = self.check_return(node)?,
            crate::NodeKind::Member => result = self.check_member(node)?,
            crate::NodeKind::MCall => result = self.check_mcall(node)?,
            _ => {}
        }
        self.nodes[node as usize].ty = Some(result);
        Ok(result)
    }

    // ---- __init__ の全フィールド初期化チェック ----
    fn check_init_complete(
        &mut self,
        klass: ClassId,
        assigned: &[bool],
    ) -> Result<(), Flow> {
        let mut cur = self.classes[klass as usize].fields;
        while let Some(f) = cur {
            let ff = self.fields[f as usize].clone();
            if !assigned[(ff.offset / 8) as usize] {
                let msg = crate::msg(&[
                    "クラス`".as_bytes(),
                    &self.name_bytes(self.classes[klass as usize].name).to_vec(),
                    "`の`__init__`で、フィールド`".as_bytes(),
                    &self.name_bytes(ff.name).to_vec(),
                    "`が初期化されていません".as_bytes(),
                ]);
                self.err_plain(msg)?;
            }
            cur = ff.next;
        }
        Ok(())
    }

    fn check_init_if(
        &mut self,
        n: NodeId,
        klass: ClassId,
        assigned: &mut [bool],
        nfields: usize,
    ) -> Result<bool, Flow> {
        let (then, els) = match &self.nodes[n as usize].data {
            NodeData::If { then, els, .. } => (*then, *els),
            _ => return Ok(false),
        };
        let mut then_set = assigned.to_vec();
        let mut else_set = assigned.to_vec();
        let then_ends = self.check_init_stmts(Some(then), klass, &mut then_set, nfields)?;
        let else_ends = match els {
            Some(e) => self.check_init_stmts(Some(e), klass, &mut else_set, nfields)?,
            None => false,
        };
        if !then_ends && !else_ends {
            for i in 0..nfields {
                assigned[i] = then_set[i] && else_set[i];
            }
        } else if then_ends && !else_ends {
            assigned.copy_from_slice(&else_set);
        } else if !then_ends && else_ends {
            assigned.copy_from_slice(&then_set);
        }
        Ok(then_ends && else_ends)
    }

    fn check_init_stmt(
        &mut self,
        n: NodeId,
        klass: ClassId,
        assigned: &mut [bool],
        nfields: usize,
    ) -> Result<bool, Flow> {
        match &self.nodes[n as usize].data.clone() {
            NodeData::Assign { left, .. } => {
                if let NodeData::Member {
                    left: mleft,
                    offset,
                    ..
                } = &self.nodes[*left as usize].data
                {
                    if let NodeData::LVar { offset: lo, .. } =
                        &self.nodes[*mleft as usize].data
                    {
                        if *lo == 8 {
                            assigned[(*offset / 8) as usize] = true;
                        }
                    }
                }
                Ok(false)
            }
            NodeData::Block { body } => self.check_init_stmts(*body, klass, assigned, nfields),
            NodeData::Return { .. } => {
                self.check_init_complete(klass, assigned)?;
                Ok(true)
            }
            NodeData::Break | NodeData::Continue => Ok(true),
            NodeData::If { .. } => self.check_init_if(n, klass, assigned, nfields),
            NodeData::While { .. } => {
                // C では while 本体ではなく body フィールド（常に空）を
                // 読むため、本体中の代入は数えられない。そのまま再現する
                Ok(false)
            }
            _ => Ok(false),
        }
    }

    fn check_init_stmts(
        &mut self,
        stmt: Option<NodeId>,
        klass: ClassId,
        assigned: &mut [bool],
        nfields: usize,
    ) -> Result<bool, Flow> {
        let mut cur = stmt;
        while let Some(n) = cur {
            if self.check_init_stmt(n, klass, assigned, nfields)? {
                return Ok(true);
            }
            cur = self.nodes[n as usize].next;
        }
        Ok(false)
    }

    fn check_init(&mut self, func: FunctionId) -> Result<(), Flow> {
        let klass = match self.funcs[func as usize].klass {
            Some(k) => k,
            None => return Ok(()),
        };
        let mut nfields = 0usize;
        let mut cur = self.classes[klass as usize].fields;
        while let Some(f) = cur {
            nfields += 1;
            cur = self.fields[f as usize].next;
        }
        if nfields == 0 {
            return Ok(());
        }
        let mut assigned = vec![false; nfields];
        let body = self.funcs[func as usize].body;
        if !self.check_init_stmts(body, klass, &mut assigned, nfields)? {
            self.check_init_complete(klass, &assigned)?;
        }
        Ok(())
    }

    pub fn typecheck(&mut self, prog: Option<FunctionId>) -> Result<(), Flow> {
        self.functions_head = prog;
        // 1パス目: 本体の型検査。fn->next は検査後に読む（実体化で
        // 末尾に足された関数も C と同じく巡回する）
        let mut cur = prog;
        while let Some(f) = cur {
            let (fklass, is_lambda) = {
                let func = &self.funcs[f as usize];
                (func.klass, func.is_lambda)
            };
            let skip = is_lambda
                || fklass
                    .map(|k| self.classes[k as usize].tparam_count > 0)
                    .unwrap_or(false);
            if !skip {
                self.lt_clear();
                self.current_fn = Some(f);
                self.current_return = self.funcs[f as usize].return_type;
                let f2 = self.funcs[f as usize].clone();
                for i in 0..(f2.nparams as usize).min(6) {
                    if let Some(pt) = f2.param_types[i] {
                        self.lt_set(i + 1, Some(pt));
                    }
                }
                let mut cur2 = f2.body;
                while let Some(stmt) = cur2 {
                    self.check_node(Some(stmt))?;
                    cur2 = self.nodes[stmt as usize].next;
                }
            }
            cur = self.funcs[f as usize].next;
        }
        // 2パス目: __init__ の初期化チェック
        let mut cur = prog;
        while let Some(f) = cur {
            let (fklass, fname) = {
                let func = &self.funcs[f as usize];
                (func.klass, func.name)
            };
            let skip = fklass
                .map(|k| self.classes[k as usize].tparam_count > 0)
                .unwrap_or(false);
            if !skip {
                if fklass.is_some() && self.name_eq_lit(fname, "__init__") {
                    self.check_init(f)?;
                }
            }
            cur = self.funcs[f as usize].next;
        }
        Ok(())
    }
}
