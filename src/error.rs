// エラー出力と回収（error.c の移植）。
// 通常モード（CLI）では表示して exit(1)。キャプチャ有効中（LSP）は
// 診断リストへ記録し、C の longjmp の行き先を Flow で表す:
// - 上限（50件）到達・継続モードでない・内部エラー → Flow::Abort
// - 文単位のリカバリ先が有効 → Flow::Recover
// - 継続モード → 記録だけして Ok を返す
use crate::{Cx, Diag, Flow, NodeId};
use std::io::Write;

pub const ERROR_DIAG_MAX: usize = 50;

impl Cx {
    pub fn error_capture_begin(&mut self) {
        self.sink.capture = true;
        self.sink.recover_armed = false;
        self.sink.cont = false;
        self.sink.diags.clear();
    }

    pub fn error_capture_end(&mut self) {
        self.sink.capture = false;
        self.sink.recover_armed = false;
        self.sink.cont = false;
    }

    pub fn error_capture_enabled(&self) -> bool {
        self.sink.capture
    }

    pub fn error_capture_set_continue(&mut self, enabled: bool) {
        self.sink.cont = enabled;
    }

    fn record_diag(&mut self, start: i64, end: i64, has_pos: bool, message: Vec<u8>) {
        if self.sink.diags.len() >= ERROR_DIAG_MAX {
            return;
        }
        // C の vsnprintf(..., 4096) と同じくメッセージは 4095 バイトまで
        let mut message = message;
        if message.len() > 4095 {
            message.truncate(4095);
        }
        let src = self.cur_src;
        self.sink.diags.push(Diag {
            message,
            start,
            end,
            has_pos,
            src,
        });
    }

    // capture 有効時の戻り値を決める。Ok(()) は「記録だけして戻る」
    fn capture_flow(&mut self, start: i64, end: i64, has_pos: bool, message: Vec<u8>) -> Result<(), Flow> {
        self.record_diag(start, end, has_pos, message);
        if self.sink.diags.len() >= ERROR_DIAG_MAX {
            return Err(Flow::Abort);
        }
        if self.sink.recover_armed {
            return Err(Flow::Recover);
        }
        if !self.sink.cont {
            return Err(Flow::Abort);
        }
        Ok(())
    }

    // CLI モードの終了処理。stdio の flush（C の exit）に相当するよう、
    // アセンブリ出力を flush してから exit(1) する
    fn cli_exit(&mut self) -> ! {
        if let Some(w) = self.asm_out.as_mut() {
            let _ = w.flush();
        }
        let _ = std::io::stderr().flush();
        std::process::exit(1);
    }

    pub fn err_at(&mut self, start: i64, end: i64, message: Vec<u8>) -> Result<(), Flow> {
        if self.sink.capture {
            return self.capture_flow(start, end, true, message);
        }
        let rendered = self.render_at(start, end, &message);
        let mut stderr = std::io::stderr().lock();
        let _ = stderr.write_all(&rendered);
        let _ = stderr.flush();
        self.cli_exit();
    }

    pub fn err_plain(&mut self, message: Vec<u8>) -> Result<(), Flow> {
        if self.sink.capture {
            return self.capture_flow(0, 0, false, message);
        }
        let mut stderr = std::io::stderr().lock();
        let _ = stderr.write_all("エラー: ".as_bytes());
        let _ = stderr.write_all(&message);
        let _ = stderr.write_all(b"\n");
        let _ = stderr.flush();
        self.cli_exit();
    }

    pub fn err_internal(&mut self, message: Vec<u8>) -> Result<(), Flow> {
        if self.sink.capture {
            // 内部エラーはどのモードでも致命的
            self.record_diag(0, 0, false, message);
            return Err(Flow::Abort);
        }
        let mut stderr = std::io::stderr().lock();
        let _ = stderr.write_all("内部エラー: ".as_bytes());
        let _ = stderr.write_all(&message);
        let _ = stderr.write_all(b"\n");
        let _ = stderr.write_all("プログラムではなくコンパイラ側の問題です\n".as_bytes());
        let _ = stderr.flush();
        self.cli_exit();
    }

    // error_node: ノードの代表トークンが現在のソースの範囲内にあれば
    // 位置つきで、なければ位置なしでエラーを出す
    pub fn err_node(&mut self, node: Option<NodeId>, message: Vec<u8>) -> Result<(), Flow> {
        let tok = node.and_then(|n| self.nodes[n as usize].tok);
        if let Some(t) = tok {
            let t = self.tokens[t as usize].clone();
            if !t.heap_span && t.src == self.cur_src {
                let eff = self.sources[t.src as usize].eff_len();
                let start = t.start as usize;
                let end = start + t.len as usize;
                if end <= eff {
                    return self.err_at(start as i64, end as i64, message);
                }
            }
        }
        self.err_plain(message)
    }

    // 位置つきエラー（error_at の CLI 表示部分）
    fn render_at(&self, start: i64, end: i64, message: &[u8]) -> Vec<u8> {
        let src = if self.cur_src == crate::NO_SRC {
            &[][..]
        } else {
            self.sources[self.cur_src as usize].text()
        };
        let src_name: &[u8] = if self.cur_src == crate::NO_SRC {
            &[]
        } else {
            &self.sources[self.cur_src as usize].name
        };
        let start = start.max(0) as usize;
        let end = end.max(0) as usize;
        let mut line: i64 = 1;
        let mut line_start: usize = 0;
        for i in 0..start.min(src.len()) {
            if src[i] == b'\n' {
                line += 1;
                line_start = i + 1;
            }
        }
        let mut col: i64 = 1;
        {
            let mut i = line_start;
            while i < start {
                let (_, len) = utf8_read(src, i);
                i += len;
                col += 1;
            }
        }
        let mut out = Vec::new();
        out.extend_from_slice(src_name);
        out.extend_from_slice(format!(":{}:{}: エラー: ", line, col).as_bytes());
        out.extend_from_slice(message);
        out.push(b'\n');

        let mut line_end = line_start;
        while line_end < src.len() && src[line_end] != b'\n' {
            line_end += 1;
        }
        out.extend_from_slice(format!("{} | ", line).as_bytes());
        let mut disp: i64 = 0;
        {
            let mut i = line_start;
            while i < line_end {
                if src[i] == b'\t' {
                    let n = 4 - (disp % 4);
                    for _ in 0..n {
                        out.push(b' ');
                    }
                    disp += n;
                    i += 1;
                } else {
                    let (cp, len) = utf8_read(src, i);
                    out.extend_from_slice(&src[i..i + len.min(src.len() - i)]);
                    disp += char_width(cp);
                    i += len;
                }
            }
        }
        out.push(b'\n');

        let mut digits: i64 = 1;
        let mut t = line;
        while t >= 10 {
            t /= 10;
            digits += 1;
        }
        for _ in 0..digits {
            out.push(b' ');
        }
        out.extend_from_slice(" | ".as_bytes());

        let mut udisp: i64 = 0;
        {
            let mut i = line_start;
            while i < start && i < line_end {
                if src[i] == b'\t' {
                    udisp += 4 - (udisp % 4);
                    i += 1;
                } else {
                    let (cp, len) = utf8_read(src, i);
                    udisp += char_width(cp);
                    i += len;
                }
            }
        }
        for _ in 0..udisp {
            out.push(b' ');
        }
        let mut w: i64 = 0;
        {
            let mut i = start;
            while i < end && i < line_end {
                if src[i] == b'\t' {
                    w += 4 - ((udisp + w) % 4);
                    i += 1;
                } else {
                    let (cp, len) = utf8_read(src, i);
                    w += char_width(cp);
                    i += len;
                }
            }
        }
        if w == 0 {
            w = 1;
        }
        for _ in 0..w {
            out.push(b'^');
        }
        out.push(b'\n');
        out
    }
}

// UTF-8 を1文字読み、(コードポイント, バイト長) を返す（error.c の
// utf8_read と同じ。不正な先行バイトも4バイト扱いになる点まで同じ。
// 範囲外の読みは 0 として扱う）
pub fn utf8_read(src: &[u8], i: usize) -> (i64, usize) {
    let b = |k: usize| -> i64 {
        if i + k < src.len() {
            src[i + k] as i64
        } else {
            0
        }
    };
    let c = b(0);
    if c < 0x80 {
        (c, 1)
    } else if (c & 0xE0) == 0xC0 {
        (((c & 0x1F) << 6) | (b(1) & 0x3F), 2)
    } else if (c & 0xF0) == 0xE0 {
        (((c & 0x0F) << 12) | ((b(1) & 0x3F) << 6) | (b(2) & 0x3F), 3)
    } else {
        (
            ((c & 0x07) << 18) | ((b(1) & 0x3F) << 12) | ((b(2) & 0x3F) << 6) | (b(3) & 0x3F),
            4,
        )
    }
}

pub fn char_width(cp: i64) -> i64 {
    if (0x1100..=0x115F).contains(&cp)
        || (0x2E80..=0x9FFF).contains(&cp)
        || (0xAC00..=0xD7A3).contains(&cp)
        || (0xF900..=0xFAFF).contains(&cp)
        || (0xFE30..=0xFE6F).contains(&cp)
        || (0xFF00..=0xFF60).contains(&cp)
        || (0xFFE0..=0xFFE6).contains(&cp)
        || cp >= 0x20000
    {
        2
    } else {
        1
    }
}
