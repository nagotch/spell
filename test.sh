#!/bin/bash

tests=(
    "main() { 0; }|0"
    "main() { 1; }|1"
    "main() { 42; }|42"
    "main() { 254; }|254"
    "main() { 255; }|255"
    "main() { 5+20-4; }|21"
    "main() { 10-3-2; }|5"
    "main() { 2*3+4; }|10"
    "main() { 2+3*4; }|14"
    "main() { (2+3)*4; }|20"
    "main() { 20/4; }|5"
    "main() { 7/2; }|3"
    "main() { 12 + 3 * ( 2 + 4 ); }|30"
    "main() { -10+20; }|10"
    "main() { -(3+5)+10; }|2"
    "main() { -3*+5+20; }|5"
    "main() { +42; }|42"
    "main() { -(-5); }|5"
    "main() { 2*-3+10; }|4"
    "main() { 0==1; }|0"
    "main() { 42==42; }|1"
    "main() { 0!=1; }|1"
    "main() { 42!=42; }|0"
    "main() { 0<1; }|1"
    "main() { 1<1; }|0"
    "main() { 2<1; }|0"
    "main() { 0<=1; }|1"
    "main() { 1<=1; }|1"
    "main() { 2<=1; }|0"
    "main() { 1>0; }|1"
    "main() { 1>1; }|0"
    "main() { 1>2; }|0"
    "main() { 1>=0; }|1"
    "main() { 1>=1; }|1"
    "main() { 1>=2; }|0"
    "main() { 1+2==3; }|1"
    "main() { 1<2==1; }|1"
    "main() { a=3; a; }|3"
    "main() { a=3; b=5*6-8; a+b/2; }|14"
    "main() { a=b=3; a+b; }|6"
    "main() { foo=3; foo; }|3"
    "main() { foo=3; bar=5; foo+bar; }|8"
    "main() { fo=1; foo=2; fo+foo; }|3"
    "main() { foo1=5; foo1; }|5"
    "main() { return 42; }|42"
    "main() { return 5; return 8; }|5"
    "main() { a=3; b=5*6-8; return a+b/2; }|14"
    "main() { returnx=3; returnx; }|3"
    "main() { return1=7; return1; }|7"
    "main() { if (0) return 1; return 2; }|2"
    "main() { if (1) return 1; return 2; }|1"
    "main() { if (0) return 1; else return 2; }|2"
    "main() { if (1) return 1; else return 2; }|1"
    "main() { a=0; if (1) a=1; return a; }|1"
    "main() { a=0; if (0) a=1; return a; }|0"
    "main() { a=0; while (a<10) a=a+1; return a; }|10"
    "main() { s=0; for (i=0; i<10; i=i+1) s=s+i; return s; }|45"
    "main() { for (;;) return 3; }|3"
    "main() { a=0; for (; a<3;) a=a+1; return a; }|3"
    "main() { a=0; { a=1; a=2; } return a; }|2"
    "main() { a=0; if (1) { a=1; a=2; } return a; }|2"
    "main() { a=0; if (0) { a=1; } else { a=2; } return a; }|2"
    "main() { a=0; while (a<3) { a=a+1; a=a+1; } return a; }|4"
    "main() { s=0; for (i=0; i<3; i=i+1) { s=s+i; s=s+1; } return s; }|6"
    "main() { a=0; { a=1; { a=2; } } return a; }|2"
    "main() { a=5; {} return a; }|5"
    "main() { return ret3(); }|3"
    "main() { return add(3, 5); }|8"
    "main() { return sub(5, 3); }|2"
    "main() { return add6(1, 2, 3, 4, 5, 6); }|21"
    "main() { return sub6(100, 1, 2, 3, 4, 5); }|85"
    "main() { return add6(1, 2, add6(3,4,5,6,7,8),9,10,11); }|66"
    "main() { return add6(1,2,add6(3,add6(4,5,6,7,8,9),10,11,12,13),14,15,16); }|136"
    "main() { a=3; return add(a,4); }|7"
    "main() { return ret3() + ret3(); }|6"
    "three() { return 3; } main() { return three(); }|3"
    "plus(x, y) { return x + y; } main() { return plus(3, 5); }|8"
    "f(x) { y = x * 2; return y; } main() { return f(21); }|42"
    "sum6(a, b, c, d, e, f) { return a + b + c + d + e + f; } main() { return sum6(1, 2, 3, 4, 5, 6); }|21"
    "fib(x) { if (x <= 1) return 1; return fib(x - 1) + fib(x - 2); } main() { return fib(9); }|55"
    "main() { x=3; y=&x; return *y; }|3"
    "main() { x=3; return *&x; }|3"
    "main() { a=3; b=4; p=&a; q=&b; return *p + *q; }|7"
    "main() { x=42; p=&x; q=&p; return **q; }|42"
    "main() { x=3; y=&x; *y=5; return x; }|5"
    "main() { x=3; y=&x; *y=*y+2; return x; }|5"
    "main() { x=7; y=&x; z=&y; **z=9; return x; }|9"
    "main() { x=3; y=&x; *y=8; return *y; }|8"
    "main() { a = [10, 20, 30]; return a[0] + a[1] + a[2]; }|60"
    "main() { a = [1, 2, 3]; a[1] = 42; return a[1]; }|42"
    "main() { a = [5, 6, 7]; i = 2; return a[i]; }|7"
    "main() { a = [1, 2, 3, 4]; s = 0; for (i = 0; i < 4; i = i+ 1) s = s + a[i]; return s; }|10"
    "main() { x = 3; a = [x, x * 2]; return a[1]; }|6"
    "main() { a = [10, 20, 30]; return a.len; }|3"
    "main() { a = [7]; return a.len; }|1"
    "main() { a = [1, 2, 3, 4]; s = 0; for (i = 0; i < a.len; i = i + 1) s = s + a[i]; return s;}|10"
    "count(a) { return a.len; } main() { x = [1, 2, 3, 4, 5]; return count(x); }|5"
    "main() { n = 3; a = [0; n]; a[0] = 10; a[2] = 30; return a[0] + a[1] + a[2] + a.len; }|43"
    "main() { a = [7; 4]; return a[0] + a[3]; }|14"
    "main() { a = [5; 0]; return a.len; }|0"
    "main() { n = 5; a = [0; n]; for (i = 0; i < a.len; i = i + 1) a[i] = i * 2; s = 0; for (i = 0; i < a.len; i = i + 1) s = s + a[i]; return s; }|20"
    "make(n) { return [9; n]; } main() { a = make(3); return a[2] + a.len; }|12"
    "main() { s = \"hello\"; return s.len; }|5"
    "main() { s = \"hello\"; return s[0]; }|104"
    "main() { s = \"A\\n\"; return s[1]; }|10"
    "main() { s = \"abc\"; s[1] = 120; return s[1]; }|120"
    "main() { s = \"hello\"; c = 0; for (i = 0; i < s.len; i = i + 1) if (s[i] == 108) c = c + 1; return c;}|2"
    "main() { s = \"hi\"; putchar(s[0]); putchar(s[1]); return 0; }|0"
    "main() { s = \"\"; return s.len; }|0"
)

error_tests=(
    "main() { a = [1,2,3]; return a.5; }|不明なメンバ"
)

cc -c -o funcs.o funcs.c

one_test() {
    IFS='|' read -r input expected <<< "${tests[$1]}"
    i=$(printf '%03d' $1)
    printf '%s' "$input" > tmp$i.sp
    ./spell -S -o tmp$i tmp$i.sp
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
    ./spell -S -o etmp etmp.sp 2> etmp.err
    exit_code=$?
    if [ "$exit_code" = "1" ] && grep -q "$expected" etmp.err; then
        echo "error test passed, input $input"
    else
        echo "erorr test failed, input $input, exit $exit_code."
        fail_count=$((fail_count+1))
    fi
    n=$((n+1))
    rm -rf -f etmp.sp etmp.err etmp.s
}

run_suite() {
    cat io.sp $1 > suite_all.sp
    ./spell -S -o suite suite_all.sp
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

n=${#tests[@]}

for i in "${!tests[@]}"; do
    one_test $i &
done
wait

for i in "${!error_tests[@]}"; do
    one_error_test $i
done

fail_count=0
for i in "${!tests[@]}"; do
    i=$(printf '%03d' $i)
    if grep -q failed result$i; then
        fail_count=$((fail_count+1))
        cat result$i
    fi
    rm result$i
done

echo $(($n-$fail_count)) / $n tests passed.
if [ $fail_count -eq 0 ]; then
    exit 0
else
    exit 1
fi
