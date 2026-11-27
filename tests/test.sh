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
one_driver_test "def main() { ok = write_file(\"drv_data.txt\", \"ab\"); d = read_file(\"drv_data.txt\"); i = 0; while (i < 100) { d.text.chars.append(99); i = i + 1; } print(d.text.len); print(d.text[0]); print(d.text[101]); return 0; }" "102\n97\n99\n" "" 0
# step 65: 読み取り失敗時に返る空配列（ヘルパの失敗経路）への append
one_driver_test "def main() { d = read_file(\"drv_missing.txt\"); d.text.chars.append(65); print(d.text); print(d.text.len); return 0; }" "A\n1\n" "" 0
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
                if (rows[nr][nc] != 35 && dist[nr * 4 + nc] == 0 - 1) {
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

echo $(($n-$fail_count)) / $n tests passed.
if [ $fail_count -eq 0 ]; then
    exit 0
else
    exit 1
fi
