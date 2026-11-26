// 字句解析（tokenize.c の移植）。ソースの生バイト列を走査し、
// トークンを arena へ追加する。行番号は各トークンへ記録する
// （step 55: 文の終わりを改行で判定するため）。
use crate::{Cx, Flow, Keyword, Token, TokenId, TokenKind};

fn is_space_byte(c: u8) -> bool {
    matches!(c, b' ' | b'\t' | b'\n' | 0x0B | 0x0C | b'\r')
}

fn can_use_variables_first(c: u8) -> bool {
    c.is_ascii_lowercase() || c.is_ascii_uppercase() || c == b'_'
}

fn can_use_variables(c: u8) -> bool {
    can_use_variables_first(c) || c.is_ascii_digit()
}

const SYMBOL_TARGETS: &[&str] = &[
    "==", "!=", "<=", ">=", "<<", ">>", "&&", "||", "+=", "-=", "*=", "/=", "%=", "->", "..=", "..",
    "++", "--", "+", "-", "*", "/", "(", ")", "<", ">", "=", ";", "{", "}", ",", "&", "[", "]", ".",
    "%", "!", ":", "@", "^",
];

const KEYWORDS: &[(&str, Keyword)] = &[
    ("return", Keyword::Return),
    ("if", Keyword::If),
    ("else", Keyword::Else),
    ("while", Keyword::While),
    ("for", Keyword::For),
    ("break", Keyword::Break),
    ("continue", Keyword::Continue),
    ("def", Keyword::Def),
    ("class", Keyword::Class),
    ("in", Keyword::In),
    ("elif", Keyword::Elif),
    ("in_str", Keyword::InStr),
    ("in_line", Keyword::InLine),
    ("put", Keyword::Put),
    ("yn", Keyword::Yn),
    ("fn", Keyword::Fn),
];

struct Lexer {
    src_id: u32,
    cur_line: i32,
    prev: Option<TokenId>,
    run_start: usize,
}

impl Cx {
    fn lex_push(&mut self, lx: &mut Lexer, tok: Token) -> TokenId {
        let id = self.tokens.len() as TokenId;
        let mut tok = tok;
        tok.prev = lx.prev;
        tok.line = lx.cur_line;
        tok.src = lx.src_id;
        self.tokens.push(tok);
        lx.prev = Some(id);
        id
    }

    fn blank_token(kind: TokenKind) -> Token {
        Token {
            kind,
            keyword: Keyword::Return,
            val: 0,
            src: 0,
            start: 0,
            len: 0,
            line: 1,
            prev: None,
            str_buf: None,
            heap_span: false,
        }
    }

    // tokenize: 現在のソース（cur_src）を字句解析し、ランの先頭
    // トークンのインデックスを返す。カーソルはランの先頭に置かれる
    pub fn tokenize(&mut self) -> Result<usize, Flow> {
        let src_id = self.cur_src;
        let text = self.sources[src_id as usize].text().to_vec();
        let eff = text.len();
        let byte = |i: usize| -> u8 {
            if i < eff {
                text[i]
            } else {
                0
            }
        };
        let mut lx = Lexer {
            src_id,
            cur_line: 1,
            prev: None,
            run_start: self.tokens.len(),
        };
        let mut pos = 0usize;
        while pos < eff {
            let c = text[pos];
            if is_space_byte(c) {
                if c == b'\n' {
                    lx.cur_line += 1;
                }
                pos += 1;
                continue;
            }

            if byte(pos + 1) != 0 && &text[pos..(pos + 2).min(eff)] == b"//" {
                while pos < eff && text[pos] != b'\n' {
                    pos += 1;
                }
                continue;
            }

            if pos + 2 <= eff && &text[pos..pos + 2] == b"/*" {
                // "*/" を探す（C の strstr と同じく有効範囲内だけ）
                let mut q: Option<usize> = None;
                let mut j = pos + 2;
                while j + 2 <= eff {
                    if &text[j..j + 2] == b"*/" {
                        q = Some(j);
                        break;
                    }
                    j += 1;
                }
                let q = match q {
                    Some(q) => q,
                    None => {
                        return self.err_at(
                            pos as i64,
                            pos as i64 + 2,
                            "コメントを閉じる`*/`がないまま、ファイルの末尾に達しました".as_bytes().to_vec(),
                        ).map(|_| 0);
                    }
                };
                for p in pos..q + 2 {
                    if text[p] == b'\n' {
                        lx.cur_line += 1;
                    }
                }
                pos = q + 2;
                continue;
            }

            let mut is_symbol = false;
            for target in SYMBOL_TARGETS {
                let tb = target.as_bytes();
                if pos + tb.len() <= eff && &text[pos..pos + tb.len()] == tb {
                    let mut tok = Self::blank_token(TokenKind::Symbol);
                    tok.start = pos as u32;
                    tok.len = tb.len() as u32;
                    self.lex_push(&mut lx, tok);
                    pos += tb.len();
                    is_symbol = true;
                    break;
                }
            }
            if is_symbol {
                continue;
            }

            if c.is_ascii_digit() {
                let start = pos;
                // strtol と同じく10進の数字列を読み、オーバーフローは
                // LONG_MAX に飽和させる
                let mut num: i64 = 0;
                while pos < eff && text[pos].is_ascii_digit() {
                    let d = (text[pos] - b'0') as i64;
                    num = num.saturating_mul(10).saturating_add(d);
                    pos += 1;
                }
                let mut tok = Self::blank_token(TokenKind::Num);
                tok.val = num;
                tok.start = start as u32;
                tok.len = (pos - start) as u32;
                self.lex_push(&mut lx, tok);
                continue;
            }

            let mut is_keyword = false;
            for (keyword, kind) in KEYWORDS {
                let kb = keyword.as_bytes();
                if pos + kb.len() <= eff
                    && &text[pos..pos + kb.len()] == kb
                    && !can_use_variables(byte(pos + kb.len()))
                {
                    let mut tok = Self::blank_token(TokenKind::Keyword);
                    tok.keyword = *kind;
                    tok.start = pos as u32;
                    tok.len = kb.len() as u32;
                    self.lex_push(&mut lx, tok);
                    pos += kb.len();
                    is_keyword = true;
                    break;
                }
            }
            if is_keyword {
                continue;
            }

            if c == b'"' {
                pos = self.tokenize_str_literal(&mut lx, &text, pos)?;
                continue;
            }

            if can_use_variables_first(c) {
                let start = pos;
                while pos < eff && can_use_variables(text[pos]) {
                    pos += 1;
                }
                let mut tok = Self::blank_token(TokenKind::Ident);
                tok.start = start as u32;
                tok.len = (pos - start) as u32;
                self.lex_push(&mut lx, tok);
                continue;
            }

            let mut m = Vec::new();
            m.extend_from_slice("ここでは文字`".as_bytes());
            m.push(c);
            m.extend_from_slice("`を使えません".as_bytes());
            return self.err_at(pos as i64, pos as i64 + 1, m).map(|_| 0);
        }
        let mut tok = Self::blank_token(TokenKind::Eof);
        tok.start = eff as u32;
        tok.len = 0;
        let eof_id = self.lex_push(&mut lx, tok);
        // 先頭トークンの prev は無い（C がダミーへの参照を切るのと同じ）
        let run_start = lx.run_start;
        self.tokens[run_start].prev = None;
        self.tpos = run_start;
        self.trun_eof = eof_id as usize;
        Ok(run_start)
    }

    fn tokenize_str_literal(
        &mut self,
        lx: &mut Lexer,
        text: &[u8],
        first: usize,
    ) -> Result<usize, Flow> {
        let eff = text.len();
        let byte = |i: usize| -> u8 {
            if i < eff {
                text[i]
            } else {
                0
            }
        };
        let mut pos = first + 1;
        let mut buf: Vec<u8> = Vec::new();
        loop {
            let c = byte(pos);
            if c == b'"' {
                break;
            }
            if c == 0 || c == b'\n' {
                return self.err_at(
                    first as i64,
                    first as i64 + 1,
                    "文字列を閉じる`\"`がないまま、ファイルの末尾に達しました"
                        .as_bytes()
                        .to_vec(),
                ).map(|_| first);
            }
            if c == b'\\' {
                let e = byte(pos + 1);
                let v = match e {
                    b'n' => 10,
                    b't' => 9,
                    b'\\' => 92,
                    b'"' => 34,
                    b'0' => 0,
                    other => other,
                };
                buf.push(v);
                pos += 1;
            } else {
                buf.push(c);
            }
            pos += 1;
        }
        pos += 1;
        let decoded_len = buf.len() as u32;
        let mut tok = Self::blank_token(TokenKind::Str);
        tok.start = first as u32;
        tok.len = decoded_len;
        tok.str_buf = Some(buf);
        tok.heap_span = true;
        self.lex_push(lx, tok);
        Ok(pos)
    }
}
