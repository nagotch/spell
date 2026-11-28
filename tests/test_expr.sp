def t001() { 3 - 45; }
def t002() { 100 * 3; }
def t003() { 0; }
def t004() { 1; }
def t005() { 42; }
def t006() { 254; }
def t007() { 255; }
def t008() { 5+20-4; }
def t009() { 10-3-2; }
def t010() { 2*3+4; }
def t011() { 2+3*4; }
def t012() { (2+3)*4; }
def t013() { 20/4; }
def t014() { 7/2; }
def t015() { 12 + 3 * ( 2 + 4 ); }
def t016() { -10+20; }
def t017() { -(3+5)+10; }
def t018() { -3*+5+20; }
def t019() { +42; }
def t020() { -(-5); }
def t021() { 2*-3+10; }
def t022() { 0==1; }
def t023() { 42==42; }
def t024() { 0; }
def t025() { 42; }
def t026() { 0<1; }
def t027() { 1<1; }
def t028() { 2<1; }
def t029() { 0<=1; }
def t030() { 1<=1; }
def t031() { 2<=1; }
def t032() { 1>0; }
def t033() { 1>1; }
def t034() { 1>2; }
def t035() { 1>=0; }
def t036() { 1>=1; }
def t037() { 1>=2; }
def t038() { 1+2==3; }
def t039() { 1<2==1; }
def t040() { 10%3; }
def t041() { 2+10%4; }
def t042() { (0-10)%3; }
def t043() { 1&&2; }
def t044() { 1||0&&0; }
def t045() { 0&&10%0; }
def t046() { a = 1; a += 2; return a; }
def t047() { a = 10; a += 2 * 3; return a; }
def t048() { a = 10; a -= 3; return a; }
def t049() { a = 3; a *= 4; return a; }
def t050() { a = 20; a /= 3; return a; }
def t051() { a = 20; a %= 3; return a; }
def t052() { 7/2; }
def t053() { (0-7)/2; }
def t054() { 7/(0-2); }
def t055() { (0-7)/(0-2); }
def t056() { 7%2; }
def t057() { (0-7)%2; }
def t058() { 7%(0-2); }
def t059() { (0-7)%(0-2); }
def t060() { (0-6)/2; }
def t061() { (0-6)%2; }
def t062() { 6/(0-2); }
def t063() { 6%(0-2); }
def t064() { (0-7)/2*2 + (0-7)%2; }
def t065() { 7/(0-2)*(0-2) + 7%(0-2); }
def t066() { a = 0-7; a /= 2; return a; }
def t067() { a = 0-7; a %= 2; return a; }
def t068() { (0-6)/(0-2); }
def t069() { (0-6)%(0-2); }
def t070() { true; }
def t071() { false; }
def t072() { T; }
def t073() { F; }
def t074() { !true; }
def t075() { !F; }
def t076() { true && false; }
def t077() { T || F; }
def t078() { true == T; }
def t079() { false != T; }
def t080() { true && (1 < 2); }
def t081() { true and false; }
def t082() { true or false; }
def t083() { false or true and false; }
def t084() { true or false and false; }
def t085() { not true; }
def t086() { not false and false; }
def t087() { not not true; }
def t088() { !false and not false; }
def t089() { true && false or not false; }
def t090() { not true == false; }
def t091() { 0 and 10 % 0; }
def t092() { 1 or 10 % 0; }

def mark(n) {
    print_int(n); putchar(10); fflush(0);
    return 0;
}

def check(got, expected) {
    if (got != expected) return 1;
    return 0;
}

def main() {
    fails = 0;

    mark(1); fails=fails+check(t001(), -42);
    mark(2); fails=fails+check(t002(), 300);
    mark(3); fails=fails+check(t003(), 0);
    mark(4); fails=fails+check(t004(), 1);
    mark(5); fails=fails+check(t005(), 42);
    mark(6); fails=fails+check(t006(), 254);
    mark(7); fails=fails+check(t007(), 255);
    mark(8); fails=fails+check(t008(), 21);
    mark(9); fails=fails+check(t009(), 5);
    mark(10); fails=fails+check(t010(), 10);
    mark(11); fails=fails+check(t011(), 14);
    mark(12); fails=fails+check(t012(), 20);
    mark(13); fails=fails+check(t013(), 5);
    mark(14); fails=fails+check(t014(), 3);
    mark(15); fails=fails+check(t015(), 30);
    mark(16); fails=fails+check(t016(), 10);
    mark(17); fails=fails+check(t017(), 2);
    mark(18); fails=fails+check(t018(), 5);
    mark(19); fails=fails+check(t019(), 42);
    mark(20); fails=fails+check(t020(), 5);
    mark(21); fails=fails+check(t021(), 4);
    mark(22); fails=fails+check(t022(), 0);
    mark(23); fails=fails+check(t023(), 1);
    mark(24); fails=fails+check(t024(), 0);
    mark(25); fails=fails+check(t025(), 42);
    mark(26); fails=fails+check(t026(), 1);
    mark(27); fails=fails+check(t027(), 0);
    mark(28); fails=fails+check(t028(), 0);
    mark(29); fails=fails+check(t029(), 1);
    mark(30); fails=fails+check(t030(), 1);
    mark(31); fails=fails+check(t031(), 0);
    mark(32); fails=fails+check(t032(), 1);
    mark(33); fails=fails+check(t033(), 0);
    mark(34); fails=fails+check(t034(), 0);
    mark(35); fails=fails+check(t035(), 1);
    mark(36); fails=fails+check(t036(), 1);
    mark(37); fails=fails+check(t037(), 0);
    mark(38); fails=fails+check(t038(), 1);
    mark(39); fails=fails+check(t039(), 1);
    mark(40); fails=fails+check(t040(), 1);
    mark(41); fails=fails+check(t041(), 4);
    mark(42); fails=fails+check(t042(), 2);
    mark(43); fails=fails+check(t043(), 1);
    mark(44); fails=fails+check(t044(), 1);
    mark(45); fails=fails+check(t045(), 0);
    mark(46); fails=fails+check(t046(), 3);
    mark(47); fails=fails+check(t047(), 16);
    mark(48); fails=fails+check(t048(), 7);
    mark(49); fails=fails+check(t049(), 12);
    mark(50); fails=fails+check(t050(), 6);
    mark(51); fails=fails+check(t051(), 2);
    mark(52); fails=fails+check(t052(), 3);
    mark(53); fails=fails+check(t053(), -4);
    mark(54); fails=fails+check(t054(), -4);
    mark(55); fails=fails+check(t055(), 3);
    mark(56); fails=fails+check(t056(), 1);
    mark(57); fails=fails+check(t057(), 1);
    mark(58); fails=fails+check(t058(), -1);
    mark(59); fails=fails+check(t059(), -1);
    mark(60); fails=fails+check(t060(), -3);
    mark(61); fails=fails+check(t061(), 0);
    mark(62); fails=fails+check(t062(), -3);
    mark(63); fails=fails+check(t063(), 0);
    mark(64); fails=fails+check(t064(), -7);
    mark(65); fails=fails+check(t065(), 7);
    mark(66); fails=fails+check(t066(), -4);
    mark(67); fails=fails+check(t067(), 1);
    mark(68); fails=fails+check(t068(), 3);
    mark(69); fails=fails+check(t069(), 0);
    mark(70); fails=fails+check(t070(), 1);
    mark(71); fails=fails+check(t071(), 0);
    mark(72); fails=fails+check(t072(), 1);
    mark(73); fails=fails+check(t073(), 0);
    mark(74); fails=fails+check(t074(), 0);
    mark(75); fails=fails+check(t075(), 1);
    mark(76); fails=fails+check(t076(), 0);
    mark(77); fails=fails+check(t077(), 1);
    mark(78); fails=fails+check(t078(), 1);
    mark(79); fails=fails+check(t079(), 1);
    mark(80); fails=fails+check(t080(), 1);
    mark(81); fails=fails+check(t081(), 0);
    mark(82); fails=fails+check(t082(), 1);
    mark(83); fails=fails+check(t083(), 0);
    mark(84); fails=fails+check(t084(), 1);
    mark(85); fails=fails+check(t085(), 0);
    mark(86); fails=fails+check(t086(), 0);
    mark(87); fails=fails+check(t087(), 1);
    mark(88); fails=fails+check(t088(), 1);
    mark(89); fails=fails+check(t089(), 1);
    mark(90); fails=fails+check(t090(), 1);
    mark(91); fails=fails+check(t091(), 0);
    mark(92); fails=fails+check(t092(), 1);

    print_str("done"); putchar(10);
    return fails > 0;
}
