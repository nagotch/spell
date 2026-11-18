#!/bin/bash
cc -o spell main.c

tests=(
    "0|0"
    "1|1"
    "42|42"
    "254|254"
    "255|255"
    "5+20-4|21"
    "10-3-2|5"
    "2*3+4|10"
    "2+3*4|14"
    "(2+3)*4|20"
    "20/4|5"
    "7/2|3"
    "12 + 3 * ( 2 + 4 )|30"
    "-10+20|10"
    "-(3+5)+10|2"
    "-3*+5+20|5"
    "+42|42"
    "-(-5)|5"
    "2*-3+10|4"
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
    cat result$i
    if grep -q failed result$i; then
        fail_count=$((fail_count+1))
    fi
    rm result$i
done

echo $(($n-$fail_count)) / $n tests passed.
if [ $fail_count -eq 0 ]; then
    exit 0
else
    exit 1
fi
