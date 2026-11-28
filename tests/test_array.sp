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

    print_str("done"); putchar(10);
    return fails > 0;
}
