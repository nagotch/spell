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
)

cc -c -o funcs.o funcs.c

one_test() {
    IFS='|' read -r input expected <<< "${tests[$1]}"
    i=$(printf '%02d' $1)
    ./spell "$input" > tmp$1.s
    cc -o tmp$1 tmp$1.s funcs.o
    ./tmp$1
    exit_code=$?
    if [ "$exit_code" != "$expected" ]; then
        echo $i test failed, input $input, expected $expected, result $exit_code. > result$1
    else
        echo $i test passed. > result$1
    fi
    rm tmp$1 tmp$1.s
}

n=${#tests[@]}

for i in "${!tests[@]}"; do
    one_test $i &
done
wait

fail_count=0
for i in "${!tests[@]}"; do
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
