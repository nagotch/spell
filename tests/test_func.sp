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

    print_str("done"); putchar(10);
    return fails > 0;
}
