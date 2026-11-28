def t001() { return ret3(); }
def t002() { return add(3, 5); }
def t003() { return sub(5, 3); }
def t004() { return add6(1, 2, 3, 4, 5, 6); }
def t005() { return sub6(100, 1, 2, 3, 4, 5); }
def t006() { return add6(1, 2, add6(3,4,5,6,7,8),9,10,11); }
def t007() { return add6(1,2,add6(3,add6(4,5,6,7,8,9),10,11,12,13),14,15,16); }
def t008() { a=3; return add(a,4); }
def t009() { return ret3() + ret3(); }
def three() { return 3; }
def t010() { return three(); }
def plus(x, y) { return x + y; }
def t011() { return plus(3, 5); }
def f(x) { y = x * 2; return y; }
def t012() { return f(21); }
def sum6(a, b, c, d, e, f) { return a + b + c + d + e + f; }
def t013() { return sum6(1, 2, 3, 4, 5, 6); }
def fib(x) { if (x <= 1) return 1; return fib(x - 1) + fib(x - 2); }
def t014() { return fib(9); }
def t015() {
    // 1を返す
    return 1;
}
def t016() {
    /*
    1を返す
    */
    return 1;
}
def t017(p) { x = p; x = 1; return x;}
def t018(x: int, y: int) -> int { return x + y; }

g_val = 40 + 2;
g_counter = 0;
def get_g() { return g_val; }
def bump() { g_counter += 1; return g_counter; }

arrow_cnt = 0;
def arrow_bump(v: int) -> int { arrow_cnt = arrow_cnt + 1; return v; }

def dbl(x: int) -> int { return x * 2; }
def apply1(f: fn(int) -> int, x: int) -> int { return f(x); }
def apply6(f: fn(int, int, int, int, int, int) -> int, a: int) -> int {
    return f(a, a + 1, a + 2, a + 3, a + 4, a + 5);
}
def sort_with(a: [int], cmp: fn(int, int) -> int) -> [int] {
    n = a.len;
    i = 0;
    while (i < n) {
        j = 0;
        while (j + 1 < n - i) {
            if (cmp(a[j], a[j + 1]) > 0) {
                t = a[j];
                a[j] = a[j + 1];
                a[j + 1] = t;
            }
            j = j + 1;
        }
        i = i + 1;
    }
    return a;
}
def t019() { g = dbl; return g(10); }
def t020() { return apply1(dbl, 1); }
def t021() { return apply1(fn(v: int) -> int { return v + 100; }, 1); }
def t022() { return apply1(fn(v) { return v + 1000; }, 1); }
def t023() { return apply1(v -> v + 5, 1); }
def t024() { return (fn(x: int) -> int { return x * 3; })(7); }
def t025() { x = 10; f = fn() -> int { return x + 1; }; return f(); }
def t026() {
    y = 1;
    c = fn() -> int { y = y + 1; return y; };
    c();
    return c();
}
def t027() {
    y = 1;
    c = fn() -> int { y = y + 1; return y; };
    c();
    c();
    return y;
}
def t028() { z = 1; g = fn() -> int { return z; }; z = 99; return g(); }
def t029() {
    w = 7;
    maker = fn() -> fn() -> int { return fn() -> int { return w; }; };
    inner = maker();
    return inner();
}
def t030() {
    maker = fn() -> fn() -> int { local = 5; return fn() -> int { return local; }; };
    inner = maker();
    return inner();
}
def t031() { a = [1, 2, 3]; s = fn() -> int { return a[0] + a[1] + a[2]; }; return s(); }
def t032() {
    fs = [fn() -> int { return 0; }; 3];
    i = 0;
    while (i < 3) {
        v = i * 10;
        fs[i] = fn() -> int { return v; };
        i = i + 1;
    }
    f0 = fs[0];
    f1 = fs[1];
    f2 = fs[2];
    return f0() + f1() + f2();
}
def t033() {
    factory = fn() -> [int] { return [0; 2]; };
    a1 = factory();
    a2 = factory();
    a1[0] = 5;
    return a1[0] * 10 + a2[0];
}
def t034() {
    h = dbl;
    return apply6(fn(a: int, b: int, c: int, d: int, e: int, g: int) -> int { return a + b + c + d + e + g; }, h(1) - 1);
}
def t035() {
    base = dbl;
    wrap = fn(x: int) -> int { return base(x) + 1; };
    return wrap(5);
}
def t036() {
    a = [3, 1, 2];
    // step 96: 引数の配列はコピーされるため、並べ替えた結果は
    // 戻り値で受け取る（引数経由の書き換えは呼び出し側へ届かない）
    a = sort_with(a, (x, y) -> x - y);
    return a[0] * 100 + a[1] * 10 + a[2];
}
def t037() {
    x = 1;
    maker = fn() -> fn() -> int {
        x = x + 10;
        return fn() -> int { return x; };
    };
    inner = maker();
    r = inner();
    if (x != 1) { return -1; }
    return r;
}

def t038() {
    // step 66: 両辺が int の矢印は比較（相手を符号反転）。
    // `a <- b` は a < (-b)、`a -> b` は a > (-b) と一致する
    a = 3; b = 5;
    r = 0;
    if (a <- b) { r = r + 1; }
    if (a -> b) { r = r + 2; }
    if ((0 - 7) <- (0 - 2)) { r = r + 4; }
    if ((0 - 7) -> (0 - 2)) { r = r + 8; }
    if (b <- a) { r = r + 16; }
    if (b -> a) { r = r + 32; }
    return r;
}

def t039() {
    // step 66: 比較形の右辺はちょうど1回だけ評価される
    arrow_cnt = 0;
    a = (3 <- arrow_bump(5));
    b = ((0 - 3) -> arrow_bump(2));
    if (a || b) { return -1; }
    return arrow_cnt;
}

def t040() {
    // step 66: ラムダの3形は従来どおり（裸の識別子は未束縛名のとき、
    // (x, y) 形と fn 形は形で区別される）
    f: fn(int) -> int = v -> v * 3;
    g: fn(int, int) -> int = (p, q) -> p - q;
    h = fn(x: int) -> int { return x + 1; };
    return f(10) + g(10, 3) + h(1);
}

def t041() {
    // step 66: 束縛済みの int 変数と同名では裸ラムダにならず比較になる
    x = 6;
    if (x -> 2) { return 1; }
    return 0;
}

def t042() {
    x = 0 - 5;
    if (x -> 2) { return 1; }
    return 2;
}
def yes() -> bool { return T; }
def no() -> bool { return false; }
def t043() { if (yes() && !no()) { return 1; } return 0; }

// step 91: オーバーロード（型違い・個数違い・bool と int の区別）
def ov1(x: int) -> int { return 1; }
def ov1(x: str) -> int { return 2; }
def ov1(x: int, y: int) -> int { return 3; }
def t044() { return ov1(0) * 100 + ov1("a") * 10 + ov1(1, 2); }

def ov2(x: int) -> int { return 10; }
def ov2(x: bool) -> int { return 20; }
def t045() { b = 1 < 2; return ov2(0) + ov2(b); }

// step 91: 関数値は注釈の fn 型でオーバーロードを選ぶ
def t046() { g: fn(int) -> int = ov1; h: fn(str) -> int = ov1; return g(0) + h("a"); }

// step 91: 高階関数の引数位置にある関数参照は、呼び出しの解決後に
// 候補側の型で選び直される
def apply91(g: fn(int) -> int, v: int) -> int { return g(v); }
def apply91(g: fn(str) -> int, v: str) -> int { return g(v); }
def t047() { return apply91(ov1, 0) * 10 + apply91(ov1, "a"); }

// step 91: 注釈なし引数は緩く合う。個数で1件に絞れれば解決できる
def ov3(x: int) -> int { return 5; }
def ov3(x: int, y: int) -> int { return 6; }
def passthrough91(v) -> int { return ov3(v); }
def t048() { return passthrough91(0); }

// step 92: 可変引数の min / max と abs（組み込み）
def t049() { return min(3, 5) * 10 + max(3, 5); }
def t050() { return min(7) + max(9); }
def t051() { return min(4, 2, 8, 1, 9, 3, 5) * 10 + max(4, 2, 8, 1, 9, 3, 5); }
mm_cnt = 0;
mm_log = 0;
def mm_nxt(v: int) -> int { mm_cnt = mm_cnt + 1; mm_log = mm_log * 10 + v; return v; }
// 引数は左から右へちょうど1回ずつ評価される（cnt=3, log=312）
def t052() { r = min(mm_nxt(3), mm_nxt(1), mm_nxt(2)); return r * 1000 + mm_cnt * 100 + mm_log; }
def t053() {
    a = min("b", "a", "c");
    b = max("b", "a", "c");
    if (a == "a" && b == "c") { return 1; }
    return 0;
}
def t054() {
    p = min((1, 2), (0, 5), (3, 0));
    q = max((1, 2), (0, 5), (3, 0));
    return (p.x * 10 + p.y) + (q.x * 10 + q.y);
}
def t055() { x = 10; return min(x + 1, x * 2, x - 3) + max(x + 1, x * 2, x - 3); }
def t056() { return abs(-5) + abs(5) + abs(0); }
def pick92(v: int) -> int { return min(v, 3); }
def t057() { return min(max(1, 9), max(2, 3)) + pick92(5) + pick92(2); }
def fmax92(v) -> int { return max(v, 10); }
def t058() { return fmax92(5) + fmax92(20); }

def stomp96(a: [[int]]) { a[0][0] = 99; }
def maker96() -> [[int]] { r = [[5]]; return r; }
def ident96(a: [[int]]) -> [[int]] { return a; }
def t059() { g = [[1]]; stomp96(g); return g[0][0]; }
def t060() { a = maker96(); a[0][0] = 9; b = maker96(); return a[0][0] * 10 + b[0][0]; }
def t061() { g = [[3]]; h = ident96(g); h[0][0] = 8; return g[0][0] * 10 + h[0][0]; }

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
    mark(2); fails=fails+check(t002(), 8);
    mark(3); fails=fails+check(t003(), 2);
    mark(4); fails=fails+check(t004(), 21);
    mark(5); fails=fails+check(t005(), 85);
    mark(6); fails=fails+check(t006(), 66);
    mark(7); fails=fails+check(t007(), 136);
    mark(8); fails=fails+check(t008(), 7);
    mark(9); fails=fails+check(t009(), 6);
    mark(10); fails=fails+check(t010(), 3);
    mark(11); fails=fails+check(t011(), 8);
    mark(12); fails=fails+check(t012(), 42);
    mark(13); fails=fails+check(t013(), 21);
    mark(14); fails=fails+check(t014(), 55);
    mark(15); fails=fails+check(t015(), 1);
    mark(16); fails=fails+check(t016(), 1);
    mark(17); fails=fails+check(get_g(), 42);
    mark(18); fails=fails+check(bump(), 1);
    mark(19); fails=fails+check(bump(), 2);
    mark(20); fails=fails+check(t017(3), 1);
    mark(21); fails=fails+check(t018(40, 2), 42);
    mark(22); fails=fails+check(t019(), 20);
    mark(23); fails=fails+check(t020(), 2);
    mark(24); fails=fails+check(t021(), 101);
    mark(25); fails=fails+check(t022(), 1001);
    mark(26); fails=fails+check(t023(), 6);
    mark(27); fails=fails+check(t024(), 21);
    mark(28); fails=fails+check(t025(), 11);
    mark(29); fails=fails+check(t026(), 3);
    mark(30); fails=fails+check(t027(), 1);
    mark(31); fails=fails+check(t028(), 1);
    mark(32); fails=fails+check(t029(), 7);
    mark(33); fails=fails+check(t030(), 5);
    mark(34); fails=fails+check(t031(), 6);
    mark(35); fails=fails+check(t032(), 30);
    mark(36); fails=fails+check(t033(), 50);
    mark(37); fails=fails+check(t034(), 21);
    mark(38); fails=fails+check(t035(), 11);
    mark(39); fails=fails+check(t036(), 123);
    mark(40); fails=fails+check(t037(), 11);
    mark(41); fails=fails+check(t038(), 38);
    mark(42); fails=fails+check(t039(), 2);
    mark(43); fails=fails+check(t040(), 39);
    mark(44); fails=fails+check(t041(), 1);
    mark(45); fails=fails+check(t042(), 2);
    mark(46); fails=fails+check(t043(), 1);
    mark(47); fails=fails+check(t044(), 123);
    mark(48); fails=fails+check(t045(), 30);
    mark(49); fails=fails+check(t046(), 3);
    mark(50); fails=fails+check(t047(), 12);
    mark(51); fails=fails+check(t048(), 5);
    mark(52); fails=fails+check(t049(), 35);
    mark(53); fails=fails+check(t050(), 16);
    mark(54); fails=fails+check(t051(), 19);
    mark(55); fails=fails+check(t052(), 1612);
    mark(56); fails=fails+check(t053(), 1);
    mark(57); fails=fails+check(t054(), 35);
    mark(58); fails=fails+check(t055(), 27);
    mark(59); fails=fails+check(t056(), 10);
    mark(60); fails=fails+check(t057(), 8);
    mark(61); fails=fails+check(t058(), 30);
    mark(62); fails=fails+check(t059(), 1);
    mark(63); fails=fails+check(t060(), 95);
    mark(64); fails=fails+check(t061(), 38);

    print_str("done"); putchar(10);
    return fails > 0;
}
