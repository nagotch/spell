class Point {
    x: int;
    y: int;
    def __init__(self, x: int, y: int) {
        self.x = x;
        self.y = y;
    }
}

class Box {
    v: int;
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

def mark(n) {
    print_int(n); putchar(10); fflush(0);
    return 0;
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

    print_str("done"); putchar(10);
    return fails > 0;
}
