t001() { x=3; y=&x; return *y; }
t002() { x=3; return *&x; }
t003() { a=3; b=4; p=&a; q=&b; return *p + *q; }
t004() { x=42; p=&x; q=&p; return **q; }
t005() { x=3; y=&x; *y=5; return x; }
t006() { x=3; y=&x; *y=*y+2; return x; }
t007() { x=7; y=&x; z=&y; **z=9; return x; }
t008() { x=3; y=&x; *y=8; return *y; }
t009() { a = [10, 20, 30]; return a[0] + a[1] + a[2]; }
t010() { a = [1, 2, 3]; a[1] = 42; return a[1]; }
t011() { a = [5, 6, 7]; i = 2; return a[i]; }
t012() { a = [1, 2, 3, 4]; s = 0; for (i = 0; i < 4; i = i+ 1) s = s + a[i]; return s; }
t013() { x = 3; a = [x, x * 2]; return a[1]; }
t014() { a = [10, 20, 30]; return a.len; }
t015() { a = [7]; return a.len; }
t016() { a = [1, 2, 3, 4]; s = 0; for (i = 0; i < a.len; i = i + 1) s = s + a[i]; return s; }
count(a) { return a.len; } t017() { x = [1, 2, 3, 4, 5]; return count(x); }
t018() { n = 3; a = [0; n]; a[0] = 10; a[2] = 30; return a[0] + a[1] + a[2] + a.len; }
t019() { a = [7; 4]; return a[0] + a[3]; }
t020() { a = [5; 0]; return a.len; }
t021() { n = 5; a = [0; n]; for (i = 0; i < a.len; i = i + 1) a[i] = i * 2; s = 0; for (i = 0; i < a.len; i = i + 1) s = s + a[i]; return s; }
make(n) { return [9; n]; } t022() { a = make(3); return a[2] + a.len; }
t023() { s = "hello"; return s.len; }
t024() { s = "hello"; return s[0]; }
t025() { s = "A\n"; return s[1]; }
t026() { s = "abc"; s[1] = 120; return s[1]; }
t027() { s = "hello"; c = 0; for (i = 0; i < s.len; i = i + 1) if (s[i] == 108) c = c + 1; return c; }
t028() { s = "hi"; putchar(s[0]); putchar(s[1]); return 0; }
t029() { s = ""; return s.len; }

main() {
    fails = 0;

    print_int(1); putchar(10); fflush(0);
    if (t001() != 3) { print_str("NG"); print_int(1); putchar(10); fails=fails+1; }

    print_int(2); putchar(10); fflush(0);
    if (t002() != 3) { print_str("NG"); print_int(2); putchar(10); fails=fails+1; }

    print_int(3); putchar(10); fflush(0);
    if (t003() != 7) { print_str("NG"); print_int(3); putchar(10); fails=fails+1; }

    print_int(4); putchar(10); fflush(0);
    if (t004() != 42) { print_str("NG"); print_int(4); putchar(10); fails=fails+1; }

    print_int(5); putchar(10); fflush(0);
    if (t005() != 5) { print_str("NG"); print_int(5); putchar(10); fails=fails+1; }

    print_int(6); putchar(10); fflush(0);
    if (t006() != 5) { print_str("NG"); print_int(6); putchar(10); fails=fails+1; }

    print_int(7); putchar(10); fflush(0);
    if (t007() != 9) { print_str("NG"); print_int(7); putchar(10); fails=fails+1; }

    print_int(8); putchar(10); fflush(0);
    if (t008() != 8) { print_str("NG"); print_int(8); putchar(10); fails=fails+1; }

    print_int(9); putchar(10); fflush(0);
    if (t009() != 60) { print_str("NG"); print_int(9); putchar(10); fails=fails+1; }

    print_int(10); putchar(10); fflush(0);
    if (t010() != 42) { print_str("NG"); print_int(10); putchar(10); fails=fails+1; }

    print_int(11); putchar(10); fflush(0);
    if (t011() != 7) { print_str("NG"); print_int(11); putchar(10); fails=fails+1; }

    print_int(12); putchar(10); fflush(0);
    if (t012() != 10) { print_str("NG"); print_int(12); putchar(10); fails=fails+1; }

    print_int(13); putchar(10); fflush(0);
    if (t013() != 6) { print_str("NG"); print_int(13); putchar(10); fails=fails+1; }

    print_int(14); putchar(10); fflush(0);
    if (t014() != 3) { print_str("NG"); print_int(14); putchar(10); fails=fails+1; }

    print_int(15); putchar(10); fflush(0);
    if (t015() != 1) { print_str("NG"); print_int(15); putchar(10); fails=fails+1; }

    print_int(16); putchar(10); fflush(0);
    if (t016() != 10) { print_str("NG"); print_int(16); putchar(10); fails=fails+1; }

    print_int(17); putchar(10); fflush(0);
    if (t017() != 5) { print_str("NG"); print_int(17); putchar(10); fails=fails+1; }

    print_int(18); putchar(10); fflush(0);
    if (t018() != 43) { print_str("NG"); print_int(18); putchar(10); fails=fails+1; }

    print_int(19); putchar(10); fflush(0);
    if (t019() != 14) { print_str("NG"); print_int(19); putchar(10); fails=fails+1; }

    print_int(20); putchar(10); fflush(0);
    if (t020() != 0) { print_str("NG"); print_int(20); putchar(10); fails=fails+1; }

    print_int(21); putchar(10); fflush(0);
    if (t021() != 20) { print_str("NG"); print_int(21); putchar(10); fails=fails+1; }

    print_int(22); putchar(10); fflush(0);
    if (t022() != 12) { print_str("NG"); print_int(22); putchar(10); fails=fails+1; }

    print_int(23); putchar(10); fflush(0);
    if (t023() != 5) { print_str("NG"); print_int(23); putchar(10); fails=fails+1; }

    print_int(24); putchar(10); fflush(0);
    if (t024() != 104) { print_str("NG"); print_int(24); putchar(10); fails=fails+1; }

    print_int(25); putchar(10); fflush(0);
    if (t025() != 10) { print_str("NG"); print_int(25); putchar(10); fails=fails+1; }

    print_int(26); putchar(10); fflush(0);
    if (t026() != 120) { print_str("NG"); print_int(26); putchar(10); fails=fails+1; }

    print_int(27); putchar(10); fflush(0);
    if (t027() != 2) { print_str("NG"); print_int(27); putchar(10); fails=fails+1; }

    print_int(28); putchar(10); fflush(0);
    if (t028() != 0) { print_str("NG"); print_int(28); putchar(10); fails=fails+1; }

    print_int(29); putchar(10); fflush(0);
    if (t029() != 0) { print_str("NG"); print_int(29); putchar(10); fails=fails+1; }


    print_str("done"); putchar(10);
    return fails > 0;
}
