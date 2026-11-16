cc -o spell main.c

faile_count=256

for i in {0..255}
do
    ./spell "$i" > tmp.s
    cc -o tmp tmp.s
    ./tmp
    if [ $? -ne $i ]; then
        echo number $i test failed, expected $i, result $?.
        faile_count-=1
    else
        echo number $i test passed.
    fi
done

echo test $faile_count / 256 passed
