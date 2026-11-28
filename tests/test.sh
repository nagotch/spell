#!/bin/bash

cd "$(dirname "$0")"

make -C ../src
SPELL=../src/spell

tests=(
    "def main() { i = 0; while (i < 1000000) i = i + 1; return 0; }|0"
    "def main() { s = 0; for i in 1000000 { s = s + 1; } return 0; }|0"
    "x = 30; y = 12; return x + y;|42"
    "class P { x: int; def one(self) -> int { return 1; } } def main() { return 42; }|42"
    "class B<T> { n: int; def __init__(self) { self.n = 7; } def get(self) -> int { return self.n; } } def main() { b: B<str> = B(); return b.get(); }|7"
    "class G { a: [int]; def __init__(self) { self.a = []; } def get(self) -> int { x = self.a.len; return x; } } def main() { g = G(); return g.get(); }|0"
    "def main() { print(42); }|0"
    "def main() { putchar(65); }|0"
    "def main() -> int { return 3; }|3"
    # step 60: rax 経路の評価順・位置合わせの回帰テスト
    "g = 0; def f() -> int { g = 100; return 1; } def main() { g = 10; r = g + f(); return r; }|11"
    "def main() { x = 5; y = (x = 9) - x; return y; }|0"
    "def main() { x = 5; y = (x = 9) + x; return y; }|18"
    "def one(v: int) -> int { return v; } def two(a: int, b: int) -> int { return a * 10 + b; } def main() { return two(one(1) + one(2), one(3) + one(4) * one(5)); }|53"
    "def one(v: int) -> int { return v; } def main() { s = 0; i = 0; while (i + one(0) < 5) { s = s + one(i); i = i + 1; } return s; }|10"
    "def plus(a: int, b: int) -> int { return a + b; } def inc(v: int) -> int { return v + 1; } def main() { f = plus; g = inc; return f(g(10), 5); }|16"
    "def add2(a: int, b: int) -> int { return a + b; } def main() { base = 100; f = fn(v: int) -> int { return v + base; }; return f(add2(1, 2)); }|103"
    # step 61: 引数の評価順（リーフな前の引数は、後の引数の代入より先に読まれる）
    "def two(a: int, b: int) -> int { return a * 100 + b; } def main() { x = 1; r = two(x, x = 7); return r; }|107"
    # step 61: 単純な __init__ の埋め込み（引数の入れ替え・同じフィールドへの再代入）
    "class P { x: int; y: int; def __init__(self, a: int, b: int) { self.y = b; self.x = a; } } def main() { p = P(3, 4); return p.x * 10 + p.y; }|34"
    "class P { x: int; def __init__(self, a: int) { self.x = 1; self.x = a; } } def main() { p = P(42); return p.x; }|42"
    "class B<T> { v: T; def __init__(self, v: T) { self.v = v; } def get(self) -> int { return self.v; } } def main() { b: B<int> = B(42); return b.get(); }|42"
    # step 61: 埋め込み対象外の __init__ は従来どおり呼ばれる（式の右辺・副作用・条件分岐）
    "class P { x: int; def __init__(self, a: int) { self.x = a + 1; } } def main() { p = P(41); return p.x; }|42"
    "cnt = 0; def bump() -> int { cnt = cnt + 1; return 7; } class P { x: int; def __init__(self) { self.x = bump(); } } def main() { p = P(); q = P(); return cnt * 10 + p.x + q.x; }|34"
    "class P { x: int; y: int; def __init__(self, c: int) { self.x = 1; if (c > 0) { self.y = 2; } else { self.y = 3; } } } def main() { p = P(1); q = P(0); return p.y * 10 + q.y; }|23"
    # step 61: __init__ を普通のメソッドとして明示的に呼ぶこともできる
    "class P { x: int; def __init__(self, a: int) { self.x = a; } } def main() { p = P(1); p.__init__(5); return p.x; }|5"
    # step 76: クラス名と同名の変数は値として扱い、比較式を構築と誤認しない
    "class Pair { x: int; def __init__(self, x: int) { self.x = x; } } def main() { Pair = 5; if (Pair < 10 > 3) { return 42; } return 0; }|42"
    # step 78: レジスタ名と同名のグローバル変数・関数が壊れない
    "si = 40; def main() { return si + 2; }|42"
    "sp = 1; bp = 2; ax = 3; def main() { return sp + bp + ax + 36; }|42"
    "def si() -> int { return 42; } def main() { return si(); }|42"
    "def r8() -> int { return 40; } def main() { f = r8; return f() + 2; }|42"
    # step 83: 呼ばれない実体化メソッドの本体エラーは報告されない
    # （実体化メソッドの遅延検査）
    "class Wrap<T> { v: T; def __init__(self, v: T) { self.v = v; } def broken(self) -> int { return self.v + \"x\"; } def get(self) -> T { return self.v; } } def main() { w = Wrap<int>(3); return w.get(); }|3"
    # step 79: 後方で定義されるグローバルを関数から読む（前方参照）
    "def f() -> int { return g; } g = 42; def main() { return f(); }|42"
    "def f() -> int { return g + 1; } def main() { return f(); } g = 41;|42"
    # step 79: 関数名を値として使う形は従来どおり
    "def inc(v: int) -> int { return v + 1; } def apply(f: fn(int) -> int, v: int) -> int { return f(v); } def main() { return apply(inc, 41); }|42"
    # step 79: ユーザー定義クラスの __bool__ で条件分岐する
    "class Box { n: int; def __init__(self, n: int) { self.n = n; } def __bool__(self) -> bool { return self.n > 0; } } def main() { a = Box(0); b = Box(3); if (a) { return 1; } if (b) { return 42; } return 0; }|42"
    # step 79: while の条件に Deque を直接書く（空になるまで）
    "def main() { q = Deque(); q.push_back(1); q.push_back(2); q.push_back(3); n = 0; while q { q.pop_front(); n = n + 1; } return n; }|3"
    # step 80: `@?` は while の糖衣（波括弧形・コロン形とも test_stmt で検証）
    "def main() { n = 5; s = 0; @? n > 0 { s = s + n; n = n - 1; } return s; }|15"
    # step 80: `@?` の条件でもユーザー定義 __bool__ が効く
    "class Box { n: int; def __init__(self, n: int) { self.n = n; } def __bool__(self) -> bool { return self.n > 0; } def dec(self) -> int { self.n = self.n - 1; return self.n; } } def main() { b = Box(3); c = 0; @? b { b.dec(); c = c + 1; } return c; }|3"
    # step 81: 安全添え字 a?[i][j]。単独は bool、比較では範囲外が
    # 比較全体を偽にする。step 78 でエラーだった if の形が通る回帰
    "def main() { a = [10, 20]; r = 0; if a?[0] == 10: r = 42; return r; }|42"
    "def main() { a = [1, 2]; if (a?[2]) { return 1; } if (a?[1]) { return 42; } return 0; }|42"
    "def main() { a = [1]; if (a?[5] != 1) { return 1; } if (a?[0] != 2) { return 42; } return 0; }|42"
    "def main() { a = [[[1, 2], [3, 4]], [[5, 6], [7, 8]]]; if (a?[1][0][1] == 6) { return 42; } return 0; }|42"
    "def main() { a = [[[1, 2], [3, 4]], [[5, 6], [7, 8]]]; if (a?[1][2][0] == 7) { return 1; } if (a?[0][1][5] == 4) { return 1; } return 42; }|42"
    "def main() { a = [1]; if (a?[-1] == 1) { return 1; } return 42; }|42"
    "def main() { s = \"abc\"; if (s?[1]) { return 42; } return 0; }|42"
    "def main() { s = \"abc\"; if (s?[3] == \"x\") { return 1; } if (s?[-1] == \"x\") { return 1; } return 42; }|42"
    "def main() { g = [\"ab\", \"cd\"]; c = g[(0, 1)]; if (g?[(1, 0)] && g?[(0, 1)]) { return 42; } return 0; }|42"
    "def main() { g = [\"ab\", \"cd\"]; if (g?[(0, 2)] == \"x\") { return 1; } if (g?[(2, 0)] == \"x\") { return 1; } return 42; }|42"
    "def main() { a = [5]; if (5 == a?[0] && 6 == a?[1]) { return 1; } return 42; }|42"
    "cnt = 0; def idx() -> int { cnt = cnt + 1; return 5; } def main() { a = [1, 2]; r = 0; if (a?[idx()] == 1) { r = 1; } return cnt * 10 + r; }|10"
    "def main() { a = [10, 20]; if (a?[1] > 15 && a?[1] >= 20) { return 42; } return 0; }|42"
    "def main() { a = [10, 20]; if (a?[5] > 1) { return 1; } if (a?[5] >= 0) { return 1; } return 42; }|42"
    # step 85: スライス。対象と境界は1回だけ評価される
    "cnt = 0; def lo() -> int { cnt = cnt + 1; return 1; } def hi() -> int { cnt = cnt + 10; return 4; } def tgt() -> str { cnt = cnt + 100; return \"abcdef\"; } def main() { s = tgt()[lo():hi()]; if (s == \"bcd\") { return cnt; } return 0; }|111"
    # step 85: 入れ子ループの中のスライス（step 82 の教訓の回帰）
    "def main() { n = 0; i = 0; while (i < 3) { j = 0; while (j < 3) { n = n + \"abcdef\"[i:j + 3].len; j = j + 1; } i = i + 1; } return n; }|27"
    # step 85: 注釈つき引数へのスライスと、期待型で対象の型を採る形
    "def f(s: str) -> str { return s[1:3]; } def main() { if (f(\"hello\") == \"el\") { return 42; } return 0; }|42"
    "def f(a) -> int { r: [int] = a[1:3]; return r.len; } def main() { return f([10, 20, 30, 40]) + 40; }|42"
    # step 85: スライスの評価順は対象→下限→上限（Luna レビューの回帰）
    "log = \"\"; def tgt() -> str { log = log + \"T\"; return \"abcdef\"; } def lo2() -> int { log = log + \"L\"; return 1; } def hi2() -> int { log = log + \"H\"; return 4; } def main() { s = tgt()[lo2():hi2()]; if (log == \"TLH\" && s == \"bcd\") { return 42; } return 0; }|42"
    # step 87: 32ビットに収まらない整数リテラルがアセンブラエラーに
    # ならず使えること（代入・比較・配列要素）
    "def main() { x = 2147483648; if (x == 2147483648) { return 42; } return 0; }|42"
    "def main() { if (9223372036854775807 - 9223372036854775806 == 1) { return 42; } return 0; }|42"
    "def main() { a = [3000000000, 5]; return (a[0] + a[1] - 3000000000) + 37; }|42"
    # step 87: 配列リテラルは評価ごとに新しい配列になる（ループ内での
    # 再評価で前に保持した配列が化けない／関数から返しても化けない）
    "def main() { first = [0]; i = 0; while (i < 2) { a = [i]; if (i == 0) { first = a; } i = i + 1; } return first[0] + 42; }|42"
    "def one(x: int) -> [int] { return [x]; } def main() { a = one(11); b = one(22); return a[0] + 31; }|42"
    # step 87: リテラルの要素に関数呼び出しがあっても正しい（要素の
    # 評価の間、先頭ポインタをスタックに退避する形の回帰。呼び出しの
    # アラインメントは emit_call が実行時に rsp&15 で調整する）
    "def f(x: int) -> int { return x * 2; } def main() { a = [f(1), f(2), f(3)]; return a[0] + a[1] + a[2] + 30; }|42"
    "def f(x: int) -> int { return x + 1; } def main() { b = [[f(1)], [f(2)]]; return b[0][0] + b[1][0] + 37; }|42"
    # step 87: append で要素型が確定し、育てたグリッドでも安全添え字の
    # 範囲チェックと Pos 添え字の深さ判定が効く
    "def main() { rows = []; rows.append([7]); if (rows[0]?[5]) { return 1; } if (rows[0]?[0]) { return 42; } return 0; }|42"
    "def main() { d = []; d.append([0, 0]); d.append([0, 0]); d[(1, 1)] = 42; return d[(1, 1)]; }|42"
    # step 87: 複合代入・++ は代入先の添え字・オブジェクト式を1回だけ
    # 評価する（副作用つき添え字と、呼び出し結果へのフィールド代入）
    "cnt = 0; def idx() -> int { cnt = cnt + 1; return cnt - 1; } def main() { a = [10, 20]; a[idx()] += 5; if (a[0] == 15 && a[1] == 20 && cnt == 1) { return 42; } return 0; }|42"
    "class B { v: int; def __init__(self, v: int) { self.v = v; } } boxes: [B] = []; def mk() -> B { b = B(10); boxes.append(b); return b; } def main() { mk().v += 32; return boxes[0].v; }|42"
    # step 87: 矢印の pop 先（代入先になる側）は従来どおり新しい変数の
    # 定義として扱える（値側の検査を締めても壊れないこと）
    "def main() { q = Deque(int); q <- 42; x <- q; return x; }|42"
    "def main() { q = Deque(int); q <- 7; w = 0; q -> w; return w + 35; }|42"
    # step 88: 異型 `==` をエラーにしても、正当な形は壊れないこと。
    # bool と int の比較（実体が同じ 0/1 の int 値）と、型が分からない
    # 値との比較（緩く通す）
    "def main() { b = 1 == 1; if (b == 1 && 1 == b && b == true) { return 42; } return 0; }|42"
    "def g(x) { return x; } def main() { if (g(1) == 1 && g(1) != 2) { return 42; } return 0; }|42"
    # step 88: ジェネリック本体の [T] と [int] の比較は緩く通り、
    # 実体化された時点で具体型として判定される
    "class Box88<T> { v: [T]; def __init__(self, v: [T]) { self.v = v; } def same(self, y: [int]) -> bool { return self.v == y; } } def main() { b = Box88<int>([1, 2]); if (b.same([1, 2])) { return 42; } return 0; }|42"
    # step 88: 定義なしで呼べる外部関数（署名表にあるもの）は従来
    # どおり呼べる
    "def main() { putchar(65); return ret3() + add(2, 3) + 34; }|42"
    # step 91: オーバーロードは引数の型と個数で解決される。bool と
    # int は別型。prelude 関数にも別署名で追加できる
    "def f(x: int) -> int { return 1; } def f(x: str) -> int { return 2; } def main() { return f(1) * 10 + f(\"a\"); }|12"
    "def f(x: int) -> int { return 1; } def f(x: int, y: int) -> int { return 2; } def main() { return f(1) * 10 + f(1, 2); }|12"
    "def f(x: int) -> int { return 1; } def f(x: bool) -> int { return 2; } def main() { b = 1 < 2; return f(0) * 10 + f(b); }|12"
    "class C { def m(self, x: int) -> int { return 1; } def m(self, x: str) -> int { return 2; } } def main() { c = C(); return c.m(1) * 10 + c.m(\"a\"); }|12"
    "def zero_int(x: str) -> int { return 7; } def main() { return zero_int() + zero_int(\"a\"); }|7"
    # step 91: 引数なしの main が入口になり、同名の別署名は共存できる
    "def main(x: int) -> int { return 1; } def main() { return 42; }|42"
    # step 91: 関数型を引数に取るオーバーロード同士でラベルが衝突
    # しないこと（fn 型の表記には引数の個数も入る）
    "def f(g: fn() -> fn(int) -> str) -> int { return 1; } def f(g: fn(fn() -> int) -> str) -> int { return 2; } def main() { a: fn() -> fn(int) -> str = fn() -> fn(int) -> str { return fn(x: int) -> str { return \"a\"; }; }; b: fn(fn() -> int) -> str = fn(g: fn() -> int) -> str { return \"b\"; }; return f(a) * 10 + f(b); }|12"
    # step 92: 可変引数の min / max と abs。ユーザ定義の同名関数が
    # あればそちらが優先される（print と同じ方針）
    "def main() { return min(3, 5) * 10 + max(3, 5) + 7; }|42"
    "def min(a: int, b: int) -> int { return a - b; } def main() { return min(45, 3); }|42"
    "def main() { return abs(0 - 42); }|42"
    # step 93: f-string。埋め込み・連結・後置（添え字・len）が通常の
    # str と同じに効くこと（出力の照合は io テスト側）
    "def main() { x = 40; s = f\"{x + 2}\"; if (s == \"42\") { return 42; } return 0; }|42"
    "def main() { s = f\"a{1}b{2}c\"; if (s.len == 5 && s[4] == \"c\") { return 42; } return 0; }|42"
    "def main() { s = f\"{{}}\"; if (s == \"{}\") { return 42; } return 0; }|42"
    "def main() { s = f\"{f\"{7}\"};\"; if (s == \"7;\") { return 42; } return 0; }|42"
    # step 93: 入れ子の f-string の式部にある波括弧入りの文字列で、
    # 外側の補間の終端を誤認しないこと
    "def main() { s = f\"{f'{'}'}'}\"; if (s == \"}\") { return 42; } return 0; }|42"
    "def main() { s = f\"{f\"{ '}' }\"}\"; if (s == \"}\") { return 42; } return 0; }|42"
    "def main() { s = f\"\" + f\"x\"; t = 'q'; u = f'{t}'; if (s == \"x\" && u == \"q\") { return 42; } return 0; }|42"
    # step 97: `&` / `*` の正当な利用（ディープコピーしたくないときの
    # 逃げ道）。アドレスは変数に保持でき、算術・比較・戻り値推論でも
    # アドレスとして受け渡せる
    "def inc(p) { *p = *p + 1; } def main() { x = 41; inc(&x); return x; }|42"
    "def main() { x = 1; p = &x; *p = 42; return x; }|42"
    "def f(p) { (*p)[0] = 42; } def main() { a = [1, 2, 3]; f(&a); return a[0]; }|42"
    "def set_first(p, v) { *p = v; } def main() { a = [1, 2, 3]; set_first(&a[0], 42); return a[0]; }|42"
    "def main() { a = [1, 2, 3]; p = &a[0]; p = p + 8; *p = 42; return a[1]; }|42"
    "g = 7; def get() { return &g; } def main() { p = get(); *p = 42; return g; }|42"
    "def main() { a = [1, 2, 3]; if (&a[0] < &a[1] && &a[0] == &a[0]) { return 42; } return 0; }|42"
    "class Pt { x: int; def __init__(self, x: int) { self.x = x; } } def main() { pt = Pt(1); p = &pt.x; *p = 42; return pt.x; }|42"
)

error_tests=(
    "class I { def __has_next__(self) -> bool { return 1 == 1; } def __next__(self, x: int) -> int { return x; } } class B { def __iter__(self) -> I { return I(); } } def main() { for i in B() { } return 0; }|\`__next__\`は引数を取れません"
    "class Q { a: int; def __init__(self, a: int) { self.a = a; } } def main() { for i in Q(1) { } return 0; }|型の値は反復できません"
    "def main() { a = [1,2,3]; return a.5; }|数値\`5\`が指定されています"
    "def main() { return 1; } /*|コメントを閉じる"
    "def main() { return #; }|ここでは文字\`#\`を使えません"
    "def main() { return (1 + 2; }|\`)\`が必要ですが、\`;\`が見つかりました"
    "def main() { return }; }|数値が必要ですが、\`}\`が見つかりました"
    "def main() { s = \"abc; return 1; }|文字列を閉じる"
    "def main() { s = 'abc; return 1; }|文字列を閉じる"
    "def main() { x: int = 1 < 2; return 0; }|が代入されています"
    "def main() { x = 1; x = 1 < 2; return 0; }|が代入されています"
    "def main() { x: foo = 1; return 0; }|は定義されていません"
    "def main() { (1 + 2): int = 3; return 0; }|型注釈は変数にだけ付けられます"
    "def main() { a, b = 1; return 0; }|個数が一致しません"
    "def main() { s = in_line[3]; return 0; }|に添え字は付けられません"
    "def main() { a = [1, 1 < 2]; return 0; }|が含まれています"
    "def main() { x = 5; return x[0]; }|添え字は使えません"
    "def main() { a = [1, 2]; return a[1 < 2]; }|添え字には\`int\`が必要ですが"
    "def main() { a = [1, 2]; a[0] = 1 < 2; return 0; }|配列の要素は\`int\`ですが"
    "def main() { a = [0; 1 < 2]; return 0; }|長さには\`int\`が必要ですが"
    "def add(x: int, y: int) -> int { return x + y; } def main() { return add(1, 1 < 2); }|引数には\`int\`が必要ですが"
    "def add(x: int, y: int) -> int { return x + y; } def main() { return add(1); }|引数は2個ですが"
    "def f() -> int { return 1 < 2; } def main() { return 0; }|戻り値は\`int\`ですが"
    "def f() { if (1) { return 1; } return 1 < 2; } def main() { return 0; }|戻り値は\`int\`ですが"
    "class P { x: int; x: int; } def main() { return 0; }|フィールド\`x\`は既に定義されています"
    "class P { x: int; } def f(p: P) -> int { return 1; } def main() { return f(0); }|引数には\`P\`が必要ですが"
    "class P { x: int; } def main() { v: P = 1; return 0; }|変数\`v\`の型は\`P\`ですが"
    "def main() { x = 1 y = 2; return 0; }|文の終わりには\`;\`か改行が必要です"
    "class P { x: int y: int; } def main() { return 0; }|フィールドの終わりには\`;\`か改行が必要です"
    "class P { def f() { return 1; } } def main() { return 0; }|最初の引数\`self\`が必要です"
    "class P { x: int; } def f(p: P) { return p.y; } def main() { return 0; }|フィールド\`y\`は定義されていません"
    "def main() { x = 5; return x.foo; }|クラス\`int\`にフィールド\`foo\`は定義されていません"
    "class P { x: int; def __init__(self, x: int) { } } def main() { p = P(1, 2); return 0; }|の\`__init__\`の引数は1個ですが、2個が指定されています"
    "class P { x: int; def __init__(self, x: int) { } } def main() { p = P(1 < 2); return 0; }|の\`__init__\`の引数には\`int\`が必要ですが、\`bool\`が指定されています"
    "def f() { c: bool = getchar(); return 1; } def main() { return 0; }|の型は\`bool\`ですが、\`int\`が代入されています"
    "def main() { x = 5; return x.foo(); }|クラス\`int\`にメソッド\`foo\`は定義されていません"
    "class P { x: int; } def f(p: P) { return p.m(); } def main() { return 0; }|メソッド\`m\`は定義されていません"
    "class P { x: int; def add(self, a: int) -> int { return self.x + a; } } def f(p: P) { return p.add(1, 2); } def main() { return 0; }|メソッド\`add\`の引数は1個ですが、2個が指定されています"
    "class P { x: int; def add(self, a: int) -> int { return self.x + a; } } def f(p: P) { return p.add(1 < 2); } def main() { return 0; }|メソッド\`add\`の引数には\`int\`が必要ですが、\`bool\`が指定されています"
    "def m(a: int, b: int, c: int, d: int, e: int, f: int, g: int) -> int { return a; } def main() { return 0; }|引数は6個までですが、7個が指定されています"
    "def main() { a = [1,2,3]; return a.len(1); }|\`len\`の引数は0個ですが、1個が指定されています"
    "class P { x: int; def add(self, a: int) -> int { return self.x + a; } } def f(p: P) { return p.add; } def main() { return 0; }|メソッド\`add\`には引数が必要ですが、括弧なしで使われています"
    "def main() { a = [1,2,3]; a.len = 5; return 0; }|\`len\`には代入できません"
    "class P { x: int; def get(self) -> int { return self.x; } } def f(p: P) { p.get = 5; return 1; } def main() { return 0; }|メソッド\`get\`には代入できません"
    "class P { x: int; y: int; def __init__(self) { self.x = 1; } } def main() { p = P(); return p.x; }|フィールド\`y\`が初期化されていません"
    "class P { x: int; y: int; def __init__(self, c: int) { self.x = 1; if (c) { self.y = 2; } } } def main() { p = P(1); return p.x; }|フィールド\`y\`が初期化されていません"
    "class P { x: int; y: int; def __init__(self, c: int) { self.x = 1; while (c) { self.y = 2; c = 0; } } } def main() { p = P(1); return p.x; }|フィールド\`y\`が初期化されていません"
    "class P { x: int; def __init__(self, x: int) { self.x = x; } } def main() { p = P(1); q = P(2); r = p + q; return 0; }|演算子\`+\`は定義されていません"
    "def main() { return 1 + (1 < 2); }|演算子\`+\`は使えません"
    "def main() { a = [1]; b = a + 1; return 0; }|型の値に演算子\`+\`は使えません"
    "class P { x: int; def __init__(self, x: int) { self.x = x; } } def main() { p = P(1); q = P(2); if (p < q) { return 1; } return 0; }|演算子\`<\`は定義されていません"
    "class P { x: int; def __init__(self, x: int) { self.x = x; } } def main() { p = P(1); return p[0]; }|型の値に添え字は使えません"
    "class Arr { a: [int]; def __init__(self) { self.a = [0; 3]; } def __getitem__(self, i: int) -> int { return self.a[i]; } } def main() { arr = Arr(); arr[0] = 1; return 0; }|\`__setitem__\`は定義されていません"
    "def main() { if ([1] < [2]) { return 1; } return 0; }|型の値に演算子\`<\`は使えません"
    "def main() { a = [1]; b = a - a; return 0; }|型の値に演算子\`-\`は使えません"
    "def main() { s = \"abc\"; s[0] = 65; return 0; }|クラス\`str\`に\`__setitem__\`は定義されていません"
    "class Q { x: int; } def main() { print(Q()); return 0; }|型の値は print できません"
    "def main() { x = int(42); return 0; }|はクラスではありません"
    "def main() { print(1, 2); return 0; }|引数は1個ですが、2個が指定されています"
    "class P<T> { a: T; } def main() { p = P(); return 0; }|型引数を推論できません"
    "class P<A, B, C, D, E> { x: int; } def main() { return 0; }|型パラメータは4個までですが、5個が指定されています"
    "class P<A, B> { x: A; y: B; def __init__(self, x: A, y: B) { self.x = x; self.y = y; } } def main() { p: P<int> = P(1, 2); return 0; }|型引数は2個ですが、1個が指定されています"
    "def f(p: P<int, int, int, int, int>) -> int { return 1; } class P<A, B, C, D, E> { x: int; } def main() { return 0; }|型パラメータは4個までですが、5個が指定されています"
    "class B1<T> { v: T; def __init__(self, v: T) { self.v = v; } } class B2<T> { v: T; def __init__(self, v: T) { self.v = v; } } class W<T> { v: B1<T>; def __init__(self, v: B1<T>) { self.v = v; } } def main() { w = W(B2(5)); return 0; }|型引数を推論できません"
    "class P { x: int; def __init__(self, x: int) { self.x = x; } } def main() { d: Dict<P, int> = Dict(zero_int); d[P(1)] = 1; return 0; }|メソッド\`__hash__\`は定義されていません"
    "def main() { d = Dict(zero_int); return 0; }|型引数を推論できません"
    "def main() { g = x -> x; return 0; }|ラムダの引数の型が分かりません"
    "def id_str(s: str) -> str { return s; } def main() { f: fn(int) -> int = id_str; return 0; }|fn(int) -> int"
    "def main() { x = 5; return x(1); }|型の値は呼び出せません"
    "def twice(x: int) -> int { return x * 2; } def main() { twice += 1; return 0; }|fn(int) -> int\`型と"
    "def main() { for i in 0..3 { i = 1; } return 0; }|書き戻されないため、代入できません"
    "def main() { x = __spell_argc(1); return 0; }|引数は0個ですが、1個が指定されています"
    "def main() { x = __spell_read_file_chars(\"s\"); return 0; }|ですが、\`str\`が指定されています"
    # step 62: >> の末尾カンマは対象が必要というエラーになる
    "def main() { >> a,; return 0; }|数値が必要ですが、\`;\`が見つかりました"
    # step 65: append の対象はスロットを持つ場所だけ。一時値は書き戻せない
    "def main() { a = [1, 2]; (a + a).append(1); return 0; }|の対象は変数・フィールド・配列の要素でなければなりません"
    # step 67: dedent がどの段にも合わない場合はエラー
    "def main():
    if (1 < 2):
        x = 1
      y = 2
    return 0
|インデントが揃っていません"
    # step 67: dedent が外側の段にも合わないままトップレベルへ
    # 吸収される形もエラー（Luna レビュー指摘）
    "def f():
    x = 1
  y = 2
def main():
    return 0
|インデントが揃っていません"
    # step 67: 空の suite はエラー
    "def main():|の後に文がありません"
    # step 67: suite は頭の行より深くインデントしなければならない
    "def main():
return 0
|インデントされたブロックが必要です"
    # step 67: 空のクラス suite はエラー
    "class P:
def main():
    return 0
|インデントされたブロックが必要です"
    "def f() -> [int] { return [1]; } def main() { f().append(1); return 0; }|の対象は変数・フィールド・配列の要素でなければなりません"
    "def main() { a = [1]; a.append(1, 2); return 0; }|\`append\`の引数は1個ですが、2個が指定されています"
    "def main() { a = [1]; a.append(1 < 2); return 0; }|\`append\`の引数には\`int\`が必要ですが、\`bool\`が指定されています"
    # step 71: 文頭 `<` の出力糖衣は削除された（`<<` は残る）
    "def main() { < \"hi\"; return 0; }|数値が必要ですが、\`<\`が見つかりました"
    # step 66: 矢印は dunder プロトコルへの書き換え。持たない型には使えない
    "class P { x: int; def __init__(self, x: int) { self.x = x; } } def main() { p = P(1); p <- 1; return 0; }|値に演算子\`<-\`は使えません"
    "def main() { v: Vec<int> = Vec(zero_int); v <- 1; return 0; }|値に演算子\`<-\`は使えません"
    "def main() { q: Deque<int> = Deque(zero_int); a = 1; a <- q <- 2; return 0; }|矢印演算子は連鎖できません"
    "def main() { q: Deque<int> = Deque(zero_int); q <- 1; (1 + 2) <- q; return 0; }|代入先は変数・フィールド・配列の要素でなければなりません"
    # step 70: 真理値リテラルには代入できない
    "def main() { true = 1; return 0; }|\`true\`には代入できません"
    "def main() { T = 0; return 0; }|\`T\`には代入できません"
    # step 73: `?` は文頭の if 糖衣専用で、式の中では使えない
    "def main() { x = true ? 1 : 2; return 0; }|文の終わりには\`;\`か改行が必要ですが"
    # step 76: 構築時の型引数の個数が合わない
    "def main() { q = Deque<int, int>(); return 0; }|型引数は1個ですが、2個が指定されています"
    "class Pair<A, B> { a: A; b: B; def __init__(self, a: A, b: B) { self.a = a; self.b = b; } } def main() { p = Pair<int>(1); return 0; }|型引数は2個ですが、1個が指定されています"
    # step 76: 明示的な型引数が期待型（注釈）と矛盾する
    "def main() { q: Deque<int> = Deque<str>(); return 0; }|と矛盾します"
    # step 76: 要素型がクラスの引数なし構築は、ファクトリを渡すよう促す
    "class P { x: int; def __init__(self, x: int) { self.x = x; } } def main() { q = Deque<P>(); return 0; }|要素型がクラスの場合はファクトリを渡してください"
    # step 76: 要素型が配列の引数なし構築も、既定値を自動で作れない
    "def main() { v = Vec<[int]>(); return 0; }|既定値は自動で作れないため、ファクトリを渡してください"
    # step 77: Pos リテラルの要素は2個か3個だけ
    "def main() { x = (1, 2, 3, 4); return 0; }|Pos リテラルの要素は2個か3個ですが"
    # step 77: a[p] の次元は配列の入れ子の深さと一致しなければならない
    "def main() { a = [[1, 2], [3, 4]]; print(a[(1, 0, 1)]); return 0; }|は3次元ですが、配列の入れ子は2段です"
    "def main() { a = [1, 2]; print(a[(0, 1)]); return 0; }|は2次元ですが、配列の入れ子は1段です"
    # step 77: ゼロ化できない要素型への型渡しは、ファクトリを促すエラー
    "class Wrap { f: fn() -> int; def __init__(self, f: fn() -> int) { self.f = f; } } def main() { q = Deque(Wrap); return 0; }|ゼロ値は自動で作れないため、ファクトリを渡してください"
    # step 78: 条件の直後の `?` は、静かに飲み込まないよう位置つき
    # エラーにする（if / while / elif）。step 81 で安全添え字
    # `a?[0] == 10` が有効な形になったため、ここでは `[` を伴わない
    # `?`（a? == 10）で拒否そのものを固定する
    "def main() { a = [10, 20]; if a? == 10: print(1); return 0; }|条件の直後に\`?\`は書けません"
    "def main() { a = [10, 20]; while a? == 10: print(1); return 0; }|条件の直後に\`?\`は書けません"
    # step 80: `@?` の条件直後の `?` も同じくエラー
    "def main() { a = [10, 20]; @? a? == 10: print(1); return 0; }|条件の直後に\`?\`は書けません"
    "def main() { a = [10, 20]; if a[0] == 1: print(1); elif a? == 10: print(2); return 0; }|条件の直後に\`?\`は書けません"
    # step 79: 未定義変数の読み取りはエラー
    "def main() { return j; }|変数\`j\`は定義されていません"
    "def main() { x = y + 1; return x; }|変数\`y\`は定義されていません"
    # step 79: タイポ候補が1つだけ突出していればヒントを出す
    "def main() { count = 1; return cuont; }|もしかして \`count\` ですか"
    # step 79: __bool__ の無いクラスは条件に使えない
    "class P { x: int; def __init__(self, x: int) { self.x = x; } } def main() { p = P(1); if (p) { return 1; } return 0; }|条件に使えません"
    "def main() { p = (1, 2); if (p) { return 1; } return 0; }|条件に使えません"
    "class P { x: int; def __init__(self, x: int) { self.x = x; } } def main() { p = P(1); if (!p) { return 1; } return 0; }|条件に使えません"
    # step 83: 要素型に __lt__ が無い配列の sort は実体化時の型エラー
    "class NoLt { v: int; def __init__(self, v: int) { self.v = v; } def __add__(self, o: NoLt) -> NoLt { return NoLt(self.v + o.v); } def __eq__(self, o: NoLt) -> bool { return self.v == o.v; } def __ne__(self, o: NoLt) -> bool { return !(self == o); } def __hash__(self) -> int { return self.v; } def __str__(self) -> str { return self.v.__str__(); } } def main() { a = [NoLt(2), NoLt(1)]; a.sort(); return 0; }|クラス\`NoLt\`に演算子\`<\`は定義されていません"
    # step 83: 構築された実体化クラスの __init__ 本体は検査される
    # （遅延検査でも構築経由の呼び出しは拾う）
    "class Box<T> { v: T; def __init__(self, v: T) { self.v = v + \"x\"; } } def main() { b = Box<int>(3); return 0; }|演算子\`+\`は使えません"
    # step 83: 要素型に __add__ が無い配列の sum は実体化時の型エラー
    "class NoAdd { v: int; def __init__(self, v: int) { self.v = v; } def __lt__(self, o: NoAdd) -> bool { return self.v < o.v; } def __le__(self, o: NoAdd) -> bool { return self.v <= o.v; } def __eq__(self, o: NoAdd) -> bool { return self.v == o.v; } def __ne__(self, o: NoAdd) -> bool { return !(self == o); } def __hash__(self) -> int { return self.v; } def __str__(self) -> str { return self.v.__str__(); } } def main() { a = [NoAdd(2), NoAdd(1)]; print(a.sum()); return 0; }|クラス\`NoAdd\`に演算子\`+\`は定義されていません"
    # step 81: 安全添え字は読み取り専用。代入・複合代入はエラー
    "def main() { a = [10, 20]; a?[0] = 5; return 0; }|安全添え字には代入できません"
    "def main() { a = [10, 20]; a?[0] += 5; return 0; }|安全添え字には代入できません"
    # step 81: Dict は安全添え字の対象外（長さの契約が無いため）
    "def main() { d: Dict<str, int> = Dict(zero_int); if (d?[\"a\"] == 1) { return 1; } return 0; }|安全添え字は配列と文字列にだけ使えます"
    # step 81: 配列でも文字列でもない値には安全添え字は使えない
    "def main() { x = 5; if (x?[0] == 5) { return 1; } return 0; }|型の値に安全添え字は使えません"
    # step 81: 連鎖比較の中では安全添え字は使えない
    "def main() { a = [1, 2]; if (0 < a?[0] < 3) { return 1; } return 0; }|連鎖比較の中では安全添え字は使えません"
    "def main() { a = [1, 2]; if (0 < 1 < a?[0]) { return 1; } return 0; }|連鎖比較の中では安全添え字は使えません"
    # step 81: Pos の次元と深さの不一致（安全形・通常形とも）
    "def main() { a = [1, 2]; if (a?[(0, 1)] == 1) { return 1; } return 0; }|は2次元ですが、配列の入れ子は1段です"
    "def main() { g = [\"ab\"]; print(g[(0, 1, 2)]); return 0; }|は3次元ですが、配列の入れ子は2段です"
    # step 81: for のヘッダの直後の `?` は入れ子の if として
    # 飲み込まずエラーにする（step 78 の残件）
    "def main() { a = [1, 2]; x@a ? x == 1: print(1); return 0; }|for のヘッダの直後に\`?\`は書けません"
    "def main() { a = [1, 2]; for x in a ? x == 1 { print(1); } return 0; }|for のヘッダの直後に\`?\`は書けません"
    "def main() { a = [1, 2]; @a ? print(1); return 0; }|for のヘッダの直後に\`?\`は書けません"
    # step 81: 安全添え字の反復は bool を回すことになり型エラー
    "def main() { a = [1, 2]; x@a?[0] { print(x); } return 0; }|型の値は反復できません"
    # step 85: スライスは読み取り専用（代入・複合代入・複数代入）
    "def main() { s = \"abc\"; s[0:2] = \"x\"; return 0; }|スライスには代入できません"
    "def main() { a = [1, 2, 3]; a[0:2] += [9]; return 0; }|スライスには代入できません"
    "def main() { a = [1, 2, 3]; a[0:2], x = [9], 5; return 0; }|スライスには代入できません"
    # step 85: 安全添え字の中ではスライスは使えない
    "def main() { a = [1, 2, 3]; if (a?[0:2]) { return 1; } return 0; }|安全添え字の中ではスライスは使えません"
    # step 85: スライスにステップは無い
    "def main() { a = [1, 2, 3]; b = a[0:2:1]; return 0; }|スライスにステップは指定できません"
    # step 85: スライスの対象は配列と文字列だけ
    "def main() { x = 5; y = x[0:1]; return 0; }|スライスは配列と文字列にだけ使えます"
    # step 85: 対象の型が分からない場合はエラー（注釈で明示する）
    "def f(s) { return s[0:2]; } def main() { print(f(\"abc\")); return 0; }|スライスの対象の型が分かりません"
    # step 85: 境界は int でなければならない
    "def main() { s = \"abc\"; t = s[\"a\":2]; return 0; }|スライスの境界には\`int\`が必要です"
    # step 85: スライスの戻り型は要素型で具体化される（Luna レビューの回帰）
    "def main() { a = [1, 2, 3]; b: [str] = a[0:2]; return 0; }|変数\`b\`の型は\`\\[str\\]\`ですが、\`\\[int\\]\`が代入されています"
    # step 85（追加決定）: 長さがリテラルの負数ならコンパイルエラー。
    # 定数式（0 - 2 の形）も含む。変数経由の負数は実行時に 0 に
    # 丸められる（t107 が固定）
    "def main() { a = [5; -2]; return 0; }|長さに負の数は指定できません"
    "def main() { a = [5; 0 - 2]; return 0; }|長さに負の数は指定できません"
    "def main() { a = [5; 9223372036854775807 + 1]; return 0; }|長さに負の数は指定できません"
    # step 85（追加決定）: 個数形の個数がリテラルの負数でもエラー
    "def main() { >> a[-2]; return 0; }|個数に負の数は指定できません"
    "def main() { >> a[2][-1]; return 0; }|個数に負の数は指定できません"
    "def main() { >>> s[-1]; return 0; }|個数に負の数は指定できません"
    # step 87: 名前の無いパラメータ／パラメータリスト途中の EOF は
    # コンパイラが落ちず位置つきエラーになる
    "def f(x: int, : str) -> bool { return true; } def main() { return 0; }|引数名が必要ですが"
    "def main (|引数名が必要ですが"
    "def main() { f = fn(: int) -> int { return 1; }; return f(0); }|引数名が必要ですが"
    # step 87: append で要素型が確定するため、宣言済みの戻り値型と
    # 違う要素を append して返すと検査で止まる。確定後の型違いの
    # append もエラーになる
    "def ints() -> [int] { a = []; a.append(\"wrong\"); return a; } def main() { return 0; }|戻り値は"
    "def main() { a = []; a.append(1); a.append(\"s\"); return 0; }|append\`の引数には\`int\`が必要です"
    # step 87: 矢印 push の値側でも未定義変数の検査が効く
    "def main() { q = Deque(int); q <- misspelled; return 0; }|変数\`misspelled\`は定義されていません"
    # step 88: 未定義の関数呼び出しはリンクエラーに漏らさず、位置つき
    # のコンパイルエラーにする。typo の候補も出す（step 92 で min は
    # 組み込みになったため、例は minimum に差し替え）
    "def main() { x: int = minimum(3, 5); return x; }|関数\`minimum\`は定義されていません"
    "def main() { x = getchr(); return x; }|関数\`getchr\`は定義されていません ヒント: もしかして \`getchar\` ですか"
    "def main() { nosuchfunc(); return 0; }|関数\`nosuchfunc\`は定義されていません"
    # step 88: 外部関数（署名表）も引数の個数と型を照合する
    "def main() { putchar(1, 2); return 0; }|関数\`putchar\`の引数は1個ですが"
    "def main() { fflush(\"x\"); return 0; }|関数\`fflush\`の引数には\`int\`が必要ですが"
    # step 88: 型が分からないレシーバへのメソッド・フィールドは、
    # コンパイラが落ちず位置つきエラーになる
    "def main() { y: int = nosuchfunc().m(); return y; }|関数\`nosuchfunc\`は定義されていません"
    "def f(x) { return x.m(); } def main() { y: int = f(1); return y; }|型が分からない値にメソッド\`m\`は使えません"
    "def f(x) { return x.field; } def main() { y: int = f(1); return y; }|型が分からない値にフィールド\`field\`は使えません"
    # step 88: 型が分からない値と配列リテラルの `==` でコンパイラが
    # 落ちない（未定義関数のエラーが先に出る）
    "def main() { print(nosuchfunc() == [1, 2]); return 0; }|関数\`nosuchfunc\`は定義されていません"
    # step 88: 異型同士の `==` / `!=` は無言で false にならずエラー
    "def main() { b = 1 == \"a\"; return 0; }|\`int\`型と\`str\`型の値に演算子\`==\`は使えません"
    "def main() { s = \"abc\"; b = s.chars[0] == \"a\"; return 0; }|\`int\`型と\`str\`型の値に演算子\`==\`は使えません"
    "def main() { b = [1, 2] == 3; return 0; }|型と\`int\`型の値に演算子\`==\`は使えません"
    "def main() { b = 3 != [1, 2]; return 0; }|\`int\`型と\`\[int\]\`型の値に演算子\`!=\`は使えません"
    "def main() { s = \"abc\"; b = s == 1; return 0; }|メソッド\`__eq__\`の引数には\`str\`が必要ですが、\`int\`が指定されています"
    "class A88 { v: int; def __init__(self, v: int) { self.v = v; } } class B88 { v: int; def __init__(self, v: int) { self.v = v; } } def main() { b = A88(1) == B88(1); return 0; }|\`A88\`型と\`B88\`型の値に演算子\`==\`は使えません"
    "class A88 { v: int; def __init__(self, v: int) { self.v = v; } } def main() { b = A88(1) == 1; return 0; }|\`A88\`型と\`int\`型の値に演算子\`==\`は使えません"
    "class A88 { v: int; def __init__(self, v: int) { self.v = v; } } def main() { b = A88(1) != \"x\"; return 0; }|\`A88\`型と\`str\`型の値に演算子\`!=\`は使えません"
    # step 88: 連鎖比較の中でも異型の `==` はエラーになる
    "def main() { b = 1 < 2 == \"a\"; return 0; }|\`bool\`型と\`str\`型の値に演算子\`==\`は使えません"
    # step 90: 型が分からない対象への安全添え字は、範囲チェックなしで
    # 通さず位置つきエラーにする（単独形と比較形の両方）
    "def g(x) { b = x?[0]; return 0; } def main() { return g([1, 2]); }|型が分からない値に安全添え字は使えません"
    "def g(x) { if (x?[0] == 1) { return 1; } return 0; } def main() { return g([1, 2]); }|型が分からない値に安全添え字は使えません"
    # step 90: 未知のエスケープと、不正な `\x` は位置つきエラー
    "def main() { s = \"a\\qb\"; return 0; }|文字列ではエスケープ"
    "def main() { s = \"\\xzz\"; return 0; }|16進数2桁が必要です"
    "def main() { s = \"\\x4\"; return 0; }|16進数2桁が必要です"
    "def main() { s = \"\\x\"; return 0; }|16進数2桁が必要です"
    # step 90: 文字列の繰り返しは str * int の向きだけ。逆向きは
    # 反射演算子の仕組みが無いため異型エラーになる
    "def main() { s = 3 * \"ab\"; return 0; }|\`int\`型と\`str\`型の値に演算子\`\*\`は使えません"
    # step 91: 同じ名前・同じ引数シグネチャの再定義はエラー（関数・
    # メソッド・クラス。戻り値型だけの違いも再定義）
    "def f(x: int) -> int { return 1; } def f(x: int) -> int { return 2; } def main() { return f(1); }|関数\`f(int)\`は既に定義されています"
    "def f(x: int) -> int { return 1; } def f(x: int) -> str { return \"a\"; } def main() { return f(1); }|関数\`f(int)\`は既に定義されています"
    "class C { def m(self, x: int) -> int { return 1; } def m(self, x: int) -> int { return 2; } } def main() { return 0; }|メソッド\`m(int)\`は既に定義されています"
    "class C { x: int; } class C { y: int; } def main() { return 0; }|クラス\`C\`は既に定義されています"
    "def zero_int() -> int { return 5; } def main() { return zero_int(); }|関数\`zero_int()\`は既に定義されています"
    # step 91: 曖昧な呼び出しと、合う候補がない呼び出しはエラー
    "def f(x: int) -> int { return 1; } def f(x: str) -> int { return 2; } def g(v) -> int { return f(v); } def main() { return g(1); }|呼び出しが曖昧です"
    "def f(x: int) -> int { return 1; } def f(x: str) -> int { return 2; } def main() { return f(true); }|合うオーバーロードがありません"
    "def f(x: int) -> int { return 1; } def f(x: str) -> int { return 2; } def main() { g = f; return 0; }|型注釈でシグネチャを指定してください"
    # step 91: ジェネリックの型引数を当てはめると同じ署名になる
    # メソッドの組は、実体化の時点でエラー（アセンブラエラーにしない）
    "class Box<T> { def store(self, v: T) -> int { return 1; } def store(self, v: int) -> int { return 2; } } def main() { b: Box<int> = Box(); return b.store(3); }|型引数を当てはめると同じ引数を持つ定義と重なります"
    # step 91: 定義側の上限（6個）を超える引数の呼び出しは、個数の
    # 合う候補なしとしてエラーになる（配列外アクセスをしない）
    "def f(a: int, b: int, c: int, d: int, e: int, g: int) -> int { return 1; } def f(a: int, b: int, c: int, d: int, e: int, g: str) -> int { return 2; } def main() { return f(1, 2, 3, 4, 5, 6, 7); }|合うオーバーロードがありません"
    # step 92: min / max は1個以上。0個は位置つきエラー
    "def main() { x = min(); return 0; }|関数\`min\`の引数は1個以上ですが"
    "def main() { x = max(); return 0; }|関数\`max\`の引数は1個以上ですが"
    # step 92: 引数の型が揃わなければエラー（配列リテラルの要素型の
    # 統一検査がそのまま効く）。`__lt__` の無い型同士もエラー
    "def main() { x = min(1, \"a\"); return 0; }|配列の要素は\`int\`ですが、\`str\`が含まれています"
    "def main() { b = min(true, false); return 0; }|クラス\`bool\`に演算子\`<\`は定義されていません"
    # step 92: abs は int 用（prelude 関数なので通常の引数照合が効く）
    "def main() { x = abs(\"a\"); return 0; }|関数\`abs\`の引数には\`int\`が必要ですが"
    # step 93: f-string の構造エラーは全て位置つきエラー
    "def main() { s = f\"{}\"; return 0; }|f-string の\`{}\`には式が必要です"
    "def main() { s = f\"a}b\"; return 0; }|対応する\`{\`がありません"
    "def main() { x = 1; s = f'{x'; return 0; }|f-string の\`{\`を閉じる\`}\`がありません"
    "def main() { x = 1; s = f\"{x\"; return 0; }|f-string の\`{\`を閉じる\`}\`がありません"
    "def main() { s = f\"{fn(x: int) -> int { return x; }(3)}\"; return 0; }|式の中では\`{\`は使えません"
    "def main() { x = 1; s = f\"{x // y}\"; return 0; }|コメントは書けません"
    # step 93: 書式指定は未対応。式の後に残る `:` で検出する
    "def main() { x = 1; s = f\"{x:03d}\"; return 0; }|書式指定（\`:\`以降）は未対応です"
    "def main() { x = 1; s = f\"{x: int}\"; return 0; }|書式指定（\`:\`以降）は未対応です"
    # step 93: 式の後に余分なものがあればエラー
    "def main() { x = 1; y = 2; s = f\"{x y}\"; return 0; }|式の後に"
    # step 93: 埋め込めるのは print と同じく `__str__` を持つ型だけ
    "def g(x) { return f\"{x}\"; } def main() { return 0; }|型が分からない値は f-string に埋め込めません"
    "class P { x: int; def __init__(self, x: int) { self.x = x; } } def main() { p = P(1); s = f\"{p}\"; return 0; }|型の値は f-string に埋め込めません"
    # step 95: 内包表記（for 節は1つだけ、ループ変数への代入は禁止、
    # 反復できない対象は for 文と同じ診断、結果は一時値なので
    # append の対象にはならない）
    "def main() { a = [1, 2]; b = [x for x in a for y in a]; return 0; }|内包表記の\`for\`節は1つだけです"
    "def main() { a = [1, 2]; b = [(x = 5) for x in a]; return 0; }|内包表記の中ではループ変数\`x\`へ代入できません"
    "def main() { a = [1, 2]; b = [x for x in a if (x = 2) == 2]; return 0; }|内包表記の中ではループ変数\`x\`へ代入できません"
    "def main() { a = [1, 2]; b = [x for x a]; return 0; }|内包表記の\`in\`が必要ですが"
    "def main() { p = Pos2(1, 2); b = [v for v in p]; return 0; }|\`Pos2\`型の値は反復できません"
    "def main() { a = [1, 2]; b = [x x@a].append(9); return 0; }|\`append\`の対象は変数・フィールド・配列の要素でなければなりません"
    # step 97: 型が確定していてアドレスでない値への `*` は位置つき
    # エラー（fill の個数に `* h` と書く元の事故の形を含む）
    "def main() { h = 3; w = 2; d = [[-1; w]; * h]; return 0; }|\`*\`にはアドレスが必要ですが、\`int\`が指定されています"
    "def main() { a = [1, 2]; x = *a; return 0; }|\`*\`にはアドレスが必要ですが、\`\[int\]\`が指定されています"
    "def main() { s = \"a\"; x = *s; return 0; }|\`*\`にはアドレスが必要ですが、\`str\`が指定されています"
    "def main() { b = true; x = *b; return 0; }|\`*\`にはアドレスが必要ですが、\`bool\`が指定されています"
    # step 97: 注釈で int と決まった変数はアドレスを入れても int と
    # して扱い、その後の `*` はエラーで一貫させる
    "def main() { y = 3; x: int = &y; return *x; }|\`*\`にはアドレスが必要ですが、\`int\`が指定されています"
    # step 97: `&` の対象が左辺値でない式は位置つきエラー
    # （以前は codegen の内部エラーになっていた）
    "def main() { x = &(1 + 2); return 0; }|\`&\`の対象は変数・要素・フィールドでなければなりません"
)

cc -c -o funcs.o funcs.c

one_test() {
    IFS='|' read -r input expected <<< "${tests[$1]}"
    i=$(printf '%03d' $1)
    printf '%s' "$input" > tmp$i.sp
    "$SPELL" -S -o tmp$i tmp$i.sp
    cc -o tmp$i tmp$i.s funcs.o
    ./tmp$i > /dev/null
    exit_code=$?
    if [ "$exit_code" != "$expected" ]; then
        echo $i test failed, input $input, expected $expected, result $exit_code. > result$i
    else
        echo $i test passed. > result$i
    fi
    rm tmp$i tmp$i.s tmp$i.sp
}

one_error_test() {
    IFS='|' read -r input expected <<< "${error_tests[$1]}"
    printf '%s' "$input" > etmp.sp
    "$SPELL" -S -o etmp etmp.sp 2> etmp.err
    exit_code=$?
    if [ "$exit_code" = "1" ] && grep -q "$expected" etmp.err; then
        echo "error test passed, input $input" > /dev/null
    else
        echo "erorr test failed, input $input, exit $exit_code."
        fail_count=$((fail_count+1))
    fi
    n=$((n+1))
    rm -rf -f etmp.sp etmp.err etmp.s
}

run_suite() {
    cp $1 suite_all.sp
    "$SPELL" -S -o suite suite_all.sp
    cc -o suite suite.s funcs.o
    ./suite > suite.out
    exit_code=$?
    if [ "$exit_code" != "0" ] || ! grep -q done suite.out; then
        echo "suite $1 failed"
        cat suite.out
        fail_count=$((fail_count+1))
    fi
    n=$((n+1))
    rm -f suite_all.sp suite suite.s suite.out
}

run_selfcheck() {
    cp test_selfcheck.sp suite_all.sp
    "$SPELL" -S -o suite suite_all.sp
    cc -o suite suite.s funcs.o
    ./suite > suite.out
    exit_code=$?
    if [ "$exit_code" != "1" ]; then
        echo "selfchek failed"
        cat suite.out
        fail_count=$((fail_count+1))
    fi
    n=$((n+1))
    rm -f suite_all.sp suite suite.s suite.out
}

one_io_test() {
    : > io_all.sp
    echo "$1" >> io_all.sp
    "$SPELL" -S -o io_prog io_all.sp
    cc -o io_prog io_prog.s funcs.o
    printf '%b' "$2" | ./io_prog > io_out.txt
    printf '%b' "$3" > io_expected.txt
    if ! cmp -s io_expected.txt io_out.txt; then
        echo "io test failed: $1"
        fail_count=$((fail_count+1))
    fi
    n=$((n+1))
    rm -rf io_all.sp io_prog io_prog.s io_out.txt io_expected.txt
}

n=${#tests[@]}

for i in "${!tests[@]}"; do
    one_test $i &
done
wait

fail_count=0
for i in "${!tests[@]}"; do
    i=$(printf '%03d' $i)
    if grep -q failed result$i; then
        fail_count=$((fail_count+1))
        cat result$i
    fi
    rm result$i
done

for i in "${!error_tests[@]}"; do
    one_error_test $i
done

run_suite test_expr.sp
run_suite test_stmt.sp
run_suite test_func.sp
run_suite test_array.sp
run_suite test_class.sp
run_selfcheck
one_io_test "def main() { print_int(42); putchar(10); return 0; }" "" "42\n"
one_io_test "def main() { print_int(0 - 42); putchar(10); return 0; }" "" "-42\n"
one_io_test "def main() { b = read_int(); r = read_int(); print_int((30 - b) * (30 - r)); putchar(10); return 0; }" "10 20\n" "200\n"
one_io_test "def main() { if (getchar()==-1) { print_int(1); } else { print_int(0); } putchar(10); return 0; }" "" "1\n"
one_io_test "def main() { print(42); return 0; }" "" "42\n"
one_io_test "def main() { print(\"abc\"); return 0; }" "" "abc\n"
one_io_test "def main() { print([1, 2, 3]); return 0; }" "" "1 2 3\n"
one_io_test "def main() { print(1 < 2); return 0; }" "" "true\n"
one_io_test "def main() { print([\"a\", \"b\"]); return 0; }" "" "a b\\n"
one_io_test "def main() { print([1 < 2, 2 < 3]); return 0; }" "" "true true\\n"
one_io_test "def main() { print([\"x\", \"y\"] == [\"x\", \"y\"]); return 0; }" "" "true\\n"
one_io_test "def main() { for x in [1, 2, 3] { print(x); } return 0; }" "" "1\n2\n3\n"
one_io_test "def main() { n = in; print(n); return 0; }" "42\n" "42\n"
one_io_test "def main() { a, b = in; print(a + b); return 0; }" "3 4\n" "7\n"
one_io_test "def main() { a = in[3]; print(a); return 0; }" "1 2 3\n" "1 2 3\n"
one_io_test "def main() { a = in[2][3]; print(a[1][2]); return 0; }" "1 2 3 4 5 6\n" "6\n"
one_io_test "def main() { s = in_str; print(s); return 0; }" "hello world\n" "hello\n"
one_io_test "def main() { s = in_str[2]; print(s[1]); return 0; }" "aa bb\n" "bb\n"
one_io_test "def main() { s = in_line; print(s); return 0; }" "hello world\n" "hello world\n"
one_io_test "def main() { >> x; << x; return 0; }" "9\n" "9\n"
one_io_test "def main() {
    >> x
    << x
    put x, x
    yn x == 9
    return 0
}" "9\n" "9\n9 9\nYes\n"
# step 62: >> の複数対象（左から順に in を代入する。空白区切り・改行区切り）
one_io_test "def main() { >> a, b, c, x; print(a + b + c + x); return 0; }" "1 2 3 4\n" "10\n"
one_io_test "def main() { >> a, b, c, x; print(a + b + c + x); return 0; }" "1\n2\n3\n4\n" "10\n"
one_io_test "def main() { arr = [0, 0, 0]; >> arr[0], n; print(arr[0] + n); return 0; }" "40 5\n" "45\n"
one_io_test "class P { v: int; def __init__(self) { self.v = 0; } } def main() { p = P(); >> p.v, n; print(p.v + n); return 0; }" "10 20\n" "30\n"
one_io_test "def main() { >> a, b
    print(a + b); return 0; }" "3 4\n" "7\n"
one_io_test "def main() { << \"hi\"; return 0; }" "" "hi\n"
one_io_test "def main() { put 1, 2; return 0; }" "" "1 2\n"
one_io_test "def main() { yn 1 < 2; yn 1 > 2; return 0; }" "" "Yes\nNo\n"
one_io_test "def main() { a- = in[3]; print(a); return 0; }" "1 2 3\n" "0 1 2\n"
one_io_test "def main() { print_int(0 - 10); putchar(10); print_int(0 - 100); putchar(10); print_int(0 - 1); putchar(10); return 0; }" "" "-10\n-100\n-1\n"
one_io_test "def main() { x = in; print_int(x); putchar(10); return 0; }" "-9223372036854775808\n" "-9223372036854775808\n"
one_io_test "def main() { s = in_str; for x in s { if (x != \"A\") { x = \".\"; } } print(s); return 0; }" "ABCA\n" "A..A\n"
# step 60: 短絡評価と連鎖比較の副作用回数（中間項はちょうど1回だけ評価される）
one_io_test "cnt = 0;
def inc(v: int) -> int { cnt = cnt + 1; return v; }
def main() {
    a = (inc(0) == 1) && (inc(5) == 5);
    print(cnt);
    b = (inc(2) == 2) || (inc(5) == 5);
    print(cnt);
    d = (inc(3) == 3) && (inc(4) == 4);
    print(cnt);
    e = inc(1) < inc(2) < inc(3);
    print(cnt);
    print(a);
    print(b);
    print(d);
    print(e);
    return 0;
}" "" "1\n2\n4\n7\nfalse\ntrue\ntrue\ntrue\n"

# ドライバ層（step 59）のテスト。引数・標準出力・標準エラー・終了コードを
# 別々に照合する。$1=ソース $2=期待する標準出力 $3=期待する標準エラー
# $4=期待する終了コード 残り=プログラムへの引数
one_driver_test() {
    printf '%s' "$1" > drv.sp
    "$SPELL" -S -o drv drv.sp
    cc -o drv drv.s funcs.o
    local expected_out="$2" expected_err="$3" expected_code="$4"
    shift 4
    ./drv "$@" > drv.out 2> drv.err
    local code=$?
    printf '%b' "$expected_out" > drv.expout
    printf '%b' "$expected_err" > drv.experr
    if [ "$code" != "$expected_code" ] || ! cmp -s drv.expout drv.out || ! cmp -s drv.experr drv.err; then
        echo "driver test failed: $1 (exit $code)"
        fail_count=$((fail_count+1))
    fi
    n=$((n+1))
    rm -f drv.sp drv drv.s drv.out drv.err drv.expout drv.experr drv_data.txt
}

# 到達可能性（step 63）: 生成された asm に、入口から辿れる関数と使う
# ヘルパだけが含まれることを見る。$1=ソース $2=含まれるラベル断片
# $3=含まれてはいけないラベル断片
one_reach_test() {
    printf '%s' "$1" > reach.sp
    "$SPELL" -S -o reach reach.sp
    if ! grep -q "$2" reach.s || grep -q "$3" reach.s; then
        echo "reach test failed: $1 (want: $2, reject: $3)"
        fail_count=$((fail_count+1))
    fi
    n=$((n+1))
    rm -f reach.sp reach.s
}

# (a) hello 相当では Dict のメソッドも read_file のヘルパも出ない
one_reach_test "def main() { print(42); return 0; }" "__println" "Dict"
one_reach_test "def main() { print(42); return 0; }" "__println" "__rt_spell_read_file_chars"
# (b) read_file を使うと対応するヘルパが出る。Vec は使わないので出ない
one_reach_test "def main() { d = read_file(\"x\"); print(d.ok); return 0; }" "__rt_spell_read_file_chars" "Vec"
# (c) 呼ばれない自作関数は出ず、呼ばれるものは出る
one_reach_test "def alpha() -> int { return 1; } def omega() -> int { return 2; } def main() { return alpha(); }" "alpha" "omega"
# (d) 関数値を保持して後から呼ぶ・クロージャ・Dict のファクトリ関数名渡し
one_io_test "def main() { base = 40; f = fn(v: int) -> int { return v + base; }; print(f(2)); return 0; }" "" "42\n"
one_io_test "def main() { d: Dict<str, int> = Dict(zero_int); d[\"a\"] = 40; d[\"b\"] = 2; s = 0; for k in d.keys() { s = s + d[k]; } print(s); return 0; }" "" "42\n"

one_driver_test "def main() { a = args(); print(a[1]); print(a[2]); return 0; }" "hello\nworld\n" "" 0 hello world
one_driver_test "def main() { ok = write_file(\"drv_data.txt\", \"roundtrip 59\\n\"); d = read_file(\"drv_data.txt\"); print(ok); print(d.ok); print(d.text); return 0; }" "true\ntrue\nroundtrip 59\n\n" "" 0
one_driver_test "def main() { d = read_file(\"drv_missing.txt\"); print(d.ok); print(d.text.len); return 0; }" "false\n0\n" "" 0
one_driver_test "def main() { print(\"out\"); eprint(\"err\"); eprint(42); return 0; }" "out\n" "err\n42\n" 0
one_driver_test "def main() { print(\"before\"); exit(3); print(\"after\"); return 0; }" "before\n" "" 3
# step 65: append の基本（連結と同じ見た目で、長さが伸びる）
one_io_test "def main() { a = [1, 2, 3]; a.append(4); a.append(5); print(a); print(a.len); return 0; }" "" "1 2 3 4 5\n5\n"
# step 65: ランタイムヘルパが作る配列（read_file の文字コード列）にも
# cap が書かれていて、append で伸ばせること。100回伸ばして中身を確かめる
one_driver_test "def main() { ok = write_file(\"drv_data.txt\", \"ab\"); d = read_file(\"drv_data.txt\"); i = 0; while (i < 100) { d.text.chars.append(99); i = i + 1; } print(d.text.len); print(d.text.chars[0]); print(d.text.chars[101]); return 0; }" "102\n97\n99\n" "" 0
# step 65: 読み取り失敗時に返る空配列（ヘルパの失敗経路）への append
one_driver_test "def main() { d = read_file(\"drv_missing.txt\"); d.text.chars.append(65); print(d.text); print(d.text.len); return 0; }" "A\n1\n" "" 0
# step 88: 入口（main）が無いプログラムでもリンクできること。空の
# main が合成され、終了コード 0 で終わる
one_driver_test "" "" "" 0
one_driver_test "def foo() { return 1; }" "" "" 0
# step 88: メソッド名 main は入口と混同されないこと（トップレベル文の
# 合成関数は main になる）
one_driver_test "class C { def main(self) -> int { return 5; } } print(42);" "42\n" "" 0
# step 91: オーバーロードされた関数は署名つきラベルになり、呼ばれた
# 側だけが出力される
one_reach_test "def f(x: int) -> int { return 1; } def f(x: str) -> int { return 2; } def main() { return f(1); }" 'spell_f_f$1$int:' 'spell_f_f$1$str:'
# step 91: ユーザーが定義した getchar は外部シンボルではないので、
# 32 ビットへの切り詰め（movsxd）を受けない
one_io_test "def getchar() -> int { return 5000000000; } def main() { print(getchar()); return 0; }" "" "5000000000\n"
# step 66: 矢印で書く BFS と、メソッド形との等価性
one_io_test "def main() {
    rows = [\"S..#\", \"##..\", \"...#\", \"..G.\"];
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
                if (rows[nr][nc] != \"#\" && dist[nr * 4 + nc] == 0 - 1) {
                    dist[nr * 4 + nc] = dist[cur] + 1;
                    q <- nr * 4 + nc;
                }
            }
            k = k + 1;
        }
    }
    print(dist[14]);
    return 0;
}" "" "5\n"
one_io_test "def main() {
    a: Deque<int> = Deque(zero_int);
    b: Deque<int> = Deque(zero_int);
    i = 0;
    while (i < 10) { a <- i; b.push_back(i); i = i + 1; }
    100 -> a; b.push_front(100);
    a -> u; t = b.pop_back();
    s = 0;
    while (a.len > 0) { x <- a; y = b.pop_front(); s = s + x - y; }
    print(s);
    print(u + t);
    print(a.len + b.len);
    return 0;
}" "" "0\n18\n0\n"
# step 67: `:` ＋インデント形式（suite）。def / if-elif-else / while /
# for / for...else を colon 形式で書いたプログラム
one_io_test "def fib(n: int) -> int:
    if (n < 2):
        return n
    return fib(n - 1) + fib(n - 2)

def main():
    print(fib(10))
    x = 0
    while (x < 3):
        print(x)
        x = x + 1
    for i in 3:
        print(i * 10)
    else:
        print(999)
    if (fib(5) == 5):
        print(1)
    elif (fib(5) == 6):
        print(2)
    else:
        print(3)
    return 0
" "" "55\n0\n1\n2\n0\n10\n20\n999\n1\n"
# step 67: class の colon 形式（フィールド＋メソッド、ジェネリクス）
one_io_test "class Pt:
    x: int
    y: int
    def __init__(self, x: int, y: int):
        self.x = x
        self.y = y
    def sum(self) -> int:
        return self.x + self.y

class Box<T>:
    v: T
    def __init__(self, v: T):
        self.v = v
    def get(self) -> T:
        return self.v

def main():
    p = Pt(3, 4)
    print(p.sum())
    b: Box<int> = Box(42)
    print(b.get())
    return 0
" "" "7\n42\n"
# step 67: 1行形 suite（def と if、class も）
one_io_test "def one(): return 7

class P: v: int; def get(self) -> int { return self.v; }

def main():
    print(one())
    if (one() == 7): print(100)
    if (one() == 9): print(200)
    else: print(300)
    p = P()
    print(p.get())
    return 0
" "" "7\n100\n300\n0\n"
# step 67: 混在（colon の中に {}、{} の中に colon、3段）
one_io_test "def main() {
    a = 0
    if (a == 0):
        b = 0
        while (b < 3) {
            if (b == 1):
                a = a + 10
            else {
                a = a + 1
            }
            b = b + 1
        }
        a = a + 100
    print(a)
    return 0
}" "" "112\n"
# step 67: @ 系の colon 形式（i@n:、@:、@n:）
one_io_test "def main():
    s = 0
    i@4:
        s = s + i
    print(s)
    c = 0
    @:
        c = c + 1
        if (c >= 3): !;
    print(c)
    t = 0
    @3:
        t = t + 2
    print(t)
    return 0
" "" "6\n3\n6\n"
# step 67: コメント行・空行を挟んだ suite と、式の継続行
one_io_test "def main():
    x = 0
    if (x == 0):

        // コメントだけの行
        x = 5

    print(x)
    y = 1 +
        2
    print(y)
    return 0
" "" "5\n3\n"
# step 67: 同一プログラムの {} 形と : 形で出力が一致すること（2本）
one_io_test "def main() { s = 0; for i in 5 { if (i == 2) { s = s + 10; } else { s = s + i; } } print(s); return 0; }" "" "18\n"
one_io_test "def main():
    s = 0
    for i in 5:
        if (i == 2):
            s = s + 10
        else:
            s = s + i
    print(s)
    return 0
" "" "18\n"
one_io_test "class Acc { n: int; def __init__(self) { self.n = 0; } def add(self, v: int) { self.n = self.n + v; } } def main() { a = Acc(); a.add(5); a.add(37); print(a.n); return 0; }" "" "42\n"
one_io_test "class Acc:
    n: int
    def __init__(self):
        self.n = 0
    def add(self, v: int):
        self.n = self.n + v

def main():
    a = Acc()
    a.add(5)
    a.add(37)
    print(a.n)
    return 0
" "" "42\n"
# step 67: トップレベルの colon 文と、注釈つき反復対象（colon 形でも
# 注釈として読める。suite の最初の文が代入でも誤認しない）
one_io_test "n = in
s = 0
for i in n:
    s = s + i
print(s)
for i in x: [int] = [7, 8]:
    print(i)
for i in [1]:
    z = 50
    print(z)
" "3\n" "3\n7\n8\n50\n"
# step 67: for...else の colon 形式（break したら else は動かない）
one_io_test "def main():
    s = 0
    for i in 5:
        if (i == 2):
            !;
        s = s + i
    else:
        s = 999
    print(s)
    return 0
" "" "1\n"
# step 70: 真理値リテラル。4綴りの print、yn、括弧なし条件
one_io_test "def main() { print(true); print(false); print(T); print(F); yn F; yn T; return 0; }" "" "true\nfalse\ntrue\nfalse\nNo\nYes\n"
one_io_test "def main():
    if true: print(1)
    if F: print(2)
    else: print(3)
    while F { print(4); }
    print(5)
    return 0
" "" "1\n3\n5\n"
# step 70: 配列の要素・Dict の値としての真理値リテラル
one_io_test "def main() { a = [true, F, T]; print(a[0]); print(a[1]); print(a.len); d: Dict<str, bool> = Dict(zero_bool); d[\"x\"] = true; print(d[\"x\"]); print(d[\"y\"]); return 0; }" "" "true\nfalse\n3\ntrue\nfalse\n"
# step 72: >>> は文字列トークンの入力（対象 = in_str と同じ）
one_io_test "def main() { >>> s; print(s); print(s.len); print(s[1]); return 0; }" "hello\n" "hello\n5\ne\n"
one_io_test "def main() { >>> s, t; print(s); print(t); return 0; }" "foo bar\n" "foo\nbar\n"
one_io_test "class P { s: str; def __init__(self) { self.s = \"\"; } } def main() { s2 = \"x\"; >>> s2; print(s2); p = P(); >>> p.s; print(p.s); arr = [\"\", \"\"]; >>> arr[1]; print(arr[1]); return 0; }" "baz qux zzz\n" "baz\nqux\nzzz\n"
one_io_test "def main() { >> n; >>> s; print(n); print(s); return 0; }" "42 abc\n" "42\nabc\n"
# step 72: 3重にネストしたジェネリクス注釈（>>> が閉じ括弧3つとして読める）
one_io_test "class Box<T> { v: T; def __init__(self, v: T) { self.v = v; } } def unwrap(b: Box<Box<Box<int>>>) -> int { return b.v.v.v; } def main() { b: Box<Box<Box<int>>> = Box(Box(Box(42))); print(unwrap(b)); return 0; }" "" "42\n"
one_io_test "def empty_inner() -> Vec<int> { v: Vec<int> = Vec(zero_int); return v; }
def empty_mid() -> Vec<Vec<int>> { m: Vec<Vec<int>> = Vec(empty_inner); return m; }
def main():
    for k in d: Dict<str, Vec<Vec<int>>> = Dict(empty_mid):
        print(k)
    print(999)
    return 0
" "" "999\n"
# step 72: 個数形 >> a[n]（未束縛の基底名は個数形、束縛済みは添字形）
one_io_test "def main() { >> n, a[n]; print(a.len); print(a); return 0; }" "3 10 20 30\n" "3\n10 20 30\n"
one_io_test "def main() { >> n; >>> s[n]; print(s); return 0; }" "3 apple banana cherry\n" "apple banana cherry\n"
one_io_test "def main() { a = [0] * 5; >> a[2]; print(a); return 0; }" "7\n" "0 0 7 0 0\n"
one_io_test "def main() { >> n, a[n]; print(a.len); return 0; }" "0\n" "0\n"
one_io_test "def main() { >> n, m, a[n], b[m]; print(a); print(b); return 0; }" "2 1 2 3 4 5 6\n" "2 3\n4\n"
# step 75: 個数形の n 次元化（未束縛の基底名なら角括弧の並びは全て個数）
one_io_test "def main():
    >> h, w, a[h][w]
    print(a.len)
    print(a[0])
    print(a[1])
    print(a[1][2])
    return 0
" "2 3\n1 2 3 4 5 6\n" "2\n1 2 3\n4 5 6\n6\n"
one_io_test "def main():
    >> n, a[n][n]
    print(a[0][0])
    print(a[2][2])
    print(a[0])
    print(a[2])
    return 0
" "3\n1 2 3 4 5 6 7 8 9\n" "1\n9\n1 2 3\n7 8 9\n"
one_io_test "def main():
    >> a[2][3][4]
    print(a.len)
    print(a[0].len)
    print(a[0][0].len)
    s = 0
    for x in a:
        for y in x:
            for z in y:
                s = s + z
    print(s)
    print(a[1][2][3])
    return 0
" "1 2 3 4 5 6 7 8 9 10 11 12 13 14 15 16 17 18 19 20 21 22 23 24\n" "2\n3\n4\n300\n24\n"
# step 75: 小配列は枠ごとに作られる（行同士で共有しない）
one_io_test "def main():
    >> a[2][2]
    a[0][0] = 99
    print(a[0])
    print(a[1])
    return 0
" "1 2 3 4\n" "99 2\n3 4\n"
# step 75: >>> の二次元（文字列）
one_io_test "def main():
    >> h, w
    >>> s[h][w]
    print(s[0])
    print(s[1][1])
    return 0
" "2 2\naa bb cc dd\n" "aa bb\ndd\n"
# step 75: 束縛済みの二次元配列への >> a[1][2] は従来どおり1要素読み
one_io_test "def main() { a = [[0, 0, 0], [0, 0, 0]]; >> a[1][2]; print(a[1][2]); print(a[0]); print(a[1]); return 0; }" "7\n" "7\n0 0 0\n0 0 7\n"
# step 75: 次元 0（何も読まず、空の方向ができるだけ）
one_io_test "def main():
    >> a[0][3]
    print(a.len)
    >> b[3][0]
    print(b.len)
    print(b[0].len)
    print(b[2].len)
    return 0
" "" "0\n3\n0\n0\n"
# step 75: 個数式はそれぞれ1回だけ評価される（副作用つき関数で数える）
one_io_test "def nxt() -> int { print(777); return 2; }
def main() {
    >> a[nxt()][nxt()];
    print(a.len);
    print(a[1][1]);
    return 0;
}
" "1 2 3 4\n" "777\n777\n2\n4\n"
# step 75: 個数式は左から順に評価される（返す個数が違う関数で順序を区別する）
one_io_test "def f() -> int { print(111); return 2; }
def g() -> int { print(222); return 3; }
def main() {
    >> a[f()][g()];
    print(a.len);
    print(a[0].len);
    print(a[1][2]);
    return 0;
}
" "1 2 3 4 5 6\n" "111\n222\n2\n3\n6\n"
# step 75: 角括弧の継続は同じ行だけ（次の行の `[` を個数として飲み込まない）
one_io_test "def main() {
    >> a[2]
    [5]
    print(a.len);
    print(a);
    return 0;
}
" "10 20\n" "2\n10 20\n"
# step 72: 対象は空白区切りでも並べられる（カンマとの混在も可）
one_io_test "def main() { >> x y; print(x + y); return 0; }" "1 2\n" "3\n"
one_io_test "def main() { >>> s t; print(s); print(t); return 0; }" "foo bar\n" "foo\nbar\n"
one_io_test "def main() { >> x, y z; print(x + y + z); return 0; }" "1 2 3\n" "6\n"
one_io_test "def main() { >> n a[n]; print(a); return 0; }" "3 7 8 9\n" "7 8 9\n"
# step 72: 空白区切りの継続は同じ行だけ（次の行の文を飲み込まない）
one_io_test "def main() {
    >> x
    y = 3;
    print(x + y);
    return 0;
}" "10\n" "13\n"
# step 73: 文頭 `?` の if 糖衣（elif / else の連鎖、入力との組み合わせ）
one_io_test "def main():
    x = in
    ? x > 10:
        print(1)
    elif x > 5:
        print(2)
    else:
        print(3)
    ? x == 7 { print(4); }
    return 0
" "7\n" "2\n4\n"
# step 76: 引数なし構築。要素型の既定は int で、push / pop / len /
# 反復まで通常の Deque<int> として動く
one_io_test "def main() {
    q = Deque();
    q.push_back(10);
    q.push_back(20);
    q.push_front(5);
    print(q.len);
    print(q.pop_front());
    print(q.pop_back());
    s = 0;
    for x in q { s = s + x; }
    print(s);
    return 0;
}" "" "3\n5\n20\n10\n"
# step 76: Vec() の引数なし構築。容量の倍増（補ったファクトリが呼ばれる
# 経路）でも中身が保たれる
one_io_test "def main() {
    v = Vec();
    i = 0;
    while (i < 20) { v.append(i * 2); i = i + 1; }
    print(v.len);
    print(v[19]);
    v[0] = 99;
    print(v[0]);
    print(v[1]);
    return 0;
}" "" "20\n38\n99\n2\n"
# step 76: 構築時の型引数（Deque<str> / Deque<bool>）
one_io_test "def main() {
    q = Deque<str>();
    q.push_back(\"aa\");
    q.push_front(\"bb\");
    print(q.pop_front());
    print(q.pop_back());
    print(q.len);
    b = Deque<bool>();
    b.push_back(true);
    b.push_back(F);
    print(b.pop_front());
    print(b.pop_back());
    return 0;
}" "" "bb\naa\n0\ntrue\nfalse\n"
# step 76: 期待型が効く引数なし構築（関数の引数位置と注釈）
one_io_test "def take(q: Deque<str>) -> int {
    q.push_back(\"in\");
    return q.len;
}
def main() {
    print(take(Deque()));
    r: Deque<str> = Deque();
    r.push_back(\"zz\");
    print(r.pop_front());
    v: Vec<str> = Vec();
    v.append(\"s1\");
    v.append(\"s2\");
    print(v[1]);
    return 0;
}" "" "1\nzz\ns2\n"
# step 76: ユーザー定義ジェネリッククラスでの型引数明示（注釈なし構築）。
# 入れ子の Vec<Vec<int>> も、ファクトリを渡せば従来どおり構築できる
one_io_test "class Pair<A, B> {
    a: A;
    b: B;
    def __init__(self, a: A, b: B) { self.a = a; self.b = b; }
}
class Cell<T> {
    v: T;
    def __init__(self, v: T) { self.v = v; }
    def get(self) -> T { return self.v; }
}
def main() {
    p = Pair<int, str>(42, \"xy\");
    print(p.a);
    print(p.b);
    c = Cell<bool>(F);
    print(c.get());
    w = Vec<Vec<int>>(fn() -> Vec<int> { return Vec(zero_int); });
    print(w.len);
    n = Vec<int>();
    n.append(7);
    print(n[0]);
    return 0;
}" "" "42\nxy\nfalse\n0\n7\n"
# step 76: Dict<str, int>(zero_int) を注釈なしで構築できる（型引数の
# 明示は一般機能）。無いキーの読み取りでは初期値が入る（意味論は不変）
one_io_test "def main() {
    d = Dict<str, int>(zero_int);
    d[\"a\"] = 5;
    print(d[\"a\"]);
    print(d[\"missing\"]);
    print(d.len);
    return 0;
}" "" "5\n0\n2\n"
# step 76: ファクトリ形でも、注釈が無ければ要素型の既定 int が効く
one_io_test "def main() {
    q = Deque(zero_int);
    q.push_back(3);
    q.push_back(4);
    print(q.pop_front() + q.pop_front());
    return 0;
}" "" "7\n"
# step 77: Pos リテラルの表示（__str__ は空白区切り）と演算
one_io_test "def main() {
    print((1, 2) + (2, 3));
    print((1, 2, 3) * 2);
    print((5, 5) - (1, 2));
    return 0;
}" "" "3 5\n2 4 6\n4 3\n"
# step 77: a[p] の添字は一度だけ評価される（副作用の回数で固定する）
one_io_test "calls = 0;
def mk() -> Pos2 { calls = calls + 1; return (1, 0); }
def main() {
    a = [[1, 2], [3, 4]];
    print(a[mk()]);
    print(calls);
    a[mk()] = 99;
    print(calls);
    print(a[1][0]);
    return 0;
}
" "" "3\n1\n2\n99\n"
# step 77: 型を引数に渡す構築（int / str / Pos2、Vec も）
one_io_test "def main() {
    q = Deque(int);
    q <- 10;
    q <- 20;
    print(q.pop_front());
    s = Deque(str);
    s <- \"hi\";
    print(s.pop_back());
    v = Vec(int);
    v.append(7);
    print(v[0]);
    pq = Deque(Pos2);
    pq <- (1, 2);
    pq <- (3, 4);
    print(pq.pop_front());
    print(pq.pop_back());
    z: Deque<Pos2> = Deque();
    z <- (8, 9);
    print(z.pop_front());
    return 0;
}" "" "10\nhi\n7\n1 2\n3 4\n8 9\n"
# step 77: Deque([int]) の要素配列は互いに独立（片方への append が
# 他方へ漏れない）
one_io_test "def main() {
    q = Deque([int]);
    q <- [0, 0];
    q <- [1, 1];
    row = q.pop_front();
    row.append(5);
    print(row);
    print(q.pop_front());
    return 0;
}" "" "0 0 5\n1 1\n"
# step 77: ゼロ化できるユーザークラスへの型渡し（__init__ の引数を
# ゼロで埋めた構築がファクトリになる）
one_io_test "class Cell {
    v: int;
    def __init__(self, v: int) { self.v = v; }
}
def main() {
    q = Deque(Cell);
    q <- Cell(5);
    c = q.pop_front();
    print(c.v);
    n = Deque([[int]]);
    n <- [[1, 2]];
    print(n.pop_front());
    return 0;
}" "" "5\n1 2\n"
# step 77: BFS の実例。グリッドを >> / >>> で読み、dirs の Pos リテラル
# 配列と p + d の加算で隣接セルを列挙し、Deque(Pos2) と
# Dict<Pos2, int> で最短路を出す（利用者の想定イディオムを固定する）
one_io_test "def main() {
    >> h, w
    >>> s[h]
    dirs = [(1, 0), (0, 1), (-1, 0), (0, -1)]
    dist: Dict<Pos2, int> = Dict(zero_int)
    q = Deque(Pos2)
    start = (0, 0)
    goal = (w - 1, h - 1)
    dist[start] = 0
    q <- start
    while q.len > 0:
        p = q.pop_front()
        for d in dirs:
            np = p + d
            if np.x < 0 || np.x >= w || np.y < 0 || np.y >= h: continue
            if s[np.y][np.x] == \"#\": continue
            if dist.has(np): continue
            dist[np] = dist[p] + 1
            q <- np
    print(dist[goal])
    return 0;
}
" "5 5\n.....\n.###.\n.#...\n.###.\n.....\n" "8\n"
# step 78: レジスタ名のグローバル変数を関数内から読み書きする
one_io_test "si = 0
xmm0 = 40
def bump() { si = si + 1; }
def main() {
    bump();
    bump();
    print(si);
    print(xmm0 + 2);
    return 0;
}" "" "2\n42\n"
# step 78: トップレベルの @ ループ変数がレジスタ名（si）でも壊れない
# （利用者が ABC088 D で踏んだ実例の形）
one_io_test "s = [\"ab\", \"cd\"]
si@s:
    print(si)" "" "ab\ncd\n"
# step 78: 条件の直後の `?` はエラーになるが、コロン形・波括弧形の
# 正当な入れ子の `?` は動き続ける
one_io_test "def main() {
    a = [10, 20]
    if a[0] == 10:
        ? a[1] == 20:
            print(1)
        |:
            print(2)
    if a[0] == 10 { ? a[1] == 99 { print(3) } | { print(4) } }
    return 0
}" "" "1\n4\n"
# step 79: while の条件に Deque を直接書く BFS 風ループ（空で止まる）
one_io_test "def main() { q = Deque(); q.push_back(0); seen = [0; 8]; seen[0] = 1; order = \"\"; while q { v = q.pop_front(); order = order + v.__str__(); if (v + 1 < 6 && seen[v + 1] == 0) { seen[v + 1] = 1; q.push_back(v + 1); } if (v + 2 < 6 && seen[v + 2] == 0) { seen[v + 2] = 1; q.push_back(v + 2); } } print(order); return 0; }" "" "012345\n"
# step 80: `@? q:` でも同じ BFS 風ループが書ける（コロン形・矢印ポップ）
one_io_test "
q = Deque()
q.push_back(0)
seen = [0; 8]
seen[0] = 1
order = \"\"
@? q:
    v <- q
    order = order + v.__str__()
    if (v + 1 < 6 && seen[v + 1] == 0) { seen[v + 1] = 1; q.push_back(v + 1); }
    if (v + 2 < 6 && seen[v + 2] == 0) { seen[v + 2] = 1; q.push_back(v + 2); }
print(order)
" "" "012345\n"
# step 79: Vec / str / Dict / 配列の __bool__（空かどうか）
one_io_test "def main() { v = Vec(); if (v) { print(1); } else { print(0); } v.append(1); if (v) { print(1); } else { print(0); } s = \"\"; if (s) { print(1); } else { print(0); } s = \"x\"; if (s) { print(1); } else { print(0); } d: Dict<str, int> = Dict(zero_int); if (d) { print(1); } else { print(0); } d[\"a\"] = 1; if (d) { print(1); } else { print(0); } a = [0; 0]; if (a) { print(1); } else { print(0); } a = [7]; if (a) { print(1); } else { print(0); } return 0; }" "" "0\n1\n0\n1\n0\n1\n0\n1\n"
# step 79: && の短絡と ! でも __bool__ が効く（空の間は pop_front を呼ばない）
one_io_test "def main() { q = Deque(); x = 0; if (q && q.pop_front()) { x = 1; } print(x); q.push_back(5); if (q && q.pop_front() == 5) { x = 2; } print(x); if (!q) { print(9); } return 0; }" "" "0\n2\n9\n"
# step 81: 利用者のグリッド BFS の形。隣接マスの判定を安全添え字
# （dist?[np] == -1 の値比較と、s?[np] の単独 bool 形）だけで行い、
# 盤外の隣接は範囲チェックが弾く。最短路は 8。壁つきグリッドでの
# s?[np] == "." の値比較つきの形は、s[i] が1文字 str を返すように
# なる後続 step で追加する
one_io_test "
def main() {
    >> h, w
    >>> s[h]
    row0 = [-1] * w
    dist = [row0]
    i = 1
    while i < h:
        dist.append([-1] * w)
        i += 1
    q = Deque(Pos2)
    start = (0, 0)
    goal = (w - 1, h - 1)
    dist[start] = 0
    q <- start
    dirs = [(1, 0), (0, 1), (-1, 0), (0, -1)]
    while q:
        p <- q
        for d in dirs:
            np = p + d
            if dist?[np] == -1 && s?[np]:
                dist[np] = dist[p] + 1
                q <- np
    print(dist[goal])
    return 0
}
" "5 5
.....
.....
.....
.....
.....
" "8\n"
# step 81: 単独形（bool）と多段の比較、!= で範囲外が偽になること
one_io_test "def main() { a = [[1, 2], [3, 4]]; print(a?[0][1]); print(a?[2][0]); print(a?[1][1] == 4); print(a?[1][2] == 4); print(a?[0][0] != 9); print(a?[9][0] != 9); return 0; }" "" "true\nfalse\ntrue\nfalse\ntrue\nfalse\n"
# step 81: [str] への Pos 安全添え字と、str 対象の安全添え字。
# str の要素の値そのもの（int か str か）は固定せず、範囲チェックの
# bool 形と、範囲外で比較が false になる形だけで確認する
one_io_test "def main() { g = [\"ab\", \"cd\"]; print(g?[(0, 0)]); print(g?[(1, 1)]); print(g?[(1, 2)]); print(g?[(2, 0)]); s = \"xyz\"; print(s?[2]); print(s?[-1]); print(s?[1] == s[1]); print(s?[3] == s[0]); return 0; }" "" "true\ntrue\nfalse\nfalse\ntrue\nfalse\ntrue\nfalse\n"
# step 82: 利用者の書き方そのままの BFS（step 81/84 の持ち越し）。
# dist は [[-1; w]; h] で作り、隣接マスの判定は s?[np] == '.' の
# 値比較つき安全添え字で行う。ゴールは添え字糖衣の成分順
# （a[p] = a[p[0]][p[1]]、第0成分が行）で (h - 1, w - 1)
one_io_test "
def main() {
    >> h, w
    >>> s[h]
    dist = [[-1; w]; h]
    start = (0, 0)
    goal = (h - 1, w - 1)
    dist[start] = 0
    q = Deque(Pos2)
    q <- start
    @? q:
        p <- q
        d@[(1, 0), (0, 1), (-1, 0), (0, -1)]:
            np = p + d
            ? dist?[np] == -1 && s?[np] == '.':
                dist[np] = dist[p] + 1
                q <- np
    print(dist[goal])
    return 0
}
" "3 4\n....\n.##.\n....\n" "5\n"
# step 82: while + for の入れ子の中での比較形の安全添え字の数え上げ。
# 開始セルはいずれも盤内（(0, 3) は0行3列）で、有効な隣接は各2つ
one_io_test "
s = [\"....\", \".##.\", \"....\"]
dist = [[-1; 4]; 3]
starts = [(0, 0), (0, 3)]
n = 0
si = 0
while si < 2:
    p = starts[si]
    si = si + 1
    d@[(1, 0), (0, 1), (-1, 0), (0, -1)]:
        np = p + d
        ? dist?[np] == -1 && s?[np] == '.':
            n = n + 1
print(n)
" "" "4\n"
# step 85: スライスと文字列メソッド第1弾の出力
one_io_test "def main() { s = \"hello world\"; print(s[0:5]); print(s[6:]); print(s[-5:]); print(s[3:2].len); a = [10, 20, 30, 40]; print(a[1:3]); print(a[-2:]); print(s.find(\"o\")); print(s.find(\"zz\")); print(\"a,b,,c\".split(\",\")); print(\"-\".join([\"a\", \"b\", \"c\"])); print(\"  -42abc\".to_int()); print(\"x\".starts_with(\"\")); print(\"x\".ends_with(\"y\")); return 0; }" "" "hello\nworld\nworld\n0\n20 30\n30 40\n4\n-1\na b  c\na-b-c\n-42\ntrue\nfalse\n"
# step 87: 32ビットに収まらない整数リテラルの値そのもの
one_io_test "def main() { print(2147483648); print(9223372036854775807); print(-2147483649); return 0; }" "" "2147483648\n9223372036854775807\n-2147483649\n"
# step 87: read_int が先行空白（連続・先頭）を読み飛ばす
one_io_test "def main() { a = read_ints(2); print(a[0]); print(a[1]); return 0; }" "12  34\n" "12\n34\n"
one_io_test "def main() { print(in); print(in); return 0; }" " 12 34\n" "12\n34\n"
# step 93: f-string の出力（埋め込み・複数箇所・波括弧エスケープ・
# 入れ子の文字列と f-string・スライス・配列と bool の文字列化）
one_io_test "def main() { x = 42; s = \"ab\"; print(f\"v={x}!\"); print(f\"{x + 1} and {s} and {1 < 2}\"); print(f\"{[1, 2, 3]}\"); print(f\"{{x}} {x}\"); print(f\"{ \"in\" }\"); print(f\"{f\"n{x}\"}\"); print(f'{s[0:1]}'); print(f\"\"); print(f\"end\"); return 0; }" "" "v=42!\n43 and ab and true\n1 2 3\n{x} 42\nin\nn42\na\n\nend\n"

echo $(($n-$fail_count)) / $n tests passed.
if [ $fail_count -eq 0 ]; then
    exit 0
else
    exit 1
fi
