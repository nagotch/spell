t001() { return 5 + 20 - 4; }
t002() { return 3 - 45; }
t003() { return 100 * 3; }

main() {
    fails = 0;

    print_int(1); putchar(10); fflush(0);
    if (t001() != 21) { print_str("NG"); print_int(1); putchar(10); fails=fails+1; }

    print_int(2); putchar(10); fflush(0);

    if (t002() != -42) { print_str("NG"); print_int(2); putchar(10); fails=fails+1; }
    print_int(3); putchar(10); fflush(0);
    if (t003() != 300) { print_str("NG"); print_int(3); putchar(10); fails=fails+1; }

    print_str("done"); putchar(10);
    return fails > 0;
}
