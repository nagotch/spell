t001() { a=3; a; }
t002() { a=3; b=5*6-8; a+b/2; }
t003() { a=b=3; a+b; }
t004() { foo=3; foo; }
t005() { foo=3; bar=5; foo+bar; }
t006() { fo=1; foo=2; fo+foo; }
t007() { foo1=5; foo1; }
t008() { return 42; }
t009() { return 5; return 8; }
t010() { a=3; b=5*6-8; return a+b/2; }
t011() { returnx=3; returnx; }
t012() { return1=7; return1; }
t013() { if (0) return 1; return 2; }
t014() { if (1) return 1; return 2; }
t015() { if (0) return 1; else return 2; }
t016() { if (1) return 1; else return 2; }
t017() { a=0; if (1) a=1; return a; }
t018() { a=0; if (0) a=1; return a; }
t019() { a=0; while (a<10) a=a+1; return a; }
t020() { s=0; for (i=0; i<10; i=i+1) s=s+i; return s; }
t021() { for (;;) return 3; }
t022() { a=0; for (; a<3;) a=a+1; return a; }
t023() { a=0; { a=1; a=2; } return a; }
t024() { a=0; if (1) { a=1; a=2; } return a; }
t025() { a=0; if (0) { a=1; } else { a=2; } return a; }
t026() { a=0; while (a<3) { a=a+1; a=a+1; } return a; }
t027() { s=0; for (i=0; i<3; i=i+1) { s=s+i; s=s+1; } return s; }
t028() { a=0; { a=1; { a=2; } } return a; }
t029() { a=5; {} return a; }

main() {
    fails = 0;

    print_int(1); putchar(10); fflush(0);
    if (t001() != 3) { print_str("NG"); print_int(1); putchar(10); fails=fails+1; }

    print_int(2); putchar(10); fflush(0);
    if (t002() != 14) { print_str("NG"); print_int(2); putchar(10); fails=fails+1; }

    print_int(3); putchar(10); fflush(0);
    if (t003() != 6) { print_str("NG"); print_int(3); putchar(10); fails=fails+1; }

    print_int(4); putchar(10); fflush(0);
    if (t004() != 3) { print_str("NG"); print_int(4); putchar(10); fails=fails+1; }

    print_int(5); putchar(10); fflush(0);
    if (t005() != 8) { print_str("NG"); print_int(5); putchar(10); fails=fails+1; }

    print_int(6); putchar(10); fflush(0);
    if (t006() != 3) { print_str("NG"); print_int(6); putchar(10); fails=fails+1; }

    print_int(7); putchar(10); fflush(0);
    if (t007() != 5) { print_str("NG"); print_int(7); putchar(10); fails=fails+1; }

    print_int(8); putchar(10); fflush(0);
    if (t008() != 42) { print_str("NG"); print_int(8); putchar(10); fails=fails+1; }

    print_int(9); putchar(10); fflush(0);
    if (t009() != 5) { print_str("NG"); print_int(9); putchar(10); fails=fails+1; }

    print_int(10); putchar(10); fflush(0);
    if (t010() != 14) { print_str("NG"); print_int(10); putchar(10); fails=fails+1; }

    print_int(11); putchar(10); fflush(0);
    if (t011() != 3) { print_str("NG"); print_int(11); putchar(10); fails=fails+1; }

    print_int(12); putchar(10); fflush(0);
    if (t012() != 7) { print_str("NG"); print_int(12); putchar(10); fails=fails+1; }

    print_int(13); putchar(10); fflush(0);
    if (t013() != 2) { print_str("NG"); print_int(13); putchar(10); fails=fails+1; }

    print_int(14); putchar(10); fflush(0);
    if (t014() != 1) { print_str("NG"); print_int(14); putchar(10); fails=fails+1; }

    print_int(15); putchar(10); fflush(0);
    if (t015() != 2) { print_str("NG"); print_int(15); putchar(10); fails=fails+1; }

    print_int(16); putchar(10); fflush(0);
    if (t016() != 1) { print_str("NG"); print_int(16); putchar(10); fails=fails+1; }

    print_int(17); putchar(10); fflush(0);
    if (t017() != 1) { print_str("NG"); print_int(17); putchar(10); fails=fails+1; }

    print_int(18); putchar(10); fflush(0);
    if (t018() != 0) { print_str("NG"); print_int(18); putchar(10); fails=fails+1; }

    print_int(19); putchar(10); fflush(0);
    if (t019() != 10) { print_str("NG"); print_int(19); putchar(10); fails=fails+1; }

    print_int(20); putchar(10); fflush(0);
    if (t020() != 45) { print_str("NG"); print_int(20); putchar(10); fails=fails+1; }

    print_int(21); putchar(10); fflush(0);
    if (t021() != 3) { print_str("NG"); print_int(21); putchar(10); fails=fails+1; }

    print_int(22); putchar(10); fflush(0);
    if (t022() != 3) { print_str("NG"); print_int(22); putchar(10); fails=fails+1; }

    print_int(23); putchar(10); fflush(0);
    if (t023() != 2) { print_str("NG"); print_int(23); putchar(10); fails=fails+1; }

    print_int(24); putchar(10); fflush(0);
    if (t024() != 2) { print_str("NG"); print_int(24); putchar(10); fails=fails+1; }

    print_int(25); putchar(10); fflush(0);
    if (t025() != 2) { print_str("NG"); print_int(25); putchar(10); fails=fails+1; }

    print_int(26); putchar(10); fflush(0);
    if (t026() != 4) { print_str("NG"); print_int(26); putchar(10); fails=fails+1; }

    print_int(27); putchar(10); fflush(0);
    if (t027() != 6) { print_str("NG"); print_int(27); putchar(10); fails=fails+1; }

    print_int(28); putchar(10); fflush(0);
    if (t028() != 2) { print_str("NG"); print_int(28); putchar(10); fails=fails+1; }

    print_int(29); putchar(10); fflush(0);
    if (t029() != 5) { print_str("NG"); print_int(29); putchar(10); fails=fails+1; }


    print_str("done"); putchar(10);
    return fails > 0;
}
