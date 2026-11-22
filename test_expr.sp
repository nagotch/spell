t001() { 3 - 45; }
t002() { 100 * 3; }
t003() { 0; }
t004() { 1; }
t005() { 42; }
t006() { 254; }
t007() { 255; }
t008() { 5+20-4; }
t009() { 10-3-2; }
t010() { 2*3+4; }
t011() { 2+3*4; }
t012() { (2+3)*4; }
t013() { 20/4; }
t014() { 7/2; }
t015() { 12 + 3 * ( 2 + 4 ); }
t016() { -10+20; }
t017() { -(3+5)+10; }
t018() { -3*+5+20; }
t019() { +42; }
t020() { -(-5); }
t021() { 2*-3+10; }
t022() { 0==1; }
t023() { 42==42; }
t024() { 0; }
t025() { 42; }
t026() { 0<1; }
t027() { 1<1; }
t028() { 2<1; }
t029() { 0<=1; }
t030() { 1<=1; }
t031() { 2<=1; }
t032() { 1>0; }
t033() { 1>1; }
t034() { 1>2; }
t035() { 1>=0; }
t036() { 1>=1; }
t037() { 1>=2; }
t038() { 1+2==3; }
t039() { 1<2==1; }

main() {
    fails = 0;

    print_int(1); putchar(10); fflush(0);
    if (t001() != -42) { print_str("NG"); print_int(1); putchar(10); fails=fails+1; }

    print_int(2); putchar(10); fflush(0);
    if (t002() != 300) { print_str("NG"); print_int(2); putchar(10); fails=fails+1; }

    print_int(3); putchar(10); fflush(0);
    if (t003() != 0) { print_str("NG"); print_int(3); putchar(10); fails=fails+1; }

    print_int(4); putchar(10); fflush(0);
    if (t004() != 1) { print_str("NG"); print_int(4); putchar(10); fails=fails+1; }

    print_int(5); putchar(10); fflush(0);
    if (t005() != 42) { print_str("NG"); print_int(5); putchar(10); fails=fails+1; }

    print_int(6); putchar(10); fflush(0);
    if (t006() != 254) { print_str("NG"); print_int(6); putchar(10); fails=fails+1; }

    print_int(7); putchar(10); fflush(0);
    if (t007() != 255) { print_str("NG"); print_int(7); putchar(10); fails=fails+1; }

    print_int(8); putchar(10); fflush(0);
    if (t008() != 21) { print_str("NG"); print_int(8); putchar(10); fails=fails+1; }

    print_int(9); putchar(10); fflush(0);
    if (t009() != 5) { print_str("NG"); print_int(9); putchar(10); fails=fails+1; }

    print_int(10); putchar(10); fflush(0);
    if (t010() != 10) { print_str("NG"); print_int(10); putchar(10); fails=fails+1; }

    print_int(11); putchar(10); fflush(0);
    if (t011() != 14) { print_str("NG"); print_int(11); putchar(10); fails=fails+1; }

    print_int(12); putchar(10); fflush(0);
    if (t012() != 20) { print_str("NG"); print_int(12); putchar(10); fails=fails+1; }

    print_int(13); putchar(10); fflush(0);
    if (t013() != 5) { print_str("NG"); print_int(13); putchar(10); fails=fails+1; }

    print_int(14); putchar(10); fflush(0);
    if (t014() != 3) { print_str("NG"); print_int(14); putchar(10); fails=fails+1; }

    print_int(15); putchar(10); fflush(0);
    if (t015() != 30) { print_str("NG"); print_int(15); putchar(10); fails=fails+1; }

    print_int(16); putchar(10); fflush(0);
    if (t016() != 10) { print_str("NG"); print_int(16); putchar(10); fails=fails+1; }

    print_int(17); putchar(10); fflush(0);
    if (t017() != 2) { print_str("NG"); print_int(17); putchar(10); fails=fails+1; }

    print_int(18); putchar(10); fflush(0);
    if (t018() != 5) { print_str("NG"); print_int(18); putchar(10); fails=fails+1; }

    print_int(19); putchar(10); fflush(0);
    if (t019() != 42) { print_str("NG"); print_int(19); putchar(10); fails=fails+1; }

    print_int(20); putchar(10); fflush(0);
    if (t020() != 5) { print_str("NG"); print_int(20); putchar(10); fails=fails+1; }

    print_int(21); putchar(10); fflush(0);
    if (t021() != 4) { print_str("NG"); print_int(21); putchar(10); fails=fails+1; }

    print_int(22); putchar(10); fflush(0);
    if (t022() != 0) { print_str("NG"); print_int(22); putchar(10); fails=fails+1; }

    print_int(23); putchar(10); fflush(0);
    if (t023() != 1) { print_str("NG"); print_int(23); putchar(10); fails=fails+1; }

    print_int(24); putchar(10); fflush(0);
    if (t024() != 0) { print_str("NG"); print_int(24); putchar(10); fails=fails+1; }

    print_int(25); putchar(10); fflush(0);
    if (t025() != 42) { print_str("NG"); print_int(25); putchar(10); fails=fails+1; }

    print_int(26); putchar(10); fflush(0);
    if (t026() != 1) { print_str("NG"); print_int(26); putchar(10); fails=fails+1; }

    print_int(27); putchar(10); fflush(0);
    if (t027() != 0) { print_str("NG"); print_int(27); putchar(10); fails=fails+1; }

    print_int(28); putchar(10); fflush(0);
    if (t028() != 0) { print_str("NG"); print_int(28); putchar(10); fails=fails+1; }

    print_int(29); putchar(10); fflush(0);
    if (t029() != 1) { print_str("NG"); print_int(29); putchar(10); fails=fails+1; }

    print_int(30); putchar(10); fflush(0);
    if (t030() != 1) { print_str("NG"); print_int(30); putchar(10); fails=fails+1; }

    print_int(31); putchar(10); fflush(0);
    if (t031() != 0) { print_str("NG"); print_int(31); putchar(10); fails=fails+1; }

    print_int(32); putchar(10); fflush(0);
    if (t032() != 1) { print_str("NG"); print_int(32); putchar(10); fails=fails+1; }

    print_int(33); putchar(10); fflush(0);
    if (t033() != 0) { print_str("NG"); print_int(33); putchar(10); fails=fails+1; }

    print_int(34); putchar(10); fflush(0);
    if (t034() != 0) { print_str("NG"); print_int(34); putchar(10); fails=fails+1; }

    print_int(35); putchar(10); fflush(0);
    if (t035() != 1) { print_str("NG"); print_int(35); putchar(10); fails=fails+1; }

    print_int(36); putchar(10); fflush(0);
    if (t036() != 1) { print_str("NG"); print_int(36); putchar(10); fails=fails+1; }

    print_int(37); putchar(10); fflush(0);
    if (t037() != 0) { print_str("NG"); print_int(37); putchar(10); fails=fails+1; }

    print_int(38); putchar(10); fflush(0);
    if (t038() != 1) { print_str("NG"); print_int(38); putchar(10); fails=fails+1; }

    print_int(39); putchar(10); fflush(0);
    if (t039() != 1) { print_str("NG"); print_int(39); putchar(10); fails=fails+1; }


    print_str("done"); putchar(10);
    return fails > 0;
}
