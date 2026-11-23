main() {
    print_int(1); putchar(10); fflush(0);
    if (5 + 20 - 4 != 999) { print_str("NG"); print_int(1); putchar(10); return 1; }
    print_str("done"); putchar(10);
    return 0;
}
