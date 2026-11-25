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

def dbl(x: int) -> int { return x * 2; }
def apply1(f: fn(int) -> int, x: int) -> int { return f(x); }
def apply6(f: fn(int, int, int, int, int, int) -> int, a: int) -> int {
    return f(a, a + 1, a + 2, a + 3, a + 4, a + 5);
}
def sort_with(a: [int], cmp: fn(int, int) -> int) {
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
    sort_with(a, (x, y) -> x - y);
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

    print_str("done"); putchar(10);
    return fails > 0;
}
