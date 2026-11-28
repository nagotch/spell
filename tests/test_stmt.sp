def t001() { a=3; a; }
def t002() { a=3; b=5*6-8; a+b/2; }
def t003() { a=b=3; a+b; }
def t004() { foo=3; foo; }
def t005() { foo=3; bar=5; foo+bar; }
def t006() { fo=1; foo=2; fo+foo; }
def t007() { foo1=5; foo1; }
def t008() { return 42; }
def t009() { return 5; return 8; }
def t010() { a=3; b=5*6-8; return a+b/2; }
def t011() { returnx=3; returnx; }
def t012() { return1=7; return1; }
def t013() { if (0) return 1; return 2; }
def t014() { if (1) return 1; return 2; }
def t015() { if (0) return 1; else return 2; }
def t016() { if (1) return 1; else return 2; }
def t017() { a=0; if (1) a=1; return a; }
def t018() { a=0; if (0) a=1; return a; }
def t019() { a=0; while (a<10) a=a+1; return a; }
def t020() { s=0; for i in 10 s=s+i; return s; }
def t021() { while (1) return 3; }
def t022() { a=0; while (a<3) a=a+1; return a; }
def t023() { a=0; { a=1; a=2; } return a; }
def t024() { a=0; if (1) { a=1; a=2; } return a; }
def t025() { a=0; if (0) { a=1; } else { a=2; } return a; }
def t026() { a=0; while (a<3) { a=a+1; a=a+1; } return a; }
def t027() { s=0; for i in 3 { s=s+i; s=s+1; } return s; }
def t028() { a=0; { a=1; { a=2; } } return a; }
def t029() { a=5; {} return a; }
def t030() { i = 0; while (1) { i += 1; if (i == 5) { break; } } return i; }
def t031() { i = 0; s = 0; while (i < 10) { i += 1; if (i == 5) { continue; } s += i; } return s; }
def t032() { s = 0; for i in 5 { if (i == 2) { continue; } s += i; } return s; }
def t033() { total = 0; for i in 3 { for j in 10 { if (j == 2) { break; } total += 1; } } return total; }
def t034() { x: int = 40; x = x + 2; return x; }
def t035() { s = 0; for i in 1..5 { s = s + i; } return s; }
def t036() { s = 0; for 4 { s = s + 3; } return s; }
def t037() { for i in 3 { } return i; }
def t038() { s = 0; j = 0; for i in 3 { j = 99; s = s + 1; } return s; }
def t039() { x = 2; if (x == 1) { return 1; } elif (x == 2) { return 2; } else { return 3; } }
def t040() { s = 0; i = 0; while (i < 10) { i++; if (i == 3) { ^; } if (i == 6) { !; } s = s + i; } return s; }
def t041() { x = 5; x++; x++; x--; a = [10]; a[0]++; return x * 100 + a[0]; }
def t042() { t = 0; i@5 { t = t + i; } u = 0; i@1..4 { u = u + i; } v = 0; @3 { v = v + 2; } return t * 100 + u * 10 + v; }
def t043() { s = 0; for i in 1..=3 { s = s + i; } for i in 3..=3 { s = s + i; } return s; }
def t044() { r = 0; for i in 3..0 { r = r * 10 + i; } q = 0; for i in 5..=2 { q = q * 10 + i; } return r * 10000 + q; }
def t045() { c = 0; @-2 { c++; } for -3 { c++; } return c; }
def t046() { m = 1; for i in 62 { m = m * 2; } m = m * 2 - 1; c = 0; for i in m..=m { c++; } d = 0; for i in m..=(m - 2) { d++; } return c * 10 + d; }
cnt = 0;
def bump() { cnt = cnt + 1; return cnt; }
def sum2(a, b) { return a + b; }
def t047() { a = 1; b = 2; a, b = b, a; x = 0; y = 0; x, y = 5, x + 10; return a * 1000 + b * 100 + x * 10 + y; }
def t048() { arr = [7, 8]; arr[0], arr[1] = arr[1], arr[0]; return arr[0] * 10 + arr[1]; }
def t049() { r = 0; if (1 < 2 < 3) { r = r + 1; } if (3 > 2 > 1) { r = r + 2; } if (1 < 2 < 2) { r = r + 4; } q = 2 < bump() < 3; if (q) { r = r + 8; } return r * 100 + cnt; }
def t050() { r = 0; if (5 == 5 == 5) { r = r + 1; } if (5 == 5 != 5) { r = r + 2; } if (1 == 1 == 1) { r = r + 4; } return r; }
def t051() { s = 0; for i in 5 { if (i == 2) { break; } s = s + 1; } else { s = 100; } t = 0; for i in 3 { t = t + 1; } else { t = 100; } return s * 1000 + t; }
def t052() { u = 0; for i in 3 { for j in 2 { break; } u = u + 1; } else { u = 100; } return u; }
def t053() { v = 0; for i in 0 { v = 1; } else { v = 42; } return v; }
def t054() {
    a = 3
    b = 4
    return a * 10 + b
}
def t055() {
    i = 0
    s = 0
    while (i < 5) {
        s = s + i
        i = i + 1
    }
    return s
}
def t056() {
    x = 0
    if (1)
        x = 1
    else
        x = 2
    return x
}
def t057() {
    s = 0
    for i in 4
        s = s + i
    return s
}
def t058() {
    a = 1 +
        2
    return a
}
def t059() {
    a = 1 +
    2
    b = a
    - 1
    return b * 10 + a
}
def t060() {
    i = 0
    s = 0
    while (1) {
        i = i + 1
        if (i == 3)
            ^
        if (i == 5)
            !
        s = s + i
    }
    return s * 100 + i
}
def t061() {
    ;;a = 1;
    b = 2
    ; c = a + b;
    return c * 10 + b
}
def t062() {
    a = [
        1,
        2,
        3
    ]
    s = 0
    for x in a
        s = s + x
    return s
}
def t063() {
    r = sum2(10,
        5)
    return r
}
def t064() {
    a = [10, 20, 30]
    x = a
    [1]
    return x[0] * 100 + x[1]
}
def t065() {
    i = 0
    s = 0
    while (i < 3) {
        i = i + 1
        !i
        s = s + 1
    }
    return s
}
def t066() {
    i = 0
    while (i <
        3) {
        i = i + 1
    }
    return i
}
def t067() {
    s = 0
    @3
        s = s + 2
    i@1..3
        s = s + i
    return s
}
def t068() {
    i = 0
    s = 0
    while (1) {
        i = i + 1
        if (i == 2)
            continue
        if (i == 4)
            break
        s = s + i
    }
    return s * 10 + i
}

def t069() {
    i = 0
    s = 0
    @ {
        i = i + 1
        if (i > 5)
            break
        s = s + i
    }
    return s
}

def t070() {
    i = 0
    s = 0
    @ {
        i = i + 1
        if (i % 2 == 0)
            continue
        if (i >= 9)
            !
        s = s + i
    }
    return s
}

def t071() {
    n = 0
    t = 0
    while (n < 3) {
        n = n + 1
        @ {
            t = t + 1
            break
        }
    }
    return t * 10 + n
}

def t072() { a = [1, 2, 3]; x@a { x = x * 2; } return a[0] * 100 + a[1] * 10 + a[2]; }

def t073() { a = [1, 2, 3, 4]; for x in a { x = x * 10; if (x == 20) { break; } } return a[0] * 1000 + a[1] * 100 + a[2] * 10 + a[3]; }

def t074() { a = [1, 2, 3]; for x in a { x = x + 1; if (x == 3) { continue; } x = x * 10; } return a[0] * 10000 + a[1] * 100 + a[2]; }

def t075() { a = [1, 2]; b = [10, 20]; for x in a { for y in b { y = y + 1; } } return a[0] * 1000 + a[1] * 100 + b[0] * 10 + b[1]; }

def t076() { s = "ABCA"; for x in s { if (x != "A") { x = "."; } } if (s == "A..A") { return 1; } return 0; }

def t077() { s = "abc"; n = 0; for x in s { n = n + x.len; } return n; }

def t078() { x = 2; r = 0; if x == 1 { r = 1; } elif x == 2 { r = 2; } else { r = 3; } if x == 2 { r = r + 10; } return r; }

def t079() {
    x = 5
    r = 0
    if x < 0:
        r = 1
    elif x < 10:
        r = 2
    else:
        r = 3
    if x == 5: r = r + 20
    return r
}

def t080() {
    i = 0
    s = 0
    while i < 5:
        s = s + i
        i = i + 1
    j = 0
    while j < 3 { s = s + 100; j = j + 1; }
    return s * 100 + i * 10 + j
}

def t081() { a = 2; b = 3; r = 0; if (a + b) * 2 > 9 { r = 1; } if (a + b) * 2 == 10: r = r + 2; return r; }

def t082() { x = 3; r = 0; if 0 <= x < 4 { r = r + 1; } if 0 <= x && x < 4: r = r + 2; if 1 < x < 3 { r = r + 4; } if x < 2 || x > 2 { r = r + 8; } return r; }

def t083() { done = 1 > 2; r = 0; if !done { r = 1; } i = 0; while sum2(i, 1) < 5 { i = i + 1; } return r * 10 + i; }

def pairp(n: int) -> int { s = 0; i = 0; while (i < n) { if (i % 2 == 0) { s = s + i; } i = i + 1; } return s; }

def pairn(n: int) -> int:
    s = 0
    i = 0
    while i < n:
        if i % 2 == 0: s = s + i
        i = i + 1
    return s

def t084() { return pairp(10) * 1000 + pairn(10); }

def t085() {
    a = 1
    b = 2
    r = 0
    if a == 1 &&
        b == 2
        r = 5
    i = 0
    while i < 3 &&
        r == 5 { i = i + 1; }
    return r * 10 + i
}

def t086() {
    r = 0
    if true: r = r + 1
    if F: r = r + 10
    else: r = r + 2
    if (false) { r = r + 100; }
    while F { r = r + 1000; }
    return r
}

def t087() {
    r = 0
    if T && !F:
        r = 7
    if false || F:
        r = 99
    return r
}

def t088() {
    // step 73: 文頭の `?` は if の糖衣（真・偽・else）
    r = 0
    ? 1 < 2: r = r + 1
    ? 1 > 2: r = r + 100
    else: r = r + 2
    ? false { r = r + 1000; }
    return r
}

def t089() {
    // step 73: elif / else との併用、括弧つき条件、`{}` 形式ブロック
    x = 2
    r = 0
    ? (x == 1) {
        r = 10
    } elif (x == 2) {
        r = 20
    } else {
        r = 30
    }
    ? x == 3:
        r = r + 1000
    elif x == 2:
        r = r + 200
    else:
        r = r + 3000
    return r
}

def t090() {
    // step 73: ネストした `?` と、while / for との混在
    s = 0
    i = 0
    while i < 5:
        ? i % 2 == 0:
            ? i > 0:
                s = s + i
        i = i + 1
    for j in 3:
        ? j == 2:
            s = s + 100
    return s
}

def t091() {
    // step 73: if 文と `?` の混在（`?` は if 専用）
    r = 0
    if true { r = r + 1; }
    ? T: r = r + 2
    if F: r = r + 100
    ? F: r = r + 1000
    else: r = r + 4
    return r
}

def mark(n) {
    print_int(n); putchar(10); fflush(0);
    return 0;
}

def check(got, expected) {
    if (got != expected) return 1;
    return 0;
}

def t092() {
    r = 0
    a = 1
    if a == 0:
        r = 100
    | a == 1:
        r = 101
    | a == 2:
        r = 102
    |:
        r = 199
    b = 9
    if b == 0:
        r = r + 1000
    | b == 1:
        r = r + 2000
    |:
        r = r + 3000
    return r
}

def t093() {
    r = 0
    x = 2
    if x == 1:
        r = 1
    elif x == 2:
        r = 2
    |:
        r = 3
    y = 3
    if y == 1:
        r = r + 10
    | y == 2:
        r = r + 20
    else:
        r = r + 30
    z = 0
    if z == 1 {
        r = r + 100
    } | z == 0 {
        r = r + 200
    } else {
        r = r + 400
    }
    return r
}

def t094() { a = 2; r = 0; if a == 0 { r = 1; } | a == 1 { r = 2; } | a == 2 { r = 3; } | { r = 4; } return r; }

def t095() {
    s = 0
    for i in 0..5:
        if i == 3:
            break
        s = s + 1
    |:
        s = 100
    t = 0
    for j in 3:
        t = t + 1
    |:
        t = t + 10
    return s * 100 + t
}

def t096() {
    r = 0
    if 1 == 1:
        if 2 == 3:
            r = 1
        |:
            r = 2
    |:
        r = 3
    if 1 == 2:
        r = r + 10
    |:
        r = r + 20
    return r
}

def t097() {
    r = 0
    if 1 == 2: r = 1
    |: r = 2
    if 1 == 1: r = r + 10
    |: r = r + 100
    return r
}

def t098() {
    n = 10
    s = 0
    @? n > 0:
        n -= 1
        ? n == 5: continue
        ? n == 2: break
        s += n
    return s * 10 + n
}

def t099() {
    q = Deque()
    q.push_back(1)
    q.push_back(2)
    q.push_back(3)
    s = 0
    @? q:
        v <- q
        s = s * 10 + v
    return s
}

def t100() {
    i = 0
    s = 0
    @? i < 3 {
        i += 1
        @2 { s += 1 }
        ? i == 2: s += 10
    }
    return s * 10 + i
}

def t101() {
    i = 0
    s = 0
    @? i < 2:
        j = 0
        @? j < 3: j += 1
        s += j
        i += 1
    return s * 10 + i
}

def t102() {
    a = 0
    if not false and true:
        a = 1
    if true and not true:
        a = 2
    return a
}

def t103() {
    i = 0
    while i < 10 and not (i == 4):
        i += 1
    s = 0
    while s < 3 or false:
        s += 1
    return i * 10 + s
}

def t104() {
    a = 2
    ? not a == 1 or false:
        a = 5
    @? a < 10 and a > 0:
        a += 5
    return a
}

def main() {
    fails = 0;

    mark(1); fails=fails+check(t001(), 3);
    mark(2); fails=fails+check(t002(), 14);
    mark(3); fails=fails+check(t003(), 6);
    mark(4); fails=fails+check(t004(), 3);
    mark(5); fails=fails+check(t005(), 8);
    mark(6); fails=fails+check(t006(), 3);
    mark(7); fails=fails+check(t007(), 5);
    mark(8); fails=fails+check(t008(), 42);
    mark(9); fails=fails+check(t009(), 5);
    mark(10); fails=fails+check(t010(), 14);
    mark(11); fails=fails+check(t011(), 3);
    mark(12); fails=fails+check(t012(), 7);
    mark(13); fails=fails+check(t013(), 2);
    mark(14); fails=fails+check(t014(), 1);
    mark(15); fails=fails+check(t015(), 2);
    mark(16); fails=fails+check(t016(), 1);
    mark(17); fails=fails+check(t017(), 1);
    mark(18); fails=fails+check(t018(), 0);
    mark(19); fails=fails+check(t019(), 10);
    mark(20); fails=fails+check(t020(), 45);
    mark(21); fails=fails+check(t021(), 3);
    mark(22); fails=fails+check(t022(), 3);
    mark(23); fails=fails+check(t023(), 2);
    mark(24); fails=fails+check(t024(), 2);
    mark(25); fails=fails+check(t025(), 2);
    mark(26); fails=fails+check(t026(), 4);
    mark(27); fails=fails+check(t027(), 6);
    mark(28); fails=fails+check(t028(), 2);
    mark(29); fails=fails+check(t029(), 5);
    mark(30); fails=fails+check(t030(), 5);
    mark(31); fails=fails+check(t031(), 50);
    mark(32); fails=fails+check(t032(), 8);
    mark(33); fails=fails+check(t033(), 6);
    mark(34); fails=fails+check(t034(), 42);
    mark(35); fails=fails+check(t035(), 10);
    mark(36); fails=fails+check(t036(), 12);
    mark(37); fails=fails+check(t037(), 2);
    mark(38); fails=fails+check(t038(), 3);
    mark(39); fails=fails+check(t039(), 2);
    mark(40); fails=fails+check(t040(), 12);
    mark(41); fails=fails+check(t041(), 611);
    mark(42); fails=fails+check(t042(), 1066);
    mark(43); fails=fails+check(t043(), 9);
    mark(44); fails=fails+check(t044(), 2105432);
    mark(45); fails=fails+check(t045(), 0);
    mark(46); fails=fails+check(t046(), 13);
    mark(47); fails=fails+check(t047(), 2160);
    mark(48); fails=fails+check(t048(), 87);
    mark(49); fails=fails+check(t049(), 301);
    mark(50); fails=fails+check(t050(), 5);
    mark(51); fails=fails+check(t051(), 2100);
    mark(52); fails=fails+check(t052(), 100);
    mark(53); fails=fails+check(t053(), 42);
    mark(54); fails=fails+check(t054(), 34);
    mark(55); fails=fails+check(t055(), 10);
    mark(56); fails=fails+check(t056(), 1);
    mark(57); fails=fails+check(t057(), 6);
    mark(58); fails=fails+check(t058(), 3);
    mark(59); fails=fails+check(t059(), 33);
    mark(60); fails=fails+check(t060(), 705);
    mark(61); fails=fails+check(t061(), 32);
    mark(62); fails=fails+check(t062(), 6);
    mark(63); fails=fails+check(t063(), 15);
    mark(64); fails=fails+check(t064(), 1020);
    mark(65); fails=fails+check(t065(), 3);
    mark(66); fails=fails+check(t066(), 3);
    mark(67); fails=fails+check(t067(), 9);
    mark(68); fails=fails+check(t068(), 44);
    mark(69); fails=fails+check(t069(), 15);
    mark(70); fails=fails+check(t070(), 16);
    mark(71); fails=fails+check(t071(), 33);
    mark(72); fails=fails+check(t072(), 246);
    mark(73); fails=fails+check(t073(), 12034);
    mark(74); fails=fails+check(t074(), 200340);
    mark(75); fails=fails+check(t075(), 1342);
    mark(76); fails=fails+check(t076(), 1);
    mark(77); fails=fails+check(t077(), 3);
    mark(78); fails=fails+check(t078(), 12);
    mark(79); fails=fails+check(t079(), 22);
    mark(80); fails=fails+check(t080(), 31053);
    mark(81); fails=fails+check(t081(), 3);
    mark(82); fails=fails+check(t082(), 11);
    mark(83); fails=fails+check(t083(), 14);
    mark(84); fails=fails+check(t084(), 20020);
    mark(85); fails=fails+check(t085(), 53);
    mark(86); fails=fails+check(t086(), 3);
    mark(87); fails=fails+check(t087(), 7);
    mark(88); fails=fails+check(t088(), 3);
    mark(89); fails=fails+check(t089(), 220);
    mark(90); fails=fails+check(t090(), 106);
    mark(91); fails=fails+check(t091(), 7);
    mark(92); fails=fails+check(t092(), 3101);
    mark(93); fails=fails+check(t093(), 232);
    mark(94); fails=fails+check(t094(), 3);
    mark(95); fails=fails+check(t095(), 313);
    mark(96); fails=fails+check(t096(), 22);
    mark(97); fails=fails+check(t097(), 12);
    mark(98); fails=fails+check(t098(), 372);
    mark(99); fails=fails+check(t099(), 123);
    mark(100); fails=fails+check(t100(), 163);
    mark(101); fails=fails+check(t101(), 62);
    mark(102); fails=fails+check(t102(), 1);
    mark(103); fails=fails+check(t103(), 43);
    mark(104); fails=fails+check(t104(), 12);

    print_str("done"); putchar(10);
    return fails > 0;
}
