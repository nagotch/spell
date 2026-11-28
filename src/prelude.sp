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

    def __getitem__(self, i: int) -> str {
        return str([self.chars[i]]);
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

    // 辞書順（文字コードの列としての大小）。共通部分が等しければ
    // 短い方が小さい
    def __lt__(self, o: str) -> bool {
        n = self.chars.len;
        if (o.chars.len < n) { n = o.chars.len; }
        i = 0;
        while (i < n) {
            a = self.chars[i];
            b = o.chars[i];
            if (a != b) { return a < b; }
            i = i + 1;
        }
        return self.chars.len < o.chars.len;
    }

    def __le__(self, o: str) -> bool {
        return !(o < self);
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

    def __bool__(self) -> bool {
        return self.len > 0;
    }

    def __iter__(self) -> StrIter {
        return StrIter(self);
    }

    def rev(self) -> str {
        return str(self.chars.rev);
    }

    // 部分文字列（スライス `s[lo:hi]` の実体）。境界は型検査が
    // 正規化済み（0 <= lo <= hi <= 長さ）で渡してくる
    def __slice__(self, lo: int, hi: int) -> str {
        return str(self.chars.__slice__(lo, hi));
    }

    // t で始まるかどうか
    def starts_with(self, t: str) -> bool {
        if (t.len > self.len) { return 1 == 0; }
        i = 0;
        while (i < t.len) {
            if (self.chars[i] != t.chars[i]) { return 1 == 0; }
            i = i + 1;
        }
        return 1 == 1;
    }

    // t で終わるかどうか
    def ends_with(self, t: str) -> bool {
        if (t.len > self.len) { return 1 == 0; }
        base = self.len - t.len;
        i = 0;
        while (i < t.len) {
            if (self.chars[base + i] != t.chars[i]) { return 1 == 0; }
            i = i + 1;
        }
        return 1 == 1;
    }

    // t が最初に現れる位置。無ければ -1。空の t は 0（Python と同じ）
    def find(self, t: str) -> int {
        n = self.len;
        m = t.len;
        i = 0;
        while (i + m <= n) {
            j = 0;
            while (j < m && self.chars[i + j] == t.chars[j]) {
                j = j + 1;
            }
            if (j == m) { return i; }
            i = i + 1;
        }
        return -1;
    }

    // t を含むかどうか
    def contains(self, t: str) -> bool {
        return self.find(t) >= 0;
    }

    // sep で分割した部分文字列の配列。区切りが連続・端にある場合は
    // 空文字列が含まれる（Python の str.split(sep) と同じ）。
    // sep が空文字列の場合は分割せず、自分だけからなる配列を返す
    def split(self, sep: str) -> [str] {
        if (sep.len == 0) { return [self]; }
        cnt = 1;
        i = 0;
        while (i + sep.len <= self.len) {
            if (self.__slice__(i, i + sep.len) == sep) {
                cnt = cnt + 1;
                i = i + sep.len;
            } else {
                i = i + 1;
            }
        }
        r = [self; cnt];
        k = 0;
        start = 0;
        i = 0;
        while (i + sep.len <= self.len) {
            if (self.__slice__(i, i + sep.len) == sep) {
                r[k] = self.__slice__(start, i);
                k = k + 1;
                i = i + sep.len;
                start = i;
            } else {
                i = i + 1;
            }
        }
        r[k] = self.__slice__(start, self.len);
        return r;
    }

    // a の要素を、自分の文字列を区切りにしてつないだ文字列
    def join(self, a: [str]) -> str {
        r = "";
        i = 0;
        while (i < a.len) {
            if (i > 0) { r = r + self; }
            r = r + a[i];
            i = i + 1;
        }
        return r;
    }

    // atoi 式の変換: 先頭の空白（空白・タブ・改行など）と符号を
    // 読み、数字が続く間だけ読む。数字以外で止まり、数字が
    // 無ければ 0
    def to_int(self) -> int {
        i = 0;
        while (i < self.len && (self.chars[i] == 32 || (self.chars[i] >= 9 && self.chars[i] <= 13))) {
            i = i + 1;
        }
        sign = 1;
        if (i < self.len && self.chars[i] == 45) {
            sign = -1;
            i = i + 1;
        } elif (i < self.len && self.chars[i] == 43) {
            i = i + 1;
        }
        n = 0;
        while (i < self.len && self.chars[i] >= 48 && self.chars[i] <= 57) {
            n = n * 10 + (self.chars[i] - 48);
            i = i + 1;
        }
        return sign * n;
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

    def __bool__(self) -> bool {
        return self.len > 0;
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

    // 昇順に並べ替える（インプレース）。要素型に `__lt__` が必要。
    // 安定ソートではない（等しい要素の前後関係は保たれない）
    def sort(self) {
        self.sort_range(0, self.len);
    }

    // [lo, hi) を並べ替える（sort の実体）。median-of-three のピボットを
    // 持つクイックソート（Hoare 分割）で、分割後は小さい方へだけ再帰し
    // 大きい方はループで続けるため、スタックの深さは log n 程度に収まる。
    // 区間が小さい間は挿入ソートに切り替える
    def sort_range(self, lo: int, hi: int) {
        while (hi - lo > 12) {
            mid = lo + (hi - lo) / 2;
            // lo, mid, hi - 1 の中央値が self[mid] に来るよう並べ替える
            if (self[mid] < self[lo]) { t = self[lo]; self[lo] = self[mid]; self[mid] = t; }
            if (self[hi - 1] < self[lo]) { t = self[lo]; self[lo] = self[hi - 1]; self[hi - 1] = t; }
            if (self[hi - 1] < self[mid]) { t = self[mid]; self[mid] = self[hi - 1]; self[hi - 1] = t; }
            pivot = self[mid];
            // Hoare 分割。ピボットの値は区間内に実在する要素なので、
            // 両側の走査は区間の外に出ない
            i = lo;
            j = hi - 1;
            while (1 == 1) {
                while (self[i] < pivot) { i = i + 1; }
                while (pivot < self[j]) { j = j - 1; }
                if (i >= j) { break; }
                t = self[i]; self[i] = self[j]; self[j] = t;
                i = i + 1;
                j = j - 1;
            }
            // 分割点は j。[lo, j + 1) の各要素は [j + 1, hi) の各要素以下
            if (j + 1 - lo < hi - j - 1) {
                self.sort_range(lo, j + 1);
                lo = j + 1;
            } else {
                self.sort_range(j + 1, hi);
                hi = j + 1;
            }
        }
        // 挿入ソート
        i = lo + 1;
        while (i < hi) {
            v = self[i];
            j = i - 1;
            while (j >= lo && v < self[j]) {
                self[j + 1] = self[j];
                j = j - 1;
            }
            self[j + 1] = v;
            i = i + 1;
        }
    }

    // ソート済みの新しい配列を返す（元の配列は変更しない）。要素型に
    // `__lt__` が必要。中身のコピーを作って sort するので、中核の処理は
    // sort と共有する
    def sorted(self) -> [T] {
        if (self.len == 0) { return []; }
        r = [self[0]; self.len];
        i = 0;
        n = self.len;
        while (i < n) {
            r[i] = self[i];
            i = i + 1;
        }
        r.sort();
        return r;
    }

    // 全要素を先頭から `+` で畳み込んだ値。要素型に `__add__` が必要で、
    // 戻り値は要素型そのもの（[str] なら連結、座標型なら成分ごとの和）。
    // 積算は最初の要素から始めるため、空の配列では結果は不定
    def sum(self) -> T {
        r = self[0];
        i = 1;
        n = self.len;
        while (i < n) {
            r = r + self[i];
            i = i + 1;
        }
        return r;
    }

    // 最小の要素。要素型に `__lt__` が必要。空の配列では結果は不定
    def min(self) -> T {
        r = self[0];
        i = 1;
        n = self.len;
        while (i < n) {
            if (self[i] < r) { r = self[i]; }
            i = i + 1;
        }
        return r;
    }

    // 最大の要素。要素型に `__lt__` が必要。空の配列では結果は不定
    def max(self) -> T {
        r = self[0];
        i = 1;
        n = self.len;
        while (i < n) {
            if (r < self[i]) { r = self[i]; }
            i = i + 1;
        }
        return r;
    }

    // v と等しい要素の個数。等しさは要素型の `==`（`__eq__` があれば
    // それ、無ければ同一性）で判定する
    def count(self, v: T) -> int {
        c = 0;
        i = 0;
        n = self.len;
        while (i < n) {
            if (self[i] == v) { c = c + 1; }
            i = i + 1;
        }
        return c;
    }

    // 部分配列（スライス `a[lo:hi]` の実体）。境界は型検査が
    // 正規化済み（0 <= lo <= hi <= 長さ）で渡してくる。要素を
    // コピーした新しい配列を返す（元の配列は変わらない）
    def __slice__(self, lo: int, hi: int) -> [T] {
        n = hi - lo;
        if (n == 0) { return []; }
        r = [self[lo]; n];
        i = 0;
        while (i < n) {
            r[i] = self[lo + i];
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

    def __bool__(self) -> bool {
        return self.count > 0;
    }

    def has(self, key: K) -> bool {
        if (self.cap == 0) { return 1 == 0; }
        // mixed_hash と find_slot の中身をここへ埋め込んである（呼び出し
        // の段数を減らすため）。ハッシュの値も探査の順序も、元の
        // mixed_hash / find_slot と完全に同じ
        m = (1481765933 * 65536) * 65536 + 1284865837;
        a = (335903614 * 65536) * 65536 - 144211633;
        h = key.__hash__() * m + a;
        i = h % self.cap;
        s = 0;
        while (1 == 1) {
            sv = self.slots[i];
            if (sv == 0) { s = -i - 1; break; }
            e = sv - 1;
            if (self.hs[e] == h && self.ks[e] == key) { s = i; break; }
            i = i + 1;
            if (i == self.cap) { i = 0; }
        }
        return s >= 0;
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
        // mixed_hash と find_slot の中身をここへ埋め込んである（has と
        // 同じ形。ハッシュの値も探査の順序も元のメソッドと同じ）
        m = (1481765933 * 65536) * 65536 + 1284865837;
        a = (335903614 * 65536) * 65536 - 144211633;
        h = key.__hash__() * m + a;
        if (self.cap > 0) {
            i = h % self.cap;
            s = 0;
            while (1 == 1) {
                sv = self.slots[i];
                if (sv == 0) { s = -i - 1; break; }
                e = sv - 1;
                if (self.hs[e] == h && self.ks[e] == key) { s = i; break; }
                i = i + 1;
                if (i == self.cap) { i = 0; }
            }
            if (s >= 0) { return self.vs[self.slots[s] - 1]; }
        }
        v = self.factory();
        // ファクトリが同じ辞書にこのキーを入れていたら、そちらを返す
        if (self.cap > 0) {
            i = h % self.cap;
            s = 0;
            while (1 == 1) {
                sv = self.slots[i];
                if (sv == 0) { s = -i - 1; break; }
                e = sv - 1;
                if (self.hs[e] == h && self.ks[e] == key) { s = i; break; }
                i = i + 1;
                if (i == self.cap) { i = 0; }
            }
            if (s >= 0) { return self.vs[self.slots[s] - 1]; }
        }
        self.insert_entry(key, h, v);
        return v;
    }

    def __setitem__(self, key: K, v: V) {
        // mixed_hash と find_slot の中身をここへ埋め込んである（has と
        // 同じ形。ハッシュの値も探査の順序も元のメソッドと同じ）
        m = (1481765933 * 65536) * 65536 + 1284865837;
        a = (335903614 * 65536) * 65536 - 144211633;
        h = key.__hash__() * m + a;
        done = 0;
        if (self.cap > 0) {
            i = h % self.cap;
            s = 0;
            while (1 == 1) {
                sv = self.slots[i];
                if (sv == 0) { s = -i - 1; break; }
                e = sv - 1;
                if (self.hs[e] == h && self.ks[e] == key) { s = i; break; }
                i = i + 1;
                if (i == self.cap) { i = 0; }
            }
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
        // cap > 0 のときだけ呼ぶこと。has / __getitem__ / __setitem__ は
        // 呼び出しの段数を減らすため、この探査を各々の中へ埋め込んである
        // （insert_entry は引き続きこちらを使う）
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
def zero_pos2() -> Pos2 { return Pos2(0, 0); }
def zero_pos3() -> Pos3 { return Pos3(0, 0, 0); }
// 配列型のゼロ値（新しい空配列）。要素型は呼び出し側の文脈で決まるため、
// 戻り値の注釈は付けず、空リテラルから推論される「要素型不明」のままにする
def zero_array() { return []; }

// 伸びる配列。末尾への追加が容量の倍増で均し定数時間になる。生成は
// Dict と同じく既定値ファクトリを受け取る（広げた領域の埋め草に使う）。
// len と反復と to_array が見るのは論理長 n で、容量ではない（添字の
// 範囲チェックは言語全体として未実装で、通常の配列と同じ）。反復は
// 開始時点のスナップショットに対するもの（書き戻しもスナップショット側）
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

    def __bool__(self) -> bool {
        return self.n > 0;
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

// 両端キュー（Queue としても Stack としても使える）。円環バッファで、
// 先頭位置 head と要素数 n だけを動かすので、両端の出し入れは容量の
// 倍増を除いて定数時間。生成は Vec / Dict と同じく既定値ファクトリを
// 受け取る（広げた領域の埋め草に使う）。添字は論理位置（0 が先頭）。
// 空のときに pop_front / pop_back / front / back を呼ぶのは誤りで、
// 結果は不定（配列の範囲外アクセスと同じで、検査はしない）。
// 反復と to_array は開始時点のスナップショットに対するもの
class Deque<T> {
    factory: fn() -> T;
    buf: [T];
    head: int;
    n: int;

    def __init__(self, factory: fn() -> T) {
        self.factory = factory;
        self.buf = [];
        self.head = 0;
        self.n = 0;
    }

    def len(self) -> int {
        return self.n;
    }

    def __bool__(self) -> bool {
        return self.n > 0;
    }

    def grow(self) {
        cap = self.buf.len * 2;
        if (cap == 0) { cap = 8; }
        nb = [self.factory(); cap];
        i = 0;
        while (i < self.n) {
            nb[i] = self.buf[(self.head + i) % self.buf.len];
            i = i + 1;
        }
        self.buf = nb;
        self.head = 0;
    }

    // 矢印演算子 `<-` / `->` の実体は、この dunder 群（step 66）。
    // 演算子は通常メソッドを呼ばず dunder へ書き換えられる、という
    // 言語の統一規則に従う。push 系は self を返す
    def __push_back__(self, v: T) -> Deque<T> {
        if (self.n == self.buf.len) { self.grow(); }
        self.buf[(self.head + self.n) % self.buf.len] = v;
        self.n = self.n + 1;
        return self;
    }

    def __push_front__(self, v: T) -> Deque<T> {
        if (self.n == self.buf.len) { self.grow(); }
        self.head = self.head - 1;
        if (self.head < 0) { self.head = self.buf.len - 1; }
        self.buf[self.head] = v;
        self.n = self.n + 1;
        return self;
    }

    def __pop_back__(self) -> T {
        self.n = self.n - 1;
        return self.buf[(self.head + self.n) % self.buf.len];
    }

    def __pop_front__(self) -> T {
        v = self.buf[self.head];
        self.head = self.head + 1;
        if (self.head == self.buf.len) { self.head = 0; }
        self.n = self.n - 1;
        return v;
    }

    // 公開メソッドは dunder の薄いラッパー
    def push_back(self, v: T) {
        self.__push_back__(v);
    }

    def push_front(self, v: T) {
        self.__push_front__(v);
    }

    def pop_back(self) -> T {
        return self.__pop_back__();
    }

    def pop_front(self) -> T {
        return self.__pop_front__();
    }

    def front(self) -> T {
        return self.buf[self.head];
    }

    def back(self) -> T {
        return self.buf[(self.head + self.n - 1) % self.buf.len];
    }

    def __getitem__(self, i: int) -> T {
        return self.buf[(self.head + i) % self.buf.len];
    }

    def __iter__(self) -> ArrayIter<T> {
        return ArrayIter(self.to_array());
    }

    def to_array(self) -> [T] {
        if (self.n == 0) { return []; }
        r = [self.buf[self.head]; self.n];
        i = 0;
        while (i < self.n) {
            r[i] = self.buf[(self.head + i) % self.buf.len];
            i = i + 1;
        }
        return r;
    }
}

// 2次元の座標（step 77）。リテラル `(x, y)` はこのクラスの構築への糖衣。
// 演算は要素ごとで、`__eq__` / `__hash__` があるため Dict のキーにも
// 使える。添字 `p[0]` / `p[1]` は x / y に対応する。グリッド配列への
// `a[p]` は、型検査が `a[p[0]][p[1]]` へ書き換える
class Pos2 {
    x: int;
    y: int;

    def __init__(self, x: int, y: int) {
        self.x = x;
        self.y = y;
    }

    def __add__(self, o: Pos2) -> Pos2 {
        return Pos2(self.x + o.x, self.y + o.y);
    }

    def __sub__(self, o: Pos2) -> Pos2 {
        return Pos2(self.x - o.x, self.y - o.y);
    }

    def __mul__(self, n: int) -> Pos2 {
        return Pos2(self.x * n, self.y * n);
    }

    def __eq__(self, o: Pos2) -> bool {
        return self.x == o.x && self.y == o.y;
    }

    def __ne__(self, o: Pos2) -> bool {
        return !(self == o);
    }

    // 辞書順（x、次に y）。座標のソートや min / max で使う
    def __lt__(self, o: Pos2) -> bool {
        if (self.x != o.x) { return self.x < o.x; }
        return self.y < o.y;
    }

    def __le__(self, o: Pos2) -> bool {
        return !(o < self);
    }

    def __hash__(self) -> int {
        h = 5381;
        h = h * 33 + self.x;
        h = h * 33 + self.y;
        return h;
    }

    def __getitem__(self, i: int) -> int {
        if (i == 0) { return self.x; }
        return self.y;
    }

    def __str__(self) -> str {
        return self.x.__str__() + " " + self.y.__str__();
    }
}

// 3次元の座標（step 77）。Pos2 の3要素版で、リテラルは `(x, y, z)`
class Pos3 {
    x: int;
    y: int;
    z: int;

    def __init__(self, x: int, y: int, z: int) {
        self.x = x;
        self.y = y;
        self.z = z;
    }

    def __add__(self, o: Pos3) -> Pos3 {
        return Pos3(self.x + o.x, self.y + o.y, self.z + o.z);
    }

    def __sub__(self, o: Pos3) -> Pos3 {
        return Pos3(self.x - o.x, self.y - o.y, self.z - o.z);
    }

    def __mul__(self, n: int) -> Pos3 {
        return Pos3(self.x * n, self.y * n, self.z * n);
    }

    def __eq__(self, o: Pos3) -> bool {
        return self.x == o.x && self.y == o.y && self.z == o.z;
    }

    def __ne__(self, o: Pos3) -> bool {
        return !(self == o);
    }

    // 辞書順（x、次に y、次に z）。Pos2 と同じ線引き
    def __lt__(self, o: Pos3) -> bool {
        if (self.x != o.x) { return self.x < o.x; }
        if (self.y != o.y) { return self.y < o.y; }
        return self.z < o.z;
    }

    def __le__(self, o: Pos3) -> bool {
        return !(o < self);
    }

    def __hash__(self) -> int {
        h = 5381;
        h = h * 33 + self.x;
        h = h * 33 + self.y;
        h = h * 33 + self.z;
        return h;
    }

    def __getitem__(self, i: int) -> int {
        if (i == 0) { return self.x; }
        if (i == 1) { return self.y; }
        return self.z;
    }

    def __str__(self) -> str {
        return self.x.__str__() + " " + self.y.__str__() + " " + self.z.__str__();
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
    // 先行する空白は読み飛ばす（read_token と揃える。step 87。これが
    // 無いと、空白が2つ以上続いたり入力の先頭に空白があると 0 を
    // 返して以降の読みが全部ずれる）
    while (char == 32 || char == 10 || char == 13 || char == 9) { char = getchar(); }
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
