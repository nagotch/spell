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
one_io_test "def main() { < \"hi\"; return 0; }" "" "hi\n"
one_io_test "def main() { put 1, 2; return 0; }" "" "1 2\n"
one_io_test "def main() { yn 1 < 2; yn 1 > 2; return 0; }" "" "Yes\nNo\n"
one_io_test "def main() { a- = in[3]; print(a); return 0; }" "1 2 3\n" "0 1 2\n"
one_io_test "def main() { print_int(0 - 10); putchar(10); print_int(0 - 100); putchar(10); print_int(0 - 1); putchar(10); return 0; }" "" "-10\n-100\n-1\n"
one_io_test "def main() { x = in; print_int(x); putchar(10); return 0; }" "-9223372036854775808\n" "-9223372036854775808\n"

echo $(($n-$fail_count)) / $n tests passed.
if [ $fail_count -eq 0 ]; then
    exit 0
else
    exit 1
fi
