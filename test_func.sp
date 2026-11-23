t001() { return ret3(); } !3!
t002() { return add(3, 5); } !8!
t003() { return sub(5, 3); } !2!
t004() { return add6(1, 2, 3, 4, 5, 6); } !21!
t005() { return sub6(100, 1, 2, 3, 4, 5); } !85!
t006() { return add6(1, 2, add6(3,4,5,6,7,8),9,10,11); } !66!
t007() { return add6(1,2,add6(3,add6(4,5,6,7,8,9),10,11,12,13),14,15,16); } !136!
t008() { a=3; return add(a,4); } !7!
t009() { return ret3() + ret3(); } !6!
three() { return 3; } t010() { return three(); } !3!
plus(x, y) { return x + y; } t011() { return plus(3, 5); } !8!
f(x) { y = x * 2; return y; } t012() { return f(21); } !42!
sum6(a, b, c, d, e, f) { return a + b + c + d + e + f; } t013() { return sum6(1, 2, 3, 4, 5, 6); } !21!
fib(x) { if (x <= 1) return 1; return fib(x - 1) + fib(x - 2); } t014() { return fib(9); } !55!


main() {
    fails = 0;

    print_int(1); putchar(10); fflush(0);
    if (t001() != 3) { print_str("NG"); print_int(1); putchar(10); fails=fails+1; }

    print_int(2); putchar(10); fflush(0);
    if (t002() != 8) { print_str("NG"); print_int(2); putchar(10); fails=fails+1; }

    print_int(3); putchar(10); fflush(0);
    if (t003() != 2) { print_str("NG"); print_int(3); putchar(10); fails=fails+1; }

    print_int(4); putchar(10); fflush(0);
    if (t004() != 21) { print_str("NG"); print_int(4); putchar(10); fails=fails+1; }

    print_int(5); putchar(10); fflush(0);
    if (t005() != 85) { print_str("NG"); print_int(5); putchar(10); fails=fails+1; }

    print_int(6); putchar(10); fflush(0);
    if (t006() != 66) { print_str("NG"); print_int(6); putchar(10); fails=fails+1; }

    print_int(7); putchar(10); fflush(0);
    if (t007() != 136) { print_str("NG"); print_int(7); putchar(10); fails=fails+1; }

    print_int(8); putchar(10); fflush(0);
    if (t008() != 7) { print_str("NG"); print_int(8); putchar(10); fails=fails+1; }

    print_int(9); putchar(10); fflush(0);
    if (t009() != 6) { print_str("NG"); print_int(9); putchar(10); fails=fails+1; }

    print_int(10); putchar(10); fflush(0);
    if (t010() != 3) { print_str("NG"); print_int(10); putchar(10); fails=fails+1; }

    print_int(11); putchar(10); fflush(0);
    if (t011() != 8) { print_str("NG"); print_int(11); putchar(10); fails=fails+1; }

    print_int(12); putchar(10); fflush(0);
    if (t012() != 42) { print_str("NG"); print_int(12); putchar(10); fails=fails+1; }

    print_int(13); putchar(10); fflush(0);
    if (t013() != 21) { print_str("NG"); print_int(13); putchar(10); fails=fails+1; }

    print_int(14); putchar(10); fflush(0);
    if (t014() != 55) { print_str("NG"); print_int(14); putchar(10); fails=fails+1; }


    print_str("done"); putchar(10);
    return fails > 0;
}
