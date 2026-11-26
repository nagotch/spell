// spell の言語サーバー（lsp.c の移植）。v1 は診断
// （publishDiagnostics）のみ。stdio で JSON-RPC を話し、
// didOpen / didChange のたびに全文をチェックして診断を push する。
//
// チェックは別プロセスで1回使い切りにする。C 版は fork を使うが、
// Rust 版では環境変数つきで自分自身を再実行して子にする（コンパイラ
// 本体はグローバル状態を積み上げる一回限りの設計のため）。
// 子では error のキャプチャを有効にし、集まった診断をシリアライズ
// して標準出力で親へ返す。codegen は実行しない。
use spell::{Cx, Flow};
use std::io::{Read, Write};

// ===== 最小 JSON パーサ（lsp.c の移植） =====

#[derive(Clone)]
enum Json {
    Null,
    Bool(bool),
    Num(f64),
    Str(Vec<u8>),
    Arr(Vec<Json>),
    Obj(Vec<(Vec<u8>, Json)>),
}

const JSON_MAX_DEPTH: i32 = 100;

struct JParser<'a> {
    b: &'a [u8],
    pos: usize,
    depth: i32,
}

impl<'a> JParser<'a> {
    fn byte(&self) -> u8 {
        self.b.get(self.pos).copied().unwrap_or(0)
    }
    fn byte_at(&self, off: usize) -> u8 {
        self.b.get(self.pos + off).copied().unwrap_or(0)
    }
    fn skip_ws(&mut self) {
        while matches!(self.byte(), b' ' | b'\t' | b'\n' | b'\r') {
            self.pos += 1;
        }
    }
}

fn hex_val(c: u8) -> i32 {
    match c {
        b'0'..=b'9' => (c - b'0') as i32,
        b'a'..=b'f' => (c - b'a') as i32 + 10,
        b'A'..=b'F' => (c - b'A') as i32 + 10,
        _ => -1,
    }
}

fn buf_put_utf8(buf: &mut Vec<u8>, cp: i64) {
    if cp < 0x80 {
        buf.push(cp as u8);
    } else if cp < 0x800 {
        buf.push((0xC0 | (cp >> 6)) as u8);
        buf.push((0x80 | (cp & 0x3F)) as u8);
    } else if cp < 0x10000 {
        buf.push((0xE0 | (cp >> 12)) as u8);
        buf.push((0x80 | ((cp >> 6) & 0x3F)) as u8);
        buf.push((0x80 | (cp & 0x3F)) as u8);
    } else {
        buf.push((0xF0 | (cp >> 18)) as u8);
        buf.push((0x80 | ((cp >> 12) & 0x3F)) as u8);
        buf.push((0x80 | ((cp >> 6) & 0x3F)) as u8);
        buf.push((0x80 | (cp & 0x3F)) as u8);
    }
}

fn json_parse_string_raw(ps: &mut JParser) -> Option<Vec<u8>> {
    if ps.byte() != b'"' {
        return None;
    }
    ps.pos += 1;
    let mut buf: Vec<u8> = Vec::new();
    while ps.byte() != 0 && ps.byte() != b'"' {
        let c = ps.byte();
        if c == b'\\' {
            ps.pos += 1;
            let e = ps.byte();
            if e == 0 {
                return None;
            }
            ps.pos += 1;
            match e {
                b'"' => buf.push(b'"'),
                b'\\' => buf.push(b'\\'),
                b'/' => buf.push(b'/'),
                b'b' => buf.push(0x08),
                b'f' => buf.push(0x0C),
                b'n' => buf.push(b'\n'),
                b'r' => buf.push(b'\r'),
                b't' => buf.push(b'\t'),
                b'u' => {
                    let mut cp: i64 = 0;
                    for i in 0..4 {
                        let h = hex_val(ps.byte_at(i));
                        if h < 0 {
                            return None;
                        }
                        cp = cp * 16 + h as i64;
                    }
                    ps.pos += 4;
                    // サロゲートペアを1文字へ
                    if (0xD800..=0xDBFF).contains(&cp)
                        && ps.byte_at(0) == b'\\'
                        && ps.byte_at(1) == b'u'
                    {
                        let mut lo: i64 = 0;
                        let mut ok = true;
                        for i in 0..4 {
                            let h = hex_val(ps.byte_at(2 + i));
                            if h < 0 {
                                ok = false;
                                break;
                            }
                            lo = lo * 16 + h as i64;
                        }
                        if ok && (0xDC00..=0xDFFF).contains(&lo) {
                            cp = 0x10000 + ((cp - 0xD800) << 10) + (lo - 0xDC00);
                            ps.pos += 6;
                        }
                    }
                    buf_put_utf8(&mut buf, cp);
                }
                _ => return None,
            }
        } else {
            buf.push(c);
            ps.pos += 1;
        }
    }
    if ps.byte() != b'"' {
        return None;
    }
    ps.pos += 1;
    Some(buf)
}

// C の strtod が受理する最長プレフィックスを読み、値を返す
fn json_parse_number(ps: &mut JParser) -> Option<f64> {
    let b = ps.b;
    let mut i = ps.pos;
    let at = |i: usize| -> u8 { b.get(i).copied().unwrap_or(0) };
    if at(i) == b'-' || at(i) == b'+' {
        i += 1;
    }
    let start_digits = i;
    // 16進浮動小数（strtod が受理する）
    if at(i) == b'0' && (at(i + 1) == b'x' || at(i + 1) == b'X') {
        let mut j = i + 2;
        let mut mant: f64 = 0.0;
        let mut any = false;
        let mut frac_bits = 0i32;
        loop {
            let h = hex_val(at(j));
            if h < 0 {
                break;
            }
            mant = mant * 16.0 + h as f64;
            any = true;
            j += 1;
        }
        if at(j) == b'.' {
            j += 1;
            loop {
                let h = hex_val(at(j));
                if h < 0 {
                    break;
                }
                mant = mant * 16.0 + h as f64;
                frac_bits += 4;
                any = true;
                j += 1;
            }
        }
        if !any {
            return None;
        }
        let mut exp: i32 = -frac_bits;
        if at(j) == b'p' || at(j) == b'P' {
            let mut k = j + 1;
            let mut sign = 1i32;
            if at(k) == b'-' {
                sign = -1;
                k += 1;
            } else if at(k) == b'+' {
                k += 1;
            }
            let mut ev: i32 = 0;
            let mut eany = false;
            while at(k).is_ascii_digit() {
                ev = ev.saturating_mul(10).saturating_add((at(k) - b'0') as i32);
                eany = true;
                k += 1;
            }
            if eany {
                exp += sign * ev;
                j = k;
            }
        }
        let v = mant * 2f64.powi(exp.clamp(-1074, 1023));
        let v = if b[ps.pos] == b'-' { -v } else { v };
        ps.pos = j;
        return Some(v);
    }
    let mut any = false;
    while at(i).is_ascii_digit() {
        any = true;
        i += 1;
    }
    if at(i) == b'.' {
        let mut j = i + 1;
        let mut fany = false;
        while at(j).is_ascii_digit() {
            fany = true;
            j += 1;
        }
        if any || fany {
            i = j;
            any = true;
        }
    }
    if !any {
        return None;
    }
    if at(i) == b'e' || at(i) == b'E' {
        let mut j = i + 1;
        if at(j) == b'-' || at(j) == b'+' {
            j += 1;
        }
        let mut eany = false;
        while at(j).is_ascii_digit() {
            eany = true;
            j += 1;
        }
        if eany {
            i = j;
        }
    }
    let _ = start_digits;
    let text = std::str::from_utf8(&b[ps.pos..i]).ok()?;
    let v: f64 = text.parse().ok()?;
    ps.pos = i;
    Some(v)
}

fn json_parse_value(ps: &mut JParser) -> Option<Json> {
    ps.skip_ws();
    let c = ps.byte();
    if c == b'{' || c == b'[' {
        ps.depth += 1;
        if ps.depth > JSON_MAX_DEPTH {
            return None;
        }
        let is_obj = c == b'{';
        let close = if is_obj { b'}' } else { b']' };
        ps.pos += 1;
        ps.skip_ws();
        let mut items: Vec<Json> = Vec::new();
        let mut pairs: Vec<(Vec<u8>, Json)> = Vec::new();
        if ps.byte() == close {
            ps.pos += 1;
            ps.depth -= 1;
            return Some(if is_obj {
                Json::Obj(pairs)
            } else {
                Json::Arr(items)
            });
        }
        loop {
            let mut key: Option<Vec<u8>> = None;
            if is_obj {
                ps.skip_ws();
                key = Some(json_parse_string_raw(ps)?);
                ps.skip_ws();
                if ps.byte() != b':' {
                    return None;
                }
                ps.pos += 1;
            }
            let value = json_parse_value(ps)?;
            if is_obj {
                pairs.push((key.unwrap(), value));
            } else {
                items.push(value);
            }
            ps.skip_ws();
            if ps.byte() == b',' {
                ps.pos += 1;
                continue;
            }
            if ps.byte() == close {
                ps.pos += 1;
                ps.depth -= 1;
                return Some(if is_obj {
                    Json::Obj(pairs)
                } else {
                    Json::Arr(items)
                });
            }
            return None;
        }
    }
    if c == b'"' {
        let s = json_parse_string_raw(ps)?;
        return Some(Json::Str(s));
    }
    if c == b't' && ps.b[ps.pos..].starts_with(b"true") {
        ps.pos += 4;
        return Some(Json::Bool(true));
    }
    if c == b'f' && ps.b[ps.pos..].starts_with(b"false") {
        ps.pos += 5;
        return Some(Json::Bool(false));
    }
    if c == b'n' && ps.b[ps.pos..].starts_with(b"null") {
        ps.pos += 4;
        return Some(Json::Null);
    }
    if c == b'-' || c.is_ascii_digit() {
        let v = json_parse_number(ps)?;
        return Some(Json::Num(v));
    }
    None
}

fn json_parse(text: &[u8]) -> Option<Json> {
    let mut ps = JParser {
        b: text,
        pos: 0,
        depth: 0,
    };
    let j = json_parse_value(&mut ps)?;
    ps.skip_ws();
    if ps.byte() != 0 {
        return None;
    }
    Some(j)
}

fn json_obj_get<'a>(j: &'a Json, key: &str) -> Option<&'a Json> {
    if let Json::Obj(pairs) = j {
        for (k, v) in pairs {
            if k == key.as_bytes() {
                return Some(v);
            }
        }
    }
    None
}

fn json_arr_get(j: &Json, idx: usize) -> Option<&Json> {
    if let Json::Arr(items) = j {
        return items.get(idx);
    }
    None
}

// C の char* として見た文字列（最初の NUL まで）
fn json_str(j: Option<&Json>) -> Option<&[u8]> {
    match j {
        Some(Json::Str(s)) => {
            let end = s.iter().position(|&c| c == 0).unwrap_or(s.len());
            Some(&s[..end])
        }
        _ => None,
    }
}

// ===== 送信側の JSON 生成 =====

fn buf_add_json_string(b: &mut Vec<u8>, s: &[u8]) {
    b.push(b'"');
    for &c in s {
        if c == 0 {
            break;
        }
        match c {
            b'"' => b.extend_from_slice(b"\\\""),
            b'\\' => b.extend_from_slice(b"\\\\"),
            b'\n' => b.extend_from_slice(b"\\n"),
            b'\r' => b.extend_from_slice(b"\\r"),
            b'\t' => b.extend_from_slice(b"\\t"),
            0x08 => b.extend_from_slice(b"\\b"),
            0x0C => b.extend_from_slice(b"\\f"),
            _ => {
                if c < 0x20 {
                    b.extend_from_slice(format!("\\u{:04x}", c).as_bytes());
                } else {
                    b.push(c);
                }
            }
        }
    }
    b.push(b'"');
}

// C の %g（有効数字6桁）
fn fmt_g(v: f64) -> String {
    if v.is_nan() {
        return "nan".to_string();
    }
    if v.is_infinite() {
        return if v > 0.0 {
            "inf".to_string()
        } else {
            "-inf".to_string()
        };
    }
    if v == 0.0 {
        return if v.is_sign_negative() {
            "-0".to_string()
        } else {
            "0".to_string()
        };
    }
    let neg = v < 0.0;
    let a = v.abs();
    let s = format!("{:.5e}", a);
    let epos = s.find('e').unwrap();
    let exp: i32 = s[epos + 1..].parse().unwrap();
    let mant: String = s[..epos].chars().filter(|c| *c != '.').collect();
    let mut digits: Vec<u8> = mant.into_bytes();
    while digits.len() > 1 && *digits.last().unwrap() == b'0' {
        digits.pop();
    }
    let mut out = String::new();
    if neg {
        out.push('-');
    }
    if exp >= -4 && exp < 6 {
        if exp >= 0 {
            let e = exp as usize;
            if digits.len() > e + 1 {
                for (i, d) in digits.iter().enumerate() {
                    if i == e + 1 {
                        out.push('.');
                    }
                    out.push(*d as char);
                }
            } else {
                for d in &digits {
                    out.push(*d as char);
                }
                for _ in 0..(e + 1 - digits.len()) {
                    out.push('0');
                }
            }
        } else {
            out.push_str("0.");
            for _ in 0..(-exp - 1) {
                out.push('0');
            }
            for d in &digits {
                out.push(*d as char);
            }
        }
    } else {
        out.push(digits[0] as char);
        if digits.len() > 1 {
            out.push('.');
            for d in &digits[1..] {
                out.push(*d as char);
            }
        }
        out.push('e');
        out.push(if exp < 0 { '-' } else { '+' });
        let ea = exp.abs();
        if ea < 10 {
            out.push('0');
        }
        out.push_str(&ea.to_string());
    }
    out
}

fn buf_add_id(b: &mut Vec<u8>, id: Option<&Json>) {
    match id {
        None => b.extend_from_slice(b"null"),
        Some(Json::Num(v)) => {
            if *v == (*v as i64) as f64 {
                b.extend_from_slice((*v as i64).to_string().as_bytes());
            } else {
                b.extend_from_slice(fmt_g(*v).as_bytes());
            }
        }
        Some(Json::Str(s)) => {
            let end = s.iter().position(|&c| c == 0).unwrap_or(s.len());
            buf_add_json_string(b, &s[..end]);
        }
        Some(Json::Bool(v)) => b.extend_from_slice(if *v { b"true" } else { b"false" }),
        _ => b.extend_from_slice(b"null"),
    }
}

// ===== トランスポート（Content-Length フレーミング） =====

struct Reader {
    inner: std::io::Stdin,
    buf: [u8; 8192],
    len: usize,
    pos: usize,
}

impl Reader {
    fn getc(&mut self) -> Option<u8> {
        if self.pos >= self.len {
            match self.inner.read(&mut self.buf) {
                Ok(0) => return None,
                Ok(n) => {
                    self.len = n;
                    self.pos = 0;
                }
                Err(_) => return None,
            }
        }
        let c = self.buf[self.pos];
        self.pos += 1;
        Some(c)
    }

    fn read_exact(&mut self, out: &mut [u8]) -> bool {
        let mut got = 0;
        while got < out.len() {
            if self.pos < self.len {
                let take = (self.len - self.pos).min(out.len() - got);
                out[got..got + take].copy_from_slice(&self.buf[self.pos..self.pos + take]);
                self.pos += take;
                got += take;
            } else {
                match self.inner.read(&mut out[got..]) {
                    Ok(0) => return false,
                    Ok(m) => got += m,
                    Err(_) => return false,
                }
            }
        }
        true
    }
}

fn strtol_prefix(s: &[u8]) -> i64 {
    let mut i = 0;
    while i < s.len() && matches!(s[i], b' ' | b'\t' | b'\n' | 0x0B | 0x0C | b'\r') {
        i += 1;
    }
    let mut neg = false;
    if i < s.len() && (s[i] == b'-' || s[i] == b'+') {
        neg = s[i] == b'-';
        i += 1;
    }
    let mut v: i64 = 0;
    let mut any = false;
    while i < s.len() && s[i].is_ascii_digit() {
        any = true;
        v = v
            .saturating_mul(10)
            .saturating_add((s[i] - b'0') as i64);
        i += 1;
    }
    if !any {
        return 0;
    }
    if neg {
        -v
    } else {
        v
    }
}

fn read_message(r: &mut Reader) -> Option<Vec<u8>> {
    let mut content_length: i64 = -1;
    loop {
        let mut line: Vec<u8> = Vec::new();
        loop {
            let c = r.getc()?;
            if c == b'\n' {
                break;
            }
            if line.len() < 1023 {
                line.push(c);
            }
        }
        if line.last() == Some(&b'\r') {
            line.pop();
        }
        if line.is_empty() {
            break;
        }
        if line.len() >= 15 && line[..15].eq_ignore_ascii_case(b"Content-Length:") {
            content_length = strtol_prefix(&line[15..]);
        }
    }
    if content_length < 0 || content_length > 64 * 1024 * 1024 {
        return None;
    }
    let mut body = vec![0u8; content_length as usize];
    if !r.read_exact(&mut body) {
        return None;
    }
    Some(body)
}

fn write_all_out(data: &[u8]) {
    let mut stdout = std::io::stdout().lock();
    let mut sent = 0;
    while sent < data.len() {
        match stdout.write(&data[sent..]) {
            Ok(0) => std::process::exit(1),
            Ok(m) => sent += m,
            Err(_) => std::process::exit(1),
        }
    }
    let _ = stdout.flush();
}

fn send_message(body: &[u8]) {
    // C の strlen と同じく最初の NUL まで
    let end = body.iter().position(|&c| c == 0).unwrap_or(body.len());
    let body = &body[..end];
    let header = format!("Content-Length: {}\r\n\r\n", body.len());
    write_all_out(header.as_bytes());
    write_all_out(body);
}

// ===== 開いているドキュメントの保持 =====

struct Doc {
    uri: Vec<u8>,
    text: Vec<u8>,
    version: i64,
}

fn open_doc(docs: &mut Vec<Doc>, uri: &[u8], text: &[u8], version: i64) -> usize {
    for (i, d) in docs.iter_mut().enumerate() {
        if d.uri == uri {
            d.text = text.to_vec();
            d.version = version;
            return i;
        }
    }
    docs.push(Doc {
        uri: uri.to_vec(),
        text: text.to_vec(),
        version,
    });
    docs.len() - 1
}

fn close_doc(docs: &mut Vec<Doc>, uri: &[u8]) {
    if let Some(i) = docs.iter().position(|d| d.uri == uri) {
        docs.swap_remove(i);
    }
}

// ===== チェック（別プロセスでコンパイラを回す） =====

#[derive(Clone)]
struct OneDiag {
    start_line: i64,
    start_char: i64,
    end_line: i64,
    end_char: i64,
    message: Vec<u8>,
}

fn offset_to_pos(text: &[u8], off: i64) -> (i64, i64) {
    let mut l: i64 = 0;
    let mut c: i64 = 0;
    let mut i: i64 = 0;
    while i < off && (i as usize) < text.len() && text[i as usize] != 0 {
        if text[i as usize] == b'\n' {
            l += 1;
            c = 0;
        } else if (text[i as usize] & 0xC0) != 0x80 {
            c += 1;
        }
        i += 1;
    }
    (l, c)
}

fn utf8_len_at(text: &[u8], off: i64) -> i64 {
    if off < 0 || off as usize >= text.len() {
        return 1;
    }
    let c = text[off as usize];
    if c < 0x80 {
        1
    } else if (c & 0xE0) == 0xC0 {
        2
    } else if (c & 0xF0) == 0xE0 {
        3
    } else if (c & 0xF8) == 0xF0 {
        4
    } else {
        1
    }
}

// 子プロセス側: パイプラインを回し、結果を標準出力へ書く
fn run_check_child(text: &[u8], is_prelude: bool) -> i32 {
    let mut cx = Cx::new();
    cx.error_capture_begin();
    let mut user_src: u32 = spell::NO_SRC;
    let run = (|| -> Result<(), Flow> {
        let mut prog = None;
        if !is_prelude {
            let prelude_path = cx.spell_prelude_path()?;
            prog = cx.load_program(&prelude_path)?;
        }
        // エラーで打ち切られても、診断の src 判定のために現在の
        // ソースだけは先に控える（C ではポインタ比較で判定する）
        let user_prog = cx.load_program_text(text.to_vec(), b"spell-lsp".to_vec());
        user_src = cx.cur_src;
        let user_prog = user_prog?;
        let prog = spell::driver::chain_programs(&mut cx, prog, user_prog);
        cx.error_capture_set_continue(true);
        cx.desugar(prog);
        cx.typecheck(prog)?;
        cx.error_capture_set_continue(false);
        Ok(())
    })();
    let _ = run;
    cx.error_capture_end();

    // 集まった診断を、位置＋メッセージで重複を取り除いて結果へ移す
    let mut result: Vec<OneDiag> = Vec::new();
    let diags = cx.sink.diags.clone();
    for d in &diags {
        let mut od = OneDiag {
            start_line: 0,
            start_char: 0,
            end_line: 0,
            end_char: 0,
            message: d.message.clone(),
        };
        if d.has_pos && d.src == user_src {
            let text_ref = cx.sources[user_src as usize].text();
            let text_len = text_ref.len() as i64;
            let mut so = d.start;
            let mut eo = d.end;
            if so < 0 {
                so = 0;
            }
            if so > text_len {
                so = text_len;
            }
            if eo <= so {
                eo = so + if so < text_len { utf8_len_at(text_ref, so) } else { 0 };
            }
            if eo > text_len {
                eo = text_len;
            }
            let (sl, sc) = offset_to_pos(text_ref, so);
            let (el, ec) = offset_to_pos(text_ref, eo);
            od.start_line = sl;
            od.start_char = sc;
            od.end_line = el;
            od.end_char = ec;
        }
        let dup = result.iter().any(|e| {
            e.start_line == od.start_line
                && e.start_char == od.start_char
                && e.end_line == od.end_line
                && e.end_char == od.end_char
                && e.message == od.message
        });
        if !dup && result.len() < 50 {
            result.push(od);
        }
    }

    // シリアライズして標準出力へ
    let mut out: Vec<u8> = Vec::new();
    out.extend_from_slice(&(result.len() as u32).to_le_bytes());
    for d in &result {
        out.extend_from_slice(&d.start_line.to_le_bytes());
        out.extend_from_slice(&d.start_char.to_le_bytes());
        out.extend_from_slice(&d.end_line.to_le_bytes());
        out.extend_from_slice(&d.end_char.to_le_bytes());
        out.extend_from_slice(&(d.message.len() as u32).to_le_bytes());
        out.extend_from_slice(&d.message);
    }
    let mut stdout = std::io::stdout().lock();
    let _ = stdout.write_all(&out);
    let _ = stdout.flush();
    0
}

fn fallback_diag(message: &str) -> Vec<OneDiag> {
    vec![OneDiag {
        start_line: 0,
        start_char: 0,
        end_line: 0,
        end_char: 0,
        message: message.as_bytes().to_vec(),
    }]
}

fn run_check(text: &[u8], is_prelude: bool) -> Vec<OneDiag> {
    let exe = match std::env::current_exe() {
        Ok(e) => e,
        Err(_) => return fallback_diag("言語サーバーの内部でプロセスを作成できませんでした"),
    };
    let mut cmd = std::process::Command::new(exe);
    cmd.env("SPELL_LSP_CHILD", "1")
        .env("SPELL_LSP_PRELUDE", if is_prelude { "1" } else { "0" })
        .stdin(std::process::Stdio::piped())
        .stdout(std::process::Stdio::piped())
        .stderr(std::process::Stdio::null());
    let mut child = match cmd.spawn() {
        Ok(c) => c,
        Err(_) => return fallback_diag("言語サーバーの内部でプロセスを作成できませんでした"),
    };
    {
        let mut stdin = child.stdin.take().unwrap();
        let _ = stdin.write_all(text);
        let _ = stdin.flush();
        // drop で閉じる（子は EOF まで読む）
    }
    let mut buf: Vec<u8> = Vec::new();
    let read_ok = match child.stdout.take() {
        Some(mut so) => so.read_to_end(&mut buf).is_ok(),
        None => false,
    };
    let _ = child.wait();
    if !read_ok || buf.len() < 4 {
        return fallback_diag("コンパイラが異常終了しました");
    }
    let count = u32::from_le_bytes(buf[0..4].try_into().unwrap()) as usize;
    if count > 50 {
        return fallback_diag("コンパイラが異常終了しました");
    }
    let mut pos = 4usize;
    let mut result = Vec::new();
    for _ in 0..count {
        if pos + 36 > buf.len() {
            return fallback_diag("コンパイラが異常終了しました");
        }
        let rd = |p: usize| -> i64 {
            i64::from_le_bytes(buf[p..p + 8].try_into().unwrap())
        };
        let (sl, sc, el, ec) = (rd(pos), rd(pos + 8), rd(pos + 16), rd(pos + 24));
        let mlen = u32::from_le_bytes(buf[pos + 32..pos + 36].try_into().unwrap()) as usize;
        pos += 36;
        if pos + mlen > buf.len() {
            return fallback_diag("コンパイラが異常終了しました");
        }
        let message = buf[pos..pos + mlen].to_vec();
        pos += mlen;
        result.push(OneDiag {
            start_line: sl,
            start_char: sc,
            end_line: el,
            end_char: ec,
            message,
        });
    }
    result
}

// ===== 診断の送信 =====

fn uri_is_prelude(uri: &[u8]) -> bool {
    uri.ends_with(b"/prelude.sp")
}

fn publish_diagnostics(doc: &Doc) {
    let r = run_check(&doc.text, uri_is_prelude(&doc.uri));
    let mut b: Vec<u8> = Vec::new();
    b.extend_from_slice(
        b"{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":",
    );
    buf_add_json_string(&mut b, &doc.uri);
    b.extend_from_slice(format!(",\"version\":{},\"diagnostics\":[", doc.version).as_bytes());
    for (i, d) in r.iter().enumerate() {
        if i > 0 {
            b.push(b',');
        }
        b.extend_from_slice(
            format!(
                "{{\"range\":{{\"start\":{{\"line\":{},\"character\":{}}},\"end\":{{\"line\":{},\"character\":{}}}}},\"severity\":1,\"source\":\"spell\",\"message\":",
                d.start_line, d.start_char, d.end_line, d.end_char
            )
            .as_bytes(),
        );
        buf_add_json_string(&mut b, &d.message);
        b.push(b'}');
    }
    b.extend_from_slice(b"]}}");
    send_message(&b);
}

fn publish_empty_diagnostics(uri: &[u8]) {
    let mut b: Vec<u8> = Vec::new();
    b.extend_from_slice(
        b"{\"jsonrpc\":\"2.0\",\"method\":\"textDocument/publishDiagnostics\",\"params\":{\"uri\":",
    );
    buf_add_json_string(&mut b, uri);
    b.extend_from_slice(b",\"diagnostics\":[]}}");
    send_message(&b);
}

fn respond_result(id: Option<&Json>, result_json: &str) {
    let mut b: Vec<u8> = Vec::new();
    b.extend_from_slice(b"{\"jsonrpc\":\"2.0\",\"id\":");
    buf_add_id(&mut b, id);
    b.extend_from_slice(b",\"result\":");
    b.extend_from_slice(result_json.as_bytes());
    b.push(b'}');
    send_message(&b);
}

fn respond_error(id: Option<&Json>, code: i64, message: &str) {
    let mut b: Vec<u8> = Vec::new();
    b.extend_from_slice(b"{\"jsonrpc\":\"2.0\",\"id\":");
    buf_add_id(&mut b, id);
    b.extend_from_slice(format!(",\"error\":{{\"code\":{},\"message\":", code).as_bytes());
    buf_add_json_string(&mut b, message.as_bytes());
    b.extend_from_slice(b"}}");
    send_message(&b);
}

// ===== メインループ =====

fn server_main() -> i32 {
    let mut reader = Reader {
        inner: std::io::stdin(),
        buf: [0; 8192],
        len: 0,
        pos: 0,
    };
    let mut docs: Vec<Doc> = Vec::new();
    let mut shutdown = false;
    while let Some(body) = read_message(&mut reader) {
        let msg = match json_parse(&body) {
            Some(m) => m,
            None => continue,
        };
        let id = json_obj_get(&msg, "id");
        let method = json_str(json_obj_get(&msg, "method"));
        let method = match method {
            Some(m) => m.to_vec(),
            None => continue,
        };
        if method.as_slice() == b"initialize" {
            respond_result(
                id,
                "{\"capabilities\":{\"textDocumentSync\":1},\"serverInfo\":{\"name\":\"spell-lsp\",\"version\":\"0.1.0\"}}",
            );
        } else if method.as_slice() == b"shutdown" {
            shutdown = true;
            respond_result(id, "null");
        } else if method.as_slice() == b"exit" {
            return if shutdown { 0 } else { 1 };
        } else if method.as_slice() == b"textDocument/didOpen" {
            let params = json_obj_get(&msg, "params");
            let td = params.and_then(|p| json_obj_get(p, "textDocument"));
            let uri = td.and_then(|t| json_str(json_obj_get(t, "uri")));
            let text = td.and_then(|t| json_str(json_obj_get(t, "text")));
            let version = td
                .and_then(|t| json_obj_get(t, "version"))
                .map(|v| match v {
                    Json::Num(n) => *n as i64,
                    _ => 0,
                })
                .unwrap_or(0);
            if let (Some(uri), Some(text)) = (uri, text) {
                let (uri, text) = (uri.to_vec(), text.to_vec());
                let idx = open_doc(&mut docs, &uri, &text, version);
                publish_diagnostics(&docs[idx]);
            }
        } else if method.as_slice() == b"textDocument/didChange" {
            let params = json_obj_get(&msg, "params");
            let td = params.and_then(|p| json_obj_get(p, "textDocument"));
            let uri = td.and_then(|t| json_str(json_obj_get(t, "uri")));
            let version = td
                .and_then(|t| json_obj_get(t, "version"))
                .map(|v| match v {
                    Json::Num(n) => *n as i64,
                    _ => 0,
                })
                .unwrap_or(0);
            let changes = params.and_then(|p| json_obj_get(p, "contentChanges"));
            let first = changes.and_then(|c| json_arr_get(c, 0));
            let text = first.and_then(|f| json_str(json_obj_get(f, "text")));
            if let (Some(uri), Some(text)) = (uri, text) {
                let (uri, text) = (uri.to_vec(), text.to_vec());
                let idx = open_doc(&mut docs, &uri, &text, version);
                publish_diagnostics(&docs[idx]);
            }
        } else if method.as_slice() == b"textDocument/didClose" {
            let params = json_obj_get(&msg, "params");
            let td = params.and_then(|p| json_obj_get(p, "textDocument"));
            let uri = td.and_then(|t| json_str(json_obj_get(t, "uri")));
            if let Some(uri) = uri {
                let uri = uri.to_vec();
                publish_empty_diagnostics(&uri);
                close_doc(&mut docs, &uri);
            }
        } else if id.is_some() {
            respond_error(id, -32601, "対応していないメソッドです");
        }
    }
    0
}

fn main() {
    if std::env::var_os("SPELL_LSP_CHILD").is_some() {
        // チェック用の子プロセスとして起動された
        let is_prelude = std::env::var("SPELL_LSP_PRELUDE")
            .map(|v| v == "1")
            .unwrap_or(false);
        let mut text: Vec<u8> = Vec::new();
        let _ = std::io::stdin().read_to_end(&mut text);
        std::process::exit(run_check_child(&text, is_prelude));
    }
    std::process::exit(server_main());
}
