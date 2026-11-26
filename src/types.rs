// 型そのものの道具（types.c の移植）: 型の比較・表示・変換と、
// グローバル変数の型表。
use crate::{Cx, Name, Type, TypeId, TypeKind, MAX_TPARAMS, TY_INT_ID, TY_UNKNOWN_ID};

impl Cx {
    // 型の比較。strict でない間は「不明な型は全部と一致する」緩い比較、
    // strict では不明な型同士だけを一致とする厳密な比較
    pub fn type_cmp(&self, a: TypeId, b: TypeId, strict: bool) -> bool {
        let ta = &self.types[a as usize];
        let tb = &self.types[b as usize];
        if !strict {
            // 緩い比較では型変数同士は名前が違っても一致とする
            if ta.kind == TypeKind::Var && tb.kind == TypeKind::Var {
                return true;
            }
            if ta.kind == TypeKind::Unknown || tb.kind == TypeKind::Unknown {
                return true;
            }
        }
        if ta.kind != tb.kind {
            return false;
        }
        match ta.kind {
            TypeKind::Array => {
                let (ea, eb) = (ta.elem, tb.elem);
                match (ea, eb) {
                    (Some(ea), Some(eb)) => self.type_cmp(ea, eb, strict),
                    _ => true,
                }
            }
            TypeKind::Class => {
                let ka = ta.klass.unwrap();
                let kb = tb.klass.unwrap();
                let oa = self.classes[ka as usize].origin.unwrap_or(ka);
                let ob = self.classes[kb as usize].origin.unwrap_or(kb);
                if oa != ob {
                    return false;
                }
                if ta.targ_count != tb.targ_count {
                    return false;
                }
                for i in 0..ta.targ_count as usize {
                    let (xa, xb) = (ta.targs[i], tb.targs[i]);
                    match (xa, xb) {
                        (Some(xa), Some(xb)) => {
                            if !self.type_cmp(xa, xb, strict) {
                                return false;
                            }
                        }
                        _ => {}
                    }
                }
                true
            }
            TypeKind::Var => {
                let (na, nb) = (ta.name.unwrap(), tb.name.unwrap());
                self.name_eq(na, nb)
            }
            TypeKind::Func => {
                if ta.func_nparams != tb.func_nparams {
                    return false;
                }
                for i in 0..ta.func_nparams as usize {
                    let pa = ta.func_params[i].unwrap_or(TY_UNKNOWN_ID);
                    let pb = tb.func_params[i].unwrap_or(TY_UNKNOWN_ID);
                    if !self.type_cmp(pa, pb, strict) {
                        return false;
                    }
                }
                let ra = ta.func_ret.unwrap_or(TY_UNKNOWN_ID);
                let rb = tb.func_ret.unwrap_or(TY_UNKNOWN_ID);
                self.type_cmp(ra, rb, strict)
            }
            _ => true,
        }
    }

    pub fn type_equals(&self, a: TypeId, b: TypeId) -> bool {
        self.type_cmp(a, b, false)
    }

    pub fn type_strict_equals(&self, a: TypeId, b: TypeId) -> bool {
        self.type_cmp(a, b, true)
    }

    fn type_text_write(&self, t: TypeId, out: &mut Vec<u8>) {
        let ty = self.types[t as usize].clone();
        match ty.kind {
            TypeKind::Int => out.extend_from_slice(b"int"),
            TypeKind::Array => {
                out.push(b'[');
                if let Some(e) = ty.elem {
                    self.type_text_write(e, out);
                }
                out.push(b']');
            }
            TypeKind::Unknown => out.extend_from_slice(b"unknown"),
            TypeKind::Class => {
                let k = ty.klass.unwrap();
                let name = self.classes[k as usize].name;
                out.extend_from_slice(&self.name_bytes(name).to_vec());
                if ty.targ_count > 0 {
                    out.push(b'<');
                    for i in 0..ty.targ_count as usize {
                        if i > 0 {
                            out.extend_from_slice(b", ");
                        }
                        if let Some(x) = ty.targs[i] {
                            self.type_text_write(x, out);
                        }
                    }
                    out.push(b'>');
                }
            }
            TypeKind::Var => {
                let name = ty.name.unwrap();
                out.extend_from_slice(&self.name_bytes(name).to_vec());
            }
            TypeKind::Func => {
                out.extend_from_slice(b"fn(");
                for i in 0..ty.func_nparams as usize {
                    if i > 0 {
                        out.extend_from_slice(b", ");
                    }
                    let p = ty.func_params[i].unwrap_or(TY_UNKNOWN_ID);
                    self.type_text_write(p, out);
                }
                out.extend_from_slice(b") -> ");
                let r = ty.func_ret.unwrap_or(TY_UNKNOWN_ID);
                self.type_text_write(r, out);
            }
        }
    }

    pub fn type_text(&self, t: TypeId) -> Vec<u8> {
        let mut out = Vec::new();
        self.type_text_write(t, &mut out);
        out
    }

    pub fn find_global_type(&self, name: Name) -> TypeId {
        for (n, t) in &self.global_types {
            if self.name_eq(*n, name) {
                return *t;
            }
        }
        TY_UNKNOWN_ID
    }

    pub fn record_global_type(&mut self, name: Name, ty: TypeId) {
        // フィールド単位の借用で名前バイト列と型表を同時に触る
        let names = &self.names;
        let nb = &names[name.off as usize..(name.off + name.len) as usize];
        for e in &mut self.global_types {
            if names[e.0.off as usize..(e.0.off + e.0.len) as usize] == *nb {
                e.1 = ty;
                return;
            }
        }
        self.global_types.push((name, ty));
    }

    pub fn array_type(&mut self, elem: TypeId) -> TypeId {
        let mut t = Type::new(TypeKind::Array);
        t.elem = Some(elem);
        self.new_type(t)
    }

    pub fn func_type(&mut self, params: &[Option<TypeId>], nparams: i32, ret: Option<TypeId>) -> TypeId {
        let mut t = Type::new(TypeKind::Func);
        t.func_nparams = nparams;
        for i in 0..(nparams as usize).min(6) {
            t.func_params[i] = params[i];
        }
        t.func_ret = ret;
        self.new_type(t)
    }

    // tmpl の型パラメータ名と targs を位置で対応させて置換する
    pub fn subst_type(&mut self, t: Option<TypeId>, tmpl: crate::ClassId, targs: &[Option<TypeId>]) -> Option<TypeId> {
        let t = t?;
        let ty = self.types[t as usize].clone();
        match ty.kind {
            TypeKind::Var => {
                let name = ty.name.unwrap();
                let k = &self.classes[tmpl as usize];
                for i in 0..(k.tparam_count as usize).min(MAX_TPARAMS) {
                    if let Some(pn) = k.tparams[i] {
                        if self.name_eq(name, pn) {
                            return targs[i];
                        }
                    }
                }
                Some(t)
            }
            TypeKind::Array => {
                let e = self.subst_type(ty.elem, tmpl, targs);
                match e {
                    Some(e) => Some(self.array_type(e)),
                    None => {
                        // 要素なし配列（C の ty_array 単例）の置換は要素なしのまま
                        let mut nt = Type::new(TypeKind::Array);
                        nt.elem = None;
                        Some(self.new_type(nt))
                    }
                }
            }
            TypeKind::Class => {
                if ty.targ_count > 0 {
                    let mut nt = Type::new(TypeKind::Class);
                    nt.klass = ty.klass;
                    nt.targ_count = ty.targ_count;
                    for i in 0..ty.targ_count as usize {
                        nt.targs[i] = self.subst_type(ty.targs[i], tmpl, targs);
                    }
                    Some(self.new_type(nt))
                } else {
                    Some(t)
                }
            }
            TypeKind::Func => {
                let mut nt = Type::new(TypeKind::Func);
                nt.func_nparams = ty.func_nparams;
                for i in 0..ty.func_nparams as usize {
                    nt.func_params[i] = self.subst_type(ty.func_params[i], tmpl, targs);
                }
                nt.func_ret = self.subst_type(ty.func_ret, tmpl, targs);
                Some(self.new_type(nt))
            }
            _ => Some(t),
        }
    }

    pub fn type_has_unknown(&self, t: TypeId) -> bool {
        let ty = &self.types[t as usize];
        match ty.kind {
            TypeKind::Unknown => true,
            TypeKind::Array => ty.elem.map(|e| self.type_has_unknown(e)).unwrap_or(false),
            TypeKind::Class => {
                for i in 0..ty.targ_count as usize {
                    if let Some(x) = ty.targs[i] {
                        if self.type_has_unknown(x) {
                            return true;
                        }
                    }
                }
                false
            }
            TypeKind::Func => {
                for i in 0..ty.func_nparams as usize {
                    if let Some(p) = ty.func_params[i] {
                        if self.type_has_unknown(p) {
                            return true;
                        }
                    }
                }
                ty.func_ret.map(|r| self.type_has_unknown(r)).unwrap_or(false)
            }
            _ => false,
        }
    }

    pub fn adopt_type(&mut self, anno: TypeId, rhs: TypeId) -> TypeId {
        if self.types[rhs as usize].kind == TypeKind::Unknown {
            return anno;
        }
        let (ak, rk) = (
            self.types[anno as usize].kind,
            self.types[rhs as usize].kind,
        );
        if ak == TypeKind::Array && rk == TypeKind::Array {
            let ae = self.types[anno as usize].elem.unwrap();
            let re = self.types[rhs as usize].elem.unwrap();
            let elem = self.adopt_type(ae, re);
            if elem == re {
                return rhs;
            }
            return self.array_type(elem);
        }
        rhs
    }

    pub fn ty_int(&self) -> TypeId {
        TY_INT_ID
    }
}

pub fn targs_array(slice: &[Option<TypeId>]) -> [Option<TypeId>; MAX_TPARAMS] {
    let mut out = [None; MAX_TPARAMS];
    for (i, v) in slice.iter().enumerate() {
        if i < MAX_TPARAMS {
            out[i] = *v;
        }
    }
    out
}
