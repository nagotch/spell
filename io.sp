read_int() {
    char = getchar();
    ret=0;
    is_negative = 0;
    if (char == 45) {
        is_negative = 1;
        char = getchar();
    }
    for (i = 0; char != -1; i = i + 1) {
        ret = ret + char - 48;
        ret=ret*10;
        char = getchar();
    }
    if (is_negative) {
        ret = -ret;
    }
    return ret;
}

read_str() {
    str = [0; 100];
    n = 0;
    char = getchar();
    while (char>32) {
        str[n] = char;
        char = getchar();
        n=n+1;
    }
    r_str = [0; n];
    for (i=0; i<n; i=i+1) {
        r_str[i]=str[i];
    }
    return r_str;
}

print_int(n) {
    is_negative = n<0;
    if (n=0) {
        putchar(48);
        return;
    }
    if (is_negative) n = -n;
    out = [0; 20];
    for (i=0; i<out.len; i=i+1) {
        out[i] = n-n/10*10;
        n=n/10;
    }
    start=0;
    if (is_negative) {
        putchar(45);
    }
    for (i=19; i>=0;i=i-1) {
        if (out[i] != 0) {
            start = 1;
        }
        if (start) {
            putchar(out[i]+48);
        }
    }
}

print_str(s) {
    for (i=0; i<s.len; i=i+1) {
        putchar(s[i]);
    }
}
