// コンパイルのパイプラインの入口（driver.c の移植）。ファイルを読み、
// 字句解析して構文木（関数リスト）を返すところまで。
use crate::{Cx, Flow, FunctionId};
use std::io::Read;

impl Cx {
    // 実行ファイルと同じ場所にある prelude.sp のパス
    pub fn spell_prelude_path(&mut self) -> Result<Vec<u8>, Flow> {
        match std::env::current_exe() {
            Ok(mut p) => {
                p.set_file_name("prelude.sp");
                Ok(p.as_os_str().as_encoded_bytes().to_vec())
            }
            Err(_) => self
                .err_plain(
                    "コンパイラの実行ファイルの場所を特定できませんでした"
                        .as_bytes()
                        .to_vec(),
                )
                .map(|_| Vec::new()),
        }
    }

    fn read_file(&mut self, path: &[u8]) -> Result<Vec<u8>, Flow> {
        let path_str = String::from_utf8_lossy(path).into_owned();
        let mut f = match std::fs::File::open(&path_str) {
            Ok(f) => f,
            Err(_) => {
                let msg = crate::msg(&[
                    "ファイル`".as_bytes(),
                    path,
                    "`を開けませんでした".as_bytes(),
                ]);
                return self.err_plain(msg).map(|_| Vec::new());
            }
        };
        let size = f.metadata().map(|m| m.len()).unwrap_or(0);
        let mut buf = Vec::new();
        match f.read_to_end(&mut buf) {
            Ok(_) => Ok(buf),
            Err(_) => {
                let msg = crate::msg(&[
                    "ファイル`".as_bytes(),
                    path,
                    "`から".as_bytes(),
                    size.to_string().as_bytes(),
                    "バイト読み取る予定でしたが、".as_bytes(),
                    buf.len().to_string().as_bytes(),
                    "バイトを読み取りました".as_bytes(),
                ]);
                self.err_plain(msg).map(|_| Vec::new())
            }
        }
    }

    pub fn load_program(&mut self, path: &[u8]) -> Result<Option<FunctionId>, Flow> {
        let bytes = self.read_file(path)?;
        self.load_program_with(bytes, path.to_vec())
    }

    // ファイルではなく、メモリ上のソース文字列から読み込む（LSP 用）
    pub fn load_program_text(
        &mut self,
        text: Vec<u8>,
        name: Vec<u8>,
    ) -> Result<Option<FunctionId>, Flow> {
        self.load_program_with(text, name)
    }

    fn load_program_with(
        &mut self,
        bytes: Vec<u8>,
        name: Vec<u8>,
    ) -> Result<Option<FunctionId>, Flow> {
        let id = self.add_source(name, bytes);
        self.cur_src = id;
        self.tokenize()?;
        self.program()
    }

    // prelude → 利用者ファイルの順に読み、関数リストをつなぐ（main.c /
    // lsp.c の子プロセスで共通の流れ）
    pub fn load_all(&mut self, user_path: &[u8]) -> Result<Option<FunctionId>, Flow> {
        let prelude_path = self.spell_prelude_path()?;
        let prog = self.load_program(&prelude_path)?;
        let user_prog = self.load_program(user_path)?;
        Ok(chain_programs(self, prog, user_prog))
    }
}

pub fn chain_programs(
    cx: &mut Cx,
    prog: Option<FunctionId>,
    user_prog: Option<FunctionId>,
) -> Option<FunctionId> {
    match prog {
        Some(p) => {
            let mut tail = p;
            while let Some(n) = cx.funcs[tail as usize].next {
                tail = n;
            }
            cx.funcs[tail as usize].next = user_prog;
            Some(p)
        }
        None => user_prog,
    }
}
