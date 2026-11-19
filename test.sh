#!/bin/bash

tests=(
    "0;|0"
    "1;|1"
    "42;|42"
    "254;|254"
    "255;|255"
    "5+20-4;|21"
    "10-3-2;|5"
    "2*3+4;|10"
    "2+3*4;|14"
    "(2+3)*4;|20"
    "20/4;|5"
    "7/2;|3"
    "12 + 3 * ( 2 + 4 );|30"
    "-10+20;|10"
    "-(3+5)+10;|2"
    "-3*+5+20;|5"
    "+42;|42"
    "-(-5);|5"
    "2*-3+10;|4"
    "0==1;|0"
    "42==42;|1"
    "0!=1;|1"
    "42!=42;|0"
    "0<1;|1"
    "1<1;|0"
    "2<1;|0"
    "0<=1;|1"
    "1<=1;|1"
    "2<=1;|0"
    "1>0;|1"
    "1>1;|0"
    "1>2;|0"
    "1>=0;|1"
    "1>=1;|1"
    "1>=2;|0"
    "1+2==3;|1"
    "1<2==1;|1"
    "a=3; a;|3"
    "a=3; b=5*6-8; a+b/2;|14"
    "a=b=3; a+b;|6"
    "foo=3; foo;|3"
    "foo=3; bar=5; foo+bar;|8"
    "fo=1; foo=2; fo+foo;|3"
    "foo1=5; foo1;|5"
    "return 42;|42"
    "return 5; return 8;|5"
    "a=3; b=5*6-8; return a+b/2;|14"
    "returnx=3; returnx;|3"
    "return1=7; return1;|7"
    "if (0) return 1; return 2;|2"
    "if (1) return 1; return 2;|1"
    "if (0) return 1; else return 2;|2"
    "if (1) return 1; else return 2;|1"
    "a=0; if (1) a=1; return a;|1"
    "a=0; if (0) a=1; return a;|0"
    "a=0; while (a<10) a=a+1; return a;|10"
    "s=0; for (i=0; i<10; i=i+1) s=s+i; return s;|45"
    "for (;;) return 3;|3"
    "a=0; for (; a<3;) a=a+1; return a;|3"
)

one_test() {
    IFS='|' read -r input expected <<< "${tests[$1]}"
    i=$(printf '%02d' $1)
    ./spell "$input" > tmp$1.s
    cc -o tmp$1 tmp$1.s
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
