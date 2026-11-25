def __array_add(a, b) {
    r = [0; a.len + b.len];
    i = 0;
    while (i < a.len) {
        r[i] = a[i];
        i = i + 1;
    }
    j = 0;
    while (j < b.len) {
        r[a.len + j] = b[j];
        j = j + 1;
    }
    return r;
}

def __array_eq(a, b) {
    if (a.len != b.len) { return 0; }
    i = 0;
    while (i < a.len) {
        if (a[i] != b[i]) { return 0; }
        i = i + 1;
    }
    return 1;
}

def __array_ne(a, b) {
    if (__array_eq(a, b)) { return 0; }
    return 1;
}

def __array_mul(a, n) {
    r = [0; a.len * n];
    i = 0;
    while (i < n) {
        j = 0;
        while (j < a.len) {
            r[i * a.len + j] = a[j];
            j = j + 1;
        }
        i = i + 1;
    }
    return r;
}

class str {
    chars: [int];
    def __init__(self, chars: [int]) {
        self.chars = chars;
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
        return self.charas == o.chars;
    }
}
