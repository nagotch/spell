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


    print_str("done"); putchar(10);
    return fails > 0;
}
