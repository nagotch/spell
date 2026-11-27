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

class Vec2 {
    x: int;
    y: int;
    def __init__(self, x: int, y: int) { self.x = x; self.y = y; }
    def __add__(self, o: Vec2) -> Vec2 { return Vec2(self.x + o.x, self.y + o.y); }
    def __eq__(self, o: Vec2) -> bool { return self.x == o.x && self.y == o.y; }
    def __lt__(self, o: Vec2) -> bool { return self.x < o.x; }
}

class Arr {
    a: [int];
    def __init__(self) { self.a = [0; 3]; }
    def __getitem__(self, i: int) -> int { return self.a[i]; }
    def __setitem__(self, i: int, v: int) -> int { self.a[i] = v; return v; }
}

def t014() {
    v = Vec2(1, 2) + Vec2(3, 4);
    return v.x * 100 + v.y;
}

def t015() {
    a = Vec2(1, 2); b = Vec2(2, 1); c = Vec2(1, 2);
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
class Cmp {
    v: int;
    def __init__(self, v: int) { self.v = v; }
    def __lt__(self, o: Cmp) -> bool { return self.v < o.v; }
}
def t024() { if (Cmp(1) < Cmp(2) < Cmp(3)) { return 1; } return 0; }

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

class Pair<A, B> {
    first: A;
    second: B;
    def __init__(self, a: A, b: B) {
        self.first = a;
        self.second = b;
    }
    def get_first(self) -> A {
        return self.first;
    }
    def get_second(self) -> B {
        return self.second;
    }
    def swap(self) -> Pair<B, A> {
        return Pair(self.second, self.first);
    }
}

class Triple<A, B, C> {
    x: A;
    y: B;
    z: C;
    def __init__(self, x: A, y: B, z: C) {
        self.x = x;
        self.y = y;
        self.z = z;
    }
}

def t025() {
    p = Pair(42, "ab");
    return p.first + p.second.len;
}

def t026() {
    p: Pair<int, str> = Pair(7, "xyz");
    return p.get_second().len * 10 + p.get_first();
}

def t027() {
    p = Pair(5, "ab");
    q = p.swap();
    return q.first.len * 10 + q.second;
}

def t028() {
    b: GBox<Pair<int, str>> = GBox(Pair(3, "abcd"));
    return b.get().second.len + b.get().first;
}

def t029() {
    p1 = Pair(1, 2);
    p2 = Pair(1, "ab");
    return p1.second + p2.second.len;
}

def t030() {
    t = Triple(1, "ab", [10, 20, 30]);
    return t.x + t.y.len + t.z.len;
}

class Handler {
    cb: fn(int) -> int;
    def __init__(self, cb: fn(int) -> int) { self.cb = cb; }
    def run(self, x: int) -> int { return self.cb(x); }
}

def t031() {
    h = Handler(fn(v: int) -> int { return v * 2; });
    return h.run(21);
}

class FnBox<T> {
    v: T;
    def __init__(self, v: T) { self.v = v; }
    def getter(self) -> fn() -> T {
        return fn() -> T { return self.v; };
    }
}

def t032() {
    bi = FnBox(42);
    gi = bi.getter();
    bs = FnBox("ab");
    gs = bs.getter();
    s = gs();
    return gi() + s.len;
}

def t033() {
    // 辞書: 文字列キーで単語を数える（未存在キーは 0 で生える）
    cnt: Dict<str, int> = Dict(zero_int);
    words = ["a", "b", "a", "c", "b", "a"];
    for w in words { cnt[w]++; }
    return cnt["a"] * 100 + cnt["b"] * 10 + cnt["c"] + cnt.len;
}

def t034() {
    // 辞書: int キー、生やす読み取りと上書き
    d: Dict<int, int> = Dict(zero_int);
    d[5]++;
    d[5]++;
    d[9] = 10;
    d[5] = d[5] + d[9];
    x = d[7];
    return d[5] * 100 + d[9] * 10 + x * 1000 + d.len;
}

class Pt {
    x: int;
    y: int;
    def __init__(self, x: int, y: int) { self.x = x; self.y = y; }
    def __hash__(self) -> int { return self.x * 31 + self.y; }
    def __eq__(self, o: Pt) -> bool { return self.x == o.x && self.y == o.y; }
}

def t035() {
    // 辞書: __hash__/__eq__ を定義した自作クラスがキーになる
    d: Dict<Pt, int> = Dict(zero_int);
    d[Pt(1, 2)] = 5;
    d[Pt(1, 2)] = d[Pt(1, 2)] + 1;
    d[Pt(3, 4)]++;
    return d[Pt(1, 2)] * 100 + d[Pt(3, 4)] * 10 + d.len;
}

def t036() {
    // 辞書: ファクトリ既定値（キーごとに新しい配列が生える）
    g: Dict<str, [int]> = Dict(fn() -> [int] { return []; });
    g["x"] = g["x"] + [7];
    g["x"] = g["x"] + [8];
    gy = g["y"];
    return g["x"].len * 100 + g["x"][0] * 10 + g["x"][1] + gy.len * 1000 + g.len;
}

def t037() {
    // 辞書: 挿入順イテレーション（上書きしても位置は変わらない）と keys()
    d: Dict<int, str> = Dict(zero_str);
    d[3] = "c";
    d[1] = "a";
    d[2] = "b";
    d[1] = "A";
    s = 0;
    for k in d { s = s * 10 + k; }
    ks = d.keys();
    v = 0;
    for k in d { v = v + d[k].len; }
    if (ks.len != 3 || ks[0] != 3 || ks[1] != 1 || ks[2] != 2) { return 1; }
    return s * 10 + v;
}

def t038() {
    // 辞書: has / len と zero_bool / zero_str の既定値
    d: Dict<str, bool> = Dict(zero_bool);
    a = 0;
    if (d.has("x")) { a = 100; }
    b = d["x"];
    if (b) { a = a + 100; }
    d["x"] = 1 == 1;
    if (d.has("x")) { a = a + 1; }
    e: Dict<int, str> = Dict(zero_str);
    sv = e[42];
    return a * 100 + d.len * 10 + sv.len;
}

def t039() {
    // 辞書: 大量挿入でリハッシュを繰り返したあとの全件検証
    d: Dict<int, int> = Dict(zero_int);
    i = 0;
    while (i < 5000) { d[i] = i * 3; i = i + 1; }
    s = 0;
    i = 0;
    while (i < 5000) { s = s + d[i]; i = i + 1; }
    if (d.len != 5000) { return 1; }
    if (d.has(5000)) { return 2; }
    return s % 1000;
}

def t040() {
    // 辞書: bool キーと、値で等しい配列キー
    d: Dict<bool, int> = Dict(zero_int);
    d[1 == 1] = 3;
    d[1 == 0] = 4;
    a: Dict<[int], int> = Dict(zero_int);
    a[[1, 2]] = 5;
    a[[1, 2]] = a[[1, 2]] + 1;
    a[[3]] = 7;
    return d[1 == 1] * 100 + d[1 == 0] * 10 + d.len + a[[1, 2]] * 1000 + a.len * 10000;
}

class NoSemi {
    a: int
    b: int
    def __init__(self, a: int, b: int) {
        self.a = a
        self.b = b
    }
    def sum(self) -> int {
        return self.a + self.b
    }
}
def t041() {
    p = NoSemi(3, 4)
    return p.sum()
}

class SemiMix {
    a: int;
    ;
    b: int
    def __init__(self) { self.a = 1; self.b = 2; }
    def sum(self) -> int { return self.a + self.b; }
}
def t042() {
    p = SemiMix()
    return p.sum()
}

def make_pt() -> Pt { return Pt(0, 0); }

def t043() {
    // Vec: 容量の倍増（8→16→32→64）をまたいでも順序と値が保たれる
    v: Vec<int> = Vec(zero_int);
    i = 0;
    while (i < 50) { v.append(i); i = i + 1; }
    s = 0;
    j = 0;
    while (j < 50) { s = s + v[j] * (j + 1); j = j + 1; }
    return s;
}

def t044() {
    // Vec: len と添字の読み書き、to_array
    v: Vec<int> = Vec(zero_int);
    v.append(10);
    v.append(20);
    v.append(30);
    v[1] = 99;
    a = v.to_array();
    return v.len * 1000 + v[0] * 100 + v[1] + a[2];
}

def t045() {
    // Vec: 反復は論理長ぶんだけ（容量ではない）
    v: Vec<int> = Vec(zero_int);
    i = 0;
    while (i < 20) { v.append(1); i = i + 1; }
    s = 0;
    cnt = 0;
    for x in v { s = s + x; cnt = cnt + 1; }
    return s * 100 + cnt;
}

def t046() {
    // Vec: str の Vec
    v: Vec<str> = Vec(zero_str);
    v.append("ab");
    v.append("cde");
    v.append("f");
    s = "";
    for x in v { s = s + x; }
    return s.len * 10 + v[1].len;
}

def t047() {
    // Vec: クラスの Vec（ファクトリで Pt を生やす）
    v: Vec<Pt> = Vec(make_pt);
    v.append(Pt(1, 2));
    v.append(Pt(3, 4));
    v[0] = Pt(10, 20);
    return v[0].x * 100 + v[1].y * 10 + v.len;
}

def t048() {
    // Deque: FIFO と push_front の混在。両端からの取り出し順
    q: Deque<int> = Deque(zero_int);
    q.push_back(1);
    q.push_back(2);
    q.push_back(3);
    q.push_front(0);
    a = q.pop_front();
    b = q.pop_back();
    c = q.pop_front();
    d = q.pop_back();
    return a * 10000 + b * 1000 + c * 100 + d * 10 + q.len;
}

def t049() {
    // Deque: LIFO（後ろで出し入れする Stack、先頭で出し入れする Stack）
    q: Deque<int> = Deque(zero_int);
    q.push_back(1);
    q.push_back(2);
    q.push_back(3);
    x = q.pop_back() * 100 + q.pop_back() * 10 + q.pop_back();
    q.push_front(4);
    q.push_front(5);
    q.push_front(6);
    y = q.pop_front() * 100 + q.pop_front() * 10 + q.pop_front();
    return x * 1000 + y;
}

def t050() {
    // Deque: 巻き戻し。容量 8 を満たし、先頭を 5 つ抜いてから物理末尾を
    // 越えて push_back し、全順序を drain で確認する
    q: Deque<int> = Deque(zero_int);
    i = 0;
    while (i < 8) { q.push_back(i); i = i + 1; }
    i = 0;
    while (i < 5) { x = q.pop_front(); i = i + 1; }
    i = 8;
    while (i < 13) { q.push_back(i); i = i + 1; }
    n = q.len;
    s = 0;
    j = 0;
    while (q.len > 0) { s = s + q.pop_front() * (j + 1); j = j + 1; }
    return s + n;
}

def t051() {
    // Deque: 両端を交互に 100 個入れ、倍増をまたいでも全順序が保たれる
    q: Deque<int> = Deque(zero_int);
    i = 0;
    while (i < 100) {
        if (i % 2 == 0) { q.push_back(i); } else { q.push_front(i); }
        i = i + 1;
    }
    s = 0;
    j = 0;
    while (j < 100) { s = s + q[j] * (j + 1); j = j + 1; }
    return s;
}

def t052() {
    // Deque: push_front 側から倍増させる（100 個入れて逆順に drain）
    q: Deque<int> = Deque(zero_int);
    i = 0;
    while (i < 100) { q.push_front(i); i = i + 1; }
    s = 0;
    j = 0;
    while (q.len > 0) { s = s + q.pop_front() * (j + 1); j = j + 1; }
    return s;
}

def t053() {
    // Deque: 巻き戻した状態での添字・front・back と、物理的に巻き戻った
    // 位置からの pop_back
    q: Deque<int> = Deque(zero_int);
    i = 0;
    while (i < 8) { q.push_back(i); i = i + 1; }
    i = 0;
    while (i < 3) { x = q.pop_front(); i = i + 1; }
    q.push_back(8);
    q.push_back(9);
    r = q[0] * 10000 + q[3] * 1000 + q[q.len - 1] * 100 + q.front() * 10 + q.back();
    a = q.pop_back();
    b = q.pop_back();
    return r * 100 + a * 10 + b;
}

def t054() {
    // Deque: 巻き戻した状態での反復（先頭から末尾の順）
    q: Deque<int> = Deque(zero_int);
    i = 0;
    while (i < 8) { q.push_back(i); i = i + 1; }
    i = 0;
    while (i < 5) { x = q.pop_front(); i = i + 1; }
    i = 8;
    while (i < 13) { q.push_back(i); i = i + 1; }
    s = 0;
    j = 0;
    for v in q { s = s + v * (j + 1); j = j + 1; }
    // to_array も同じ並びを返し、反復はスナップショットなので反復中の
    // push_back は周回回数に影響しない
    a = q.to_array();
    cnt = 0;
    sum2 = 0;
    for v in q { q.push_back(99); sum2 = sum2 + v; cnt = cnt + 1; }
    r = s * 10 + cnt;
    r = r * 100 + sum2;
    return r * 100 + a[0] * 10 + a[a.len - 1];
}

def t055() {
    // Deque: str の Deque
    d: Deque<str> = Deque(zero_str);
    d.push_back("b");
    d.push_front("a");
    d.push_back("c");
    d.push_front("z");
    x = d.pop_back();
    s = d.pop_front() + d.pop_front() + d.pop_front();
    r = 0;
    if (s == "zab" && x == "c") { r = 1; }
    return r * 1000 + s.len;
}

def t056() {
    // Deque: クラスの Deque（ファクトリで Pt を生やす）
    q: Deque<Pt> = Deque(make_pt);
    q.push_back(Pt(1, 2));
    q.push_front(Pt(3, 4));
    q.push_back(Pt(5, 6));
    a = q.pop_front();
    b = q.pop_back();
    return a.x * 1000 + b.y * 100 + q.front().y * 10 + q.len;
}

def t057() {
    // Deque をキューにした幅優先探索（4x4 グリッドの最短路）
    rows = ["S..#", "##..", "...#", "..G."];
    dist = [0 - 1; 16];
    q: Deque<int> = Deque(zero_int);
    dist[0] = 0;
    q.push_back(0);
    dr = [0 - 1, 1, 0, 0];
    dc = [0, 0, 0 - 1, 1];
    while (q.len > 0) {
        cur = q.pop_front();
        r = cur / 4;
        c = cur % 4;
        k = 0;
        while (k < 4) {
            nr = r + dr[k];
            nc = c + dc[k];
            if (0 <= nr && nr < 4 && 0 <= nc && nc < 4) {
                if (rows[nr][nc] != 35 && dist[nr * 4 + nc] == 0 - 1) {
                    dist[nr * 4 + nc] = dist[cur] + 1;
                    q.push_back(nr * 4 + nc);
                }
            }
            k = k + 1;
        }
    }
    return dist[3 * 4 + 2];
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
    mark(24); fails=fails+check(t024(), 1);
    mark(25); fails=fails+check(t025(), 44);
    mark(26); fails=fails+check(t026(), 37);
    mark(27); fails=fails+check(t027(), 25);
    mark(28); fails=fails+check(t028(), 7);
    mark(29); fails=fails+check(t029(), 4);
    mark(30); fails=fails+check(t030(), 6);
    mark(31); fails=fails+check(t031(), 42);
    mark(32); fails=fails+check(t032(), 44);
    mark(33); fails=fails+check(t033(), 324);
    mark(34); fails=fails+check(t034(), 1303);
    mark(35); fails=fails+check(t035(), 612);
    mark(36); fails=fails+check(t036(), 280);
    mark(37); fails=fails+check(t037(), 3123);
    mark(38); fails=fails+check(t038(), 110);
    mark(39); fails=fails+check(t039(), 500);
    mark(40); fails=fails+check(t040(), 26342);
    mark(41); fails=fails+check(t041(), 7);
    mark(42); fails=fails+check(t042(), 3);
    mark(43); fails=fails+check(t043(), 41650);
    mark(44); fails=fails+check(t044(), 4129);
    mark(45); fails=fails+check(t045(), 2020);
    mark(46); fails=fails+check(t046(), 63);
    mark(47); fails=fails+check(t047(), 1042);
    mark(48); fails=fails+check(t048(), 3120);
    mark(49); fails=fails+check(t049(), 321654);
    mark(50); fails=fails+check(t050(), 356);
    mark(51); fails=fails+check(t051(), 248725);
    mark(52); fails=fails+check(t052(), 166650);
    mark(53); fails=fails+check(t053(), 3693998);
    mark(54); fails=fails+check(t054(), 34886862);
    mark(55); fails=fails+check(t055(), 1003);
    mark(56); fails=fails+check(t056(), 3621);
    mark(57); fails=fails+check(t057(), 5);

    print_str("done"); putchar(10);
    return fails > 0;
}
