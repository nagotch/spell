def t001() { a=3; a; }
def t002() { a=3; b=5*6-8; a+b/2; }
def t003() { a=b=3; a+b; }
def t004() { foo=3; foo; }
def t005() { foo=3; bar=5; foo+bar; }
def t006() { fo=1; foo=2; fo+foo; }
def t007() { foo1=5; foo1; }
def t008() { return 42; }
def t009() { return 5; return 8; }
def t010() { a=3; b=5*6-8; return a+b/2; }
def t011() { returnx=3; returnx; }
def t012() { return1=7; return1; }
def t013() { if (0) return 1; return 2; }
def t014() { if (1) return 1; return 2; }
def t015() { if (0) return 1; else return 2; }
def t016() { if (1) return 1; else return 2; }
def t017() { a=0; if (1) a=1; return a; }
def t018() { a=0; if (0) a=1; return a; }
def t019() { a=0; while (a<10) a=a+1; return a; }
def t020() { s=0; for i in 10 s=s+i; return s; }
def t021() { while (1) return 3; }
def t022() { a=0; while (a<3) a=a+1; return a; }
def t023() { a=0; { a=1; a=2; } return a; }
def t024() { a=0; if (1) { a=1; a=2; } return a; }
def t025() { a=0; if (0) { a=1; } else { a=2; } return a; }
def t026() { a=0; while (a<3) { a=a+1; a=a+1; } return a; }
def t027() { s=0; for i in 3 { s=s+i; s=s+1; } return s; }
def t028() { a=0; { a=1; { a=2; } } return a; }
def t029() { a=5; {} return a; }
def t030() { i = 0; while (1) { i += 1; if (i == 5) { break; } } return i; }
def t031() { i = 0; s = 0; while (i < 10) { i += 1; if (i == 5) { continue; } s += i; } return s; }
def t032() { s = 0; for i in 5 { if (i == 2) { continue; } s += i; } return s; }
def t033() { total = 0; for i in 3 { for j in 10 { if (j == 2) { break; } total += 1; } } return total; }
def t034() { x: int = 40; x = x + 2; return x; }
def t035() { s = 0; for i in 1..5 { s = s + i; } return s; }
def t036() { s = 0; for 4 { s = s + 3; } return s; }
def t037() { for i in 3 { } return i; }
def t038() { s = 0; for i in 3 { i = 99; s = s + 1; } return s; }

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
    mark(2); fails=fails+check(t002(), 14);
    mark(3); fails=fails+check(t003(), 6);
    mark(4); fails=fails+check(t004(), 3);
    mark(5); fails=fails+check(t005(), 8);
    mark(6); fails=fails+check(t006(), 3);
    mark(7); fails=fails+check(t007(), 5);
    mark(8); fails=fails+check(t008(), 42);
    mark(9); fails=fails+check(t009(), 5);
    mark(10); fails=fails+check(t010(), 14);
    mark(11); fails=fails+check(t011(), 3);
    mark(12); fails=fails+check(t012(), 7);
    mark(13); fails=fails+check(t013(), 2);
    mark(14); fails=fails+check(t014(), 1);
    mark(15); fails=fails+check(t015(), 2);
    mark(16); fails=fails+check(t016(), 1);
    mark(17); fails=fails+check(t017(), 1);
    mark(18); fails=fails+check(t018(), 0);
    mark(19); fails=fails+check(t019(), 10);
    mark(20); fails=fails+check(t020(), 45);
    mark(21); fails=fails+check(t021(), 3);
    mark(22); fails=fails+check(t022(), 3);
    mark(23); fails=fails+check(t023(), 2);
    mark(24); fails=fails+check(t024(), 2);
    mark(25); fails=fails+check(t025(), 2);
    mark(26); fails=fails+check(t026(), 4);
    mark(27); fails=fails+check(t027(), 6);
    mark(28); fails=fails+check(t028(), 2);
    mark(29); fails=fails+check(t029(), 5);
    mark(30); fails=fails+check(t030(), 5);
    mark(31); fails=fails+check(t031(), 50);
    mark(32); fails=fails+check(t032(), 8);
    mark(33); fails=fails+check(t033(), 6);
    mark(34); fails=fails+check(t034(), 42);
    mark(35); fails=fails+check(t035(), 10);
    mark(36); fails=fails+check(t036(), 12);
    mark(37); fails=fails+check(t037(), 2);
    mark(38); fails=fails+check(t038(), 3);

    print_str("done"); putchar(10);
    return fails > 0;
}
