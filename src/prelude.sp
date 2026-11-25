class str {
    chars: [int];

    def __init__(self, chars: [int]) {
        self.chars = [0] * chars.len;
        i = 0;
        while (i < chars.len) {
            self.chars[i] = chars[i];
            i += 1;
        }
    }

    def len(self) -> int {
        return self.chars.len;
    }

    def __getitem__(self, i: int) -> int {
        return self.chars[i];
    }

    def __add__(self, o: str) -> str {
        return str(self.chars + o.chars);
    }

    def __eq__(self, o: str) -> bool {
        return self.chars == o.chars;
    }

    def __ne__(self, o: str) -> bool {
        return !(self == o);
    }

    def __str__(self) -> str {
        return self;
    }

    def __iter__(self) -> ArrayIter<int> {
        return self.chars.__iter__();
    }
}

class int {
    def __add__(self, o: int) -> int { return self + o; }
    def __sub__(self, o: int) -> int { return self - o; }
    def __mul__(self, o: int) -> int { return self * o; }
    def __div__(self, o: int) -> int { return self / o; }
    def __mod__(self, o: int) -> int { return self % o; }
    def __eq__(self, o: int) -> bool { return self == o; }
    def __ne__(self, o: int) -> bool { return self != o; }
    def __lt__(self, o: int) -> bool { return self < o; }
    def __le__(self, o: int) -> bool { return self <= o; }

    def __iter__(self) -> RangeIter {
        return RangeIter(0, self);
    }
    def __str__(self) -> str {
        n = self;
        if (n == 0) {
            return "0"; 
        }
        neg = 0;
        if (n < 0) {
            neg = 1;
        }
        digits = [0] * 20;
        i = 0;
        while (n != 0) {
            d = n % 10;
            if (d < 0) { d = -d; }
            digits[i] = 48 + d;
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

class array<T> {
    def __add__(self, o: [T]) -> [T] {
        if (self.len == 0 && o.len == 0) { return []; }
        if (self.len > 0) {
            r = [self[0]; self.len + o.len];
        } else {
            r = [o[0]; self.len + o.len];
        }
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

    def __eq__(self, o: [T]) -> bool {
        if (self.len != o.len) { return 1==0; }
        i = 0;
        while (i < self.len) {
            if (self[i] != o[i]) { return 1==0; }
            i = i + 1;
        }
        return 1==1;
    }

    def __ne__(self, o: [T]) -> bool {
        return !(self == o);
    }

    def __mul__(self, n: int) -> [T] {
        if (self.len == 0) { return []; }
        r = [self[0]; self.len * n];
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

    def __iter__(self) -> ArrayIter<T> {
        return ArrayIter(self);
    }

    def __str__(self) -> str {
        r = "";
        i = 0;
        while (i < self.len) {
            if (i > 0) { r = r + " "; }
            r = r + self[i].__str__();
            i = i + 1;
        }
        return r;
    }
}

class RangeIter {
    cur: int;
    hi: int;

    def __init__(self, lo: int, hi: int) {
        self.cur = lo;
        self.hi = hi;
    }

    def __has_next__(self) -> bool {
        return self.cur < self.hi;
    }

    def __next__(self) -> int {
        v = self.cur;
        self.cur = self.cur + 1;
        return v;
    }
}

class Range {
    lo: int;
    hi: int;

    def __init__(self, lo: int, hi: int) {
        self.lo = lo;
        self.hi = hi;
    }

    def __iter__(self) -> RangeIter {
        return RangeIter(self.lo, self.hi);
    }
}

class ArrayIter<T> {
    arr: [T];
    i: int;

    def __init__(self, arr: [T]) {
        self.arr = arr;
        self.i = 0;
    }

    def __has_next__(self) -> bool {
        return self.i < self.arr.len;
    }

    def __next__(self) -> T {
        v = self.arr[self.i];
        self.i = self.i + 1;
        return v;
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
    while (48 <= char && char <= 57) {
        ret = ret * 10 + char - 48;
        char = getchar();
    }
    if (is_negative) {
        ret = -ret;
    }
    return ret;
}

def read_str() -> str {
    cap = 64;
    str = [0; cap];
    n = 0;
    char = getchar();
    while (char != -1) {
        if (n == cap) {
            bigger = [0; cap * 2];
            for i in n { bigger[i] = str[i]; }
            str = bigger;
            cap = cap * 2;
        }
        str[n] = char;
        char = getchar();
        n=n+1;
    }
    r_str = [0; n];
    for i in n {
        r_str[i]=str[i];
    }
    return str(r_str);
}

def print_int(n: int) {
    s = n.__str__();
    print_str(s);
}

def print_str(s: str) {
    for c in s {
        putchar(c);
    }
}

def __println(s: str) {
    print_str(s);
    putchar(10);
}
