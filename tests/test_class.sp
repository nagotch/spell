class Point {
    x: int;
    y: int;
    def __init__(self, x: int, y: int) {
        self.x = x;
        self.y = y;
    }

    def sum(self) -> int {
        return self.x + self.y;
    }

    def add(self, a:int) -> int {
        return self.x + self.y + a;
    }

    def total(self) -> int {
        return self.add(10);
    }

    def move(self, dx: int) {
        self.x = self.x + dx;
    }
}

class Box {
    v: int;
}

class Bag {
    n: int;
    def __init__(self, n: int) {
        self.n = n;
    }
    def len(self) -> int {
        return self.n * 2;
    }
}

def t001() {
    p = Point(3, 4);
    return p.x * 10 + p.y;
}

def t002() {
    p = Point(3, 4);
    p.x = 9;
    return p.x * 10 + p.y;
}

def t003() {
    b = Box();
    b.v = 7;
    return b.v;
}

def t004() {
    p = Point(1, 2);
    q = p;
    q.y = 5;
    return p.x * 10 + p.y;
}

def t005() {
    p = Point(3, 4);
    return p.sum();
}

def t006() {
    return Point(3, 4).add(10);
}

def t007() {
    return Point(3, 4).total();
}

def t008() {
    p = Point(3, 4);
    p.move(5);
    return p.x * 10 + p.y;
}

def t009() {
    p = Point(3, 4);
    return p.sum;
}

def t010() {
    b = Bag(21);
    return b.len + b.len();
}

def t011() {
    a = [1, 2, 3];
    return a.len + a.len();
}

class Sw {
    a: int;
    b: int;
    def __init__(self, c: int) {
        if (c) { self.a = 1; self.b = 2; } else { self.a = 3; self.b = 4; }
    }
}

def t012() {
    s = Sw(1);
    t = Sw(0);
    return s.a * 1000 + s.b * 100 + t.a * 10 + t.b;
}

def t013() {
    b = Box();
    return b.v;
}

class Vec {
    x: int;
    y: int;
    def __init__(self, x: int, y: int) { self.x = x; self.y = y; }
    def __add__(self, o: Vec) -> Vec { return Vec(self.x + o.x, self.y + o.y); }
    def __eq__(self, o: Vec) -> bool { return self.x == o.x && self.y == o.y; }
    def __lt__(self, o: Vec) -> bool { return self.x < o.x; }
}

class Arr {
    a: [int];
    def __init__(self) { self.a = [0; 3]; }
    def __getitem__(self, i: int) -> int { return self.a[i]; }
    def __setitem__(self, i: int, v: int) -> int { self.a[i] = v; return v; }
}

def t014() {
    v = Vec(1, 2) + Vec(3, 4);
    return v.x * 100 + v.y;
}

def t015() {
    a = Vec(1, 2); b = Vec(2, 1); c = Vec(1, 2);
    r = 0;
    if (a < b) { r = r + 1; }
    if (b > a) { r = r + 2; }
    if (a == c) { r = r + 4; }
    if (a == b) { r = r + 8; }
    return r;
}

def t016() {
    arr = Arr();
    arr[1] = 42;
    arr[1] = arr[1] + 1;
    arr[0] = 5;
    arr[0] += 7;
    return arr[0] * 100 + arr[1];
}

class CountIter {
    n: int;
    def __init__(self, n: int) { self.n = n; }
    def __has_next__(self) -> bool { return self.n > 0; }
    def __next__(self) -> int { self.n = self.n - 1; return self.n; }
}

class Countdown {
    n: int;
    def __init__(self, n: int) { self.n = n; }
    def __iter__(self) -> CountIter { return CountIter(self.n); }
}

def t023() { s = 0; for i in Countdown(4) { s = s + i; } return s; }

def mark(n) {
    print_int(n); putchar(10); fflush(0);
    return 0;
}


class GBox<T> {
    v: T;
    def __init__(self, v: T) {
        self.v = v;
    }
    def get(self) -> T {
        return self.v;
    }
}

class GNode<T> {
    v: T;
    next: GNode<T>;
    has_next: int;
    def __init__(self, v: T) {
        self.v = v;
        self.next = self;
        self.has_next = 0;
    }
    def link(self, n: GNode<T>) {
        self.next = n;
        self.has_next = 1;
    }
    def sum(self) -> T {
        if (self.has_next) {
            return self.v + self.next.sum();
        }
        return self.v;
    }
}

class GArr<T> {
    data: [T];
    def __init__(self, d: [T]) {
        self.data = d;
    }
    def __getitem__(self, i: int) -> T {
        return self.data[i];
    }
}

def t017() {
    b = GBox(42);
    return b.get();
}

def t018() {
    a = GBox(10);
    b = GBox(32);
    return a.get() + b.get();
}

def t019() {
    a = GNode(10);
    b = GNode(20);
    c = GNode(30);
    b.link(c);
    a.link(b);
    return a.sum();
}

def t020() {
    a = GArr([10, 20, 30]);
    return a[1];
}

def t021() {
    b: GBox<int> = GBox(7);
    b.v = 9;
    return b.v;
}

def t022() {
    b = GBox("abcd");
    return b.get().len;
}

def check(got, expected) {
    if (got != expected) return 1;
    return 0;
}

def main() {
    fails = 0;
    mark(1); fails=fails+check(t001(), 34);
    mark(2); fails=fails+check(t002(), 94);
    mark(3); fails=fails+check(t003(), 7);
    mark(4); fails=fails+check(t004(), 15);
    mark(5); fails=fails+check(t005(), 7);
    mark(6); fails=fails+check(t006(), 17);
    mark(7); fails=fails+check(t007(), 17);
    mark(8); fails=fails+check(t008(), 84);
    mark(9); fails=fails+check(t009(), 7);
    mark(10); fails=fails+check(t010(), 84);
    mark(11); fails=fails+check(t011(), 6);
    mark(12); fails=fails+check(t012(), 1234);
    mark(13); fails=fails+check(t013(), 0);
    mark(14); fails=fails+check(t014(), 406);
    mark(15); fails=fails+check(t015(), 7);
    mark(16); fails=fails+check(t016(), 1243);
    mark(17); fails=fails+check(t017(), 42);
    mark(18); fails=fails+check(t018(), 42);
    mark(19); fails=fails+check(t019(), 60);
    mark(20); fails=fails+check(t020(), 20);
    mark(21); fails=fails+check(t021(), 9);
    mark(22); fails=fails+check(t022(), 4);
    mark(23); fails=fails+check(t023(), 6);

    print_str("done"); putchar(10);
    return fails > 0;
}
