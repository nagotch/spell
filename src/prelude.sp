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

    def __hash__(self) -> int {
        h = 5381;
        i = 0;
        while (i < self.chars.len) {
            h = h * 33 + self.chars[i];
            i = i + 1;
        }
        return h;
    }

    def __str__(self) -> str {
        return self;
    }

    def __iter__(self) -> StrIter {
        return StrIter(self);
    }

    def rev(self) -> str {
        return str(self.chars.rev);
    }
}

class StrIter {
    s: str;
    i: int;

    def __init__(self, s: str) {
        self.s = s;
        self.i = 0;
    }

    def __has_next__(self) -> bool {
        return self.i < self.s.len;
    }

    def __next__(self) -> str {
        v = str([self.s.chars[self.i]]);
        self.i = self.i + 1;
        return v;
    }

    // 書き戻し: 現在位置の文字を、渡された文字列の先頭の文字で置き換える。
    // 空文字列が渡された場合は何もしない
    def __set_current__(self, v: str) {
        if (v.len > 0) {
            self.s.chars[self.i - 1] = v.chars[0];
        }
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
    def __hash__(self) -> int { return self; }

    def __iter__(self) -> RangeIter {
        return RangeIter(0, self, 1, 0);
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
            q = n / 10;
            if (n < 0 && d != 0) {
                // `/` と `%` は Python 式（床除算）なので、負の間は
                // 切り捨て除算の桁と商に直してから使う
                d = 10 - d;
                q += 1;
            }
            digits[i] = 48 + d;
            n = q;
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
    def __hash__(self) -> int {
        if (self) { return 1; }
        return 0;
    }

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

    def __hash__(self) -> int {
        // 要素のハッシュを合成する。配列は書き換え可能なので、辞書のキーに
        // 使ったあとで中身を変えると、そのエントリは引けなくなる
        h = 5381;
        i = 0;
        while (i < self.len) {
            h = h * 33 + self[i].__hash__();
            i = i + 1;
        }
        return h;
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

    def rev(self) -> [T] {
        if (self.len == 0) { return []; }
        r = [self[0]; self.len];
        i = 0;
        while (i < self.len) {
            r[i] = self[self.len - 1 - i];
            i = i + 1;
        }
        return r;
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
    end: int;
    step: int;
    incl: int;
    fin: int;

    def __init__(self, cur: int, end: int, step: int, incl: int) {
        self.cur = cur;
        self.end = end;
        self.step = step;
        self.incl = incl;
        self.fin = 0;
    }

    def __has_next__(self) -> bool {
        // 終端を含む範囲では、端の値を返した後に cur が巻き戻っても
        // 継続と誤判定しないよう、返し終えたことを fin で覚える
        if (self.fin == 1) { return 1 == 0; }
        if (self.step > 0) { return self.cur < self.end || (self.incl == 1 && self.cur == self.end); }
        return self.cur > self.end || (self.incl == 1 && self.cur == self.end);
    }

    def __next__(self) -> int {
        if (self.step > 0) {
            v = self.cur;
            self.cur = self.cur + 1;
            if (self.incl == 1 && v == self.end) { self.fin = 1; }
            return v;
        }
        if (self.incl == 1) {
            v = self.cur;
            self.cur = self.cur - 1;
            if (v == self.end) { self.fin = 1; }
            return v;
        }
        self.cur = self.cur - 1;
        return self.cur;
    }
}

class Range {
    lo: int;
    hi: int;
    incl: int;

    def __init__(self, lo: int, hi: int, incl: int) {
        self.lo = lo;
        self.hi = hi;
        self.incl = incl;
    }

    def __iter__(self) -> RangeIter {
        if (self.lo <= self.hi) {
            return RangeIter(self.lo, self.hi, 1, self.incl);
        }
        return RangeIter(self.lo, self.hi, -1, self.incl);
    }

    def rev(self) -> Range {
        return Range(self.hi, self.lo, self.incl);
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

    // 書き戻し: 直前に __next__ で返した要素を置き換える
    def __set_current__(self, v: T) {
        self.arr[self.i - 1] = v;
    }
}

class DictIter<K> {
    ks: [K];
    n: int;
    i: int;

    def __init__(self, ks: [K], n: int) {
        self.ks = ks;
        self.n = n;
        self.i = 0;
    }

    def __has_next__(self) -> bool {
        return self.i < self.n;
    }

    def __next__(self) -> K {
        v = self.ks[self.i];
        self.i = self.i + 1;
        return v;
    }
}

def __dict_is_prime(n: int) -> int {
    if (n % 2 == 0) { return 0; }
    d = 3;
    while (d * d <= n) {
        if (n % d == 0) { return 0; }
        d = d + 2;
    }
    return 1;
}

def __dict_next_cap(cap: int) -> int {
    // 容量は素数にする（恒等ハッシュでも、剰余で全体に散らすため）。
    // おおよそ2倍ずつ大きくなる素数を返す
    n = cap * 2 + 1;
    if (n < 17) { n = 17; }
    while (__dict_is_prime(n) == 0) { n = n + 2; }
    return n;
}

// 辞書。キーは __hash__ と __eq__ を持つ型なら何でもよい（等しいキーは
// 等しいハッシュを返すこと）。読み取り d[k] は、キーが無ければ構築時に
// 渡したファクトリで作った既定値を登録してから返す（defaultdict 式）。
// ファクトリはキーごとに呼ばれるので、既定値が共有されることはない。
// ファクトリの中で同じ辞書を触ってはいけない（キーを入れた場合だけ、
// 入れた側の値が返るよう __getitem__ が引き直す）。
// 内部はオープンアドレス法のハッシュテーブルで、キーと値は挿入順の
// 並行配列に持ち、テーブルはエントリ番号を引く。削除はないので
// tombstone は持たない。イテレーションは開始時点のキー並びを見る。
class Dict<K, V> {
    factory: fn() -> V;
    ks: [K];
    vs: [V];
    hs: [int];
    slots: [int];
    count: int;
    cap: int;

    def __init__(self, factory: fn() -> V) {
        self.factory = factory;
        self.ks = [];
        self.vs = [];
        self.hs = [];
        self.slots = [];
        self.count = 0;
        self.cap = 0;
    }

    def len(self) -> int {
        return self.count;
    }

    def has(self, key: K) -> bool {
        if (self.cap == 0) { return 1 == 0; }
        return self.find_slot(key, self.mixed_hash(key)) >= 0;
    }

    def keys(self) -> [K] {
        if (self.count == 0) { return []; }
        r = [self.ks[0]; self.count];
        i = 0;
        while (i < self.count) {
            r[i] = self.ks[i];
            i = i + 1;
        }
        return r;
    }

    def __getitem__(self, key: K) -> V {
        h = self.mixed_hash(key);
        if (self.cap > 0) {
            s = self.find_slot(key, h);
            if (s >= 0) { return self.vs[self.slots[s] - 1]; }
        }
        v = self.factory();
        // ファクトリが同じ辞書にこのキーを入れていたら、そちらを返す
        if (self.cap > 0) {
            s = self.find_slot(key, h);
            if (s >= 0) { return self.vs[self.slots[s] - 1]; }
        }
        self.insert_entry(key, h, v);
        return v;
    }

    def __setitem__(self, key: K, v: V) {
        h = self.mixed_hash(key);
        done = 0;
        if (self.cap > 0) {
            s = self.find_slot(key, h);
            if (s >= 0) {
                // 上書きではキーの並び（挿入位置）は変えない
                self.vs[self.slots[s] - 1] = v;
                done = 1;
            }
        }
        if (done == 0) { self.insert_entry(key, h, v); }
    }

    def __iter__(self) -> DictIter<K> {
        return DictIter(self.ks, self.count);
    }

    def mixed_hash(self, key: K) -> int {
        // キーのハッシュをテーブル用に64ビット全体へ混ぜる。ビット演算が
        // 無いので、Knuth の LCG 定数による乗算と加算で代用する。定数は
        // 32ビット即値に収まらないため、上位と下位に分けて組み立てる
        // （6364136223846793005 = 1481765933 * 2^32 + 1284865837、
        //   1442695040888963407 = 335903614 * 2^32 + (2^32 - 144211633)）
        m = (1481765933 * 65536) * 65536 + 1284865837;
        a = (335903614 * 65536) * 65536 - 144211633;
        return key.__hash__() * m + a;
    }

    def find_slot(self, key: K, h: int) -> int {
        // 戻り値: キーがあればそのスロット位置、なければ -(挿入位置) - 1。
        // cap > 0 のときだけ呼ぶこと
        i = h % self.cap;
        while (1 == 1) {
            s = self.slots[i];
            if (s == 0) { return -i - 1; }
            e = s - 1;
            if (self.hs[e] == h && self.ks[e] == key) { return i; }
            i = i + 1;
            if (i == self.cap) { i = 0; }
        }
        return 0;
    }

    def insert_entry(self, key: K, h: int, v: V) {
        // 負荷率が 2/3 を超えるなら先にテーブルを広げる
        if ((self.count + 1) * 3 > self.cap * 2) { self.grow_table(); }
        // エントリ配列の容量が一杯なら、今の cap まで広げる。埋め草は
        // 挿入するキー・値・ハッシュ自身で、count の範囲外は読まれない
        if (self.count == self.ks.len) {
            nk = [key; self.cap];
            nv = [v; self.cap];
            nh = [h; self.cap];
            i = 0;
            while (i < self.count) {
                nk[i] = self.ks[i];
                nv[i] = self.vs[i];
                nh[i] = self.hs[i];
                i = i + 1;
            }
            self.ks = nk;
            self.vs = nv;
            self.hs = nh;
        }
        s = self.find_slot(key, h);
        self.ks[self.count] = key;
        self.vs[self.count] = v;
        self.hs[self.count] = h;
        self.slots[-s - 1] = self.count + 1;
        self.count = self.count + 1;
    }

    def grow_table(self) {
        ncap = __dict_next_cap(self.cap);
        nslots = [0; ncap];
        e = 0;
        while (e < self.count) {
            i = self.hs[e] % ncap;
            while (nslots[i] != 0) {
                i = i + 1;
                if (i == ncap) { i = 0; }
            }
            nslots[i] = e + 1;
            e = e + 1;
        }
        self.slots = nslots;
        self.cap = ncap;
    }
}

// Dict の既定値ファクトリ（型ごとのゼロ値）
def zero_int() -> int { return 0; }
def zero_bool() -> bool { return 1 == 0; }
def zero_str() -> str { return ""; }

// 伸びる配列。末尾への追加が容量の倍増で均し定数時間になる。生成は
// Dict と同じく既定値ファクトリを受け取る（広げた領域の埋め草に使う）。
// 添字と len が見るのは論理長 n で、容量ではない。反復は開始時点の
// スナップショットに対するもの（書き戻しもスナップショット側に行く）
class Vec<T> {
    factory: fn() -> T;
    buf: [T];
    n: int;

    def __init__(self, factory: fn() -> T) {
        self.factory = factory;
        self.buf = [];
        self.n = 0;
    }

    def len(self) -> int {
        return self.n;
    }

    def append(self, v: T) {
        if (self.n == self.buf.len) {
            cap = self.buf.len * 2;
            if (cap == 0) { cap = 8; }
            nb = [self.factory(); cap];
            i = 0;
            while (i < self.n) {
                nb[i] = self.buf[i];
                i = i + 1;
            }
            self.buf = nb;
        }
        self.buf[self.n] = v;
        self.n = self.n + 1;
    }

    def __getitem__(self, i: int) -> T {
        return self.buf[i];
    }

    def __setitem__(self, i: int, v: T) {
        self.buf[i] = v;
    }

    def __iter__(self) -> ArrayIter<T> {
        return ArrayIter(self.to_array());
    }

    def to_array(self) -> [T] {
        if (self.n == 0) { return []; }
        r = [self.buf[0]; self.n];
        i = 0;
        while (i < self.n) {
            r[i] = self.buf[i];
            i = i + 1;
        }
        return r;
    }
}

def read_ints(n: int) -> [int] {
    a = [0; n];
    for i in n { a[i] = read_int(); }
    return a;
}

def read_ints2(h: int, w: int) -> [[int]] {
    a = [[0]; h];
    for i in h { a[i] = read_ints(w); }
    return a;
}

def read_ints_dec(n: int) -> [int] {
    a = read_ints(n);
    for i in n { a[i] = a[i] - 1; }
    return a;
}

def read_token() -> str {
    cap = 64;
    buf = [0; cap];
    n = 0;
    char = getchar();
    while (char == 32 || char == 10 || char == 13 || char == 9) { char = getchar(); }
    while (char != -1 && char != 32 && char != 10 && char != 13 && char != 9) {
        if (n == cap) {
            bigger = [0; cap * 2];
            for i in n { bigger[i] = buf[i]; }
            buf = bigger;
            cap = cap * 2;
        }
        buf[n] = char;
        n = n + 1;
        char = getchar();
    }
    r = [0; n];
    for i in n { r[i] = buf[i]; }
    return str(r);
}

def read_tokens(n: int) -> [str] {
    if (n == 0) { return []; }
    a = [""; n];
    for i in n { a[i] = read_token(); }
    return a;
}

def read_line() -> str {
    cap = 64;
    buf = [0; cap];
    n = 0;
    char = getchar();
    while (char != -1 && char != 10) {
        if (n == cap) {
            bigger = [0; cap * 2];
            for i in n { bigger[i] = buf[i]; }
            buf = bigger;
            cap = cap * 2;
        }
        buf[n] = char;
        n = n + 1;
        char = getchar();
    }
    r = [0; n];
    for i in n { r[i] = buf[i]; }
    return str(r);
}

def print_yn(b: bool) {
    if (b) { print("Yes"); } else { print("No"); }
}

def __count_range(n: int) -> Range {
    if (n < 0) { return Range(0, 0, 0); }
    return Range(0, n, 0);
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
    buf = [0; cap];
    n = 0;
    char = getchar();
    while (char != -1) {
        if (n == cap) {
            bigger = [0; cap * 2];
            for i in n { bigger[i] = buf[i]; }
            buf = bigger;
            cap = cap * 2;
        }
        buf[n] = char;
        char = getchar();
        n=n+1;
    }
    r_str = [0; n];
    for i in n {
        r_str[i]=buf[i];
    }
    return str(r_str);
}

def print_int(n: int) {
    s = n.__str__();
    print_str(s);
}

def print_str(s: str) {
    for c in s.chars {
        putchar(c);
    }
}

def __println(s: str) {
    print_str(s);
    putchar(10);
}

// ドライバ層（step 59）。コンパイラ組み込みプリミティブ（__spell_*）の
// 薄いラッパで、argv・ファイル・stderr・終了コードを扱う

// コマンドライン引数。[0] はプログラム名（C / sys.argv と同じ）
def args() -> [str] {
    n = __spell_argc();
    if (n == 0) { return []; }
    r = [""; n];
    i = 0;
    while (i < n) {
        r[i] = str(__spell_argv_chars(i));
        i = i + 1;
    }
    return r;
}

// read_file の結果。ok は、開けて最後まで読めたときだけ true。
// 空のファイルは ok = true で text = ""
class FileData {
    ok: bool;
    text: str;

    def __init__(self, ok: bool, text: str) {
        self.ok = ok;
        self.text = text;
    }
}

def read_file(path: str) -> FileData {
    chars = __spell_read_file_chars(path.chars);
    // __spell_io_ok は直前の読み書きの結果を返すので、他のプリミティブを
    // 挟まずにここで読む
    ok = __spell_io_ok();
    return FileData(ok, str(chars));
}

// 全部書けて閉じられたら true
def write_file(path: str, text: str) -> bool {
    return __spell_write_file_chars(path.chars, text.chars) == 1;
}

// eprint の実体（print の __println に相当）。文字列化は型検査側で済み、
// ここでは改行を付けて stderr へ出す
def __eprintln(s: str) {
    __spell_stderr_chars(s.chars + [10]);
}
