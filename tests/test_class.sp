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

class Grid96 {
    rows: [[int]];
    def __init__(self) { self.rows = []; }
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
                if (rows[nr][nc] != "#" && dist[nr * 4 + nc] == 0 - 1) {
                    dist[nr * 4 + nc] = dist[cur] + 1;
                    q.push_back(nr * 4 + nc);
                }
            }
            k = k + 1;
        }
    }
    return dist[3 * 4 + 2];
}

// step 65: フィールドへの append、評価順序、ユーザークラスの append
class AppBox {
    a: [int];
    def __init__(self) { self.a = [1; 1]; }
}
def t058() {
    p = AppBox();
    p.a.append(2);
    p.a.append(3);
    return p.a.len * 100 + p.a[1] * 10 + p.a[2];
}

class AppH {
    a: [int];
    def __init__(self) {
        self.a = [0; 3];
        self.a[0] = 1;
        self.a[1] = 2;
        self.a[2] = 3;
    }
    def g(self) -> int { self.a.append(100); return 7; }
}
def t059() {
    h = AppH();
    h.a.append(h.g());
    return h.a.len * 10000 + h.a[3] * 10 + h.a[4];
}

class AppBag {
    n: int;
    def __init__(self) { self.n = 0; }
    def append(self, v: int) -> int { self.n = self.n + v; return self.n; }
}
def t060() {
    bag = AppBag();
    r1 = bag.append(5);
    r2 = bag.append(3);
    return r1 * 100 + r2 * 10 + bag.n;
}

// ジェネリックなクラスの中でも、[T] フィールドへ append できる
// （実体化したクローン側で要素型つきに検査される）
class GBag<T> {
    a: [T];
    def __init__(self) { self.a = []; }
    def add(self, v: T) { self.a.append(v); }
    def get(self, i: int) -> T { return self.a[i]; }
    def n(self) -> int { return self.a.len; }
}
def t061() {
    b: GBag<int> = GBag();
    b.add(5);
    b.add(6);
    return b.n() * 100 + b.get(0) * 10 + b.get(1);
}

// step 66: 矢印演算子は dunder プロトコルへの書き換え。
// `q <- x` = __push_back__、`x <- q` = __pop_front__、
// `x -> q` = __push_front__、`q -> x` = __pop_back__
def t062() {
    q: Deque<int> = Deque(zero_int);
    q <- 1;
    q <- 2;
    0 -> q;
    q <- 3;
    a <- q;
    q -> b;
    return a * 1000 + b * 100 + q[0] * 10 + q.len;
}

def t063() {
    // 矢印で書く幅優先探索（t057 と同じ迷路・同じ答え）
    rows = ["S..#", "##..", "...#", "..G."];
    dist = [0 - 1; 16];
    q: Deque<int> = Deque(zero_int);
    dist[0] = 0;
    q <- 0;
    dr = [0 - 1, 1, 0, 0];
    dc = [0, 0, 0 - 1, 1];
    while (q.len > 0) {
        cur <- q;
        r = cur / 4;
        c = cur % 4;
        k = 0;
        while (k < 4) {
            nr = r + dr[k];
            nc = c + dc[k];
            if (0 <= nr && nr < 4 && 0 <= nc && nc < 4) {
                if (rows[nr][nc] != "#" && dist[nr * 4 + nc] == 0 - 1) {
                    dist[nr * 4 + nc] = dist[cur] + 1;
                    q <- nr * 4 + nc;
                }
            }
            k = k + 1;
        }
    }
    return dist[3 * 4 + 2];
}

def t064() {
    // push 系の矢印の値はコンテナ自身（dunder が self を返す）
    q: Deque<int> = Deque(zero_int);
    r = (q <- 7);
    s = (8 -> q);
    ok = 0;
    if (r == q && s == q) { ok = 1; }
    return ok * 1000 + q[0] * 10 + q[1];
}

// dunder を自分で定義したクラスでも矢印が使える（プロトコルであって
// Deque 専用の決め打ちではない）
class ArrowSt {
    a: [int];
    n: int;
    def __init__(self) { self.a = [0; 16]; self.n = 0; }
    def __push_back__(self, v: int) -> ArrowSt {
        self.a[self.n] = v;
        self.n = self.n + 1;
        return self;
    }
    def __pop_back__(self) -> int {
        self.n = self.n - 1;
        return self.a[self.n];
    }
    def __push_front__(self, v: int) -> ArrowSt {
        i = self.n;
        while (i > 0) { self.a[i] = self.a[i - 1]; i = i - 1; }
        self.a[0] = v;
        self.n = self.n + 1;
        return self;
    }
    def __pop_front__(self) -> int {
        v = self.a[0];
        i = 0;
        while (i + 1 < self.n) { self.a[i] = self.a[i + 1]; i = i + 1; }
        self.n = self.n - 1;
        return v;
    }
}
def t065() {
    s = ArrowSt();
    s <- 1;
    s <- 2;
    0 -> s;
    a <- s;
    s -> b;
    return a * 100 + b * 10 + s.n;
}

// pop の代入先: 配列の要素・フィールド・__setitem__ を持つクラスへの添字
class ArrowArr {
    a: [int];
    def __init__(self) { self.a = [0; 3]; }
    def __getitem__(self, i: int) -> int { return self.a[i]; }
    def __setitem__(self, i: int, v: int) { self.a[i] = v; }
}
class ArrowHolder {
    v: int;
    def __init__(self) { self.v = 0; }
}
def t066() {
    q: Deque<int> = Deque(zero_int);
    q <- 10;
    q <- 20;
    q <- 30;
    arr = [0, 0];
    arr[1] <- q;
    h = ArrowHolder();
    h.v <- q;
    aa = ArrowArr();
    aa[2] <- q;
    return arr[1] * 100 + h.v * 10 + aa[2];
}

def t067() {
    // str の Deque でも矢印が使える
    q: Deque<str> = Deque(zero_str);
    q <- "b";
    "a" -> q;
    q <- "c";
    x <- q;
    q -> y;
    s = x + q[0] + y;
    if (s == "abc") { return 100 + q.len; }
    return 0;
}

def t068() {
    // コンテナが添字の先にある形（左辺が識別子でない矢印）
    qa: Deque<int> = Deque(zero_int);
    qb: Deque<int> = Deque(zero_int);
    qs = [qa, qb];
    qs[0] <- 11;
    22 -> qs[1];
    qs[0] -> w;
    v <- qs[1];
    return w * 100 + v;
}

def t069() {
    // 矢印で両端から 100 個入れ（倍増と巻き戻しをまたぐ）、前から drain
    q: Deque<int> = Deque(zero_int);
    i = 0;
    while (i < 50) { q <- i; i -> q; i = i + 1; }
    first <- q;
    s = 0;
    while (q.len > 0) { v <- q; s = s + v; }
    return first * 10000 + s;
}

class NamedT {
    T: int;
    def __init__(self, v: int) { self.T = v; }
}

class Cell<T> {
    v: T;
    def __init__(self, v: T) { self.v = v; }
}

def t070() {
    // step 70: 型パラメータ名 T とメンバ名 T は真理値リテラルと無関係に
    // 従来どおり使え、式の位置の T / true はリテラルとして読める
    c: Cell<bool> = Cell(F);
    o = NamedT(40);
    r = o.T;
    if (!c.v && T) { r = r + 2; }
    return r;
}

def t071() {
    // step 77: Pos2 リテラルとフィールド・添字読み
    p = (3, 4);
    return p.x * 1000 + p.y * 100 + p[0] * 10 + p[1];
}

def t072() {
    // step 77: Pos2 の要素ごとの演算（+ / - / スカラー倍）
    a = (1, 2) + (10, 20);
    b = a - (1, 2);
    c = (1, 2) * 3;
    return (a.x + a.y) * 100 + (b.x + b.y) * 10 + (c.x + c.y);
}

def t073() {
    // step 77: Pos2 の等価性と Dict のキーとしての読み書き
    d: Dict<Pos2, int> = Dict(zero_int);
    d[(1, 2)] = 5;
    d[(1, 2)] = d[(1, 2)] + 3;
    r = d[(1, 2)] * 10;
    if (d.has((2, 1))) { r = r + 100; }
    if ((1, 2) == (1, 2) && (1, 2) != (2, 1)) { r = r + 1; }
    return r;
}

def t074() {
    // step 77: Pos2 の __str__ は空白区切り
    if ((3, 5).__str__() == "3 5") { return 1; }
    return 0;
}

def t075() {
    // step 77: Pos3 のリテラル・演算・添字
    q = (1, 2, 3) + (10, 20, 30);
    return q.x * 1000 + q.y * 100 + q.z * 10 + q[2];
}

def t076() {
    // step 77: Pos3 の等価性と __str__
    r = 0;
    if ((1, 2, 3) == (1, 2, 3) && (1, 2, 3) != (1, 2, 4)) { r = r + 1; }
    if ((1, 2, 3).__str__() == "1 2 3") { r = r + 10; }
    return r;
}

def t077() {
    // step 77: a[p] の読み（二次元配列への Pos 添字）
    a = [[1, 2], [3, 4]];
    return a[(0, 1)] + a[(1, 0)] * 10;
}

def t078() {
    // step 77: a[p] の代入（リテラルと変数の両方）
    a = [[1, 2], [3, 4]];
    a[(1, 1)] = 99;
    p = (0, 1);
    a[p] = 7;
    return a[1][1] * 100 + a[0][1];
}

def t079() {
    // step 77: a[p] の三次元版（読みと代入）
    b = [[[1, 2], [3, 4]], [[5, 6], [7, 8]]];
    r = b[(1, 0, 1)];
    b[(0, 1, 0)] = 30;
    return r * 100 + b[0][1][0];
}

def t080() {
    // step 77: Pos リテラルを要素とする配列と、加算での隣接セル列挙
    // （BFS の dirs イディオムの形）
    dirs = [(1, 0), (0, 1), (-1, 0), (0, -1)];
    p = (5, 5);
    n1 = p + dirs[0];
    n2 = p + dirs[2];
    return n1.x * 1000 + n1.y * 100 + n2.x * 10 + n2.y;
}

// step 91: メソッドのオーバーロード（型違い・個数違い）
class OvC {
    def m(self, x: int) -> int { return 10; }
    def m(self, x: str) -> int { return 20; }
    def m(self, x: int, y: int) -> int { return 30; }
}
def t081() { c = OvC(); return c.m(1) + c.m("a") + c.m(1, 2); }

// step 91: __init__ のオーバーロードは引数で選ばれる
class OvP {
    x: int;
    def __init__(self, a: int) { self.x = a; }
    def __init__(self, a: str) { self.x = 99; }
}
def t082() { p = OvP(42); q = OvP("a"); return p.x + q.x; }

// step 91: dunder（__add__）もオーバーロードできる
class OvV {
    def __add__(self, o: int) -> int { return 100 + o; }
    def __add__(self, o: str) -> int { return 200; }
}
def t083() { v = OvV(); return (v + 1) + (v + "a"); }

// step 91: ジェネリッククラスのメソッドのオーバーロードは、実体化
// された型で解決される
class OvBox<T> {
    def store(self, v: T) -> int { return 1; }
    def store(self, v: int) -> int { return 2; }
}
def t084() { b: OvBox<str> = OvBox(); return b.store("a") * 10 + b.store(3); }

// step 91: テンプレート内では型変数同士でも別署名として共存し、
// 実体化後の具体型で呼び分けられる
class OvPair<T, U> {
    def val(self, x: T) -> int { return 1; }
    def val(self, x: U) -> int { return 2; }
    def go(self, a: T, b: U) -> int { return self.val(a) * 10 + self.val(b); }
}
def t085() { p: OvPair<int, str> = OvPair(); return p.go(1, "a"); }
def t086() { g = Grid96(); src = [[1, 2], [3, 4]]; g.rows = src; src[0][0] = 99; g.rows[1][1] = 44; return g.rows[0][0] * 1000 + src[1][1] * 10 + g.rows[1][1]; }
def t087() { d: Dict<str, [int]> = Dict(zero_array); v = [10, 20]; d["k"] = v; v[0] = 99; got = d["k"]; got[1] = 88; return d["k"][0] * 100 + d["k"][1] * 10 + got[0]; }

// ---- step 99: SegTree / LazySegTree ----
def seg_add(a: int, b: int) -> int { return a + b; }
def seg_min(a: int, b: int) -> int { if (a < b) { return a; } return b; }
def seg_max(a: int, b: int) -> int { if (a > b) { return a; } return b; }
def seg_gcd(a: int, b: int) -> int { while (b != 0) { t = a % b; a = b; b = t; } return a; }
def seg_add_map(f: int, x: int) -> int { return x + f; }
def seg_psum(a: Pos2, b: Pos2) -> Pos2 { return Pos2(a.x + b.x, a.y + b.y); }
def seg_psum_map(f: int, x: Pos2) -> Pos2 { return Pos2(x.x + f * x.y, x.y); }
def seg_asg_map(f: int, x: int) -> int { if (f < 0) { return x; } return f; }
def seg_asg_comp(f: int, g: int) -> int { if (f < 0) { return g; } return f; }
def seg_asg_map2(f: int, x: Pos2) -> Pos2 { if (f < 0) { return x; } return Pos2(f * x.y, x.y); }

def t088() {
    t = SegTree([3, 1, 4, 1, 5], 0, seg_add);
    r = t.all_prod() + t.prod(1, 4) * 10 + t.len * 100;
    t.set(2, 10);
    return r + t.all_prod() * 1000 + t.get(2) * 10000 + t.prod(2, 2) * 100000;
}
def t089() {
    t = SegTree(5, 1000000, seg_min);
    t.set(0, 7); t.set(1, 3); t.set(2, 9); t.set(3, 3); t.set(4, 8);
    u = SegTree(5, -1000000, seg_max);
    u.set(0, 7); u.set(1, 3); u.set(2, 9); u.set(3, 3); u.set(4, 8);
    return t.prod(0, 5) + t.prod(2, 4) * 10 + u.all_prod() * 100 + u.prod(0, 2) * 1000;
}
def t090() {
    t = SegTree([12, 18, 30], 0, seg_gcd);
    u = SegTree(3, 0, seg_gcd);
    u.set(0, 14); u.set(1, 21); u.set(2, 35);
    return t.all_prod() + t.prod(0, 2) * 10 + u.all_prod() * 100;
}
def t091() {
    act: Act<int, int> = Act(0, seg_add_map, seg_add);
    t = LazySegTree([3, 1, 4, 1, 5], -100, seg_max, act);
    r = t.all_prod();
    t.apply(1, 4, 10);
    r = r * 100 + t.all_prod();
    t.apply(2, 100);
    r = r * 100 + t.get(2);
    t.set(2, 7);
    return r * 100 + t.prod(0, 5);
}
def t092() {
    init: [Pos2] = [Pos2(1, 1), Pos2(2, 1), Pos2(3, 1), Pos2(4, 1)];
    act: Act<Pos2, int> = Act(0, seg_psum_map, seg_add);
    t = LazySegTree(init, Pos2(0, 0), seg_psum, act);
    r = t.all_prod().x;
    t.apply(0, 4, 5);
    r = r * 100 + t.all_prod().x;
    r = r * 100 + t.prod(1, 3).x;
    t.apply(2, 1);
    return r * 100 + t.get(2).x;
}
def t093() {
    act: Act<int, int> = Act(-1, seg_asg_map, seg_asg_comp);
    t = LazySegTree(5, -1000000, seg_max, act);
    t.apply(0, 5, 3);
    r = t.all_prod();
    t.apply(1, 3, 9);
    r = r * 100 + t.all_prod();
    r = r * 100 + t.get(0);
    t.apply(4, 2);
    r = r * 100 + t.prod(0, 5);
    init: [Pos2] = [Pos2(1, 1), Pos2(2, 1), Pos2(3, 1)];
    act2: Act<Pos2, int> = Act(-1, seg_asg_map2, seg_asg_comp);
    u = LazySegTree(init, Pos2(0, 0), seg_psum, act2);
    u.apply(0, 3, 7);
    return r * 100 + u.all_prod().x;
}

// 差分テスト用の決定的疑似乱数（LCG）
seg_seed = 987654321;
def seg_rnd(n: int) -> int {
    seg_seed = (seg_seed * 1103515245 + 12345) % 2147483648;
    return seg_seed % n;
}

// SegTree を素朴な配列実装と突き合わせる。折り畳みは素朴側も同じ
// op / e で行う。不一致の数を返す（0 が正常。比較回数が不足したら 999）
def seg_diff(op: fn(int, int) -> int, e: int) -> int {
    n = 37;
    t = SegTree(n, e, op);
    a = [0; n];
    bad = 0;
    cmps = 0;
    i = 0;
    while (i < n) { v = seg_rnd(100); t.set(i, v); a[i] = v; i = i + 1; }
    k = 0;
    while (k < 2000) {
        o = seg_rnd(4);
        if (o == 0) {
            i = seg_rnd(n); v = seg_rnd(100); t.set(i, v); a[i] = v;
        } else if (o == 1) {
            i = seg_rnd(n); cmps = cmps + 1;
            if (t.get(i) != a[i]) { bad = bad + 1; }
        } else if (o == 2) {
            l = seg_rnd(n + 1); r = seg_rnd(n + 1);
            if (l > r) { tmp = l; l = r; r = tmp; }
            s = e; j = l;
            while (j < r) { s = op(s, a[j]); j = j + 1; }
            cmps = cmps + 1;
            if (t.prod(l, r) != s) { bad = bad + 1; }
        } else {
            s = e; j = 0;
            while (j < n) { s = op(s, a[j]); j = j + 1; }
            cmps = cmps + 1;
            if (t.all_prod() != s) { bad = bad + 1; }
        }
        k = k + 1;
    }
    if (cmps < 500) { return 999; }
    return bad;
}
def t094() {
    return seg_diff(seg_add, 0) + seg_diff(seg_min, 1000000) + seg_diff(seg_max, -1000000);
}

// LazySegTree 区間 add・区間 max の差分
def seg_diff_lazy_max() -> int {
    n = 33;
    act: Act<int, int> = Act(0, seg_add_map, seg_add);
    t = LazySegTree(n, -1000000, seg_max, act);
    a = [0; n];
    bad = 0;
    cmps = 0;
    i = 0;
    while (i < n) { v = seg_rnd(100); t.set(i, v); a[i] = v; i = i + 1; }
    k = 0;
    while (k < 2000) {
        o = seg_rnd(5);
        if (o == 0) {
            l = seg_rnd(n + 1); r = seg_rnd(n + 1);
            if (l > r) { tmp = l; l = r; r = tmp; }
            f = seg_rnd(10); t.apply(l, r, f);
            j = l; while (j < r) { a[j] = a[j] + f; j = j + 1; }
        } else if (o == 1) {
            i = seg_rnd(n); f = seg_rnd(10); t.apply(i, f); a[i] = a[i] + f;
        } else if (o == 2) {
            i = seg_rnd(n); v = seg_rnd(100); t.set(i, v); a[i] = v;
        } else if (o == 3) {
            i = seg_rnd(n); cmps = cmps + 1;
            if (t.get(i) != a[i]) { bad = bad + 1; }
        } else {
            l = seg_rnd(n + 1); r = seg_rnd(n + 1);
            if (l > r) { tmp = l; l = r; r = tmp; }
            s = -1000000; j = l;
            while (j < r) { s = seg_max(s, a[j]); j = j + 1; }
            cmps = cmps + 1;
            if (t.prod(l, r) != s) { bad = bad + 1; }
            s = -1000000; j = 0;
            while (j < n) { s = seg_max(s, a[j]); j = j + 1; }
            cmps = cmps + 1;
            if (t.all_prod() != s) { bad = bad + 1; }
        }
        k = k + 1;
    }
    if (cmps < 500) { return 999; }
    return bad;
}

// LazySegTree 区間 add・区間和（Pos2 で (合計, 長さ) を持つ）の差分
def seg_diff_lazy_sum() -> int {
    n = 33;
    init = [Pos2(0, 1); n];
    a = [0; n];
    i = 0;
    while (i < n) { v = seg_rnd(100); init[i] = Pos2(v, 1); a[i] = v; i = i + 1; }
    act: Act<Pos2, int> = Act(0, seg_psum_map, seg_add);
    t = LazySegTree(init, Pos2(0, 0), seg_psum, act);
    bad = 0;
    cmps = 0;
    k = 0;
    while (k < 2000) {
        o = seg_rnd(5);
        if (o == 0) {
            l = seg_rnd(n + 1); r = seg_rnd(n + 1);
            if (l > r) { tmp = l; l = r; r = tmp; }
            f = seg_rnd(10); t.apply(l, r, f);
            j = l; while (j < r) { a[j] = a[j] + f; j = j + 1; }
        } else if (o == 1) {
            i = seg_rnd(n); f = seg_rnd(10); t.apply(i, f); a[i] = a[i] + f;
        } else if (o == 2) {
            i = seg_rnd(n); v = seg_rnd(100); t.set(i, Pos2(v, 1)); a[i] = v;
        } else if (o == 3) {
            i = seg_rnd(n); cmps = cmps + 1;
            if (t.get(i).x != a[i]) { bad = bad + 1; }
        } else {
            l = seg_rnd(n + 1); r = seg_rnd(n + 1);
            if (l > r) { tmp = l; l = r; r = tmp; }
            s = 0; j = l;
            while (j < r) { s = s + a[j]; j = j + 1; }
            cmps = cmps + 1;
            if (t.prod(l, r).x != s) { bad = bad + 1; }
            s = 0; j = 0;
            while (j < n) { s = s + a[j]; j = j + 1; }
            cmps = cmps + 1;
            if (t.all_prod().x != s) { bad = bad + 1; }
        }
        k = k + 1;
    }
    if (cmps < 500) { return 999; }
    return bad;
}
def t095() {
    return seg_diff_lazy_max() + seg_diff_lazy_sum();
}

// LazySegTree 区間代入の差分（区間 max と、Pos2 での区間和）
def seg_diff_lazy_asg() -> int {
    n = 33;
    act: Act<int, int> = Act(-1, seg_asg_map, seg_asg_comp);
    t = LazySegTree(n, -1000000, seg_max, act);
    a = [0; n];
    bad = 0;
    cmps = 0;
    i = 0;
    while (i < n) { v = seg_rnd(100); t.set(i, v); a[i] = v; i = i + 1; }
    k = 0;
    while (k < 2000) {
        o = seg_rnd(5);
        if (o == 0) {
            l = seg_rnd(n + 1); r = seg_rnd(n + 1);
            if (l > r) { tmp = l; l = r; r = tmp; }
            f = seg_rnd(10); t.apply(l, r, f);
            j = l; while (j < r) { a[j] = f; j = j + 1; }
        } else if (o == 1) {
            i = seg_rnd(n); f = seg_rnd(10); t.apply(i, f); a[i] = f;
        } else if (o == 2) {
            i = seg_rnd(n); v = seg_rnd(100); t.set(i, v); a[i] = v;
        } else if (o == 3) {
            i = seg_rnd(n); cmps = cmps + 1;
            if (t.get(i) != a[i]) { bad = bad + 1; }
        } else {
            l = seg_rnd(n + 1); r = seg_rnd(n + 1);
            if (l > r) { tmp = l; l = r; r = tmp; }
            s = -1000000; j = l;
            while (j < r) { s = seg_max(s, a[j]); j = j + 1; }
            cmps = cmps + 1;
            if (t.prod(l, r) != s) { bad = bad + 1; }
            s = -1000000; j = 0;
            while (j < n) { s = seg_max(s, a[j]); j = j + 1; }
            cmps = cmps + 1;
            if (t.all_prod() != s) { bad = bad + 1; }
        }
        k = k + 1;
    }
    if (cmps < 500) { return 999; }
    return bad;
}
def seg_diff_lazy_asg_sum() -> int {
    n = 33;
    init = [Pos2(0, 1); n];
    a = [0; n];
    i = 0;
    while (i < n) { v = seg_rnd(100); init[i] = Pos2(v, 1); a[i] = v; i = i + 1; }
    act: Act<Pos2, int> = Act(-1, seg_asg_map2, seg_asg_comp);
    t = LazySegTree(init, Pos2(0, 0), seg_psum, act);
    bad = 0;
    cmps = 0;
    k = 0;
    while (k < 2000) {
        o = seg_rnd(5);
        if (o == 0) {
            l = seg_rnd(n + 1); r = seg_rnd(n + 1);
            if (l > r) { tmp = l; l = r; r = tmp; }
            f = seg_rnd(10); t.apply(l, r, f);
            j = l; while (j < r) { a[j] = f; j = j + 1; }
        } else if (o == 1) {
            i = seg_rnd(n); f = seg_rnd(10); t.apply(i, f); a[i] = f;
        } else if (o == 2) {
            i = seg_rnd(n); v = seg_rnd(100); t.set(i, Pos2(v, 1)); a[i] = v;
        } else if (o == 3) {
            i = seg_rnd(n); cmps = cmps + 1;
            if (t.get(i).x != a[i]) { bad = bad + 1; }
        } else {
            l = seg_rnd(n + 1); r = seg_rnd(n + 1);
            if (l > r) { tmp = l; l = r; r = tmp; }
            s = 0; j = l;
            while (j < r) { s = s + a[j]; j = j + 1; }
            cmps = cmps + 1;
            if (t.prod(l, r).x != s) { bad = bad + 1; }
            s = 0; j = 0;
            while (j < n) { s = s + a[j]; j = j + 1; }
            cmps = cmps + 1;
            if (t.all_prod().x != s) { bad = bad + 1; }
        }
        k = k + 1;
    }
    if (cmps < 500) { return 999; }
    return bad;
}
def t096() {
    return seg_diff_lazy_asg() + seg_diff_lazy_asg_sum();
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
    mark(58); fails=fails+check(t058(), 323);
    mark(59); fails=fails+check(t059(), 51007);
    mark(60); fails=fails+check(t060(), 588);
    mark(61); fails=fails+check(t061(), 256);
    mark(62); fails=fails+check(t062(), 312);
    mark(63); fails=fails+check(t063(), 5);
    mark(64); fails=fails+check(t064(), 1087);
    mark(65); fails=fails+check(t065(), 21);
    mark(66); fails=fails+check(t066(), 1230);
    mark(67); fails=fails+check(t067(), 101);
    mark(68); fails=fails+check(t068(), 1122);
    mark(69); fails=fails+check(t069(), 492401);
    mark(70); fails=fails+check(t070(), 42);
    mark(71); fails=fails+check(t071(), 3434);
    mark(72); fails=fails+check(t072(), 3609);
    mark(73); fails=fails+check(t073(), 81);
    mark(74); fails=fails+check(t074(), 1);
    mark(75); fails=fails+check(t075(), 13563);
    mark(76); fails=fails+check(t076(), 11);
    mark(77); fails=fails+check(t077(), 32);
    mark(78); fails=fails+check(t078(), 9907);
    mark(79); fails=fails+check(t079(), 630);
    mark(80); fails=fails+check(t080(), 6545);
    mark(81); fails=fails+check(t081(), 60);
    mark(82); fails=fails+check(t082(), 141);
    mark(83); fails=fails+check(t083(), 301);
    mark(84); fails=fails+check(t084(), 12);
    mark(85); fails=fails+check(t085(), 12);
    mark(86); fails=fails+check(t086(), 1084);
    mark(87); fails=fails+check(t087(), 1210);
    mark(88); fails=fails+check(t088(), 120574);
    mark(89); fails=fails+check(t089(), 7933);
    mark(90); fails=fails+check(t090(), 766);
    mark(91); fails=fails+check(t091(), 5151411);
    mark(92); fails=fails+check(t092(), 10301509);
    mark(93); fails=fails+check(t093(), 309030921);
    mark(94); fails=fails+check(t094(), 0);
    mark(95); fails=fails+check(t095(), 0);
    mark(96); fails=fails+check(t096(), 0);

    print_str("done"); putchar(10);
    return fails > 0;
}
