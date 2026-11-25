class str {
    chars: [char];

    def __init__(self, chars: [char]) {
        self.chars = chars;
    }

    def len(self) -> int {
        return self.chars.len;
    }

    def __getitem__(self, i: int) -> char {
        return self.chars[i];
    }

    def __add__(self, o: str) -> str {
        return str(self.chars + o.chars);
    }

    def __eq__(self, o: str) -> bool {
        return self.chars == o.chars;
    }

    def __str__(self) -> str {
        return self;
    }
}

class int {
    def __str__(self) -> str {
        n = self;
        if (n == 0) {
            return "0"; 
        }
        neg = 0;
        if (n < 0) {
            neg = 1;
            n = -n;
        }
        digits = [0] * 20;
        i = 0;
        while (n > 0) {
            digits[i] = 48 + n % 10;
            n /= 10;
            i += 1;
        }
        r = [0] * (i + neg);
        j = 0;
        if (neg == 1) {
            r[0] = 45;
            j = 1;
        }
        while (i > 0) {
            i -= 1;
            r[j] = digits[i];
            j += 1;
        }
        return str(r);
    }
}

class bool {
    def __str__(self) -> str {
        if (self) { return "true"; }
        return "false";
    }
}

class char {
    def __str__(self) -> str {
        return str([self]);
    }
}

class array {
    def __add__(self, o) {
        r = [0] * (self.len + o.len);
        i = 0;
        while (i < self.len) {
            r[i] = self[i];
            i = i + 1;
        }
        j = 0;
        while (j < o.len) {
            r[self.len + j] = o[j];
            j = j + 1;
        }
        return r;
    }

    def __eq__(self, o) {
        if (self.len != o.len) { return 0; }
        i = 0;
        while (i < self.len) {
            if (self[i] != o[i]) { return 0; }
            i = i + 1;
        }
        return 1;
    }

    def __ne__(self, o) {
        if (__eq__(self, o)) { return 0; }
        return 1;
    }

    def __mul__(self, n) {
        r = [0; self.len * n];
        i = 0;
        while (i < n) {
            j = 0;
            while (j < self.len) {
                r[i * self.len + j] = self[j];
                j = j + 1;
            }
            i = i + 1;
        }
        return r;
    }
}

def read_int() -> int {
    char = getchar();
    ret=0;
    is_negative = 0;
    if (char == 45) {
        is_negative = 1;
        char = getchar();
    }
    for (i = 0; 48 <= char && char <= 57; i = i + 1) {
        ret = ret * 10 + char - 48;
        char = getchar();
    }
    if (is_negative) {
        ret = -ret;
    }
    return ret;
}

def read_str() -> str {
    str = [0; 100];
    n = 0;
    char = getchar();
    while (char != -1) {
        str[n] = char;
        char = getchar();
        n=n+1;
    }
    r_str = [0; n];
    for (i=0; i<n; i=i+1) {
        r_str[i]=str[i];
    }
    return str(r_str);
}

def print_int(n: int) {
    s = n.__str__();
    print_str(s);
}

def print_str(s: str) {
    for (i=0; i<s.len; i=i+1) {
        putchar(s[i]);
    }
}
