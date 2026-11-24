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
def t012() { a = [1, 2, 3, 4]; s = 0; for (i = 0; i < 4; i = i+ 1) s = s + a[i]; return s; }
def t013() { x = 3; a = [x, x * 2]; return a[1]; }
def t014() { a = [10, 20, 30]; return a.len; }
def t015() { a = [7]; return a.len; }
def t016() { a = [1, 2, 3, 4]; s = 0; for (i = 0; i < a.len; i = i + 1) s = s + a[i]; return s; }
def count(a) { return a.len; }
def t017() { x = [1, 2, 3, 4, 5]; return count(x); }
def t018() { n = 3; a = [0; n]; a[0] = 10; a[2] = 30; return a[0] + a[1] + a[2] + a.len; }
def t019() { a = [7; 4]; return a[0] + a[3]; }
def t020() { a = [5; 0]; return a.len; }
def t021() { n = 5; a = [0; n]; for (i = 0; i < a.len; i = i + 1) a[i] = i * 2; s = 0; for (i = 0; i < a.len; i = i + 1) s = s + a[i]; return s; }
def make(n) { return [9; n]; }
def t022() { a = make(3); return a[2] + a.len; }
def t023() { s = "hello"; return s.len; }
def t024() { s = "hello"; return s[0]; }
def t025() { s = "A\n"; return s[1]; }
def t026() { s = "abc"; s[1] = 120; return s[1]; }
def t027() { s = "hello"; c = 0; for (i = 0; i < s.len; i = i + 1) if (s[i] == 108) c = c + 1; return c; }
def t028() { s = "hi"; putchar(s[0]); putchar(s[1]); return 0; }
def t029() { s = ""; return s.len; }
def t030() { a = [1,2,3]; a[1] += 10; return a[1]; }

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
    mark(30); fails=fails+check(t030(), 12);

    print_str("done"); putchar(10);
    return fails > 0;
}
