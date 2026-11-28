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
def t024() { s = "hello"; return s.chars[0]; }
def t025() { s = "A\n"; return s.chars[1]; }
def t026() { s = "abc".chars; s[1] = 120; return s[1]; }
def t027() { s = "hello"; c = 0; for ch in s.chars if (ch == 108) c = c + 1; return c; }
def t028() { s = "hi"; putchar(s.chars[0]); putchar(s.chars[1]); return 0; }
def t029() { s = ""; return s.len; }
def t030() { a = [1, 2] + [3]; return a[0] * 100 + a[1] * 10 + a[2]; }
def t031() { r = 0; if ([1, 2] == [1, 2]) { r = r + 1; } if ([1, 2] != [1, 3]) { r = r + 2; } if ([1, 2] == [1, 3]) { r = r + 4; } return r; }
def t032() { a = [7] * 2; b = 3 * [1]; return a.len * 10 + b.len; }
def t033() { s = "ab" + "cd"; r = 0; if (s == "abcd") { r = 1; } return s.len * 10 + r; }
def t034() { s = "hello"; return s.chars[1] + s.len; }
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
// step 96: b = a はディープコピー。a への append も要素の書き換えも
// b へ届かない（step 65 の別名意味論は廃止）
def t046() { a = [1, 2, 3]; a.append(4); b = a; a.append(5); a[0] = 99; return b.len * 100 + b[0] * 10 + b[3]; }
// step 96: 引数の配列はコピーされる。addit の append は引数の複製に
// だけ効き、呼び出し側の a は変わらない
def t047() { a = [1]; a.append(2); addit(a, 3); return a.len * 10 + a[1]; }
def t048() { a = [1, 2, 3]; r = addlen(a, 4); return r * 10 + a.len; }
def t049() { rows = [[1, 2], [3]]; rows[0].append(9); return rows[0].len * 100 + rows[0][2]; }
def t050() { a = [1]; n = a.append(2); return n * 10 + a.len; }
def t051() { a = [1, 2, 3]; a.append(g100(a)); return a.len * 1000 + a[3]; }
def t052() { rows = [[1, 2], [3, 4]]; rows[nexti()].append(9); return icnt * 100 + rows[0].len; }
def t053() { a = [1, 2, 3]; s = 0; n = 0; for x in a { s = s + x; n = n + 1; a.append(10 + n); } return s * 1000 + n * 100 + a.len; }
// step 96: 反復はイテレータが受け取った時点のスナップショットを
// 回る。ループ中の append は b を伸ばすが、反復には増えない
def t054() { b = [1, 2, 3]; b.append(4); s = 0; n = 0; for x in b { s = s + x; n = n + 1; if (n < 4) { b.append(100); } } return s * 1000 + n * 10 + b.len; }
def t055() { a = [1, 2, 3]; f = fn() -> int { a.append(4); return a.len; }; r = f(); return r * 10 + a.len; }
def t056() { a: [int] = []; return a.append(10) * 100 + a.append(20) * 10 + a.len; }
def t057() { a = [0; 1]; i = 0; while (i < 100) { a.append(i * i); i = i + 1; } return a[51] + a.len; }
def t058() { r = [[1, 2]]; r[0].append(growrows(r)); return r.len * 1000 + r[0].len * 100 + r[0][2]; }
def t059() { s = "ab"; s.chars.append(99); return s.len * 100 + s.chars[2]; }
def t060() { garr.append(40); garr.append(50); return garr.len * 100 + garr[3] + garr[4]; }
def t061() { r = [1, 2, 3].rev; r.append(4); return r.len * 1000 + r[0] * 100 + r[3]; }
def t062() { s = "abc".rev; s.chars.append(100); return s.len * 1000 + s.chars[3]; }

// step 83: 配列メソッド sort / sorted / sum / min / max / count
class Item {
    v: int;
    def __init__(self, v: int) { self.v = v; }
    def __add__(self, o: Item) -> Item { return Item(self.v + o.v); }
    def __lt__(self, o: Item) -> bool { return self.v < o.v; }
    def __le__(self, o: Item) -> bool { return self.v <= o.v; }
    def __eq__(self, o: Item) -> bool { return self.v == o.v; }
    def __ne__(self, o: Item) -> bool { return !(self == o); }
    def __hash__(self) -> int { return self.v; }
    def __str__(self) -> str { return self.v.__str__(); }
}
def t063() { a = [5, 2, 9, 1, 7, 2, 8]; a.sort(); return a[3] * 1000 + a[0] * 100 + a[1] * 10 + a[6]; }
def t064() { a = [1, 2, 3, 4, 5]; a.sort(); return a[0] * 10 + a[4]; }
def t065() { a = [5, 4, 3, 2, 1]; a.sort(); return a[0] * 10 + a[4]; }
def t066() { a = [3, 1, 3, 1, 3]; a.sort(); return a[0] * 100 + a[2] * 10 + a[4]; }
def t067() { a: [int] = []; a.sort(); return a.len; }
def t068() { a = [42]; a.sort(); return a[0]; }
def t069() { a = ["banana", "apple", "cherry"]; a.sort(); if (a[0] == "apple" && a[1] == "banana" && a[2] == "cherry") { return 1; } return 0; }
def t070() { a = [Item(3), Item(1), Item(2)]; a.sort(); return a[0].v * 100 + a[1].v * 10 + a[2].v; }
def t071() { a = [3, 1, 2]; b = a.sorted(); r = 0; if (b[0] == 1 && b[1] == 2 && b[2] == 3) { r = r + 1; } if (a[0] == 3 && a[1] == 1 && a[2] == 2) { r = r + 2; } return r; }
def t072() { a = [1, 2, 3, 4, 5]; return a.sum(); }
def t073() { a = [-5, 3, -2, 10]; return a.sum(); }
def t074() { a = [42]; return a.sum(); }
def t075() { a = ["ab", "cd", "ef"]; if (a.sum() == "abcdef") { return 1; } return 0; }
def t076() { a = [(1, 2), (3, 4), (10, 20)]; p = a.sum(); return p.x * 100 + p.y; }
def t077() { a = [5, 2, 9, 1, 7]; return a.min(); }
def t078() { a = [5, 2, 9, 1, 7]; return a.max(); }
def t079() { a = [-3, -3, 0, -10, 5, 5]; return a.min() * 100 + a.max(); }
def t080() { a = ["banana", "apple", "cherry"]; if (a.min() == "apple" && a.max() == "cherry") { return 1; } return 0; }
def t081() { a = [1, 2, 2, 3, 2]; return a.count(2); }
def t082() { a = [1, 2, 3]; return a.count(9); }
def t083() { a = ["x", "y", "x"]; return a.count("x"); }
def t084() { a = [Item(1), Item(2), Item(1)]; return a.count(Item(1)); }
def t085() {
    n = 10000;
    a = [0; n];
    seed = 123456789;
    s0 = 0;
    i = 0;
    while (i < n) {
        seed = (seed * 1103515245 + 12345) % 2147483647;
        a[i] = seed % 100000;
        s0 = s0 + a[i];
        i = i + 1;
    }
    a.sort();
    ok = 1;
    s1 = 0;
    i = 0;
    while (i < n) {
        if (i > 0 && a[i - 1] > a[i]) { ok = 0; }
        s1 = s1 + a[i];
        i = i + 1;
    }
    if (s0 != s1) { ok = 0; }
    return ok;
}
def t086() { a: [int] = []; b = a.sorted(); return b.len; }
def t087() { a = [7]; return a.min() * 10 + a.max(); }
def t088() { a = [5; 100]; a.sort(); return a[0] * 10 + a[99]; }
def t089() { n = 1000; a = [0; n]; i = 0; while (i < n) { a[i] = n - i; i = i + 1; } a.sort(); return a[0] + a[999]; }
def t090() { a = [(2, 1), (1, 5), (1, 3)]; a.sort(); return a[0].x * 100 + a[0].y * 10 + a[2].x; }

// step 83: __lt__ / __add__ の無い要素型でも、それらを要求しない
// メソッド（count・反復）は使える（実体化メソッドの遅延検査の回帰）
class Cnt {
    v: int;
    def __init__(self, v: int) { self.v = v; }
    def __eq__(self, o: Cnt) -> bool { return self.v == o.v; }
    def __ne__(self, o: Cnt) -> bool { return !(self == o); }
    def __hash__(self) -> int { return self.v; }
    def __str__(self) -> str { return self.v.__str__(); }
}
def t091() { a = [Cnt(1), Cnt(2), Cnt(1)]; return a.count(Cnt(1)); }
def t092() { f = fn() -> int { a = [3, 1, 2]; a.sort(); return a[0] * 10 + a[2]; }; return f(); }
def t093() { a = [Cnt(1), Cnt(2), Cnt(3)]; s = 0; for x in a { s = s + x.v; } return s; }
def t094() { s = "hello"; c = s[1]; if (c == "e" && c.len == 1) { return 1; } return 0; }
def t095() { s = "abc"; t = s[0] + s[2]; if (t == "ac") { return 1; } return 0; }
def t096() { s = "Az"; return s.chars[0] * 1000 + s.chars[1]; }
def t097() { rows = ["S..", ".#.", "..G"]; if (rows[1][1] == "#" && rows[0][0] == "S" && rows[2][2] == "G") { return 1; } return 0; }
def t098() { a = 'hello'; b = "hello"; if (a == b && a.len == 5) { return 1; } return 0; }
def t099() { a = 'it\'s'; if (a == "it's" && a.len == 4) { return 1; } return 0; }
def t100() { a = 'say "hi"'; b = 'x' + "y"; if (a == "say \"hi\"" && b == "xy") { return 1; } return 0; }
def t101() { s = "abc"; if (s[1] == 'b' && s[0] == 'a') { return 1; } return 0; }
def t102() { a = [[-1; 4]; 3]; a[1][0] = 7; return a[0][0] + a[2][0] + a[1][0]; }
def t103() { a = [[[1; 2]; 3]; 4]; a[2][1][0] = 9; return a[2][1][0] * 10 + a[0][1][0] + a[2][0][0]; }
def t104() { a = [[1, 2]; 3]; a[1][0] = 9; return a[0][0] + a[1][0] + a[2][0]; }
def t105() { a = [[]; 3]; a[0].append(5); return a[0].len * 10 + a[1].len; }
// step 96: [fill; n] は fill の型が配列なら、リテラルでなくても各
// スロットが独立（step 82 ではリテラル形だけだった）。a[1] への
// 書き込みは a[0] にも row にも届かない
def t106() { row = [-1; 2]; a = [row; 2]; a[1][0] = 7; return a[0][0]; }
def t107() { n = 0 - 1; a = [[1; 2]; n]; m = 0 - 2; b = [5; m]; return a.len + b.len; }
def t108() { s = "hello world"; r = 0; if (s[0:5] == "hello") { r = r + 1; } if (s[6:11] == "world") { r = r + 2; } if (s[0:5].len == 5) { r = r + 4; } return r; }
def t109() { a = [10, 20, 30, 40, 50]; b = a[1:3]; r = b[0] * 100 + b[1]; b[0] = 99; return r * 1000 + a[1]; }
def t110() { s = "abcdef"; r = 0; if (s[:] == s) { r = r + 1; } if (s[2:] == "cdef") { r = r + 2; } if (s[:2] == "ab") { r = r + 4; } if (s[-2:] == "ef") { r = r + 8; } if (s[:-2] == "abcd") { r = r + 16; } if (s[3:2] == "") { r = r + 32; } if (s[0:100] == s) { r = r + 64; } if (s[-100:2] == "ab") { r = r + 128; } return r; }
def t111() { a = [1, 2, 3, 4]; r = 0; if (a[1:100].len == 3) { r = r + 1; } if (a[-100:2] == [1, 2]) { r = r + 2; } if (a[2:2].len == 0) { r = r + 4; } if (a[3:1].len == 0) { r = r + 8; } b = a[:]; b[0] = 9; if (a[0] == 1) { r = r + 16; } return r; }
def t112() { s = "hello"; r = 0; if (s.starts_with("he")) { r = r + 1; } if (s.starts_with("")) { r = r + 2; } if (!s.starts_with("lo")) { r = r + 4; } if (!s.starts_with("hello!")) { r = r + 8; } if (s.ends_with("lo")) { r = r + 16; } if (s.ends_with("")) { r = r + 32; } if (!s.ends_with("he")) { r = r + 64; } if (!s.ends_with("xhello")) { r = r + 128; } return r; }
def t113() { s = "hello world"; r = s.find("o") * 100 + s.find("world"); if (s.find("z") != -1) { r = r + 10000; } if (s.find("") != 0) { r = r + 10000; } if (!s.contains("lo")) { r = r + 10000; } if (s.contains("zz")) { r = r + 10000; } return r; }
def t114() { a = "a,b,,c".split(","); r = a.len * 1000; if (a[0] == "a") { r = r + 1; } if (a[1] == "b") { r = r + 2; } if (a[2] == "") { r = r + 4; } if (a[3] == "c") { r = r + 8; } b = ",x,".split(","); if (b.len == 3 && b[0] == "" && b[2] == "") { r = r + 16; } c = "abc".split(","); if (c.len == 1 && c[0] == "abc") { r = r + 32; } d = "a--b--c".split("--"); if (d.len == 3 && d[1] == "b") { r = r + 64; } e = "ab".split(""); if (e.len == 1 && e[0] == "ab") { r = r + 128; } return r; }
def t115() { r = 0; if (",".join(["x", "y", "z"]) == "x,y,z") { r = r + 1; } if (",".join([]) == "") { r = r + 2; } if ("-".join(["one"]) == "one") { r = r + 4; } if ("".join(["a", "b"]) == "ab") { r = r + 8; } if ("::".join(["a", "b"]) == "a::b") { r = r + 16; } return r; }
def t116() { return "123".to_int() * 1000000 + "-42".to_int() * 1000 + "  +17x".to_int() * 10 + "abc".to_int(); }
def t117() { s = "abcdefgh"; r = 0; if (s[1:6][1:3] == "cd") { r = r + 1; } if (s[1:3] + "!" == "bc!") { r = r + 2; } t = ""; for part in "a,b,c".split(",") { t = t + part; } if (t == "abc") { r = r + 4; } if ("hello"[1:3] == "el") { r = r + 8; } return r; }
def t118() { a = [10, 20, 30]; return a[-1] * 10000 + a[3] * 100 + a[4]; }
def t119() { a = [10, 20, 30]; return a[-4] * 10000 + a[-7] * 100 + a[-3]; }
def t120() { a = [10, 20, 30]; a[-1] = 99; a[3] = 11; a[5] = 77; return a[0] * 10000 + a[1] * 100 + a[2]; }
def t121() { g = [[1, 2], [3, 4]]; return g[-1][-1] * 100 + g[2][3]; }
def t122() { g = [[1, 2], [3, 4]]; p = (2, 3); q = (-1, -1); return g[p] * 100 + g[q]; }
def t123() { a = [1, 2, 3]; a[-1] += 10; a[3] += 100; return a[0] * 10000 + a[2]; }
def t124() { g = [[1], [2]]; g[-1].append(5); g[2].append(7); return g[0].len * 1000 + g[1].len * 100 + g[1][1] * 10 + g[0][1]; }
def t125() { s = "abc"; r = 0; if (s[-1] == "c") { r = r + 1; } if (s[3] == "a") { r = r + 2; } if (s[4] == "b") { r = r + 4; } if (s[-4] == "c") { r = r + 8; } return r; }
def t126() { a = [10, 20, 30]; r = 0; if (a?[-1]) { r = r + 1; } if (a?[3] == 10) { r = r + 2; } if (a?[2] == 30) { r = r + 4; } s = "abc"; if (s?[-1]) { r = r + 8; } if (s?[3] == "a") { r = r + 16; } if (s?[2] == "c") { r = r + 32; } return r; }
def t127() { a = [10, 20, 30]; return a[-3] * 1000000 + a[6] * 10000 + a[-4] * 100 + a[7]; }
def t128() { b = [42]; return b[1] * 1000 + b[-1] * 100 + b[2] * 10 + b[-2]; }
def t129() { a = [10, 20, 30]; a[-3] = 1; a[6] = 2; a[-4] = 3; a[7] = 4; return a[0] * 10000 + a[1] * 100 + a[2]; }
def t130() { a = [10, 20, 30]; i = 0 - 3; r = a[i]; i = 6; r = r * 100 + a[i]; i = 0 - 4; r = r * 100 + a[i]; i = 7; r = r * 100 + a[i]; return r; }
def t131() { s = "abc"; r = 0; if (s[-3] == "a") { r = r + 1; } if (s[6] == "a") { r = r + 2; } if (s[-4] == "c") { r = r + 4; } if (s[7] == "b") { r = r + 8; } return r; }
def t132() { a = [10, 20, 30]; return a[5] * 100 + a[-2]; }
def t133() { r = 0; if ("hello world".replace("o", "0") == "hell0 w0rld") { r = r + 1; } if ("aaa".replace("a", "bb") == "bbbbbb") { r = r + 2; } if ("abc".replace("z", "!") == "abc") { r = r + 4; } if ("hello".replace("hello", "bye") == "bye") { r = r + 8; } if ("a-b-c".replace("-", "") == "abc") { r = r + 16; } if ("aaa".replace("aa", "b") == "ba") { r = r + 32; } return r; }
def t134() { r = 0; if ("abc".replace("", "-") == "-a-b-c-") { r = r + 1; } if ("".replace("", "-") == "-") { r = r + 2; } if ("".replace("a", "b") == "") { r = r + 4; } if ("ab".replace("", "") == "ab") { r = r + 8; } if ("hello".replace("l", "") == "heo") { r = r + 16; } return r; }
def t135() { return "aXbXc".count("X") * 10000 + "aaaa".count("aa") * 1000 + "abc".count("") * 100 + "".count("") * 10 + "abc".count("z") + "aaa".count("aa"); }
def t136() { r = 0; if ("  hi  ".strip() == "hi") { r = r + 1; } if (("\t\n hi " + chr(13) + "\n").strip() == "hi") { r = r + 2; } if ("   ".strip() == "") { r = r + 4; } if ("".strip() == "") { r = r + 8; } if ("hi".strip() == "hi") { r = r + 16; } if (" a b ".strip() == "a b") { r = r + 32; } return r; }
def t137() { r = 0; if ("aBc9z".upper() == "ABC9Z") { r = r + 1; } if ("AbC9Z".lower() == "abc9z") { r = r + 2; } if ("".upper() == "" && "".lower() == "") { r = r + 4; } if ("hello".upper().lower() == "hello") { r = r + 8; } if ("!? ".upper() == "!? " && "!? ".lower() == "!? ") { r = r + 16; } return r; }
def t138() { r = 0; if (chr(65) == "A") { r = r + 1; } if (chr(97) + chr(98) == "ab") { r = r + 2; } if (chr(10) == "\n") { r = r + 4; } if (chr("A".chars[0]) == "A") { r = r + 8; } if (chr(48 + 5) == "5") { r = r + 16; } return r; }
def t139() { r = 0; if ("\r".chars[0] == 13) { r = r + 1; } if ("\r".len == 1) { r = r + 2; } if ('\r'.chars[0] == 13) { r = r + 4; } if ("\x41" == "A") { r = r + 8; } if ("\x61\x62" == "ab") { r = r + 16; } if ('\x41' == "A") { r = r + 32; } if ("\x4a" == "J") { r = r + 64; } if ("\n".chars[0] == 10) { r = r + 128; } if ("\t".chars[0] == 9) { r = r + 256; } if ("\\".chars[0] == 92) { r = r + 512; } if ("\"".chars[0] == 34) { r = r + 1024; } if ('\''.chars[0] == 39) { r = r + 2048; } if ("\0".chars[0] == 0) { r = r + 4096; } if ("\0".len == 1) { r = r + 8192; } return r; }
def t140() { r = 0; if ("ab" * 3 == "ababab") { r = r + 1; } if ("ab" * 1 == "ab") { r = r + 2; } if ("ab" * 0 == "") { r = r + 4; } if ("ab" * -2 == "") { r = r + 8; } if ("" * 5 == "") { r = r + 16; } if ("a" * 5 == "aaaaa") { r = r + 32; } s = "xy"; if (s * 2 == "xyxy") { r = r + 64; } if (("ab" * 2).len == 4) { r = r + 128; } if ("ab" * 3 + "c" == "abababc") { r = r + 256; } return r; }
comp_calls = 0
def comp_counted() { comp_calls = comp_calls + 1; return [7, 8]; }
comp_calls2 = 0
def comp_bump(v) { comp_calls2 = comp_calls2 + 1; return v * 10; }
def t141() { a = [1, 2, 3, 4]; b = [x * 2 for x in a]; r = 0; if (b.len == 4) { r = r + 1; } if (b[0] == 2) { r = r + 2; } if (b[3] == 8) { r = r + 4; } if (b.sum() == 20) { r = r + 8; } return r; }
def t142() { a = [1, 2, 3, 4]; b = [x x@a]; c = [x * x x@a]; r = 0; if (b.len == 4) { r = r + 1; } if (b[2] == 3) { r = r + 2; } if (c[3] == 16) { r = r + 4; } return r; }
def t143() { a = [1, 2, 3, 4]; r = 0; o = [x for x in a if x % 2 == 1]; if (o.len == 2 && o[0] == 1) { r = r + 1; } if (o[1] == 3) { r = r + 2; } g = [x x@a if x > 2]; if (g.len == 2 && g[1] == 4) { r = r + 4; } m = [x for x in a if x > 1 if x < 4]; if (m.len == 2 && m[0] == 2 && m[1] == 3) { r = r + 8; } return r; }
def t144() { r = 0; a = [i + 1 for i in 3]; if (a.len == 3 && a[2] == 3) { r = r + 1; } s = [c for c in "ab"]; if (s.len == 2) { r = r + 2; } if (s[0] == "a" && s[1] == "b") { r = r + 4; } g = [i * 2 for i in 1..4]; if (g.len == 3 && g[0] == 2 && g[2] == 6) { r = r + 8; } return r; }
def t145() { d = Dict<str, int>(zero_int); d["b"] = 2; d["a"] = 1; ks = [k for k in d]; vs = [d[k] k@d]; r = 0; if (ks.len == 2) { r = r + 1; } if (ks[0] == "b" && ks[1] == "a") { r = r + 2; } if (vs.sum() == 3) { r = r + 4; } return r; }
def t146() { r = [v for v in comp_counted()]; return comp_calls * 100 + r[0] * 10 + r[1]; }
def t147() { x = 99; a = [1, 2]; m = [x for x in a]; r = 0; if (x == 99) { r = r + 1; } if (m[1] == 2) { r = r + 2; } y = [10, 20]; s = [y for y in y]; if (s[0] == 10 && s[1] == 20) { r = r + 4; } if (y[0] == 10 && y.len == 2) { r = r + 8; } return r; }
def t148() { r = 0; e = [x for x in []]; if (e.len == 0) { r = r + 1; } e2 = [x x@[] if x > 0]; if (e2.len == 0) { r = r + 2; } e3 = [i for i in 0]; if (e3.len == 0) { r = r + 4; } return r; }
def t149() { b = [[1, 2], [3]]; n = [[y for y in x] for x in b]; r = 0; if (n.len == 2) { r = r + 1; } if (n[0][1] == 2) { r = r + 2; } if (n[1][0] == 3) { r = r + 4; } m = [[v for v in [5]], [6]]; if (m[0][0] == 5) { r = r + 8; } if (m[1][0] == 6) { r = r + 16; } return r; }
def t150() { a = [3, 1, 4, 1, 5]; r = 0; if ([v for v in a].max() == 5) { r = r + 1; } if ([v for v in a].min() == 1) { r = r + 2; } if ([v for v in a].sum() == 14) { r = r + 4; } if ([v for v in a].sorted()[0] == 1) { r = r + 8; } if ([v * 2 for v in a][1] == 2) { r = r + 16; } if ([v for v in a][1:3].len == 2) { r = r + 32; } if (![v for v in a]?[9]) { r = r + 64; } return r; }
def t151() { a = [3, 1, 4, 1, 5]; r = 0; if ([v v@a].max() == 5) { r = r + 1; } if ([v v@a].min() == 1) { r = r + 2; } if ([v v@a].sum() == 14) { r = r + 4; } if ([v v@a].sorted()[4] == 5) { r = r + 8; } if ([v v@a][0] == 3) { r = r + 16; } if ([v v@a][0:1].len == 1) { r = r + 32; } return r; }
def t152() { src = [1, 2, 3]; got = [v for v in src]; got.append(99); r = 0; if (src.len == 3) { r = r + 1; } if (got.len == 4) { r = r + 2; } m = [comp_bump(v) for v in src]; if (comp_calls2 == 3) { r = r + 4; } if (m[2] == 30) { r = r + 8; } return r; }
def t153() { a = [x for x in (x = [1])]; r = 0; if (a[0] == 1) { r = r + 1; } if (x[0] == 1) { r = r + 2; } b = [x for x in (x = [9]) if x == 9]; if (b.len == 1 && b[0] == 9) { r = r + 4; } return r; }
def t154() { w = 3; h = 2; d = [[-1] * w] * h; d[0][0] = 5; return d[1][0] * 10 + d[0][1]; }
def t155() { a = [[[7] * 2] * 2] * 2; a[0][0][0] = 1; return a[1][1][1] * 100 + a[0][1][0] * 10 + a[0][0][1]; }
def t156() { a = [[1, 2]]; b = a * 3; b[0][0] = 9; b[2][1] = 8; return a[0][0] * 1000 + b[1][0] * 100 + b[2][1] * 10 + b[0][1]; }
def t157() { r = [2, 1]; a = [r.sorted(); 2]; a[0][0] = 7; return a[1][0] * 10 + a[1][1]; }
def t158() { a = [[[1]]]; b = a; b[0][0][0] = 9; return a[0][0][0]; }
def t159() { x = [1, 2]; a = [x, x]; a[0][0] = 9; return x[0] * 100 + a[1][0] * 10 + a[0][1]; }
def t160() { v = [1]; a: [[int]] = []; a.append(v); v[0] = 9; a[0][0] = 5; return v[0] * 10 + a[0][0]; }
def t161() { src = [7, 8]; dst = [[0, 0]]; dst[0] = src; src[0] = 1; return dst[0][0] * 10 + dst[0][1]; }
def t162() { a = [1, 2] * 3; a[0] = 9; return a[3] * 100 + a[4] * 10 + a[5]; }
def t163() { a = [[1]]; b = [[2]]; a, b = b, a; a[0][0] = 9; return b[0][0] * 10 + a[0][0]; }

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
    mark(46); fails=fails+check(t046(), 414);
    mark(47); fails=fails+check(t047(), 22);
    mark(48); fails=fails+check(t048(), 43);
    mark(49); fails=fails+check(t049(), 309);
    mark(50); fails=fails+check(t050(), 22);
    mark(51); fails=fails+check(t051(), 4007);
    mark(52); fails=fails+check(t052(), 103);
    mark(53); fails=fails+check(t053(), 6306);
    mark(54); fails=fails+check(t054(), 10047);
    mark(55); fails=fails+check(t055(), 43);
    mark(56); fails=fails+check(t056(), 122);
    mark(57); fails=fails+check(t057(), 2601);
    mark(58); fails=fails+check(t058(), 1305);
    mark(59); fails=fails+check(t059(), 399);
    mark(60); fails=fails+check(t060(), 590);
    mark(61); fails=fails+check(t061(), 4304);
    mark(62); fails=fails+check(t062(), 4100);
    mark(63); fails=fails+check(t063(), 5129);
    mark(64); fails=fails+check(t064(), 15);
    mark(65); fails=fails+check(t065(), 15);
    mark(66); fails=fails+check(t066(), 133);
    mark(67); fails=fails+check(t067(), 0);
    mark(68); fails=fails+check(t068(), 42);
    mark(69); fails=fails+check(t069(), 1);
    mark(70); fails=fails+check(t070(), 123);
    mark(71); fails=fails+check(t071(), 3);
    mark(72); fails=fails+check(t072(), 15);
    mark(73); fails=fails+check(t073(), 6);
    mark(74); fails=fails+check(t074(), 42);
    mark(75); fails=fails+check(t075(), 1);
    mark(76); fails=fails+check(t076(), 1426);
    mark(77); fails=fails+check(t077(), 1);
    mark(78); fails=fails+check(t078(), 9);
    mark(79); fails=fails+check(t079(), -995);
    mark(80); fails=fails+check(t080(), 1);
    mark(81); fails=fails+check(t081(), 3);
    mark(82); fails=fails+check(t082(), 0);
    mark(83); fails=fails+check(t083(), 2);
    mark(84); fails=fails+check(t084(), 2);
    mark(85); fails=fails+check(t085(), 1);
    mark(86); fails=fails+check(t086(), 0);
    mark(87); fails=fails+check(t087(), 77);
    mark(88); fails=fails+check(t088(), 55);
    mark(89); fails=fails+check(t089(), 1001);
    mark(90); fails=fails+check(t090(), 132);
    mark(91); fails=fails+check(t091(), 2);
    mark(92); fails=fails+check(t092(), 13);
    mark(93); fails=fails+check(t093(), 6);
    mark(94); fails=fails+check(t094(), 1);
    mark(95); fails=fails+check(t095(), 1);
    mark(96); fails=fails+check(t096(), 65122);
    mark(97); fails=fails+check(t097(), 1);
    mark(98); fails=fails+check(t098(), 1);
    mark(99); fails=fails+check(t099(), 1);
    mark(100); fails=fails+check(t100(), 1);
    mark(101); fails=fails+check(t101(), 1);
    mark(102); fails=fails+check(t102(), 5);
    mark(103); fails=fails+check(t103(), 92);
    mark(104); fails=fails+check(t104(), 11);
    mark(105); fails=fails+check(t105(), 10);
    mark(106); fails=fails+check(t106(), -1);
    mark(107); fails=fails+check(t107(), 0);
    mark(108); fails=fails+check(t108(), 7);
    mark(109); fails=fails+check(t109(), 2030020);
    mark(110); fails=fails+check(t110(), 255);
    mark(111); fails=fails+check(t111(), 31);
    mark(112); fails=fails+check(t112(), 255);
    mark(113); fails=fails+check(t113(), 406);
    mark(114); fails=fails+check(t114(), 4255);
    mark(115); fails=fails+check(t115(), 31);
    mark(116); fails=fails+check(t116(), 122958170);
    mark(117); fails=fails+check(t117(), 15);
    mark(118); fails=fails+check(t118(), 301020);
    mark(119); fails=fails+check(t119(), 303010);
    mark(120); fails=fails+check(t120(), 112077);
    mark(121); fails=fails+check(t121(), 402);
    mark(122); fails=fails+check(t122(), 204);
    mark(123); fails=fails+check(t123(), 1010013);
    mark(124); fails=fails+check(t124(), 2257);
    mark(125); fails=fails+check(t125(), 15);
    mark(126); fails=fails+check(t126(), 36);
    mark(127); fails=fails+check(t127(), 10103020);
    mark(128); fails=fails+check(t128(), 46662);
    mark(129); fails=fails+check(t129(), 20403);
    mark(130); fails=fails+check(t130(), 10103020);
    mark(131); fails=fails+check(t131(), 15);
    mark(132); fails=fails+check(t132(), 3020);
    mark(133); fails=fails+check(t133(), 63);
    mark(134); fails=fails+check(t134(), 31);
    mark(135); fails=fails+check(t135(), 22411);
    mark(136); fails=fails+check(t136(), 63);
    mark(137); fails=fails+check(t137(), 31);
    mark(138); fails=fails+check(t138(), 31);
    mark(139); fails=fails+check(t139(), 16383);
    mark(140); fails=fails+check(t140(), 511);
    mark(141); fails=fails+check(t141(), 15);
    mark(142); fails=fails+check(t142(), 7);
    mark(143); fails=fails+check(t143(), 15);
    mark(144); fails=fails+check(t144(), 15);
    mark(145); fails=fails+check(t145(), 7);
    mark(146); fails=fails+check(t146(), 178);
    mark(147); fails=fails+check(t147(), 15);
    mark(148); fails=fails+check(t148(), 7);
    mark(149); fails=fails+check(t149(), 31);
    mark(150); fails=fails+check(t150(), 127);
    mark(151); fails=fails+check(t151(), 63);
    mark(152); fails=fails+check(t152(), 15);
    mark(153); fails=fails+check(t153(), 7);
    mark(154); fails=fails+check(t154(), -11);
    mark(155); fails=fails+check(t155(), 777);
    mark(156); fails=fails+check(t156(), 1182);
    mark(157); fails=fails+check(t157(), 12);
    mark(158); fails=fails+check(t158(), 1);
    mark(159); fails=fails+check(t159(), 112);
    mark(160); fails=fails+check(t160(), 95);
    mark(161); fails=fails+check(t161(), 78);
    mark(162); fails=fails+check(t162(), 212);
    mark(163); fails=fails+check(t163(), 19);

    print_str("done"); putchar(10);
    return fails > 0;
}
