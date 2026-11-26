// 脱糖（desugar.c の移植）: 複合代入 ND_OPASSIGN を
// ND_ASSIGN + ND_BINOP へ書き換える。全子ノードを再帰的に歩く
// （このパスでは子を歩く順序に意味はない）。
use crate::{Cx, FunctionId, NodeData, NodeId};

impl Cx {
    // 子ノードを C のフィールド順で列挙する
    pub fn child_ids(&self, id: NodeId) -> Vec<NodeId> {
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
            | NodeData::Member { left, .. } => {
                out.push(*left);
            }
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

    fn desugar_node(&mut self, id: NodeId) {
        for child in self.child_ids(id) {
            self.desugar_node(child);
        }
        let (op, left, right) = match &self.nodes[id as usize].data {
            NodeData::OpAssign { op, left, right } => (*op, *left, *right),
            _ => return,
        };
        let tok = self.nodes[id as usize].tok;
        let combined = self.new_node(NodeData::Binop {
            op,
            left,
            right: Some(right),
            val: 0,
        });
        self.nodes[combined as usize].tok = tok;
        self.nodes[id as usize].data = NodeData::Assign {
            left,
            right: combined,
            anno: None,
        };
    }

    pub fn desugar(&mut self, prog: Option<FunctionId>) {
        let mut cur = prog;
        while let Some(f) = cur {
            if let Some(body) = self.funcs[f as usize].body {
                self.desugar_node(body);
            }
            cur = self.funcs[f as usize].next;
        }
    }
}
