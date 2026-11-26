// ジェネリクス単相化（generics.c の移植）。
//
// 実体化キャッシュの監査結果（C の generics.c）: ハッシュ表ではなく
// インスタンスの連結リストを「先頭から（＝最近作られたものから）」
// 線形探索し、origin 一致・targ_count 一致・各型引数の
// type_strict_equals（厳密比較）で一致判定する。Rust 側では
// instances を Vec に積み、逆順に走査して同じ順序を再現する。
use crate::{
    Capture, Class, ClassId, Cx, Field, Flow, FunctionId, NodeData, NodeId, Type, TypeId,
    TypeKind, MAX_TPARAMS, TY_INT_ID, TY_UNKNOWN_ID,
};

pub struct CloneCtx {
    pub memo: Vec<(NodeId, NodeId)>,
    pub cap_map: Vec<(u32, u32)>,
    pub cloned_lambdas: Vec<FunctionId>,
}

impl Cx {
    // 型が属するクラスを返す（generics.c の class_of）
    pub fn class_of(&mut self, t: TypeId) -> Result<Option<ClassId>, Flow> {
        match self.types[t as usize].kind {
            TypeKind::Class => {
                let k = self.types[t as usize].klass.unwrap();
                if self.classes[k as usize].tparam_count > 0 {
                    if self.types[t as usize].targ_count == 0 {
                        let nm = self.classes[k as usize].name;
                        let msg = crate::msg(&[
                            "クラス`".as_bytes(),
                            &self.name_bytes(nm).to_vec(),
                            "`には型引数が必要です".as_bytes(),
                        ]);
                        self.err_plain(msg)?;
                        // キャプチャ有効時はテンプレート自身を返す
                        return Ok(Some(k));
                    }
                    let targs = self.types[t as usize].targs;
                    let n = self.types[t as usize].targ_count;
                    let inst = self.instantiate(k, &targs, n)?;
                    return Ok(Some(inst));
                }
                Ok(Some(k))
            }
            TypeKind::Int => {
                let n = self.intern_str("int");
                Ok(self.find_class(n))
            }
            TypeKind::Array => {
                let an = self.intern_str("array");
                let arr = self.find_class(an);
                if let Some(a) = arr {
                    if self.classes[a as usize].tparam_count > 0 {
                        // 要素型が不明な配列は int の実体化で代用する
                        let elem = self.types[t as usize].elem;
                        let elem = match elem {
                            Some(e) if !self.type_has_unknown(e) => e,
                            _ => TY_INT_ID,
                        };
                        let mut targs: [Option<TypeId>; MAX_TPARAMS] = [None; MAX_TPARAMS];
                        targs[0] = Some(elem);
                        let inst = self.instantiate(a, &targs, 1)?;
                        return Ok(Some(inst));
                    }
                }
                Ok(arr)
            }
            _ => Ok(None),
        }
    }

    pub fn instantiate(
        &mut self,
        tmpl: ClassId,
        targs: &[Option<TypeId>; MAX_TPARAMS],
        targ_count: i32,
    ) -> Result<ClassId, Flow> {
        // 既存インスタンスを新しいものから探す（C のリスト先頭走査）
        for idx in (0..self.instances.len()).rev() {
            let c = self.instances[idx];
            let cc = &self.classes[c as usize];
            if cc.origin == Some(tmpl) && cc.targ_count == targ_count {
                let mut same = true;
                for i in 0..targ_count as usize {
                    let a = cc.targs[i].unwrap_or(TY_UNKNOWN_ID);
                    let b = targs[i].unwrap_or(TY_UNKNOWN_ID);
                    if !self.type_strict_equals(a, b) {
                        same = false;
                        break;
                    }
                }
                if same {
                    return Ok(c);
                }
            }
        }

        let inst = self.classes.len() as ClassId;
        let tmpl_ty = self.classes[tmpl as usize].ty;
        let inst_ty = if self.types[tmpl_ty as usize].kind == TypeKind::Array {
            self.array_type(targs[0].unwrap_or(TY_UNKNOWN_ID))
        } else {
            let mut t = Type::new(TypeKind::Class);
            t.klass = Some(inst);
            t.targ_count = targ_count;
            t.targs = *targs;
            self.new_type(t)
        };
        let mut ic = Class {
            next: None,
            name: self.classes[tmpl as usize].name,
            fields: None,
            ty: inst_ty,
            tparams: [None; MAX_TPARAMS],
            tparam_count: 0,
            origin: Some(tmpl),
            targs: *targs,
            targ_count,
        };
        // フィールドをテンプレートの並び順のまま複製する
        let mut field_ids: Vec<u32> = Vec::new();
        let mut fcur = self.classes[tmpl as usize].fields;
        while let Some(f) = fcur {
            let (fname, fty, foff, fnext) = {
                let ff = &self.fields[f as usize];
                (ff.name, ff.ty, ff.offset, ff.next)
            };
            let st = self
                .subst_type(Some(fty), tmpl, targs)
                .unwrap_or(TY_UNKNOWN_ID);
            let nf = self.new_field(Field {
                next: None,
                name: fname,
                ty: st,
                offset: foff,
            });
            field_ids.push(nf);
            fcur = fnext;
        }
        for w in field_ids.windows(2) {
            self.fields[w[0] as usize].next = Some(w[1]);
        }
        ic.fields = field_ids.first().copied();
        self.classes.push(ic);
        self.instances.push(inst);

        // メソッドを複製する（関数リストの元の並び順）
        let mut ctx = CloneCtx {
            memo: Vec::new(),
            cap_map: Vec::new(),
            cloned_lambdas: Vec::new(),
        };
        let mut fn_ids: Vec<FunctionId> = Vec::new();
        let mut fcur = self.functions_head;
        while let Some(f) = fcur {
            fn_ids.push(f);
            fcur = self.funcs[f as usize].next;
        }
        for f in fn_ids {
            if self.funcs[f as usize].klass != Some(tmpl) {
                continue;
            }
            let mut nf = self.funcs[f as usize].clone();
            nf.next = None;
            nf.klass = Some(inst);
            for i in 0..6 {
                nf.param_types[i] = self.subst_type(nf.param_types[i], tmpl, targs);
            }
            nf.param_types[0] = Some(inst_ty);
            nf.return_type = self.subst_type(nf.return_type, tmpl, targs);
            let body = nf.body;
            let nf_id = self.new_func(nf);
            ctx.memo.clear();
            let nb = match body {
                Some(b) => Some(self.clone_node(&mut ctx, b, tmpl, targs)?),
                None => None,
            };
            self.funcs[nf_id as usize].body = nb;
            self.append_func(nf_id);
        }
        // 複製中に生まれたラムダ関数を末尾へ
        while let Some(lf) = ctx.cloned_lambdas.pop() {
            self.append_func(lf);
        }
        Ok(inst)
    }

    fn append_func(&mut self, nf: FunctionId) {
        self.funcs[nf as usize].next = None;
        match self.functions_head {
            None => self.functions_head = Some(nf),
            Some(head) => {
                let mut tail = head;
                while let Some(n) = self.funcs[tail as usize].next {
                    tail = n;
                }
                self.funcs[tail as usize].next = Some(nf);
            }
        }
    }

    pub fn clone_lambda_func(
        &mut self,
        ctx: &mut CloneCtx,
        f: FunctionId,
        tmpl: ClassId,
        targs: &[Option<TypeId>; MAX_TPARAMS],
    ) -> Result<FunctionId, Flow> {
        let mut nf = self.funcs[f as usize].clone();
        nf.next = None;
        nf.lambda_checked = false;
        nf.func_type = None;
        for i in 0..6 {
            nf.param_types[i] = self.subst_type(nf.param_types[i], tmpl, targs);
        }
        nf.return_type = self.subst_type(nf.return_type, tmpl, targs);
        // 名前は 元名$g連番（連番は 0 始まり、C の clone_counter++）
        let counter = self.clone_counter;
        self.clone_counter += 1;
        let mut nb = self.name_bytes(nf.name).to_vec();
        nb.extend_from_slice(format!("$g{}", counter).as_bytes());
        nf.name = self.intern(&nb);

        let saved_map_n = ctx.cap_map.len();
        // 自分の捕捉を複製して対応表へ登録してから、本体を複製する
        let mut cap_ids: Vec<u32> = Vec::new();
        let mut ccur = self.funcs[f as usize].captures;
        while let Some(cap) = ccur {
            let oc = self.captures[cap as usize].clone();
            let mut nc = oc.clone();
            nc.ty = None;
            nc.next = None;
            if let Some(src) = oc.src_cap {
                let mut mapped = None;
                for (o, nw) in ctx.cap_map.iter() {
                    if *o == src {
                        mapped = Some(*nw);
                        break;
                    }
                }
                nc.src_cap = mapped;
            }
            if ctx.cap_map.len() >= 256 {
                self.err_internal("ジェネリック実体化での捕捉が多すぎます".as_bytes().to_vec())?;
            }
            let nc_id = self.new_capture(nc);
            ctx.cap_map.push((cap, nc_id));
            cap_ids.push(nc_id);
            ccur = oc.next;
        }
        for w in cap_ids.windows(2) {
            self.captures[w[0] as usize].next = Some(w[1]);
        }
        nf.captures = cap_ids.first().copied();
        let body = self.funcs[f as usize].body;
        let nf_id = self.new_func(nf);
        let nb2 = match body {
            Some(b) => Some(self.clone_node(ctx, b, tmpl, targs)?),
            None => None,
        };
        self.funcs[nf_id as usize].body = nb2;
        ctx.cap_map.truncate(saved_map_n);
        ctx.cloned_lambdas.push(nf_id);
        Ok(nf_id)
    }

    pub fn clone_node(
        &mut self,
        ctx: &mut CloneCtx,
        n: NodeId,
        tmpl: ClassId,
        targs: &[Option<TypeId>; MAX_TPARAMS],
    ) -> Result<NodeId, Flow> {
        for (o, c) in ctx.memo.iter() {
            if *o == n {
                return Ok(*c);
            }
        }
        if ctx.memo.len() >= 8192 {
            self.err_internal("ジェネリックなメソッドが大きすぎます".as_bytes().to_vec())?;
        }
        // 先に複製を登録してから子を複製する（共有参照を保つため）
        let orig = self.nodes[n as usize].clone();
        let c = self.new_node(orig.data.clone());
        self.nodes[c as usize].next = orig.next;
        self.nodes[c as usize].tok = orig.tok;
        self.nodes[c as usize].ty = None;
        ctx.memo.push((n, c));

        // func / klass のリセット（呼び出しを伴わない部分）
        match &mut self.nodes[c as usize].data {
            NodeData::Funcall { func, klass, .. } => {
                *func = None;
                *klass = None;
            }
            NodeData::MCall { func, klass, .. } => {
                *func = None;
                *klass = None;
            }
            NodeData::Member { func, klass, .. } => {
                *func = None;
                *klass = None;
            }
            NodeData::FuncRef { func, .. } => {
                *func = None;
            }
            _ => {}
        }
        // anno の型代入とラムダの差し替え（&mut self の呼び出しを伴う）
        let anno_now = match &self.nodes[c as usize].data {
            NodeData::Assign { anno, .. } => *anno,
            _ => None,
        };
        if let Some(a) = anno_now {
            let st = self.subst_type(Some(a), tmpl, targs);
            if let NodeData::Assign { anno, .. } = &mut self.nodes[c as usize].data {
                *anno = st;
            }
        }
        let lambda_fn = match &self.nodes[c as usize].data {
            NodeData::Lambda { func, .. } => *func,
            _ => None,
        };
        if let Some(of) = lambda_fn {
            let nf = self.clone_lambda_func(ctx, of, tmpl, targs)?;
            let (nm, caps) = {
                let f = &self.funcs[nf as usize];
                (f.name, f.captures)
            };
            if let NodeData::Lambda {
                func,
                name,
                captures,
            } = &mut self.nodes[c as usize].data
            {
                *func = Some(nf);
                *name = nm;
                *captures = caps;
            }
        }
        self.clone_children(ctx, c, tmpl, targs)?;
        Ok(c)
    }

    fn clone_children(
        &mut self,
        ctx: &mut CloneCtx,
        c: NodeId,
        tmpl: ClassId,
        targs: &[Option<TypeId>; MAX_TPARAMS],
    ) -> Result<(), Flow> {
        // 子を C の clone_node の順（left, right, cond, then, els,
        // init, inc, body, args, next）で複製する。ラムダの $g 番号の
        // 採番順がアセンブリのラベルに現れるため順序が要る。
        macro_rules! cl {
            ($slot:expr) => {
                if let Some(ch) = $slot {
                    let nc = self.clone_node(ctx, ch, tmpl, targs)?;
                    $slot = Some(nc);
                }
            };
        }
        let mut data = self.nodes[c as usize].data.clone();
        match &mut data {
            NodeData::Assign { left, right, .. } => {
                let l = self.clone_node(ctx, *left, tmpl, targs)?;
                *left = l;
                let r = self.clone_node(ctx, *right, tmpl, targs)?;
                *right = r;
            }
            NodeData::OpAssign { left, right, .. } => {
                let l = self.clone_node(ctx, *left, tmpl, targs)?;
                *left = l;
                let r = self.clone_node(ctx, *right, tmpl, targs)?;
                *right = r;
            }
            NodeData::Return { left }
            | NodeData::Len { left }
            | NodeData::Unary { left, .. }
            | NodeData::Member { left, .. } => {
                let l = self.clone_node(ctx, *left, tmpl, targs)?;
                *left = l;
            }
            NodeData::If { cond, then, els } => {
                let x = self.clone_node(ctx, *cond, tmpl, targs)?;
                *cond = x;
                let x = self.clone_node(ctx, *then, tmpl, targs)?;
                *then = x;
                cl!(*els);
            }
            NodeData::While { cond, then } => {
                let x = self.clone_node(ctx, *cond, tmpl, targs)?;
                *cond = x;
                let x = self.clone_node(ctx, *then, tmpl, targs)?;
                *then = x;
            }
            NodeData::For {
                init,
                var,
                then,
                els,
                ..
            } => {
                let x = self.clone_node(ctx, *var, tmpl, targs)?;
                *var = x;
                let x = self.clone_node(ctx, *then, tmpl, targs)?;
                *then = x;
                cl!(*els);
                let x = self.clone_node(ctx, *init, tmpl, targs)?;
                *init = x;
            }
            NodeData::Block { body } | NodeData::Array { body, .. } => {
                cl!(*body);
            }
            NodeData::Index { left, right } | NodeData::NewArray { fill: left, len: right } => {
                let l = self.clone_node(ctx, *left, tmpl, targs)?;
                *left = l;
                let r = self.clone_node(ctx, *right, tmpl, targs)?;
                *right = r;
            }
            NodeData::Binop { left, right, .. } => {
                let l = self.clone_node(ctx, *left, tmpl, targs)?;
                *left = l;
                if let Some(r) = right {
                    let nr = self.clone_node(ctx, *r, tmpl, targs)?;
                    *r = nr;
                }
            }
            NodeData::MCall { left, args, .. } | NodeData::VCall { left, args } => {
                let l = self.clone_node(ctx, *left, tmpl, targs)?;
                *left = l;
                cl!(*args);
            }
            NodeData::MultiAssign {
                targets, values, ..
            } => {
                cl!(*targets);
                cl!(*values);
            }
            NodeData::Chain { first, pairs, .. } => {
                let x = self.clone_node(ctx, *first, tmpl, targs)?;
                *first = x;
                cl!(*pairs);
            }
            NodeData::Funcall { args, .. }
            | NodeData::Input { args, .. }
            | NodeData::Put { args } => {
                cl!(*args);
            }
            _ => {}
        }
        self.nodes[c as usize].data = data;
        if let Some(nx) = self.nodes[c as usize].next {
            let nn = self.clone_node(ctx, nx, tmpl, targs)?;
            self.nodes[c as usize].next = Some(nn);
        }
        Ok(())
    }

    // パターン中の型変数を gtargs に束縛する（generics.c の unify_type）
    pub fn unify_type(
        &mut self,
        pat: Option<TypeId>,
        arg: Option<TypeId>,
        tmpl: ClassId,
        gtargs: &mut [Option<TypeId>; MAX_TPARAMS],
    ) -> Result<(), Flow> {
        let (pat, arg) = match (pat, arg) {
            (Some(p), Some(a)) => (p, a),
            _ => return Ok(()),
        };
        match self.types[pat as usize].kind {
            TypeKind::Var => {
                let pname = self.types[pat as usize].name.unwrap();
                let tpc = self.classes[tmpl as usize].tparam_count;
                for i in 0..tpc as usize {
                    let tp = match self.classes[tmpl as usize].tparams[i] {
                        Some(tp) => tp,
                        None => continue,
                    };
                    if !self.name_eq(pname, tp) {
                        continue;
                    }
                    let cur = gtargs[i];
                    if cur.is_none() {
                        gtargs[i] = Some(arg);
                        return Ok(());
                    }
                    let cur = cur.unwrap();
                    if self.type_strict_equals(cur, arg) {
                        return Ok(());
                    }
                    if self.type_has_unknown(cur) && !self.type_has_unknown(arg) {
                        gtargs[i] = Some(arg);
                        return Ok(());
                    }
                    if self.type_has_unknown(arg) {
                        return Ok(());
                    }
                    self.err_plain("型引数を推論できません".as_bytes().to_vec())?;
                }
                Ok(())
            }
            TypeKind::Array => {
                if self.types[arg as usize].kind == TypeKind::Array {
                    let pe = self.types[pat as usize].elem;
                    let ae = self.types[arg as usize].elem;
                    return self.unify_type(pe, ae, tmpl, gtargs);
                }
                Ok(())
            }
            TypeKind::Class => {
                if self.types[arg as usize].kind == TypeKind::Class
                    && self.types[pat as usize].targ_count == self.types[arg as usize].targ_count
                {
                    let pk = self.types[pat as usize].klass.unwrap();
                    let ak = self.types[arg as usize].klass.unwrap();
                    let porigin = self.classes[pk as usize].origin.unwrap_or(pk);
                    let aorigin = self.classes[ak as usize].origin.unwrap_or(ak);
                    if porigin == aorigin {
                        let pt = self.types[pat as usize].targs;
                        let at = self.types[arg as usize].targs;
                        let n = self.types[pat as usize].targ_count;
                        for i in 0..n as usize {
                            self.unify_type(pt[i], at[i], tmpl, gtargs)?;
                        }
                    }
                }
                Ok(())
            }
            _ => Ok(()),
        }
    }
}
