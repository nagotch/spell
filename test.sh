#!/bin/bash
cc -o spell main.c

tests=(0 1 42 254 255)
faile_count=0

for i in ${tests[@]}; do
    ./spell "$i" > tmp.s
    cc -o tmp tmp.s
    ./tmp
    exit_code=$?
    if [ $exit_code -ne $i ]; then
        echo number $i test failed, expected $i, result $exit_code.
        faile_count=$((faile_count+1))
    else
        echo number $i test passed.
    fi
done

n=${#tests[@]}
echo $(($n-$faile_count)) / $n tests passed.
if [ $faile_count -eq 0 ]; then
    exit 0
else
    exit 1
fi
