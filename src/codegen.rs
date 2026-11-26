// コード生成（codegen.c の移植）: 型検査済みの AST から x86-64
// アセンブリ（GAS 形式）を出力する。スタックマシン方式で、各 gen_* は
// 「式の値を 1 つ push した状態」で戻る。printf の書式は C と一字一句
// 同じにしてある（`.Lend%d:` に改行がない点も含む）。
use crate::{BinOp, ClassId, Cx, Flow, FunctionId, NodeId, TypeId, TypeKind, UnaryOp};
use std::io::Write;

const ARG_REGS: [&str; 6] = ["rdi", "rsi", "rdx", "rcx", "r8", "r9"];

macro_rules! out {
    ($cx:expr, $($arg:tt)*) => {{
        let s = format!($($arg)*);
        $cx.emit_str(&s);
    }};
}

impl Cx {
    pub fn emit_fmt(&mut self, args: std::fmt::Arguments) {
        if let Some(w) = self.asm_out.as_mut() {
            let _ = w.write_fmt(args);
        }
    }

    pub fn emit_str(&mut self, s: &str) {
        if let Some(w) = self.asm_out.as_mut() {
            let _ = w.write_all(s.as_bytes());
        }
    }

    fn new_label(&mut self) -> i32 {
        let l = self.max_label;
        self.max_label += 1;
        l
    }

    fn klass_label(&self, klass: ClassId) -> Vec<u8> {
        let mut out = Vec::new();
        let name = self.classes[klass as usize].name;
        out.extend_from_slice(&self.name_bytes(name).to_vec());
        if self.classes[klass as usize].origin.is_some() {
            let targs = self.classes[klass as usize].targs;
            let n = self.classes[klass as usize].targ_count;
            for i in 0..n as usize {
                out.push(b'$');
                let a = targs[i];
                // print_type_label は &mut self ではなく &self で足りる
                self.print_type_label_c(a, &mut out);
            }
        }
        out
    }

    fn print_type_label_c(&self, t: Option<TypeId>, out: &mut Vec<u8>) {
        let t = match t {
            Some(t) => t,
            None => {
                out.extend_from_slice(b"unknown");
                return;
            }
        };
        match self.types[t as usize].kind {
            TypeKind::Int => out.extend_from_slice(b"int"),
            TypeKind::Array => {
                out.extend_from_slice(b"array$");
                let e = self.types[t as usize].elem;
                self.print_type_label_c(e, out);
            }
            TypeKind::Class => {
                let k = self.types[t as usize].klass.unwrap();
                let name = self.classes[k as usize].name;
                out.extend_from_slice(&self.name_bytes(name).to_vec());
                let targs = self.types[t as usize].targs;
                let n = self.types[t as usize].targ_count;
                for i in 0..n as usize {
                    out.push(b'$');
                    let a = targs[i];
                    self.print_type_label_c(a, out);
                }
            }
            _ => out.extend_from_slice(b"unknown"),
        }
    }

    fn print_call_target(&mut self, klass: Option<ClassId>, name: &[u8]) {
        out!(self, "  call ");
        if let Some(k) = klass {
            let label = self.klass_label(k);
            self.emit_fmt(format_args!("{}", String::from_utf8_lossy(&label)));
            out!(self, ".");
        }
        self.emit_fmt(format_args!(
            "{}\n",
            String::from_utf8_lossy(name)
        ));
    }

    fn emit_call(&mut self, label: i32, klass: Option<ClassId>, name: &[u8]) {
        out!(self, "  mov rax, rsp\n");
        out!(self, "  and rax, 15\n");
        out!(self, "  jnz .Lcall{}\n", label);
        out!(self, "  mov rax, 0\n");
        self.print_call_target(klass, name);
        out!(self, "  jmp .Lend{}\n", label);
        out!(self, ".Lcall{}:\n", label);
        out!(self, "  sub rsp, 8\n");
        out!(self, "  mov rax, 0\n");
        self.print_call_target(klass, name);
        out!(self, "  add rsp, 8\n");
        out!(self, ".Lend{}:\n", label);
    }

    fn emit_call_reg(&mut self, label: i32) {
        out!(self, "  mov rax, rsp\n");
        out!(self, "  and rax, 15\n");
        out!(self, "  jnz .Lcall{}\n", label);
        out!(self, "  mov rax, 0\n");
        out!(self, "  call r11\n");
        out!(self, "  jmp .Lend{}\n", label);
        out!(self, ".Lcall{}:\n", label);
        out!(self, "  sub rsp, 8\n");
        out!(self, "  mov rax, 0\n");
        out!(self, "  call r11\n");
        out!(self, "  add rsp, 8\n");
        out!(self, ".Lend{}:\n", label);
    }

    fn pop_args(&mut self, n: i32, offset: i32) {
        for i in (0..n).rev() {
            out!(self, "  pop {}\n", ARG_REGS[(i + offset) as usize]);
        }
    }

    fn gen_index_addr(&mut self, node: NodeId) -> Result<(), Flow> {
        let (left, right) = match &self.nodes[node as usize].data {
            crate::NodeData::Index { left, right } => (*left, *right),
            _ => return Ok(()),
        };
        self.gen(left)?;
        self.gen(right)?;
        out!(self, "  pop rdi\n");
        out!(self, "  pop rax\n");
        out!(self, "  imul rdi, 8\n");
        out!(self, "  add rax, rdi\n");
        Ok(())
    }

    fn gen_addr(&mut self, node: NodeId) -> Result<(), Flow> {
        match self.nodes[node as usize].data.clone() {
            crate::NodeData::GVar { name } => {
                let nb = self.name_bytes(name).to_vec();
                out!(self, "  lea rax, [rip + {}]\n", String::from_utf8_lossy(&nb));
                out!(self, "  push rax\n");
            }
            crate::NodeData::LVar { offset, .. } => {
                out!(self, "  mov rax, rbp\n");
                out!(self, "  sub rax, {}\n", offset);
                out!(self, "  push rax\n");
            }
            crate::NodeData::CapVar { offset, .. } => {
                out!(self, "  mov rax, [rbp-{}]\n", self.current_env_offset);
                out!(self, "  add rax, {}\n", offset);
                out!(self, "  push rax\n");
            }
            crate::NodeData::Unary {
                op: UnaryOp::Deref,
                left,
            } => {
                self.gen(left)?;
            }
            crate::NodeData::Index { .. } => {
                self.gen_index_addr(node)?;
                out!(self, "  push rax\n");
            }
            crate::NodeData::Member { left, offset, .. } => {
                self.gen(left)?;
                out!(self, "  pop rax\n");
                out!(self, "  add rax, {}\n", offset);
                out!(self, "  push rax\n");
            }
            _ => {
                self.err_internal(
                    "代入の処理で想定外の形式を検出しました".as_bytes().to_vec(),
                )?;
            }
        }
        Ok(())
    }

    fn gen_unary(&mut self, node: NodeId) -> Result<(), Flow> {
        let (op, left) = match &self.nodes[node as usize].data {
            crate::NodeData::Unary { op, left } => (*op, *left),
            _ => return Ok(()),
        };
        match op {
            UnaryOp::Addr => self.gen_addr(left)?,
            UnaryOp::Deref => {
                self.gen(left)?;
                out!(self, "  pop rax\n");
                out!(self, "  mov rax, [rax]\n");
                out!(self, "  push rax\n");
            }
            UnaryOp::Not => {
                self.gen(left)?;
                out!(self, "  pop rax\n");
                out!(self, "  cmp rax, 0\n");
                out!(self, "  sete al\n");
                out!(self, "  movzb rax, al\n");
                out!(self, "  push rax\n");
            }
        }
        Ok(())
    }

    fn gen_var(&mut self, node: NodeId) -> Result<(), Flow> {
        self.gen_addr(node)?;
        out!(self, "  pop rax\n");
        out!(self, "  mov rax, [rax]\n");
        out!(self, "  push rax\n");
        Ok(())
    }

    fn gen_assign(&mut self, node: NodeId) -> Result<(), Flow> {
        let (left, right) = match &self.nodes[node as usize].data {
            crate::NodeData::Assign { left, right, .. } => (*left, *right),
            _ => return Ok(()),
        };
        self.gen_addr(left)?;
        self.gen(right)?;
        out!(self, "  pop rdi\n");
        out!(self, "  pop rax\n");
        out!(self, "  mov [rax], rdi\n");
        out!(self, "  push rdi\n");
        Ok(())
    }

    fn gen_return(&mut self, node: NodeId) -> Result<(), Flow> {
        let left = match &self.nodes[node as usize].data {
            crate::NodeData::Return { left } => *left,
            _ => return Ok(()),
        };
        self.gen(left)?;
        out!(self, "  pop rax\n");
        out!(self, "  mov rsp, rbp\n");
        out!(self, "  pop rbp\n");
        out!(self, "  ret\n");
        Ok(())
    }

    fn gen_array(&mut self, node: NodeId) -> Result<(), Flow> {
        let (body, array_len, offset) = match &self.nodes[node as usize].data {
            crate::NodeData::Array {
                body,
                array_len,
                offset,
            } => (*body, *array_len, *offset),
            _ => return Ok(()),
        };
        if array_len == 0 {
            let label = self.new_label();
            out!(self, "  mov rdi, 8\n");
            self.emit_call(label, None, b"malloc");
            out!(self, "  mov rcx, 0\n");
            out!(self, "  mov [rax], rcx\n");
            out!(self, "  add rax, 8\n");
            out!(self, "  push rax\n");
            return Ok(());
        }
        out!(self, "  mov rax, {}\n", array_len);
        out!(self, "  mov [rbp-{}], rax\n", offset + 8 * array_len);
        let mut tail = body;
        let mut i = 0;
        while let Some(t) = tail {
            self.gen(t)?;
            out!(self, "  pop rax\n");
            out!(
                self,
                "  mov [rbp-{}], rax\n",
                offset + 8 * (array_len - i - 1)
            );
            tail = self.nodes[t as usize].next;
            i += 1;
        }
        out!(self, "  mov rax, rbp\n");
        out!(self, "  sub rax, {}\n", offset + 8 * (array_len - 1));
        out!(self, "  push rax\n");
        Ok(())
    }

    fn gen_index(&mut self, node: NodeId) -> Result<(), Flow> {
        self.gen_index_addr(node)?;
        out!(self, "  mov rax, [rax]\n");
        out!(self, "  push rax\n");
        Ok(())
    }

    fn gen_len(&mut self, node: NodeId) -> Result<(), Flow> {
        let left = match &self.nodes[node as usize].data {
            crate::NodeData::Len { left } => *left,
            _ => return Ok(()),
        };
        self.gen(left)?;
        out!(self, "  pop rax\n");
        out!(self, "  mov rax, [rax-8]\n");
        out!(self, "  push rax\n");
        Ok(())
    }

    fn gen_newarray(&mut self, node: NodeId) -> Result<(), Flow> {
        let (fill, len) = match &self.nodes[node as usize].data {
            crate::NodeData::NewArray { fill, len } => (*fill, *len),
            _ => return Ok(()),
        };
        let mut label = self.new_label();
        self.gen(fill)?;
        self.gen(len)?;
        out!(self, "  pop rax\n");
        out!(self, "  push rax\n");
        out!(self, "  add rax, 1\n");
        out!(self, "  imul rax, 8\n");
        out!(self, "  mov rdi, rax\n");
        out!(self, "  mov rdi, rax\n");
        self.emit_call(label, None, b"malloc");
        out!(self, "  pop rcx\n");
        out!(self, "  mov [rax], rcx\n");
        out!(self, "  pop rsi\n");
        out!(self, "  add rax, 8\n");
        out!(self, "  push rax\n");
        label = self.new_label();
        out!(self, "  mov rdx, rax\n");
        out!(self, ".Lbegin{}:\n", label);
        out!(self, "  cmp rcx, 0\n");
        out!(self, "  je .Lend{}\n", label);
        out!(self, "  mov [rdx], rsi\n");
        out!(self, "  add rdx, 8\n");
        out!(self, "  sub rcx, 1\n");
        out!(self, "  jmp .Lbegin{}\n", label);
        out!(self, ".Lend{}:\n", label);
        Ok(())
    }

    fn gen_if(&mut self, node: NodeId) -> Result<(), Flow> {
        let (cond, then, els) = match &self.nodes[node as usize].data {
            crate::NodeData::If { cond, then, els } => (*cond, *then, *els),
            _ => return Ok(()),
        };
        let label = self.new_label();
        self.gen(cond)?;
        out!(self, "  pop rax\n");
        out!(self, "  cmp rax, 0\n");
        out!(self, "  je .Lelse{}\n", label);
        self.gen(then)?;
        out!(self, "  jmp .Lend{}\n", label);
        out!(self, ".Lelse{}:\n", label);
        if let Some(e) = els {
            self.gen(e)?;
        } else {
            out!(self, "  push 0\n");
        }
        out!(self, ".Lend{}:\n", label);
        Ok(())
    }

    fn gen_while(&mut self, node: NodeId) -> Result<(), Flow> {
        let (cond, then) = match &self.nodes[node as usize].data {
            crate::NodeData::While { cond, then } => (*cond, *then),
            _ => return Ok(()),
        };
        let label = self.new_label();
        let saved_break = self.break_label;
        let saved_continue = self.continue_label;
        self.break_label = label;
        self.continue_label = label;
        out!(self, ".Lbegin{}:\n", label);
        out!(self, ".Lcont{}:\n", label);
        self.gen(cond)?;
        out!(self, "  pop rax\n");
        out!(self, "  cmp rax, 0\n");
        out!(self, "  je .Lend{}\n", label);
        self.gen(then)?;
        out!(self, "  pop rax\n");
        self.break_label = saved_break;
        self.continue_label = saved_continue;
        out!(self, "  jmp .Lbegin{}\n", label);
        out!(self, ".Lend{}:\n", label);
        out!(self, "  push 0\n");
        Ok(())
    }

    fn gen_block(&mut self, node: NodeId) -> Result<(), Flow> {
        let body = match &self.nodes[node as usize].data {
            crate::NodeData::Block { body } => *body,
            _ => return Ok(()),
        };
        let mut cur = body;
        while let Some(s) = cur {
            self.gen(s)?;
            cur = self.nodes[s as usize].next;
            out!(self, "  pop rax\n");
        }
        out!(self, "  push rax\n");
        Ok(())
    }

    fn gen_construct(&mut self, node: NodeId) -> Result<(), Flow> {
        let (args, klass, func) = match &self.nodes[node as usize].data {
            crate::NodeData::Funcall {
                args, klass, func, ..
            } => (*args, *klass, *func),
            _ => return Ok(()),
        };
        let mut args_cnt = 0;
        let mut cur = args;
        while let Some(a) = cur {
            self.gen(a)?;
            args_cnt += 1;
            cur = self.nodes[a as usize].next;
        }
        if args_cnt > 5 {
            let msg = crate::msg(&[
                "`__init__`に渡せる引数は5個までですが、".as_bytes(),
                args_cnt.to_string().as_bytes(),
                "個が指定されています".as_bytes(),
            ]);
            self.err_plain(msg)?;
        }
        let mut size = 0;
        let mut fcur = self.classes[klass.unwrap() as usize].fields;
        while let Some(f) = fcur {
            size += 8;
            fcur = self.fields[f as usize].next;
        }
        if size == 0 {
            size = 8;
        }
        let label = self.new_label();
        out!(self, "  mov rdi, 1\n");
        out!(self, "  mov rsi, {}\n", size);
        self.emit_call(label, None, b"calloc");
        out!(self, "  push rax\n");
        out!(self, "  pop rdi\n");
        self.pop_args(args_cnt, 1);
        out!(self, "  push rdi\n");
        if let Some(f) = func {
            let (fk, fname) = {
                let func = &self.funcs[f as usize];
                (func.klass, func.name)
            };
            let nb = self.name_bytes(fname).to_vec();
            let label = self.new_label();
            self.emit_call(label, fk, &nb);
        }
        Ok(())
    }

    fn gen_funcall(&mut self, node: NodeId) -> Result<(), Flow> {
        let (args, klass, name) = match &self.nodes[node as usize].data {
            crate::NodeData::Funcall {
                args, klass, name, ..
            } => (*args, *klass, *name),
            _ => return Ok(()),
        };
        if klass.is_some() {
            return self.gen_construct(node);
        }
        let label = self.new_label();
        let mut args_cnt = 0;
        let mut cur = args;
        while let Some(a) = cur {
            self.gen(a)?;
            args_cnt += 1;
            cur = self.nodes[a as usize].next;
        }
        if args_cnt > 6 {
            let msg = crate::msg(&[
                "関数に渡せる引数は6個までですが、".as_bytes(),
                args_cnt.to_string().as_bytes(),
                "個が指定されています".as_bytes(),
            ]);
            self.err_plain(msg)?;
        }
        self.pop_args(args_cnt, 0);
        let nb = self.name_bytes(name).to_vec();
        self.emit_call(label, None, &nb);
        if self.name_eq_lit(name, "getchar") || self.name_eq_lit(name, "putchar") {
            out!(self, "  movsxd rax, eax\n");
        }
        out!(self, "  push rax\n");
        Ok(())
    }

    fn gen_funcref(&mut self, node: NodeId) -> Result<(), Flow> {
        let name = match &self.nodes[node as usize].data {
            crate::NodeData::FuncRef { name, .. } => *name,
            _ => return Ok(()),
        };
        out!(self, "  mov rdi, 16\n");
        let label = self.new_label();
        self.emit_call(label, None, b"malloc");
        let nb = self.name_bytes(name).to_vec();
        out!(self, "  lea rcx, [rip + {}]\n", String::from_utf8_lossy(&nb));
        out!(self, "  mov [rax], rcx\n");
        out!(self, "  mov qword ptr [rax + 8], 0\n");
        out!(self, "  push rax\n");
        Ok(())
    }

    fn gen_lambda(&mut self, node: NodeId) -> Result<(), Flow> {
        let (func, captures) = match &self.nodes[node as usize].data {
            crate::NodeData::Lambda {
                func, captures, ..
            } => (*func, *captures),
            _ => return Ok(()),
        };
        let func = match func {
            Some(f) => f,
            None => {
                self.err_internal("ラムダの関数が解決されていません".as_bytes().to_vec())?;
                return Ok(());
            }
        };
        let mut ncaptures = 0;
        let mut cur = captures;
        while let Some(c) = cur {
            ncaptures += 1;
            cur = self.captures[c as usize].next;
        }
        let fname = self.funcs[func as usize].name;
        let nb = self.name_bytes(fname).to_vec();
        if ncaptures == 0 {
            out!(self, "  mov rdi, 16\n");
            let label = self.new_label();
            self.emit_call(label, None, b"malloc");
            out!(self, "  lea rcx, [rip + {}]\n", String::from_utf8_lossy(&nb));
            out!(self, "  mov [rax], rcx\n");
            out!(self, "  mov qword ptr [rax + 8], 0\n");
            out!(self, "  push rax\n");
            return Ok(());
        }
        out!(self, "  mov rdi, {}\n", 16 + 8 * ncaptures);
        let label = self.new_label();
        self.emit_call(label, None, b"malloc");
        out!(self, "  lea rcx, [rip + {}]\n", String::from_utf8_lossy(&nb));
        out!(self, "  mov [rax], rcx\n");
        out!(self, "  lea rcx, [rax + 16]\n");
        out!(self, "  mov [rax + 8], rcx\n");
        let mut cur = captures;
        while let Some(c) = cur {
            let cc = &self.captures[c as usize];
            let (is_env, src_off, env_off, next) =
                (cc.src_is_env, cc.src_offset, cc.env_offset, cc.next);
            if is_env {
                out!(self, "  mov rcx, [rbp-{}]\n", self.current_env_offset);
                out!(self, "  mov rcx, [rcx + {}]\n", src_off);
            } else {
                out!(self, "  mov rcx, [rbp-{}]\n", src_off);
            }
            out!(self, "  mov [rax + {}], rcx\n", 16 + env_off);
            cur = next;
        }
        out!(self, "  push rax\n");
        Ok(())
    }

    fn gen_vcall(&mut self, node: NodeId) -> Result<(), Flow> {
        let (left, args) = match &self.nodes[node as usize].data {
            crate::NodeData::VCall { left, args } => (*left, *args),
            _ => return Ok(()),
        };
        self.gen(left)?;
        let mut args_cnt = 0;
        let mut cur = args;
        while let Some(a) = cur {
            self.gen(a)?;
            args_cnt += 1;
            cur = self.nodes[a as usize].next;
        }
        if args_cnt > 6 {
            let msg = crate::msg(&[
                "関数に渡せる引数は6個までですが、".as_bytes(),
                args_cnt.to_string().as_bytes(),
                "個が指定されています".as_bytes(),
            ]);
            self.err_plain(msg)?;
        }
        self.pop_args(args_cnt, 0);
        out!(self, "  pop rax\n");
        out!(self, "  mov r11, [rax]\n");
        out!(self, "  mov r10, [rax + 8]\n");
        let label = self.new_label();
        self.emit_call_reg(label);
        out!(self, "  push rax\n");
        Ok(())
    }

    fn gen_member(&mut self, node: NodeId) -> Result<(), Flow> {
        let (left, offset, func) = match &self.nodes[node as usize].data {
            crate::NodeData::Member {
                left,
                offset,
                func,
                ..
            } => (*left, *offset, *func),
            _ => return Ok(()),
        };
        self.gen(left)?;
        if let Some(f) = func {
            out!(self, "  pop rdi\n");
            let (fk, fname) = {
                let func = &self.funcs[f as usize];
                (func.klass, func.name)
            };
            let nb = self.name_bytes(fname).to_vec();
            let label = self.new_label();
            self.emit_call(label, fk, &nb);
            out!(self, "  push rax\n");
            return Ok(());
        }
        out!(self, "  pop rax\n");
        out!(self, "  mov rax, [rax + {}]\n", offset);
        out!(self, "  push rax\n");
        Ok(())
    }

    fn gen_mcall(&mut self, node: NodeId) -> Result<(), Flow> {
        let (left, args, func) = match &self.nodes[node as usize].data {
            crate::NodeData::MCall {
                left, args, func, ..
            } => (*left, *args, *func),
            _ => return Ok(()),
        };
        self.gen(left)?;
        let mut args_cnt = 0;
        let mut cur = args;
        while let Some(a) = cur {
            self.gen(a)?;
            args_cnt += 1;
            cur = self.nodes[a as usize].next;
        }
        if args_cnt > 5 {
            let msg = crate::msg(&[
                "メソッドに渡せる引数は5個までですが、".as_bytes(),
                args_cnt.to_string().as_bytes(),
                "個が指定されています".as_bytes(),
            ]);
            self.err_plain(msg)?;
        }
        self.pop_args(args_cnt, 1);
        out!(self, "  pop rdi\n");
        if let Some(f) = func {
            let (fk, fname) = {
                let func = &self.funcs[f as usize];
                (func.klass, func.name)
            };
            let nb = self.name_bytes(fname).to_vec();
            let label = self.new_label();
            self.emit_call(label, fk, &nb);
        }
        out!(self, "  push rax\n");
        Ok(())
    }

    fn emit_divmod_adjust(&mut self, is_mod: bool) {
        let label = self.new_label();
        out!(self, "  test rdx, rdx\n");
        out!(self, "  je .Ldivmod{}\n", label);
        out!(self, "  mov rcx, rdx\n");
        out!(self, "  xor rcx, rdi\n");
        out!(self, "  jns .Ldivmod{}\n", label);
        if is_mod {
            out!(self, "  add rdx, rdi\n");
        } else {
            out!(self, "  sub rax, 1\n");
        }
        out!(self, ".Ldivmod{}:\n", label);
    }

    fn gen_binop(&mut self, node: NodeId) -> Result<(), Flow> {
        let (op, left, right) = match &self.nodes[node as usize].data {
            crate::NodeData::Binop {
                op,
                left,
                right: Some(r),
                ..
            } => (*op, *left, *r),
            _ => return Ok(()),
        };
        if op == BinOp::And {
            let label = self.new_label();
            self.gen(left)?;
            out!(self, "  pop rax\n");
            out!(self, "  cmp rax, 0\n");
            out!(self, "  je .Lfalse{}\n", label);
            self.gen(right)?;
            out!(self, "  pop rax\n");
            out!(self, "  cmp rax, 0\n");
            out!(self, "  je .Lfalse{}\n", label);
            out!(self, "  push 1\n");
            out!(self, "  jmp .Lend{}\n", label);
            out!(self, ".Lfalse{}:\n", label);
            out!(self, "  push 0\n");
            out!(self, ".Lend{}:", label);
            return Ok(());
        } else if op == BinOp::Or {
            let label = self.new_label();
            self.gen(left)?;
            out!(self, "  pop rax\n");
            out!(self, "  cmp rax, 0\n");
            out!(self, "  jne .Ltrue{}\n", label);
            self.gen(right)?;
            out!(self, "  pop rax\n");
            out!(self, "  cmp rax, 0\n");
            out!(self, "  jne .Ltrue{}\n", label);
            out!(self, "  push 0\n");
            out!(self, "  jmp .Lend{}\n", label);
            out!(self, ".Ltrue{}:\n", label);
            out!(self, "  push 1\n");
            out!(self, ".Lend{}:", label);
            return Ok(());
        }
        self.gen(left)?;
        self.gen(right)?;
        out!(self, "  pop rdi\n");
        out!(self, "  pop rax\n");
        match op {
            BinOp::Add => out!(self, "  add rax, rdi\n"),
            BinOp::Sub => out!(self, "  sub rax, rdi\n"),
            BinOp::Mul => out!(self, "  imul rax, rdi\n"),
            BinOp::Div => {
                out!(self, "  cqo\n");
                out!(self, "  idiv rdi\n");
                self.emit_divmod_adjust(false);
            }
            BinOp::Eq => {
                out!(self, "  cmp rax, rdi\n");
                out!(self, "  sete al\n");
                out!(self, "  movzb rax, al\n");
            }
            BinOp::Ne => {
                out!(self, "  cmp rax, rdi\n");
                out!(self, "  setne al\n");
                out!(self, "  movzb rax, al\n");
            }
            BinOp::Lt => {
                out!(self, "  cmp rax, rdi\n");
                out!(self, "  setl al\n");
                out!(self, "  movzb rax, al\n");
            }
            BinOp::Le => {
                out!(self, "  cmp rax, rdi\n");
                out!(self, "  setle al\n");
                out!(self, "  movzb rax, al\n");
            }
            BinOp::Mod => {
                out!(self, "  cqo\n");
                out!(self, "  idiv rdi\n");
                self.emit_divmod_adjust(true);
                out!(self, "  mov rax, rdx\n");
            }
            _ => {
                self.err_internal(
                    "二項演算の処理で想定外の演算子を検出しました"
                        .as_bytes()
                        .to_vec(),
                )?;
            }
        }
        out!(self, "  push rax\n");
        Ok(())
    }

    pub fn gen(&mut self, node: NodeId) -> Result<(), Flow> {
        match self.nodes[node as usize].kind() {
            crate::NodeKind::Unary => self.gen_unary(node),
            crate::NodeKind::LVar | crate::NodeKind::GVar | crate::NodeKind::CapVar => {
                self.gen_var(node)
            }
            crate::NodeKind::FuncRef => self.gen_funcref(node),
            crate::NodeKind::Lambda => self.gen_lambda(node),
            crate::NodeKind::VCall => self.gen_vcall(node),
            crate::NodeKind::Assign => self.gen_assign(node),
            crate::NodeKind::Num => {
                let val = match &self.nodes[node as usize].data {
                    crate::NodeData::Num { val } => *val,
                    _ => 0,
                };
                out!(self, "  push {}\n", val);
                Ok(())
            }
            crate::NodeKind::Return => self.gen_return(node),
            crate::NodeKind::Array => self.gen_array(node),
            crate::NodeKind::Index => self.gen_index(node),
            crate::NodeKind::Len => self.gen_len(node),
            crate::NodeKind::NewArray => self.gen_newarray(node),
            crate::NodeKind::If => self.gen_if(node),
            crate::NodeKind::While => self.gen_while(node),
            crate::NodeKind::Block => self.gen_block(node),
            crate::NodeKind::Funcall => self.gen_funcall(node),
            crate::NodeKind::Member => self.gen_member(node),
            crate::NodeKind::MCall => self.gen_mcall(node),
            crate::NodeKind::Binop => self.gen_binop(node),
            crate::NodeKind::Break => {
                if self.break_label < 0 {
                    self.err_plain("ここでは`break`を使えません".as_bytes().to_vec())?;
                }
                out!(self, "  jmp .Lend{}\n", self.break_label);
                Ok(())
            }
            crate::NodeKind::Continue => {
                if self.continue_label < 0 {
                    self.err_plain("ここでは`continue`を使えません".as_bytes().to_vec())?;
                }
                out!(self, "  jmp .Lcont{}\n", self.continue_label);
                Ok(())
            }
            _ => self.err_internal(
                "コード生成で想定外の形式を検出しました".as_bytes().to_vec(),
            ),
        }
    }

    pub fn codegen(&mut self, prog: Option<FunctionId>) -> Result<(), Flow> {
        let mut has_init = false;
        let mut cur = prog;
        while let Some(f) = cur {
            if self.name_eq_lit(self.funcs[f as usize].name, "__spell_init") {
                has_init = true;
            }
            cur = self.funcs[f as usize].next;
        }
        if !self.globals.is_empty() {
            let gnames: Vec<Vec<u8>> = self
                .globals
                .iter()
                .rev()
                .map(|v| self.name_bytes(v.name).to_vec())
                .collect();
            out!(self, ".bss\n");
            for nb in &gnames {
                out!(self, "{}:\n", String::from_utf8_lossy(nb));
                out!(self, "  .zero 8\n");
            }
            out!(self, ".text\n");
        }
        let mut cur = prog;
        while let Some(f) = cur {
            let func = self.funcs[f as usize].clone();
            cur = func.next;
            if let Some(k) = func.klass {
                if self.classes[k as usize].tparam_count > 0 {
                    continue;
                }
            }
            if let Some(k) = func.klass {
                let label = self.klass_label(k);
                out!(self, ".global {}.{}\n", String::from_utf8_lossy(&label), String::from_utf8_lossy(&self.name_bytes(func.name)));
                out!(self, "{}.{}:\n", String::from_utf8_lossy(&label), String::from_utf8_lossy(&self.name_bytes(func.name)));
            } else {
                let nb = self.name_bytes(func.name).to_vec();
                out!(self, ".global {}\n", String::from_utf8_lossy(&nb));
                out!(self, "{}:\n", String::from_utf8_lossy(&nb));
            }
            out!(self, "  push rbp\n");
            out!(self, "  mov rbp, rsp\n");
            out!(self, "  sub rsp, {}\n", func.stack_size);
            self.current_env_offset = if func.is_lambda { func.env_offset } else { 0 };
            if func.is_lambda {
                out!(self, "  mov [rbp-{}], r10\n", func.env_offset);
            }
            if has_init && self.name_eq_lit(func.name, "main") {
                out!(self, "  call __spell_init\n");
            }
            for i in 0..func.nparams as usize {
                out!(self, "  mov [rbp-{}], {}\n", (i + 1) * 8, ARG_REGS[i]);
            }
            let mut tail = func.body;
            while let Some(s) = tail {
                self.gen(s)?;
                tail = self.nodes[s as usize].next;
                out!(self, "  pop rax\n");
            }
            if func.klass.is_none() && self.name_eq_lit(func.name, "main") {
                out!(self, "  mov rax, 0\n");
            }
            out!(self, "  mov rsp, rbp\n");
            out!(self, "  pop rbp\n");
            out!(self, "  ret\n");
        }
        Ok(())
    }
}
