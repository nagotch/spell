t001() { 3 - 45; }
t002() { 100 * 3; }
t003() { 0; }
t004() { 1; }
t005() { 42; }
t006() { 254; }
t007() { 255; }
t008() { 5+20-4; }
t009() { 10-3-2; }
t010() { 2*3+4; }
t011() { 2+3*4; }
t012() { (2+3)*4; }
t013() { 20/4; }
t014() { 7/2; }
t015() { 12 + 3 * ( 2 + 4 ); }
t016() { -10+20; }
t017() { -(3+5)+10; }
t018() { -3*+5+20; }
t019() { +42; }
t020() { -(-5); }
t021() { 2*-3+10; }
t022() { 0==1; }
t023() { 42==42; }
t024() { 0; }
t025() { 42; }
t026() { 0<1; }
t027() { 1<1; }
t028() { 2<1; }
t029() { 0<=1; }
t030() { 1<=1; }
t031() { 2<=1; }
t032() { 1>0; }
t033() { 1>1; }
t034() { 1>2; }
t035() { 1>=0; }
t036() { 1>=1; }
t037() { 1>=2; }
t038() { 1+2==3; }
t039() { 1<2==1; }
t040() { 10%3; }
t041() { 2+10%4; }
t042() { (0-10)%3; }
t043() { 1&&2; }
t044() { 1||0&&0; }
t045() { 0&&10%0; }

mark(n) {
    print_int(n); putchar(10); fflush(0);
    return 0;
}

check(got, expected) {
    if (got != expected) return 1;
    return 0;
}

main() {
    fails = 0;

    mark(1); fails=fails+check(t001(), -42);
    mark(2); fails=fails+check(t002(), 300);
    mark(3); fails=fails+check(t003(), 0);
    mark(4); fails=fails+check(t004(), 1);
    mark(5); fails=fails+check(t005(), 42);
    mark(6); fails=fails+check(t006(), 254);
    mark(7); fails=fails+check(t007(), 255);
    mark(8); fails=fails+check(t008(), 21);
    mark(9); fails=fails+check(t009(), 5);
    mark(10); fails=fails+check(t010(), 10);
    mark(11); fails=fails+check(t011(), 14);
    mark(12); fails=fails+check(t012(), 20);
    mark(13); fails=fails+check(t013(), 5);
    mark(14); fails=fails+check(t014(), 3);
    mark(15); fails=fails+check(t015(), 30);
    mark(16); fails=fails+check(t016(), 10);
    mark(17); fails=fails+check(t017(), 2);
    mark(18); fails=fails+check(t018(), 5);
    mark(19); fails=fails+check(t019(), 42);
    mark(20); fails=fails+check(t020(), 5);
    mark(21); fails=fails+check(t021(), 4);
    mark(22); fails=fails+check(t022(), 0);
    mark(23); fails=fails+check(t023(), 1);
    mark(24); fails=fails+check(t024(), 0);
    mark(25); fails=fails+check(t025(), 42);
    mark(26); fails=fails+check(t026(), 1);
    mark(27); fails=fails+check(t027(), 0);
    mark(28); fails=fails+check(t028(), 0);
    mark(29); fails=fails+check(t029(), 1);
    mark(30); fails=fails+check(t030(), 1);
    mark(31); fails=fails+check(t031(), 0);
    mark(32); fails=fails+check(t032(), 1);
    mark(33); fails=fails+check(t033(), 0);
    mark(34); fails=fails+check(t034(), 0);
    mark(35); fails=fails+check(t035(), 1);
    mark(36); fails=fails+check(t036(), 1);
    mark(37); fails=fails+check(t037(), 0);
    mark(38); fails=fails+check(t038(), 1);
    mark(39); fails=fails+check(t039(), 1);
    mark(40); fails=fails+check(t040(), 1);
    mark(41); fails=fails+check(t041(), 4);
    mark(42); fails=fails+check(t042(), -1);
    mark(43); fails=fails+check(t043(), 1);
    mark(44); fails=fails+check(t044(), 1);
    mark(45); fails=fails+check(t045(), 0);

    print_str("done"); putchar(10);
    return fails > 0;
}
