def t001() { x=3; y=&x; return *y; }
def t002() { x=3; return *&x; }
def t003() { a=3; b=4; p=&a; q=&b; return *p + *q; }
def t004() { x=42; p=&x; q=&p; return **q; }
def t005() { x=3; y=&x; *y=5; return x; }
def t006() { x=3; y=&x; *y=*y+2; return x; }
def t007() { x=7; y=&x; z=&y; **z=9; return x; }
def t008() { x=3; y=&x; *y=8; return *y; }
def t009() { a = [10, 20, 30]; return a[0] + a[1] + a[2]; }
def t010() { a = [1, 2, 3]; a[1] = 42; return a[1]; }
def t011() { a = [5, 6, 7]; i = 2; return a[i]; }
def t012() { a = [1, 2, 3, 4]; s = 0; for x in a s = s + x; return s; }
def t013() { x = 3; a = [x, x * 2]; return a[1]; }
def t014() { a = [10, 20, 30]; return a.len; }
def t015() { a = [7]; return a.len; }
def t016() { a = [1, 2, 3, 4]; s = 0; for x in a s = s + x; return s; }
def count(a) { return a.len; }
def t017() { x = [1, 2, 3, 4, 5]; return count(x); }
def t018() { n = 3; a = [0; n]; a[0] = 10; a[2] = 30; return a[0] + a[1] + a[2] + a.len; }
def t019() { a = [7; 4]; return a[0] + a[3]; }
def t020() { a = [5; 0]; return a.len; }
def t021() { n = 5; a = [0; n]; for i in a.len a[i] = i * 2; s = 0; for x in a s = s + x; return s; }
def make(n) { return [9; n]; }
def t022() { a = make(3); return a[2] + a.len; }
def t023() { s = "hello"; return s.len; }
def t024() { s = "hello"; return s[0]; }
def t025() { s = "A\n"; return s[1]; }
def t026() { s = "abc".chars; s[1] = 120; return s[1]; }
def t027() { s = "hello"; c = 0; for ch in s.chars if (ch == 108) c = c + 1; return c; }
def t028() { s = "hi"; putchar(s[0]); putchar(s[1]); return 0; }
def t029() { s = ""; return s.len; }
def t030() { a = [1, 2] + [3]; return a[0] * 100 + a[1] * 10 + a[2]; }
def t031() { r = 0; if ([1, 2] == [1, 2]) { r = r + 1; } if ([1, 2] != [1, 3]) { r = r + 2; } if ([1, 2] == [1, 3]) { r = r + 4; } return r; }
def t032() { a = [7] * 2; b = 3 * [1]; return a.len * 10 + b.len; }
def t033() { s = "ab" + "cd"; r = 0; if (s == "abcd") { r = 1; } return s.len * 10 + r; }
def t034() { s = "hello"; return s[1] + s.len; }
def t035() { t = 0; for s in ["ab", "cde"] { t = t + s.len; } return t; }
def t036() { s = 0; for x in [1, 2, 3, 4] { if (x == 3) { break; } s = s + x; } return s; }
def t037() { s = 0; for a in [[1, 2], [3]] { for x in a { s = s + x; } } return s; }
def t038() { r = [1, 2, 3].rev; return r[0] * 100 + r[1] * 10 + r[2]; }
def t039() { s = 0; for i in (1..4).rev { s = s * 10 + i; } if ("abc".rev == "cba") { s = s + 1; } return s; }

// step 65: 配列への append
def addit(a: [int], v: int) { a.append(v); }
def addlen(a: [int], v: int) -> int { a.append(v); return a.len; }
def g100(a: [int]) -> int { a.append(100); return 7; }
def growrows(rows: [[int]]) -> int { rows.append([9]); return 5; }
icnt = 0;
def nexti() -> int { icnt = icnt + 1; return 0; }
garr = [0; 3];
def t040() { a = [1, 2, 3]; a.append(4); return a.len * 1000 + a[3] * 100 + a[0] * 10 + a[2]; }
def t041() { a: [int] = []; a.append(7); a.append(8); return a.len * 100 + a[0] * 10 + a[1]; }
def t042() { a = [0; 2]; a.append(5); return a.len * 10 + a[2]; }
def t043() { a: [int] = []; i = 0; while (i < 1000) { a.append(i); i = i + 1; } s = 0; for x in a { s = s + x; } return s + a[999]; }
def t044() { a = ["a", "b"]; a.append("c"); return a.len * 10 + a[2].len; }
def t045() { a = [1, 2, 3]; b = a; a.append(4); return b.len * 10 + a.len; }
def t046() { a = [1, 2, 3]; a.append(4); b = a; a.append(5); return b.len * 10 + b[4]; }
def t047() { a = [1]; a.append(2); addit(a, 3); return a.len * 10 + a[2]; }
def t048() { a = [1, 2, 3]; r = addlen(a, 4); return r * 10 + a.len; }
def t049() { rows = [[1, 2], [3]]; rows[0].append(9); return rows[0].len * 100 + rows[0][2]; }
def t050() { a = [1]; n = a.append(2); return n * 10 + a.len; }
def t051() { a = [1, 2, 3]; a.append(g100(a)); return a.len * 1000 + a[3]; }
def t052() { rows = [[1, 2], [3, 4]]; rows[nexti()].append(9); return icnt * 100 + rows[0].len; }
def t053() { a = [1, 2, 3]; s = 0; n = 0; for x in a { s = s + x; n = n + 1; a.append(10 + n); } return s * 1000 + n * 100 + a.len; }
def t054() { b = [1, 2, 3]; b.append(4); s = 0; n = 0; for x in b { s = s + x; n = n + 1; if (n < 4) { b.append(100); } } return s * 10 + n; }
def t055() { a = [1, 2, 3]; f = fn() -> int { a.append(4); return a.len; }; r = f(); return r * 10 + a.len; }
def t056() { a: [int] = []; return a.append(10) * 100 + a.append(20) * 10 + a.len; }
def t057() { a = [0; 1]; i = 0; while (i < 100) { a.append(i * i); i = i + 1; } return a[51] + a.len; }
def t058() { r = [[1, 2]]; r[0].append(growrows(r)); return r.len * 1000 + r[0].len * 100 + r[0][2]; }
def t059() { s = "ab"; s.chars.append(99); return s.len * 100 + s[2]; }
def t060() { garr.append(40); garr.append(50); return garr.len * 100 + garr[3] + garr[4]; }
def t061() { r = [1, 2, 3].rev; r.append(4); return r.len * 1000 + r[0] * 100 + r[3]; }
def t062() { s = "abc".rev; s.chars.append(100); return s.len * 1000 + s[3]; }

def mark(n) {
    print_int(n); putchar(10); fflush(0);
    return 0;
}

def check(got, expected) {
    if (got != expected) return 1;
    return 0;
}

def main() {
    fails = 0;
    
    mark(1); fails=fails+check(t001(), 3);
    mark(2); fails=fails+check(t002(), 3);
    mark(3); fails=fails+check(t003(), 7);
    mark(4); fails=fails+check(t004(), 42);
    mark(5); fails=fails+check(t005(), 5);
    mark(6); fails=fails+check(t006(), 5);
    mark(7); fails=fails+check(t007(), 9);
    mark(8); fails=fails+check(t008(), 8);
    mark(9); fails=fails+check(t009(), 60);
    mark(10); fails=fails+check(t010(), 42);
    mark(11); fails=fails+check(t011(), 7);
    mark(12); fails=fails+check(t012(), 10);
    mark(13); fails=fails+check(t013(), 6);
    mark(14); fails=fails+check(t014(), 3);
    mark(15); fails=fails+check(t015(), 1);
    mark(16); fails=fails+check(t016(), 10);
    mark(17); fails=fails+check(t017(), 5);
    mark(18); fails=fails+check(t018(), 43);
    mark(19); fails=fails+check(t019(), 14);
    mark(20); fails=fails+check(t020(), 0);
    mark(21); fails=fails+check(t021(), 20);
    mark(22); fails=fails+check(t022(), 12);
    mark(23); fails=fails+check(t023(), 5);
    mark(24); fails=fails+check(t024(), 104);
    mark(25); fails=fails+check(t025(), 10);
    mark(26); fails=fails+check(t026(), 120);
    mark(27); fails=fails+check(t027(), 2);
    mark(28); fails=fails+check(t028(), 0);
    mark(29); fails=fails+check(t029(), 0);
    mark(30); fails=fails+check(t030(), 123);
    mark(31); fails=fails+check(t031(), 3);
    mark(32); fails=fails+check(t032(), 23);
    mark(33); fails=fails+check(t033(), 41);
    mark(34); fails=fails+check(t034(), 106);
    mark(35); fails=fails+check(t035(), 5);
    mark(36); fails=fails+check(t036(), 3);
    mark(37); fails=fails+check(t037(), 6);
    mark(38); fails=fails+check(t038(), 321);
    mark(39); fails=fails+check(t039(), 322);
    mark(40); fails=fails+check(t040(), 4413);
    mark(41); fails=fails+check(t041(), 278);
    mark(42); fails=fails+check(t042(), 35);
    mark(43); fails=fails+check(t043(), 500499);
    mark(44); fails=fails+check(t044(), 31);
    mark(45); fails=fails+check(t045(), 34);
    mark(46); fails=fails+check(t046(), 55);
    mark(47); fails=fails+check(t047(), 33);
    mark(48); fails=fails+check(t048(), 43);
    mark(49); fails=fails+check(t049(), 309);
    mark(50); fails=fails+check(t050(), 22);
    mark(51); fails=fails+check(t051(), 4007);
    mark(52); fails=fails+check(t052(), 103);
    mark(53); fails=fails+check(t053(), 6306);
    mark(54); fails=fails+check(t054(), 3107);
    mark(55); fails=fails+check(t055(), 43);
    mark(56); fails=fails+check(t056(), 122);
    mark(57); fails=fails+check(t057(), 2601);
    mark(58); fails=fails+check(t058(), 1305);
    mark(59); fails=fails+check(t059(), 399);
    mark(60); fails=fails+check(t060(), 590);
    mark(61); fails=fails+check(t061(), 4304);
    mark(62); fails=fails+check(t062(), 4100);

    print_str("done"); putchar(10);
    return fails > 0;
}
