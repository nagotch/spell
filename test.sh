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
)
fail_count=0
i=0
for tst in "${tests[@]}"; do
    i=$(($i+1))
    IFS='|' read -r input expected <<< "$tst"
    ./spell "$input" > tmp.s
    cc -o tmp tmp.s
    ./tmp
    exit_code=$?
    if [ "$exit_code" != "$expected" ]; then
        echo number $i test failed, input $input, expected $expected, result $exit_code.
        fail_count=$((fail_count+1))
    else
        echo number $i test passed.
    fi
done

n=${#tests[@]}
echo $(($n-$fail_count)) / $n tests passed.
if [ $fail_count -eq 0 ]; then
    exit 0
else
    exit 1
fi
