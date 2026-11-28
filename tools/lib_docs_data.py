# spell ライブラリリファレンスの情報源。
#
# このファイルが唯一の正本で、tools/gen_lib_docs.py がここから
# docs/lib/ の個別ページ・目次・検索インデックスを生成する。
# 内容の正しさは src/prelude.sp と src/typecheck.c の実装に従う。
#
# ページの形:
#   slug      URL のファイル名（docs/lib/<slug>.html）
#   name      表示名（素テキスト。HTML 側でエスケープされる）
#   kind      クラス / 関数 / 組み込み
#   group     目次・サイドバーのグループ名（GROUPS のいずれか）
#   summary   一行の概要（素テキスト）
#   keywords  検索用の追加キーワード
#   signature シグネチャ（素テキスト。任意）
#   intro     導入の段落（HTML 断片のリスト）
#   sections  ブロックのリスト:
#     ("h2", 見出し) / ("h3", 見出し)
#     ("p", HTML) / ("note", HTML) / ("ul", [HTML, ...])
#     ("table", [見出し...], [[セルHTML...], ...])
#     ("methods", [(シグネチャ, 説明HTML), ...] または
#                 [(シグネチャ, 説明HTML, メソッド名), ...])
#       メソッド名を付けた行には id="m-<名前>" が付き、検索対象になる
#     ("example", {"title", "code", "stdout", 任意で "stdin" / "argv" /
#                  "stderr" / "exit"})
#       例は生成時に全てコンパイル・実行され、出力が一致しなければ
#       生成が止まる。表記はコロン形・末尾セミコロンなし。
#   sugar     [("ラベル", "../sugar.html#アンカー"), ...]
#   related   [slug, ...]

GROUPS = [
    "基本の型",
    "コレクション",
    "座標と範囲",
    "組み込み",
    "入力",
    "出力",
    "引数とファイル",
    "文字",
    "初期値ファクトリ",
]

PAGES = [

# ---------------------------------------------------------------- str
{
    "slug": "str",
    "name": "str",
    "kind": "クラス",
    "group": "基本の型",
    "summary": "文字列。文字コードの列を包む組み込みクラス",
    "keywords": ["文字列", "テキスト", "string", "文字"],
    "signature": "class str",
    "intro": [
        "文字列リテラル（<code>\"...\"</code> と <code>'...'</code> の両方が使えます）を作ると、"
        "自動でこのクラスの値になります。中身は文字コードの配列で、フィールド <code>chars: [int]</code> で"
        "直接触れます。文字列は不変です。添え字への代入はできず、書き換えはメソッドが返す新しい文字列で表します。",
        "エスケープは <code>\\n</code> <code>\\t</code> <code>\\\\</code> <code>\\\"</code> <code>\\'</code> <code>\\0</code> が"
        "あり、それ以外の文字をエスケープするとその文字自体になります（<code>\\r</code> は改行ではなく文字 <code>r</code> です）。",
    ],
    "sections": [
        ("h2", "フィールド"),
        ("table", ["名前", "型", "内容"],
         [["<code>chars</code>", "<code>[int]</code>",
           "文字コードの列。そのまま読み書きできる（書き換えると文字列自体が変わる）"]]),
        ("h2", "メソッドと演算"),
        ("methods", [
            ("len", "文字数（バイト数ではなく文字コードの個数）。プロパティとして <code>s.len</code> と書く", "len"),
            ("s[i]", "i 番目の文字だけからなる長さ1の <code>str</code>。添え字は配列と同じく回り込む"
             "（<code>s[-1]</code> は最後の文字、<code>s[s.len]</code> は先頭の文字）。空文字列への添え字は不定。"
             "文字コード（<code>int</code>）が欲しいときは <code>s.chars[i]</code>。添え字への代入はできない", "getitem"),
            ("s?[i]", "安全添え字。範囲内かどうかを返し、比較の中では範囲外が比較全体を偽にする"
             "（詳しくは <a href=\"array.html\">array</a> の安全添え字の節）", "safeindex"),
            ("s[i:j]", "スライス。i 以上 j 未満の部分文字列（新しい <code>str</code>）。"
             "端は省略でき、負の添え字は末尾から数え、範囲外は端に丸める", "slice"),
            ("s + t", "連結した新しい文字列（<code>__add__</code>）", "add"),
            ("s == t / s != t", "中身の比較（<code>__eq__</code> / <code>__ne__</code>）", "eq"),
            ("s < t / s <= t", "辞書順（文字コードの列としての大小）。共通部分が等しければ短い方が小さい"
             "（<code>__lt__</code> / <code>__le__</code>）", "lt"),
            ("starts_with(t: str) -> bool", "<code>t</code> で始まるかどうか", "starts_with"),
            ("ends_with(t: str) -> bool", "<code>t</code> で終わるかどうか", "ends_with"),
            ("find(t: str) -> int", "<code>t</code> が最初に現れる位置。無ければ <code>-1</code>。空の <code>t</code> は 0", "find"),
            ("contains(t: str) -> bool", "<code>t</code> を含むかどうか", "contains"),
            ("split(sep: str) -> [str]", "<code>sep</code> で分割した配列。区切りが連続・端にある場合は"
             "空文字列が含まれる。<code>sep</code> が空文字列の場合は分割せず、自分だけからなる配列を返す", "split"),
            ("join(a: [str]) -> str", "自分の文字列を区切りにして <code>[str]</code> の要素をつないだ文字列。"
             "<code>sep.join(a)</code> の形で使う", "join"),
            ("to_int() -> int", "atoi 式の変換。先頭の空白（スペース・タブ・改行など）と符号を読み、"
             "数字が続く間だけ読む。数字以外で止まり、数字が無ければ 0", "to_int"),
            ("replace(old: str, new: str) -> str", "<code>old</code> の重ならない出現を左から全部 "
             "<code>new</code> に置き換えた新しい文字列。<code>old</code> が空文字列の場合は、"
             "各文字の間と両端に <code>new</code> を挟む（Python と同じ）", "replace"),
            ("count(sub: str) -> int", "<code>sub</code> の重ならない出現回数。空の <code>sub</code> は長さ + 1（Python と同じ）", "count"),
            ("strip() -> str", "前後の空白（スペース・タブ・改行・CR など）を取り除いた新しい文字列", "strip"),
            ("upper() -> str", "ASCII の小文字（a-z）だけを大文字に変えた新しい文字列。それ以外の文字はそのまま", "upper"),
            ("lower() -> str", "ASCII の大文字（A-Z）だけを小文字に変えた新しい文字列。それ以外の文字はそのまま", "lower"),
            ("rev", "逆順にした新しい文字列（プロパティとして <code>s.rev</code> と書く）", "rev"),
        ]),
        ("h2", "条件と反復"),
        ("p", "条件（<code>if</code> / <code>while</code> の条件など）に文字列を書くと、"
              "<code>__bool__</code> により「空でなければ真」で判定されます。"
              "<code>for c in s</code> は1文字ずつの <code>str</code> を返し、ループ変数への代入は"
              "元の文字列へ書き戻されます（代入された文字列の先頭の文字で置き換わり、空文字列なら何もしません）。"),
        ("example", {
            "title": "基本",
            "code": "s = \"hello\"\n<< s.len\n<< s[0]\n<< s[-1]\n<< s + \" world\"\n<< s == \"hello\"",
            "stdout": "5\nh\no\nhello world\ntrue\n",
        }),
        ("example", {
            "title": "文字コードを取り出す",
            "code": "s = \"Az\"\n<< s.chars[0]\n<< s.chars[1]",
            "stdout": "65\n122\n",
        }),
        ("example", {
            "title": "検索系のメソッド",
            "code": "s = \"hello world\"\n<< s.starts_with(\"hello\")\n<< s.ends_with(\"world\")\n<< s.find(\"o\")\n<< s.find(\"zz\")\n<< s.contains(\"lo w\")",
            "stdout": "true\ntrue\n4\n-1\ntrue\n",
        }),
        ("example", {
            "title": "split / join / to_int",
            "code": "<< \"a,b,,c\".split(\",\")\n<< \"-\".join([\"a\", \"b\", \"c\"])\n<< \"  -42abc\".to_int()",
            "stdout": "a b  c\na-b-c\n-42\n",
        }),
        ("example", {
            "title": "置換・整形・変換",
            "code": "<< \"hello world\".replace(\"o\", \"0\")\n<< \"abc\".replace(\"\", \"-\")\n<< \"  hi  \".strip()\n<< \"aXbXc\".count(\"X\")\n<< \"aBc\".upper()\n<< \"AbC\".lower()",
            "stdout": "hell0 w0rld\n-a-b-c-\nhi\n2\nABC\nabc\n",
        }),
        ("example", {
            "title": "スライスと反復での書き換え",
            "code": "s = \"hello\"\n<< s[1:3]\n<< s[:2]\nt = \"ABCA\"\nfor c in t:\n    if c != \"A\":\n        c = \".\"\n<< t\n<< \"abc\".rev",
            "stdout": "el\nhe\nA..A\ncba\n",
        }),
        ("h2", "StrIter — 文字列の反復子"),
        ("p", "<code>s.__iter__()</code> が返す反復子です。直接作るものではありません。"
              "<code>__next__()</code> は1文字の <code>str</code> を返し、"
              "<code>__set_current__(v)</code> が、現在位置の文字を <code>v</code> の先頭の文字で置き換えます"
              "（これが <code>for</code> のループ変数への代入の書き戻しの実体です）。"),
        ("h2", "注意"),
        ("ul", [
            "文字列に <code>__hash__</code> があるため <a href=\"dict.html\">Dict</a> のキーにできます",
            "<code>__str__</code> は自分自身を返します",
            "長さは文字コードの個数です。マルチバイト文字もコード列として数えるため、"
            "見た目の文字数とは一致しないことがあります",
        ]),
    ],
    "sugar": [
        ("スライス x[i:j]（糖衣集 §04）", "../sugar.html#sugar-slice"),
        ("安全添え字 a?[i][j]（糖衣集 §04）", "../sugar.html#sugar-safe-index"),
        ("0引数メソッドの括弧省略（糖衣集 §05）", "../sugar.html#sugar-noparen"),
    ],
    "related": ["chr", "array", "read_token", "read_line"],
},

# ---------------------------------------------------------------- int
{
    "slug": "int",
    "name": "int",
    "kind": "クラス",
    "group": "基本の型",
    "summary": "64ビット符号つき整数",
    "keywords": ["整数", "数値", "integer", "計算"],
    "signature": "class int",
    "intro": [
        "整数は64ビット符号つきで、コンパイラが直接扱う生の値です（箱詰めされません）。"
        "大きなリテラルもそのまま書けます。オーバーフローは2の補数で巻き戻り、エラーにはなりません。"
        "ゼロ除算は未定義です（多くの場合クラッシュします）。",
    ],
    "sections": [
        ("h2", "演算"),
        ("methods", [
            ("a + b / a - b / a * b", "加減乗（<code>__add__</code> / <code>__sub__</code> / <code>__mul__</code>）"),
            ("a / b", "床除算（Python 式）。<code>-7 / 2</code> は <code>-4</code>（<code>__div__</code>）", "div"),
            ("a % b", "床除算に合わせた剰余。<code>-7 % 2</code> は <code>1</code>（<code>__mod__</code>）", "mod"),
            ("a == b ほか", "比較（<code>__eq__</code> / <code>__ne__</code> / <code>__lt__</code> / <code>__le__</code>）。"
             "<code>bool</code> との比較もできます", "eq"),
            ("__hash__()", "恒等（値そのもの）。<code>Dict</code> のキーに使える", "hash"),
            ("__str__()", "10進表記。負数もそのまま変換できる", "str"),
        ]),
        ("h2", "反復"),
        ("p", "<code>for i in n</code> のように整数を反復対象にすると、<code>__iter__</code> が "
              "<code>RangeIter(0, n, 1, 0)</code> を返し、<code>0</code> から <code>n - 1</code> までを返します。"
              "<code>n</code> が 0 以下なら1回も回りません。糖衣の <code>i@n</code> や <code>@n</code> も同じ実体です。"),
        ("example", {
            "title": "除算と剰余は Python 式",
            "code": "<< 7 / 2\n<< -7 / 2\n<< -7 % 2\n<< 2 + 3 * 4",
            "stdout": "3\n-4\n1\n14\n",
        }),
        ("example", {
            "title": "大きなリテラル",
            "code": "<< 9223372036854775807\n<< -9223372036854775807 - 1",
            "stdout": "9223372036854775807\n-9223372036854775808\n",
        }),
        ("example", {
            "title": "整数の反復",
            "code": "s = 0\nfor i in 5:\n    s = s + i\n<< s",
            "stdout": "10\n",
        }),
    ],
    "sugar": [
        ("@ 系の繰り返し（糖衣集 §03）", "../sugar.html#sugar-repeat"),
    ],
    "related": ["bool", "range", "read_int", "chr"],
},

# ---------------------------------------------------------------- bool
{
    "slug": "bool",
    "name": "bool",
    "kind": "クラス",
    "group": "基本の型",
    "summary": "真偽値。true / false と短縮形の T / F",
    "keywords": ["真偽値", "真", "偽", "boolean", "条件"],
    "signature": "class bool",
    "intro": [
        "リテラルは <code>true</code> / <code>false</code> で、短縮形の <code>T</code> / <code>F</code> も同じ意味です"
        "（型注釈の位置の <code>T</code> は型パラメータのままです）。"
        "実体は <code>int</code> と同じ生の値で、真は 1、偽は 0 として扱われます。"
        "比較の結果や、<code>__bool__</code> を持つ値の条件判定の結果がこの型になります。",
    ],
    "sections": [
        ("h2", "メソッド"),
        ("methods", [
            ("__hash__()", "真は 1、偽は 0。<code>Dict</code> のキーに使える", "hash"),
            ("__str__()", "<code>true</code> / <code>false</code>", "str"),
        ]),
        ("h2", "注意"),
        ("ul", [
            "<code>int</code> との比較（<code>1 == true</code> など）はできます",
            "条件にクラス・配列・文字列を書くと、その <code>__bool__</code> で判定されます。"
            "<code>__bool__</code> の無いクラスを条件に使うのはコンパイルエラーです",
        ]),
        ("example", {
            "title": "リテラルと出力",
            "code": "<< true\n<< false\n<< T\n<< F\n<< 1 < 2\nb = 3 == 3\nif b:\n    << \"yes\"",
            "stdout": "true\nfalse\ntrue\nfalse\ntrue\nyes\n",
        }),
    ],
    "related": ["int", "print_yn"],
},

# ---------------------------------------------------------------- array
{
    "slug": "array",
    "name": "array<T>",
    "kind": "クラス",
    "group": "基本の型",
    "summary": "配列。リテラル [1, 2, 3] が作る、要素型 T の列",
    "keywords": ["配列", "リスト", "array", "列"],
    "signature": "class array<T>",
    "intro": [
        "配列リテラル <code>[1, 2, 3]</code>、詰め物つきの生成 <code>[fill; n]</code>、空の <code>[]</code> が作ります。"
        "リテラルは評価のたびに新しい配列が作られます（前に作った配列が上書きされることはありません）。"
        "<code>[fill; n]</code> の fill が配列のときは、形（リテラル・変数・呼び出し結果）によらずスロットごとに複製が入るため、"
        "行同士が共有されることはありません。",
        "配列は<strong>値</strong>です。代入・関数の引数と戻り値・リテラルの要素・辞書への格納など、値が束縛されるたびに"
        "ディープコピーされるため、<code>b = a</code> のあとで <code>a</code> を書き換えても <code>b</code> は変わりません"
        "（使いまわし＝エイリアスはありません。クラスインスタンスは対象外で、参照のままです）。",
        "配列は容量を持ちます。作りたての配列は容量＝要素数です。",
    ],
    "sections": [
        ("h2", "メソッドと演算"),
        ("methods", [
            ("len", "要素数（プロパティとして <code>a.len</code> と書く）", "len"),
            ("a[i]", "要素の読み書き。添え字は回り込みます（下の「添え字の回り込み」参照）", "getitem"),
            ("a?[i][j]...", "安全添え字。全段の範囲を検査する読み取り専用の形（下の「安全添え字」参照）", "safeindex"),
            ("a[i:j]", "スライス。i 以上 j 未満の部分配列（要素をコピーした新しい配列）。"
             "端は省略でき、負の添え字は末尾から数え、範囲外は端に丸める", "slice"),
            ("append(v) -> int", "末尾に要素を足し、新しい要素数を返す。コンパイラが処理する組み込み操作"
             "（下の「append と値セマンティクス」参照）", "append"),
            ("a + b", "連結した新しい配列（<code>__add__</code>）", "add"),
            ("a * n", "<code>n</code> 回繰り返した新しい配列（<code>__mul__</code>）。要素が配列なら各繰り返しは独立した複製になる", "mul"),
            ("a == b / a != b", "中身の比較。順序比較（<code>&lt;</code> / <code>&lt;=</code>）は無く、エラーになる", "eq"),
            ("sort()", "昇順に並べ替える（インプレース）。<strong>安定ソートではない</strong>"
             "（等しい要素の前後関係は保たれない）。要素型に <code>__lt__</code> が必要", "sort"),
            ("sorted() -> [T]", "並べ替えた新しい配列（元の配列は変わらない）。要素型に <code>__lt__</code> が必要", "sorted"),
            ("sum() -> T", "全要素を先頭から <code>+</code> で畳み込んだ値。要素型に <code>__add__</code> が必要。"
             "<strong>空の配列では結果は不定</strong>", "sum"),
            ("min() -> T / max() -> T", "最小・最大の要素。要素型に <code>__lt__</code> が必要。"
             "<strong>空の配列では結果は不定</strong>", "min"),
            ("count(v: T) -> int", "<code>v</code> と等しい要素の個数。等しさは要素型の <code>==</code> で判定する", "count"),
            ("rev", "逆順にした新しい配列（元の配列は変わらない。プロパティとして <code>a.rev</code> と書く）", "rev"),
            ("sort_range(lo: int, hi: int)", "<code>sort</code> / <code>sorted</code> の内部補助。通常は直接使わない"),
        ]),
        ("h2", "添え字の回り込み"),
        ("p", "通常の添え字 <code>a[i]</code> は、実効添え字を <code>i % a.len</code>（<code>%</code> と同じ Python 式で、"
              "負数でも 0 以上）として解釈します。そのため <code>a[-1]</code> は最後の要素、"
              "<code>a[a.len]</code> は先頭の要素を指し、読み書きのどちらでも同じです。"
              "入れ子の配列では段ごとに回り込み、座標添え字 <code>a[p]</code> も成分ごとに回り込みます。"
              "長さ 0 の配列は余りが取れないため、添え字アクセスは不定です。"
              "範囲外を厳密に判定したい場合は安全添え字を使ってください。<code>str</code> の添え字も同じく回り込みます。"),
        ("h2", "安全添え字"),
        ("p", "<code>a?[i][j]...</code> は範囲チェックつきの読み取りです。対象の直後に <code>?</code> を1回だけ置き、"
              "後ろに通常の添え字の連鎖を続けます。単独で書くと「連鎖の全段とも範囲内か」を表す <code>bool</code> になり、"
              "比較の直接の被演算子として書くと、どこかの段で範囲外なら比較全体が <code>false</code>"
              "（<code>!=</code> でも <code>false</code>）になります。対象と各添え字はちょうど1回だけ評価され、"
              "範囲チェックは浅い段から順に短絡します。対象にできるのは配列と <code>str</code> だけで、"
              "代入先には使えず、連鎖比較の中でも使えません（どちらもコンパイルエラー）。"),
        ("h2", "append と値セマンティクス"),
        ("p", "<code>append</code> は prelude のメソッドではなく、コンパイラが処理する組み込み操作です"
              "（メソッドでは呼び出し元の変数を付け替えられないため）。対象にできるのは、変数・フィールド・"
              "配列の要素など、スロットを持つ場所だけです（<code>(a + b).append(x)</code> のような一時値への append はエラー）。"
              "配列は値なので、append の効果は対象のスロットそのものにだけ及びます。他の変数が同じ配列を持っていることはなく、"
              "引数として渡した先での append も呼び出し元には伝わりません。内部では容量を持ち、容量を超える append では"
              "新しいブロックへ移して対象だけが付け替わります。"),
        ("p", "条件に配列を書くと「空でなければ真」で判定されます（<code>__bool__</code>）。"
              "配列には <code>__hash__</code> がありますが、書き換え可能なため、辞書のキーにした後で中身を変えると"
              "そのエントリは引けなくなります。"),
        ("example", {
            "title": "基本と回り込み",
            "code": "a = [10, 20, 30]\n<< a.len\n<< a[-1]\n<< a[3]\na[-1] = 99\n<< a",
            "stdout": "3\n30\n10\n10 20 99\n",
        }),
        ("example", {
            "title": "安全添え字",
            "code": "a = [10, 20, 30]\n<< a?[1]\n<< a?[3]\n<< a?[1] == 20\n<< a?[5] != 20",
            "stdout": "true\nfalse\ntrue\nfalse\n",
        }),
        ("example", {
            "title": "スライス",
            "code": "a = [10, 20, 30, 40]\n<< a[1:3]\n<< a[-2:]\n<< a[3:2].len",
            "stdout": "20 30\n30 40\n0\n",
        }),
        ("example", {
            "title": "append と値セマンティクス",
            "code": "a = [1, 2, 3]\nr = a.append(4)\n<< a\n<< r\nb = a\na.append(5)\n<< a.len\n<< b.len\ne: [int] = []\nfor i in 20:\n    e.append(i)\n<< e.len",
            "stdout": "1 2 3 4\n4\n5\n4\n20\n",
        }),
        ("example", {
            "title": "sort / sum / min / max / count",
            "code": "a = [5, 2, 9, 1, 7]\n<< a.sum()\n<< a.min()\n<< a.max()\n<< a.count(2)\na.sort()\n<< a\nb = [3, 1, 2]\n<< b.sorted()\n<< b",
            "stdout": "24\n1\n9\n1\n1 2 5 7 9\n1 2 3\n3 1 2\n",
        }),
        ("example", {
            "title": "連結・繰り返し・逆順",
            "code": "<< [1, 2] + [3]\n<< [0] * 3\n<< [1, 2, 3].rev",
            "stdout": "1 2 3\n0 0 0\n3 2 1\n",
        }),
        ("h2", "ArrayIter — 配列の反復子"),
        ("p", "<code>a.__iter__()</code> が返す反復子で、<code>Vec</code> と <code>Deque</code> の反復にも使われます。"
              "ただし <code>for</code> の対象が変数・フィールド・添え字で辿れる配列のときは、反復子を介さず元の配列"
              "そのものを回ります（借用モード）。その場合、ループ変数への代入は元の配列へ書き戻され、要素経由の"
              "書き換えも届き、容量内の <code>append</code> は反復にも増えます。<code>ArrayIter</code> 自体は受け取った"
              "時点のスナップショットを回るため、一時値の配列の反復では書き戻しはスナップショットに効きます。"),
        ("example", {
            "title": "反復と書き戻し（借用モード）",
            "code": "a = [1, 2, 3]\ns = 0\nfor x in a:\n    x = x * 10\n    s = s + x\n<< s\n<< a",
            "stdout": "60\n10 20 30\n",
        }),
    ],
    "sugar": [
        ("安全添え字 a?[i][j]（糖衣集 §04）", "../sugar.html#sugar-safe-index"),
        ("スライス x[i:j]（糖衣集 §04）", "../sugar.html#sugar-slice"),
        (".rev（糖衣集 §04）", "../sugar.html#sugar-rev"),
        ("0引数メソッドの括弧省略（糖衣集 §05）", "../sugar.html#sugar-noparen"),
    ],
    "related": ["str", "vec", "deque", "dict"],
},

# ---------------------------------------------------------------- Dict
{
    "slug": "dict",
    "name": "Dict<K, V>",
    "kind": "クラス",
    "group": "コレクション",
    "summary": "辞書。キーが無ければ既定値を生やす defaultdict 式",
    "keywords": ["辞書", "連想配列", "マップ", "dictionary", "map", "ハッシュ"],
    "signature": "class Dict<K, V>",
    "intro": [
        "構築には既定値ファクトリが必須で、<code>d: Dict&lt;str, int&gt; = Dict(zero_int)</code> のように書きます。"
        "ファクトリは「無かったキー」を生やすための既定値を作る関数で、キーごとに呼ばれます。"
        "型引数を構築に明示すれば、注釈なしでも <code>d = Dict&lt;str, int&gt;(zero_int)</code> と書けます。",
        "読み取り <code>d[k]</code> は、キーが無ければファクトリで作った既定値を登録してから返します"
        "（grow-on-read、Python の defaultdict 式）。そのため、存在確認だけのつもりの読み取りでも辞書は育ちます。"
        "存在だけを見たいときは <code>has</code> を使ってください。"
        "ファクトリの値が読み取りで観測できるため、<code>Vec</code> / <code>Deque</code> のような引数なし構築や、"
        "型を引数に渡す構築はありません。",
    ],
    "sections": [
        ("h2", "メソッドと演算"),
        ("methods", [
            ("d[k]", "読み取り。キーが無ければ既定値を生やして返す（<code>__getitem__</code>）", "getitem"),
            ("d[k] = v", "書き込み。上書きしてもキーの挿入位置は変わらない（<code>__setitem__</code>）", "setitem"),
            ("has(key: K) -> bool", "キーの有無。読み取りで生えたキーも「有り」になる", "has"),
            ("keys() -> [K]", "挿入順のキー配列のコピー", "keys"),
            ("len", "エントリ数（プロパティとして <code>d.len</code> と書く）", "len"),
        ]),
        ("h2", "キーに使える型"),
        ("p", "キーに使えるのは <code>__hash__</code> と <code>__eq__</code> を持つ型で、等しいキーは等しいハッシュを返す必要があります。"
              "<code>int</code> / <code>bool</code> / <code>str</code> / 配列 / <code>Pos2</code> / <code>Pos3</code> などが使えます。"
              "<code>Dict</code> 自身には <code>__hash__</code> が無いため、辞書をキーにはできません。"
              "また <code>__str__</code> も無いため、辞書そのものを出力することはできません。"),
        ("h2", "反復"),
        ("p", "<code>for k in d</code> はキーを挿入順に列挙します。反復は開始時点のキー並びのスナップショットに対するもので、"
              "反復中に増えたキーは列挙されません。反復子（DictIter）には書き戻しが無いため、"
              "ループ変数への代入はコンパイルエラーになります。条件に辞書を書くと「空でなければ真」で判定されます。"),
        ("example", {
            "title": "数え上げ",
            "code": "cnt: Dict<int, int> = Dict(zero_int)\na = [1, 2, 2, 3, 3, 3]\nfor v in a:\n    cnt[v] = cnt[v] + 1\n<< cnt[1]\n<< cnt[2]\n<< cnt[3]\n<< cnt[9]\n<< cnt.len",
            "stdout": "1\n2\n3\n0\n4\n",
        }),
        ("example", {
            "title": "has / keys / 反復",
            "code": "d: Dict<str, int> = Dict(zero_int)\nd[\"a\"] = 5\n<< d.has(\"a\")\n<< d.has(\"zzz\")\n<< d.keys()\nfor k in d:\n    << k",
            "stdout": "true\nfalse\na\na\n",
        }),
        ("example", {
            "title": "ラムダをファクトリにする",
            "code": "d: Dict<str, int> = Dict(fn() -> int { return 100 })\n<< d[\"missing\"]",
            "stdout": "100\n",
        }),
        ("note", "内部では <code>mixed_hash</code> / <code>find_slot</code> / <code>insert_entry</code> / "
                 "<code>grow_table</code> というメソッドも持っていますが、実装の都合で公開されているだけで、"
                 "利用側から呼ぶものではありません。"),
    ],
    "related": ["zero_int", "zero_bool", "zero_str", "pos2", "array"],
},

# ---------------------------------------------------------------- Vec
{
    "slug": "vec",
    "name": "Vec<T>",
    "kind": "クラス",
    "group": "コレクション",
    "summary": "伸びる配列。末尾への追加が均し定数時間",
    "keywords": ["可変長配列", "伸びる配列", "vector", "リスト"],
    "signature": "class Vec<T>",
    "intro": [
        "末尾への追加が容量の倍増で均し定数時間になる、可変長の配列です。"
        "通常の配列（<a href=\"array.html\">array</a>）の <code>append</code> と違い、"
        "<code>Vec</code> の <code>append</code> は本物のメソッドで、付け替えはオブジェクト内部で完結します。"
        "そのため別名や呼び出し元にも成長がそのまま見えます。",
        "構築は、引数なしの <code>v = Vec()</code>（要素型は <code>int</code>）、型引数つきの <code>v = Vec&lt;str&gt;()</code>、"
        "注釈つきの <code>v: Vec&lt;str&gt; = Vec()</code>、既定値ファクトリを渡す <code>v: Vec&lt;int&gt; = Vec(zero_int)</code>、"
        "型を引数として渡す <code>v = Vec(str)</code> の各形で書けます。"
        "引数なしの場合、バッファの埋め草は要素型の既定値（<code>int</code> は 0、<code>bool</code> は偽、"
        "<code>str</code> は空文字列、<code>Pos2</code> / <code>Pos3</code> は原点、<code>[T]</code> は新しい空配列）で補われます。"
        "要素型がクラス・関数型の場合は自動で補えないため、ファクトリを渡してください。",
    ],
    "sections": [
        ("h2", "メソッドと演算"),
        ("methods", [
            ("append(v: T)", "末尾へ追加する。戻り値はない", "append"),
            ("len", "論理要素数（プロパティとして <code>v.len</code> と書く）", "len"),
            ("v[i]", "内部バッファへの直接の読み書き（<code>__getitem__</code> / <code>__setitem__</code>）。"
             "下の注意参照", "getitem"),
            ("to_array() -> [T]", "論理要素数分の通常配列のコピー", "to_array"),
        ]),
        ("h2", "添え字の注意"),
        ("p", "<code>v[i]</code> は内部バッファへそのまま届くため、論理要素数以上の添字も検査されません。"
              "容量の範囲では埋め草（ファクトリの値）が見え、容量を超えた添え字は不定です。"
              "通常の配列のような添え字の回り込みもありません。安全に読みたい範囲は <code>len</code> で確認してください。"),
        ("p", "反復は開始時点のスナップショット（<code>to_array()</code>）に対するもので、"
              "ループ変数への代入の書き戻しもスナップショット側に効きます（本体には戻りません）。"
              "条件に書くと「空でなければ真」で判定されます。"),
        ("example", {
            "title": "基本",
            "code": "v = Vec()\nv.append(10)\nv.append(20)\nv.append(30)\n<< v.len\n<< v[0]\n<< v.to_array()",
            "stdout": "3\n10\n10 20 30\n",
        }),
        ("example", {
            "title": "型の指定と埋め草",
            "code": "v = Vec<str>()\nv.append(\"a\")\nv.append(\"b\")\nv[0] = \"z\"\n<< v[0]\n<< v[2].len",
            "stdout": "z\n0\n",
        }),
        ("example", {
            "title": "反復",
            "code": "v = Vec()\nv.append(1)\nv.append(2)\nfor x in v:\n    << x",
            "stdout": "1\n2\n",
        }),
    ],
    "related": ["array", "deque", "zero_int"],
},

# ---------------------------------------------------------------- Deque
{
    "slug": "deque",
    "name": "Deque<T>",
    "kind": "クラス",
    "group": "コレクション",
    "summary": "両端キュー。Queue としても Stack としても使える",
    "keywords": ["両端キュー", "キュー", "スタック", "queue", "stack", "BFS", "幅優先探索"],
    "signature": "class Deque<T>",
    "intro": [
        "円環バッファで実装された両端キューです。両端の出し入れは、容量の倍増を除いて定数時間です。"
        "構築は <code>Vec</code> と同じく、引数なしの <code>q = Deque()</code>（要素型は <code>int</code>）、"
        "型引数つきの <code>q = Deque&lt;str&gt;()</code>、注釈つきの <code>q: Deque&lt;str&gt; = Deque()</code>、"
        "ファクトリを渡す <code>q: Deque&lt;int&gt; = Deque(zero_int)</code>、型を引数として渡す "
        "<code>q = Deque(Pos2)</code> や <code>q = Deque([int])</code> の各形で書けます。",
        "出し入れの実体はダンダー <code>__push_back__</code> / <code>__push_front__</code> / "
        "<code>__pop_back__</code> / <code>__pop_front__</code> で、公開メソッドはその薄いラッパーです。"
        "矢印演算子でも出し入れできます。",
    ],
    "sections": [
        ("h2", "メソッドと演算"),
        ("methods", [
            ("push_back(v: T)", "末尾へ追加する（戻り値なし）", "push_back"),
            ("push_front(v: T)", "先頭へ追加する（戻り値なし）", "push_front"),
            ("pop_back() -> T", "末尾を取り出して返す", "pop_back"),
            ("pop_front() -> T", "先頭を取り出して返す", "pop_front"),
            ("front() -> T", "先頭を覗く（取り出さない）", "front"),
            ("back() -> T", "末尾を覗く（取り出さない）", "back"),
            ("len", "要素数（プロパティとして <code>q.len</code> と書く）", "len"),
            ("q[i]", "論理位置の読み取り（0 が先頭）。書き込み用の <code>__setitem__</code> は無い", "getitem"),
            ("to_array() -> [T]", "先頭から末尾の順の通常配列のコピー", "to_array"),
        ]),
        ("h2", "注意"),
        ("ul", [
            "<strong>空のときに <code>pop_front</code> / <code>pop_back</code> / <code>front</code> / <code>back</code> "
            "を呼ぶのは誤りで、結果は不定です</strong>（検査はしません）。呼ぶ前に <code>len</code> を確認してください",
            "反復と <code>to_array()</code> は開始時点のスナップショットに対するものです",
            "条件に書くと「空でなければ真」で判定されるため、<code>while q:</code> で空になるまで回せます",
        ]),
        ("example", {
            "title": "キューとして使う（矢印演算子）",
            "code": "q: Deque<int> = Deque(zero_int)\nq <- 1\nq <- 2\nq.push_front(0)\n<< q.len\n<< q.front()\n<< q.back()\nx <- q\n<< x\n30 -> q\nq -> y\n<< y\n<< q.to_array()",
            "stdout": "3\n0\n2\n0\n2\n30 1\n",
        }),
        ("example", {
            "title": "スタックとして使う",
            "code": "s = Deque()\ns.push_back(1)\ns.push_back(2)\n<< s.pop_back()\n<< s.pop_back()\n<< s.len",
            "stdout": "2\n1\n0\n",
        }),
        ("example", {
            "title": "空になるまで回す",
            "code": "q = Deque()\nq.push_back(1)\nq.push_back(2)\nq.push_back(3)\nn = 0\nwhile q:\n    q.pop_front()\n    n = n + 1\n<< n",
            "stdout": "3\n",
        }),
    ],
    "sugar": [
        ("矢印演算子 <- / ->（糖衣集 §04）", "../sugar.html#sugar-arrow"),
    ],
    "related": ["vec", "array", "pos2", "zero_int"],
},

# ---------------------------------------------------------------- SegTree
{
    "slug": "segtree",
    "name": "SegTree<T>",
    "kind": "クラス",
    "group": "コレクション",
    "summary": "セグメント木。点更新と区間への二項演算がどちらも O(log n)",
    "keywords": ["セグメント木", "区間和", "区間最小", "区間最大", "RMQ", "gcd", "segment tree"],
    "signature": "class SegTree<T>",
    "intro": [
        "セグメント木です。区間への二項演算 <code>op</code> とその単位元 <code>e</code> を持ち、"
        "1点の更新と、半開区間 <code>[l, r)</code> への演算（積）をどちらも O(log n) で行います。"
        "演算は要素型では決められない（同じ <code>int</code> でも和・最小・最大・gcd と変わる）ため、"
        "コンストラクタで関数値として受け取ります（AtCoder Library と同じ方式）。",
        "構築は <code>t = SegTree(n, e, op)</code>（長さ <code>n</code>、全要素 <code>e</code>）と、"
        "<code>t = SegTree(a, e, op)</code>（配列 <code>a</code> から構築）の2通りです。"
        "<code>op</code> には名前付き関数か、<code>fn</code> 型の注釈つき変数へ入れたラムダを渡してください。"
        "型変数を含む引数型へラムダを直接渡す書き方は、型検査の制限で通りません。",
    ],
    "sections": [
        ("h2", "メソッドと演算"),
        ("methods", [
            ("SegTree(n: int, e: T, op: fn(T, T) -> T)", "長さ <code>n</code>、全要素 <code>e</code> で構築する", "init"),
            ("SegTree(a: [T], e: T, op: fn(T, T) -> T)", "配列 <code>a</code> の内容で構築する", "init_array"),
            ("set(i: int, x: T)", "位置 <code>i</code> の要素を <code>x</code> に置き換える", "set"),
            ("get(i: int) -> T", "位置 <code>i</code> の要素を返す", "get"),
            ("prod(l: int, r: int) -> T", "半開区間 <code>[l, r)</code> の積 <code>op(a[l], ..., a[r-1])</code>。空区間は <code>e</code>", "prod"),
            ("all_prod() -> T", "全区間の積", "all_prod"),
            ("len", "要素数（プロパティとして <code>t.len</code> と書く）", "len"),
        ]),
        ("h2", "注意"),
        ("ul", [
            "<strong>添字・区間の範囲外アクセスは未定義です</strong>（他のコンテナと同様、検査はしません）。"
            "<code>0 &lt;= l &lt;= r &lt;= len</code> の範囲で使ってください",
            "<code>op</code> は結合法則を満たし、<code>e</code> がその単位元であることが前提です"
            "（<code>op(e, x) == op(x, e) == x</code>）。交換法則は不要で、左から右への順序が保たれます",
            "要素型 <code>T</code> が配列の場合、要素は値セマンティクスで複製されます",
        ]),
        ("example", {
            "title": "区間和",
            "code": "def add(a: int, b: int) -> int:\n    return a + b\nt = SegTree([3, 1, 4, 1, 5, 9, 2, 6], 0, add)\n<< t.prod(0, 8)\n<< t.prod(2, 5)\nt.set(4, 10)\n<< t.prod(2, 5)\n<< t.all_prod()\n<< t.get(7)",
            "stdout": "31\n10\n15\n36\n6\n",
        }),
        ("example", {
            "title": "区間 min",
            "code": "def imin(a: int, b: int) -> int:\n    if a < b:\n        return a\n    return b\nt = SegTree(6, 1000000000, imin)\nt.set(0, 5)\nt.set(1, 2)\nt.set(2, 8)\nt.set(3, 1)\nt.set(4, 9)\nt.set(5, 3)\n<< t.prod(0, 6)\n<< t.prod(0, 3)\n<< t.prod(3, 6)",
            "stdout": "1\n2\n1\n",
        }),
        ("example", {
            "title": "区間 gcd",
            "code": "def gcd(a: int, b: int) -> int:\n    while b != 0:\n        tmp = a % b\n        a = b\n        b = tmp\n    return a\nt = SegTree([12, 18, 30, 7], 0, gcd)\n<< t.all_prod()\n<< t.prod(0, 3)\n<< t.prod(3, 4)",
            "stdout": "1\n6\n7\n",
        }),
    ],
    "related": ["lazysegtree", "array", "vec"],
},

# ---------------------------------------------------------------- LazySegTree
{
    "slug": "lazysegtree",
    "name": "LazySegTree<T, F>",
    "kind": "クラス",
    "group": "コレクション",
    "summary": "遅延セグメント木。区間への作用の適用と区間の積がどちらも O(log n)",
    "keywords": ["遅延セグメント木", "遅延評価", "区間加算", "区間代入", "lazy segment tree", "Act"],
    "signature": "class LazySegTree<T, F>",
    "intro": [
        "遅延評価つきのセグメント木です。<code>SegTree</code> の演算 <code>op</code> / 単位元 <code>e</code> に加えて、"
        "区間への作用（型 <code>F</code>）を扱い、区間への作用の適用と区間の積をどちらも O(log n) で行います。",
        "作用まわりの3点セット（作用の単位元 <code>idf</code>、作用を要素へ適用する <code>mapping</code>、"
        "作用同士の合成 <code>composition</code>）は <code>Act&lt;T, F&gt;</code> にまとめて渡します。"
        "spell の引数は <code>self</code> を含めて6個までという制限があり、必要な6点をそのままでは渡せないため、この形になっています。"
        "<code>Act</code> は注釈つきの変数に入れてから渡すのが確実です"
        "（<code>act: Act&lt;int, int&gt; = Act(0, add_map, add)</code> のように）。",
        "<code>composition(f, g)</code> は「<code>g</code> を適用した後に <code>f</code> を適用する」合成を返します"
        "（AtCoder Library と同じ規約）。<code>op</code> / <code>mapping</code> / <code>composition</code> には、"
        "名前付き関数か <code>fn</code> 型の注釈つき変数へ入れたラムダを渡してください。",
    ],
    "sections": [
        ("h2", "Act<T, F> — 作用の3点セット"),
        ("methods", [
            ("Act(idf: F, mapping: fn(F, T) -> T, composition: fn(F, F) -> F)",
             "作用の単位元、要素への適用、作用同士の合成をまとめる", "act_init"),
            ("idf", "作用の単位元。<code>mapping(idf, x) == x</code> が必要", "act_idf"),
            ("mapping", "<code>mapping(f, x)</code> は要素 <code>x</code> へ作用 <code>f</code> を適用した値", "act_mapping"),
            ("composition", "<code>composition(f, g)</code> は「<code>g</code> の後に <code>f</code>」の合成", "act_composition"),
        ]),
        ("h2", "メソッドと演算"),
        ("methods", [
            ("LazySegTree(n: int, e: T, op: fn(T, T) -> T, act: Act<T, F>)", "長さ <code>n</code>、全要素 <code>e</code> で構築する", "init"),
            ("LazySegTree(a: [T], e: T, op: fn(T, T) -> T, act: Act<T, F>)", "配列 <code>a</code> の内容で構築する", "init_array"),
            ("set(i: int, x: T)", "位置 <code>i</code> の要素を <code>x</code> に置き換える（積まれた作用は押し下げてから置く）", "set"),
            ("get(i: int) -> T", "位置 <code>i</code> の要素を返す", "get"),
            ("prod(l: int, r: int) -> T", "半開区間 <code>[l, r)</code> の積。空区間は <code>e</code>", "prod"),
            ("all_prod() -> T", "全区間の積", "all_prod"),
            ("apply(i: int, f: F)", "位置 <code>i</code> の1点へ作用 <code>f</code> を適用する", "apply"),
            ("apply(l: int, r: int, f: F)", "半開区間 <code>[l, r)</code> へ作用 <code>f</code> を適用する", "apply_range"),
            ("len", "要素数（プロパティとして <code>t.len</code> と書く）", "len"),
        ]),
        ("h2", "注意"),
        ("ul", [
            "<strong>添字・区間の範囲外アクセスは未定義です</strong>（検査はしません）",
            "<code>mapping(f, x)</code> は、作用後のノード値が作用前のノード値 <code>x</code> だけから決まるものでなければなりません。"
            "例えば区間代入で区間和を取りたい場合は、要素を (合計, 長さ) の組で持ち、<code>mapping</code> が長さを使える形にします"
            "（下の「区間代入＋区間和」の例）",
            "<code>mapping(idf, x) == x</code>、<code>composition(idf, f) == f</code>、"
            "<code>composition(f, idf) == f</code>、<code>composition</code> の結合則、そして作用と演算の整合性 "
            "<code>mapping(f, op(x, y)) == op(mapping(f, x), mapping(f, y))</code> の成立が前提で、"
            "遅延の押し下げはこの前提のもとで無条件に行われます",
        ]),
        ("example", {
            "title": "区間 add＋区間和",
            "code": "def add(a: int, b: int) -> int:\n    return a + b\ndef psum(a: Pos2, b: Pos2) -> Pos2:\n    return Pos2(a.x + b.x, a.y + b.y)\ndef psum_map(f: int, x: Pos2) -> Pos2:\n    return Pos2(x.x + f * x.y, x.y)\nact: Act<Pos2, int> = Act(0, psum_map, add)\na: [Pos2] = [Pos2(1, 1), Pos2(2, 1), Pos2(3, 1), Pos2(4, 1), Pos2(5, 1)]\nt = LazySegTree(a, Pos2(0, 0), psum, act)\n<< t.prod(0, 5).x\nt.apply(1, 4, 10)\n<< t.prod(0, 5).x\n<< t.prod(1, 4).x\n<< t.get(2).x\nt.apply(2, 100)\n<< t.all_prod().x",
            "stdout": "15\n45\n39\n13\n145\n",
        }),
        ("example", {
            "title": "区間 add＋区間 max",
            "code": "def imax(a: int, b: int) -> int:\n    if a > b:\n        return a\n    return b\ndef add_map(f: int, x: int) -> int:\n    return x + f\ndef add(a: int, b: int) -> int:\n    return a + b\nact: Act<int, int> = Act(0, add_map, add)\nt = LazySegTree([3, 1, 4, 1, 5], -1000000000, imax, act)\n<< t.all_prod()\nt.apply(0, 3, 10)\n<< t.all_prod()\n<< t.prod(3, 5)\n<< t.get(1)",
            "stdout": "5\n14\n5\n11\n",
        }),
        ("example", {
            "title": "区間代入＋区間和",
            "code": "def psum(a: Pos2, b: Pos2) -> Pos2:\n    return Pos2(a.x + b.x, a.y + b.y)\ndef asg_map(f: int, x: Pos2) -> Pos2:\n    if f < 0:\n        return x\n    return Pos2(f * x.y, x.y)\ndef asg_comp(f: int, g: int) -> int:\n    if f < 0:\n        return g\n    return f\nact: Act<Pos2, int> = Act(-1, asg_map, asg_comp)\na: [Pos2] = [Pos2(1, 1), Pos2(2, 1), Pos2(3, 1), Pos2(4, 1)]\nt = LazySegTree(a, Pos2(0, 0), psum, act)\nt.apply(0, 4, 7)\n<< t.all_prod().x\nt.apply(1, 3, 2)\n<< t.prod(0, 4).x\n<< t.get(0).x\n<< t.get(1).x",
            "stdout": "28\n18\n7\n2\n",
        }),
        ("p", "代入の例では、作用 <code>F</code> を「代入する値そのもの」とし、単位元を <code>-1</code> で表しています"
              "（代入する値は 0 以上だけ、という約束のもと）。<code>mapping</code> は要素の長さ <code>x.y</code> を使って"
              "区間の合計を計算し直します。"),
    ],
    "related": ["segtree", "pos2", "array"],
},

# ---------------------------------------------------------------- Pos2
{
    "slug": "pos2",
    "name": "Pos2",
    "kind": "クラス",
    "group": "座標と範囲",
    "summary": "二次元座標。リテラル (1, 2) が作る",
    "keywords": ["座標", "二次元", "グリッド", "position", "ベクトル"],
    "signature": "class Pos2 { x: int, y: int }",
    "intro": [
        "グリッド上の座標を表すクラスです。式の位置で <code>(1, 2)</code> と書くとこの型のリテラルになります"
        "（4つ以上の要素はエラーです）。要素ごとの演算と比較を備え、"
        "<code>__hash__</code> があるため <a href=\"dict.html\">Dict</a> のキーにできます。",
    ],
    "sections": [
        ("h2", "フィールド"),
        ("table", ["名前", "型", "内容"],
         [["<code>x</code>", "<code>int</code>", "第0成分"],
          ["<code>y</code>", "<code>int</code>", "第1成分"]]),
        ("h2", "メソッドと演算"),
        ("methods", [
            ("p + q / p - q", "要素ごとの加減（<code>__add__</code> / <code>__sub__</code>）", "add"),
            ("p * n", "<code>int</code> とのスカラー倍（<code>__mul__</code>）", "mul"),
            ("p == q / p != q", "要素ごとの等価性", "eq"),
            ("p < q / p <= q", "辞書順（x、次に y の順で比較）", "lt"),
            ("p[i]", "成分の読み取り。<code>p[0]</code> が x、<code>p[1]</code> が y。範囲外は検査されない", "getitem"),
            ("__hash__()", "成分からのハッシュ。<code>Dict</code> のキーに使える", "hash"),
            ("__str__()", "空白区切り（<code>\"1 2\"</code> の形）", "str"),
        ]),
        ("h2", "座標添え字 a[p]"),
        ("p", "入れ子配列への添え字としても使えます。<code>a[p]</code> は <code>a[p[0]][p[1]]</code> と同じで、"
              "<strong>第0成分が外側（グリッドでは行）</strong>です。添え字は段ごとに回り込みます。"
              "座標はちょうど1回だけ評価され、代入先（<code>a[p] = v</code>）でも使えます。"
              "安全添え字 <code>a?[p]</code> の形もあり、その場合は座標の段も含めて範囲が検査されます。"),
        ("h2", "注意"),
        ("ul", [
            "<code>__bool__</code> は無いため、座標を条件に書くのはコンパイルエラーです",
            "ゼロ値は <a href=\"zero_pos2.html\">zero_pos2()</a> で、"
            "<code>Deque&lt;Pos2&gt;()</code> のような引数なし構築の埋め草にも自動で使われます",
        ]),
        ("example", {
            "title": "演算と辞書のキー",
            "code": "p = (1, 2)\nq = p + (0, 1)\n<< q\n<< q * 2\n<< p[0]\n<< p[1]\n<< p == (1, 2)\nd: Dict<Pos2, int> = Dict(zero_int)\nd[p] = 7\n<< d[(1, 2)]\n<< d[(3, 3)]",
            "stdout": "1 3\n2 6\n1\n2\ntrue\n7\n0\n",
        }),
        ("example", {
            "title": "座標添え字",
            "code": "g = [[1, 2], [3, 4]]\np = (1, 0)\n<< g[p]\n<< g[(0, 1)]",
            "stdout": "3\n2\n",
        }),
    ],
    "sugar": [
        ("座標リテラル (1, 2)（糖衣集 §04）", "../sugar.html#sugar-pos-literal"),
        ("座標添え字 a[p]（糖衣集 §04）", "../sugar.html#sugar-pos-index"),
        ("安全添え字 a?[i][j]（糖衣集 §04）", "../sugar.html#sugar-safe-index"),
    ],
    "related": ["pos3", "dict", "deque", "zero_pos2"],
},

# ---------------------------------------------------------------- Pos3
{
    "slug": "pos3",
    "name": "Pos3",
    "kind": "クラス",
    "group": "座標と範囲",
    "summary": "三次元座標。リテラル (1, 2, 3) が作る",
    "keywords": ["座標", "三次元", "グリッド", "position", "ベクトル"],
    "signature": "class Pos3 { x: int, y: int, z: int }",
    "intro": [
        "<a href=\"pos2.html\">Pos2</a> の三次元版です。式の位置で <code>(1, 2, 3)</code> と書くとこの型のリテラルになります。"
        "要素ごとの演算と比較を備え、<code>__hash__</code> があるため <a href=\"dict.html\">Dict</a> のキーにできます。",
    ],
    "sections": [
        ("h2", "フィールド"),
        ("table", ["名前", "型", "内容"],
         [["<code>x</code>", "<code>int</code>", "第0成分"],
          ["<code>y</code>", "<code>int</code>", "第1成分"],
          ["<code>z</code>", "<code>int</code>", "第2成分"]]),
        ("h2", "メソッドと演算"),
        ("methods", [
            ("p + q / p - q", "要素ごとの加減（<code>__add__</code> / <code>__sub__</code>）", "add"),
            ("p * n", "<code>int</code> とのスカラー倍（<code>__mul__</code>）", "mul"),
            ("p == q / p != q", "要素ごとの等価性", "eq"),
            ("p < q / p <= q", "辞書順（x、次に y、次に z の順で比較）", "lt"),
            ("p[i]", "成分の読み取り。<code>p[0]</code> が x、<code>p[2]</code> が z。範囲外は検査されない", "getitem"),
            ("__hash__()", "成分からのハッシュ。<code>Dict</code> のキーに使える", "hash"),
            ("__str__()", "空白区切り（<code>\"1 2 3\"</code> の形）", "str"),
        ]),
        ("h2", "注意"),
        ("ul", [
            "入れ子配列への添え字 <code>a[p]</code> は <code>a[p[0]][p[1]][p[2]]</code> と同じです（第0成分が外側）",
            "<code>__bool__</code> は無いため、座標を条件に書くのはコンパイルエラーです",
            "ゼロ値は <a href=\"zero_pos3.html\">zero_pos3()</a> です",
        ]),
        ("example", {
            "title": "基本",
            "code": "p = (1, 2, 3)\n<< p + (10, 20, 30)\n<< p * 2\n<< p[2]\n<< p.x\n<< p.y\n<< p.z",
            "stdout": "11 22 33\n2 4 6\n3\n1\n2\n3\n",
        }),
    ],
    "sugar": [
        ("座標リテラル (1, 2)（糖衣集 §04）", "../sugar.html#sugar-pos-literal"),
        ("座標添え字 a[p]（糖衣集 §04）", "../sugar.html#sugar-pos-index"),
    ],
    "related": ["pos2", "dict", "zero_pos3"],
},

# ---------------------------------------------------------------- Range
{
    "slug": "range",
    "name": "Range",
    "kind": "クラス",
    "group": "座標と範囲",
    "summary": "範囲。a..b / a..=b が作る",
    "keywords": ["範囲", "区間", "range", "繰り返し"],
    "signature": "class Range { lo: int, hi: int, incl: int }",
    "intro": [
        "<code>a..b</code> は終端を含まない範囲、<code>a..=b</code> は終端を含む範囲で、値はこの型になります。"
        "変数に保持できます。<code>a &gt; b</code> のときは降順になります（並びの集合は昇順の場合と同じで、順序だけが逆です。"
        "たとえば <code>5..0</code> は 4, 3, 2, 1, 0）。",
    ],
    "sections": [
        ("h2", "フィールドとメソッド"),
        ("methods", [
            ("lo", "開始値（フィールド）", "lo"),
            ("hi", "終端値（フィールド）", "hi"),
            ("incl", "終端を含むなら 1、含まないなら 0（フィールド）", "incl"),
            ("rev", "向きを反転した範囲（プロパティとして <code>r.rev</code> と書く）", "rev"),
        ]),
        ("h2", "注意"),
        ("ul", [
            "<code>Range</code> に <code>__str__</code> は無いので、そのまま出力はできません",
            "反復子 RangeIter には書き戻し（<code>__set_current__</code>）が無いため、"
            "<code>for</code> のループ変数への代入はコンパイルエラーになります",
        ]),
        ("example", {
            "title": "降順と rev",
            "code": "for i in 5..0:\n    << i\nr = 1..4\nfor i in r.rev:\n    << i\n<< r.lo\n<< r.hi\n<< r.incl",
            "stdout": "4\n3\n2\n1\n0\n3\n2\n1\n1\n4\n0\n",
        }),
        ("example", {
            "title": "終端を含む範囲",
            "code": "for i in 1..=3:\n    << i",
            "stdout": "1\n2\n3\n",
        }),
        ("h2", "RangeIter — 範囲の反復子"),
        ("p", "<code>Range.__iter__()</code> と <code>int.__iter__()</code> が返す反復子です。直接作るものではありません。"
              "昇順・降順の両方を扱い、終端を含む範囲では端の値を返し終えたことを内部の印で覚えます。"),
    ],
    "sugar": [
        ("@ 系の繰り返し（糖衣集 §03）", "../sugar.html#sugar-repeat"),
        (".rev（糖衣集 §04）", "../sugar.html#sugar-rev"),
    ],
    "related": ["int"],
},

# ---------------------------------------------------------------- FileData
{
    "slug": "filedata",
    "name": "FileData",
    "kind": "クラス",
    "group": "引数とファイル",
    "summary": "read_file の結果。ok と text を持つ",
    "keywords": ["ファイル", "読み込み結果", "file"],
    "signature": "class FileData { ok: bool, text: str }",
    "intro": [
        "<a href=\"read_file.html\">read_file</a> が返す結果の型です。直接構築するものではありません。",
    ],
    "sections": [
        ("h2", "フィールド"),
        ("table", ["名前", "型", "内容"],
         [["<code>ok</code>", "<code>bool</code>", "開けて最後まで読めたときだけ真。空のファイルは真で <code>text</code> は空"],
          ["<code>text</code>", "<code>str</code>", "ファイル全体の内容。読めなかったときは空"]]),
        ("example", {
            "title": "読み書きの結果を見る",
            "code": "ok = write_file(\"memo.txt\", \"abc\")\nf = read_file(\"memo.txt\")\n<< ok\n<< f.ok\n<< f.text\ng = read_file(\"nope.txt\")\n<< g.ok\n<< g.text.len",
            "stdout": "true\ntrue\nabc\nfalse\n0\n",
        }),
    ],
    "related": ["read_file", "write_file"],
},

# ---------------------------------------------------------------- print
{
    "slug": "print",
    "name": "print",
    "kind": "組み込み",
    "group": "組み込み",
    "summary": "値を文字列化して改行つきで標準出力へ出す",
    "keywords": ["出力", "表示", "print", "標準出力"],
    "signature": "print(x)",
    "intro": [
        "prelude の関数ではなく、コンパイラが直接書き換える組み込みです。引数は1つだけです。"
        "値を文字列化して、改行つきで標準出力へ出します。複数値を空白区切りで出したいときは文形の "
        "<code>put</code> を使います。",
        "文字列化の規則: 文字列はそのまま、数値は10進、<code>bool</code> は <code>true</code> / <code>false</code>、"
        "配列は要素を空白区切り（入れ子も平らに）、クラスは <code>__str__</code> の結果です。"
        "<code>__str__</code> の無い型（<code>Range</code>、<code>Dict</code> など）はエラーになります。",
    ],
    "sections": [
        ("example", {
            "title": "いろいろな値の出力",
            "code": "print(42)\nprint(\"abc\")\nprint([1, 2, 3])\nprint(1 < 2)\nx = (3, 4)\nprint(x)",
            "stdout": "42\nabc\n1 2 3\ntrue\n3 4\n",
        }),
    ],
    "sugar": [
        ("<< 式（糖衣集 §01）", "../sugar.html#sugar-print"),
        ("put 値, 値, ...（糖衣集 §01）", "../sugar.html#sugar-put"),
    ],
    "related": ["eprint", "print_int", "print_str", "print_yn"],
},

# ---------------------------------------------------------------- eprint
{
    "slug": "eprint",
    "name": "eprint",
    "kind": "組み込み",
    "group": "組み込み",
    "summary": "print と同じ文字列化で、標準エラー出力へ出す",
    "keywords": ["出力", "標準エラー", "stderr", "警告"],
    "signature": "eprint(x)",
    "intro": [
        "コンパイラ組み込みです。<a href=\"print.html\">print</a> と同じ文字列化で、標準エラー出力へ改行つきで出します。"
        "引数は1つだけです。",
    ],
    "sections": [
        ("example", {
            "title": "標準出力と標準エラーは分かれる",
            "code": "eprint(\"warn\")\n<< \"out\"",
            "stdout": "out\n",
            "stderr": "warn\n",
        }),
    ],
    "related": ["print"],
},

# ---------------------------------------------------------------- exit
{
    "slug": "exit",
    "name": "exit",
    "kind": "組み込み",
    "group": "組み込み",
    "summary": "その場でプログラムを終了する",
    "keywords": ["終了", "終了コード", "exit", "打ち切り"],
    "signature": "exit(code: int)",
    "intro": [
        "コンパイラ組み込みです。引数のコードでその場で終了し、以降の文は実行されません。"
        "戻らないため、戻り値の型はありません。引数は <code>int</code> が1つだけです。",
    ],
    "sections": [
        ("example", {
            "title": "途中で終了する",
            "code": "<< \"before\"\nexit(3)\n<< \"after\"",
            "stdout": "before\n",
            "exit": 3,
        }),
    ],
    "related": ["print"],
},

# ---------------------------------------------------------------- min
{
    "slug": "min",
    "name": "min",
    "kind": "組み込み",
    "group": "組み込み",
    "summary": "引数のうち最も小さい値を返す（可変引数）",
    "keywords": ["最小", "最小値", "min", "比較"],
    "signature": "min(a, b, ...)",
    "intro": [
        "prelude の関数ではなく、コンパイラが直接書き換える組み込みです。引数は1個以上で、個数に上限はありません。"
        "実行時に配列は作らないため、ループの中で呼んでもメモリ確保は発生しません。",
        "すべての引数が同じ型で、その型の順序（<code>__lt__</code>）で最も小さいものを返します。"
        "<code>int</code> のほか、<code>str</code>（辞書式）や <code>Pos2</code> / <code>Pos3</code>（成分の辞書式）など、"
        "<code>__lt__</code> を持つ型に使えます。配列メソッドの <code>min()</code> と意味論は同じです。",
        "引数は左から右へ、ちょうど1回ずつ評価されます。型の揃わない引数を混ぜるとコンパイルエラーになります。"
        "同名のユーザー定義関数があれば、そちらが優先されます。",
    ],
    "sections": [
        ("example", {
            "title": "基本",
            "code": "<< min(3, 5)\n<< min(4, 2, 8, 1)\n<< min(\"b\", \"a\", \"c\")",
            "stdout": "3\n1\na\n",
        }),
        ("example", {
            "title": "1個だけでもよい",
            "code": "<< min(7)",
            "stdout": "7\n",
        }),
    ],
    "related": ["max", "abs", "array"],
},

# ---------------------------------------------------------------- max
{
    "slug": "max",
    "name": "max",
    "kind": "組み込み",
    "group": "組み込み",
    "summary": "引数のうち最も大きい値を返す（可変引数）",
    "keywords": ["最大", "最大値", "max", "比較"],
    "signature": "max(a, b, ...)",
    "intro": [
        "prelude の関数ではなく、コンパイラが直接書き換える組み込みです。引数は1個以上で、個数に上限はありません。"
        "実行時に配列は作らないため、ループの中で呼んでもメモリ確保は発生しません。",
        "すべての引数が同じ型で、その型の順序（<code>__lt__</code>）で最も大きいものを返します。"
        "<code>int</code> のほか、<code>str</code>（辞書式）や <code>Pos2</code> / <code>Pos3</code>（成分の辞書式）など、"
        "<code>__lt__</code> を持つ型に使えます。配列メソッドの <code>max()</code> と意味論は同じです。",
        "引数は左から右へ、ちょうど1回ずつ評価されます。型の揃わない引数を混ぜるとコンパイルエラーになります。"
        "同名のユーザー定義関数があれば、そちらが優先されます。",
    ],
    "sections": [
        ("example", {
            "title": "基本",
            "code": "<< max(3, 5)\n<< max(4, 2, 8, 1)\n<< max(\"b\", \"a\", \"c\")",
            "stdout": "5\n8\nc\n",
        }),
    ],
    "related": ["min", "abs", "array"],
},

# ---------------------------------------------------------------- abs
{
    "slug": "abs",
    "name": "abs",
    "kind": "関数",
    "group": "組み込み",
    "summary": "int の絶対値を返す",
    "keywords": ["絶対値", "abs", "数値"],
    "signature": "def abs(x: int) -> int",
    "intro": [
        "prelude の関数です。<code>int</code> の絶対値を返します。",
        "最も小さい <code>int</code>（-9223372036854775808）の絶対値は <code>int</code> の範囲に収まらないため未定義です"
        "（そのままオーバーフローします）。",
    ],
    "sections": [
        ("example", {
            "title": "基本",
            "code": "<< abs(-5)\n<< abs(5)\n<< abs(0)\n<< abs(3 - 10)",
            "stdout": "5\n5\n0\n7\n",
        }),
    ],
    "related": ["min", "max", "int"],
},

# ---------------------------------------------------------------- chr
{
    "slug": "chr",
    "name": "chr",
    "kind": "関数",
    "group": "文字",
    "summary": "文字コードから長さ1の文字列を作る",
    "keywords": ["文字コード", "文字", "character", "コード"],
    "signature": "def chr(n: int) -> str",
    "intro": [
        "文字コード（<code>int</code>）から長さ1の <code>str</code> を作ります。"
        "<code>s.chars[i]</code>（コードを取り出す方向）の逆です。",
        "出力時はコードの下位バイトが書かれるため、実用範囲は 0〜255（文字としては ASCII の 0〜127）です。"
        "範囲外の扱いは未定義です。",
    ],
    "sections": [
        ("example", {
            "title": "基本",
            "code": "<< chr(65)\n<< chr(97) + chr(98)\ns = \"A\"\n<< s.chars[0]",
            "stdout": "A\nab\n65\n",
        }),
    ],
    "related": ["str", "int"],
},

# ---------------------------------------------------------------- args
{
    "slug": "args",
    "name": "args",
    "kind": "関数",
    "group": "引数とファイル",
    "summary": "コマンドライン引数全体を [str] で返す",
    "keywords": ["コマンドライン引数", "引数", "argv"],
    "signature": "def args() -> [str]",
    "intro": [
        "コマンドライン引数全体を返します。<code>[0]</code> はプログラム名（起動されたときの文字列そのまま）で、"
        "C の <code>argv</code> / Python の <code>sys.argv</code> と同じ並びです。",
    ],
    "sections": [
        ("example", {
            "title": "引数を受け取る",
            "code": "a = args()\n<< a.len\n<< a[1]",
            "argv": ["xyz"],
            "stdout": "2\nxyz\n",
        }),
    ],
    "related": ["read_file", "exit"],
},

# ---------------------------------------------------------------- read_file
{
    "slug": "read_file",
    "name": "read_file",
    "kind": "関数",
    "group": "引数とファイル",
    "summary": "ファイル全体を読んで FileData で返す",
    "keywords": ["ファイル", "読み込み", "file", "読む"],
    "signature": "def read_file(path: str) -> FileData",
    "intro": [
        "ファイル全体を読み、結果を <a href=\"filedata.html\">FileData</a> で返します。"
        "開けて最後まで読めたときだけ <code>ok</code> が真になります。空のファイルは <code>ok</code> が真で "
        "<code>text</code> は空です。読めなかったときは <code>ok</code> が偽で <code>text</code> は空です。",
    ],
    "sections": [
        ("example", {
            "title": "書いてから読む",
            "code": "ok = write_file(\"memo.txt\", \"abc\")\nf = read_file(\"memo.txt\")\n<< ok\n<< f.ok\n<< f.text\ng = read_file(\"nope.txt\")\n<< g.ok\n<< g.text.len",
            "stdout": "true\ntrue\nabc\nfalse\n0\n",
        }),
    ],
    "related": ["filedata", "write_file", "read_str"],
},

# ---------------------------------------------------------------- write_file
{
    "slug": "write_file",
    "name": "write_file",
    "kind": "関数",
    "group": "引数とファイル",
    "summary": "ファイル全体を書き出す。成功したら真",
    "keywords": ["ファイル", "書き込み", "file", "書く"],
    "signature": "def write_file(path: str, text: str) -> bool",
    "intro": [
        "文字列をファイルへ丸ごと書き出します。既存のファイルは置き換えられます。"
        "全部書けて閉じられたら真、失敗したら偽を返します。",
    ],
    "sections": [
        ("example", {
            "title": "書いて読み戻す",
            "code": "ok = write_file(\"memo.txt\", \"abc\")\n<< ok\nf = read_file(\"memo.txt\")\n<< f.text",
            "stdout": "true\nabc\n",
        }),
    ],
    "related": ["filedata", "read_file"],
},

# ---------------------------------------------------------------- read_int
{
    "slug": "read_int",
    "name": "read_int",
    "kind": "関数",
    "group": "入力",
    "summary": "標準入力から整数を1つ読む。in の実体",
    "keywords": ["入力", "整数", "読む", "標準入力", "input"],
    "signature": "def read_int() -> int",
    "intro": [
        "式 <code>in</code> の実体です。先行する空白（スペース・タブ・改行・CR）を読み飛ばし、"
        "符号（<code>-</code>）と数字を読みます。数字でない文字に当たると、そこで止まります（その文字は消費されます）。"
        "数字が無ければ 0 を返します。入力が尽きた後の読み取りは不定です。",
    ],
    "sections": [
        ("example", {
            "title": "空白は読み飛ばされる",
            "code": "a = read_int()\nb = read_int()\n<< a + b",
            "stdin": "  10   20\n",
            "stdout": "30\n",
        }),
        ("example", {
            "title": "in と同じもの",
            "code": "a = read_int()\nb = in\n<< a\n<< b",
            "stdin": "42\n-7\n",
            "stdout": "42\n-7\n",
        }),
    ],
    "sugar": [
        (">> 対象, 対象, ...（糖衣集 §02）", "../sugar.html#sugar-input"),
    ],
    "related": ["read_ints", "read_ints2", "read_token", "int"],
},

# ---------------------------------------------------------------- read_ints
{
    "slug": "read_ints",
    "name": "read_ints",
    "kind": "関数",
    "group": "入力",
    "summary": "整数を n 個読んで [int] にする。in[n] の実体",
    "keywords": ["入力", "整数", "複数", "読む", "標準入力"],
    "signature": "def read_ints(n: int) -> [int]",
    "intro": [
        "式 <code>in[n]</code> の実体です。<a href=\"read_int.html\">read_int</a> を n 回繰り返したものと同じです。",
    ],
    "sections": [
        ("example", {
            "title": "基本",
            "code": "a = read_ints(4)\n<< a\n<< a.sum()",
            "stdin": "1 2 3 4\n",
            "stdout": "1 2 3 4\n10\n",
        }),
        ("example", {
            "title": "個数形の糖衣で書く",
            "code": ">> n, a[n]\n<< a",
            "stdin": "3\n10 20 30\n",
            "stdout": "10 20 30\n",
        }),
    ],
    "sugar": [
        (">> 名前[個数]（個数形）（糖衣集 §02）", "../sugar.html#sugar-input-count"),
    ],
    "related": ["read_int", "read_ints2", "read_ints_dec"],
},

# ---------------------------------------------------------------- read_ints2
{
    "slug": "read_ints2",
    "name": "read_ints2",
    "kind": "関数",
    "group": "入力",
    "summary": "整数を h 行 × w 列で読んで [[int]] にする。in[h][w] の実体",
    "keywords": ["入力", "整数", "二次元", "グリッド", "読む"],
    "signature": "def read_ints2(h: int, w: int) -> [[int]]",
    "intro": [
        "式 <code>in[h][w]</code> の実体です。各行は独立した配列で、行同士が共有されることはありません。"
        "行の並びは入力の並びのまま（第0成分が行）です。",
    ],
    "sections": [
        ("example", {
            "title": "基本",
            "code": "g = read_ints2(2, 3)\n<< g[0]\n<< g[1]\n<< g",
            "stdin": "1 2 3\n4 5 6\n",
            "stdout": "1 2 3\n4 5 6\n1 2 3 4 5 6\n",
        }),
    ],
    "sugar": [
        (">> 名前[個数][個数]（個数形）（糖衣集 §02）", "../sugar.html#sugar-input-count"),
    ],
    "related": ["read_ints", "array", "pos2"],
},

# ---------------------------------------------------------------- read_ints_dec
{
    "slug": "read_ints_dec",
    "name": "read_ints_dec",
    "kind": "関数",
    "group": "入力",
    "summary": "整数を n 個読み、各要素から 1 引く",
    "keywords": ["入力", "整数", "1引く", "0始まり", "読む"],
    "signature": "def read_ints_dec(n: int) -> [int]",
    "intro": [
        "糖衣 <code>対象- = in[n]</code> の実体です。1始まりの番号を 0 始まりの添え字へ直すための形で、"
        "<a href=\"read_ints.html\">read_ints</a> で読んだ各要素から 1 を引きます。",
    ],
    "sections": [
        ("example", {
            "title": "基本",
            "code": "a = read_ints_dec(3)\n<< a",
            "stdin": "1 2 3\n",
            "stdout": "0 1 2\n",
        }),
        ("example", {
            "title": "糖衣で書く",
            "code": "a- = in[3]\n<< a",
            "stdin": "1 2 3\n",
            "stdout": "0 1 2\n",
        }),
    ],
    "sugar": [
        ("対象- = in[n]（糖衣集 §02）", "../sugar.html#sugar-input-dec"),
    ],
    "related": ["read_ints"],
},

# ---------------------------------------------------------------- read_token
{
    "slug": "read_token",
    "name": "read_token",
    "kind": "関数",
    "group": "入力",
    "summary": "空白区切りのトークンを1つ読む。in_str の実体",
    "keywords": ["入力", "トークン", "文字列", "読む", "標準入力"],
    "signature": "def read_token() -> str",
    "intro": [
        "式 <code>in_str</code> の実体です。先行する空白（スペース・タブ・改行・CR）を読み飛ばし、"
        "空白に当たるまでの並びを1つのトークンとして読みます。",
    ],
    "sections": [
        ("example", {
            "title": "基本",
            "code": "s = read_token()\nt = in_str\n<< s\n<< t",
            "stdin": "foo bar\n",
            "stdout": "foo\nbar\n",
        }),
    ],
    "sugar": [
        (">>> 対象, 対象, ...（糖衣集 §02）", "../sugar.html#sugar-input-str"),
    ],
    "related": ["read_tokens", "read_line", "read_int", "str"],
},

# ---------------------------------------------------------------- read_tokens
{
    "slug": "read_tokens",
    "name": "read_tokens",
    "kind": "関数",
    "group": "入力",
    "summary": "トークンを n 個読んで [str] にする。in_str[n] の実体",
    "keywords": ["入力", "トークン", "文字列", "複数", "読む"],
    "signature": "def read_tokens(n: int) -> [str]",
    "intro": [
        "式 <code>in_str[n]</code> の実体です。<a href=\"read_token.html\">read_token</a> を n 回繰り返したものと同じです。"
        "<code>n</code> が 0 なら空の配列を返します。",
    ],
    "sections": [
        ("example", {
            "title": "基本",
            "code": "t = read_tokens(3)\n<< t\n<< t.len",
            "stdin": "a b c\n",
            "stdout": "a b c\n3\n",
        }),
        ("example", {
            "title": "個数形の糖衣で書く",
            "code": ">>> s[2]\n<< s",
            "stdin": "x y\n",
            "stdout": "x y\n",
        }),
    ],
    "sugar": [
        (">>> 対象（個数形）（糖衣集 §02）", "../sugar.html#sugar-input-count"),
    ],
    "related": ["read_token", "read_ints"],
},

# ---------------------------------------------------------------- read_line
{
    "slug": "read_line",
    "name": "read_line",
    "kind": "関数",
    "group": "入力",
    "summary": "1行をそのまま読む。in_line の実体",
    "keywords": ["入力", "1行", "行", "読む", "標準入力"],
    "signature": "def read_line() -> str",
    "intro": [
        "式 <code>in_line</code> の実体です。現在の位置から行末までをそのまま読みます（行末の改行は含みません）。"
        "先行する空白は読み飛ばしません。"
        "<a href=\"read_int.html\">read_int</a> は数字の直後の区切り文字を1つ消費するため、"
        "整数を読んだ直後の <code>read_line</code> は次の行から始まります。",
    ],
    "sections": [
        ("example", {
            "title": "基本",
            "code": "l = read_line()\nm = in_line\n<< l\n<< m",
            "stdin": "hello world\nnext\n",
            "stdout": "hello world\nnext\n",
        }),
        ("example", {
            "title": "整数を読んだ直後",
            "code": "n = read_int()\nl = read_line()\n<< n\n<< l",
            "stdin": "42\nhello\n",
            "stdout": "42\nhello\n",
        }),
    ],
    "related": ["read_token", "read_str", "str"],
},

# ---------------------------------------------------------------- read_str
{
    "slug": "read_str",
    "name": "read_str",
    "kind": "関数",
    "group": "入力",
    "summary": "標準入力の残りを全部読む",
    "keywords": ["入力", "全部", "残り", "読む", "標準入力"],
    "signature": "def read_str() -> str",
    "intro": [
        "現在の位置から標準入力の終わりまでを、改行も含めて全部読みます。"
        "入力全体を文字列として処理したいときに使います。",
    ],
    "sections": [
        ("example", {
            "title": "残りを全部読む",
            "code": "s = read_str()\nprint_str(s)\n<< s.len",
            "stdin": "ab\ncd\n",
            "stdout": "ab\ncd\n6\n",
        }),
    ],
    "related": ["read_line", "read_file", "str"],
},

# ---------------------------------------------------------------- print_int
{
    "slug": "print_int",
    "name": "print_int",
    "kind": "関数",
    "group": "出力",
    "summary": "整数を標準出力へ出す（改行は付かない）",
    "keywords": ["出力", "整数", "表示", "print"],
    "signature": "def print_int(n: int)",
    "intro": [
        "整数を10進で標準出力へ出します。<strong>改行は付きません</strong>。"
        "もとは prelude 内部の出力補助で、改行つきで出したいときは <a href=\"print.html\">print</a> を使うか、"
        "<code>putchar(10)</code> を続けてください。",
    ],
    "sections": [
        ("example", {
            "title": "改行は自分で付ける",
            "code": "print_int(42)\nputchar(10)\nprint_int(0 - 7)\nputchar(10)",
            "stdout": "42\n-7\n",
        }),
    ],
    "related": ["print", "print_str", "int"],
},

# ---------------------------------------------------------------- print_str
{
    "slug": "print_str",
    "name": "print_str",
    "kind": "関数",
    "group": "出力",
    "summary": "文字列をそのまま標準出力へ出す（改行は付かない）",
    "keywords": ["出力", "文字列", "表示", "print"],
    "signature": "def print_str(s: str)",
    "intro": [
        "文字列の文字コードをそのまま標準出力へ書きます。<strong>改行は付きません</strong>。"
        "<code>print</code> の実体（<code>__println</code>）が、文字列化した結果を出すのに使っている補助です。"
        "改行つきで出したいときは <a href=\"print.html\">print</a> を使ってください。",
    ],
    "sections": [
        ("example", {
            "title": "つなげて書く",
            "code": "print_str(\"ab\")\nprint_str(\"cd\")\nputchar(10)",
            "stdout": "abcd\n",
        }),
    ],
    "related": ["print", "print_int", "read_str", "str"],
},

# ---------------------------------------------------------------- print_yn
{
    "slug": "print_yn",
    "name": "print_yn",
    "kind": "関数",
    "group": "出力",
    "summary": "真なら Yes、偽なら No を1行出す。yn の実体",
    "keywords": ["出力", "Yes", "No", "真偽"],
    "signature": "def print_yn(b: bool)",
    "intro": [
        "糖衣 <code>yn 式</code> の実体です。真なら <code>Yes</code>、偽なら <code>No</code> を、改行つきで出します。",
    ],
    "sections": [
        ("example", {
            "title": "基本",
            "code": "print_yn(1 < 2)\nprint_yn(1 > 2)\nyn 3 == 3",
            "stdout": "Yes\nNo\nYes\n",
        }),
    ],
    "sugar": [
        ("yn 式（糖衣集 §01）", "../sugar.html#sugar-yn"),
    ],
    "related": ["print", "bool"],
},

# ---------------------------------------------------------------- zero_int
{
    "slug": "zero_int",
    "name": "zero_int",
    "kind": "関数",
    "group": "初期値ファクトリ",
    "summary": "0 を返す初期値ファクトリ",
    "keywords": ["初期値", "ファクトリ", "0", "既定値"],
    "signature": "def zero_int() -> int",
    "intro": [
        "呼ぶたびに 0 を返す関数です。<a href=\"dict.html\">Dict</a> / <a href=\"vec.html\">Vec</a> / "
        "<a href=\"deque.html\">Deque</a> の構築に、既定値ファクトリとして渡します。",
    ],
    "sections": [
        ("example", {
            "title": "Dict の既定値にする",
            "code": "d: Dict<str, int> = Dict(zero_int)\n<< d[\"missing\"]",
            "stdout": "0\n",
        }),
    ],
    "related": ["dict", "vec", "deque", "zero_bool", "zero_str"],
},

# ---------------------------------------------------------------- zero_bool
{
    "slug": "zero_bool",
    "name": "zero_bool",
    "kind": "関数",
    "group": "初期値ファクトリ",
    "summary": "偽を返す初期値ファクトリ",
    "keywords": ["初期値", "ファクトリ", "偽", "false", "既定値"],
    "signature": "def zero_bool() -> bool",
    "intro": [
        "呼ぶたびに偽を返す関数です。<a href=\"dict.html\">Dict</a> などの構築に、既定値ファクトリとして渡します。",
    ],
    "sections": [
        ("example", {
            "title": "Dict の既定値にする",
            "code": "d: Dict<str, bool> = Dict(zero_bool)\n<< d[\"x\"]",
            "stdout": "false\n",
        }),
    ],
    "related": ["dict", "zero_int", "bool"],
},

# ---------------------------------------------------------------- zero_str
{
    "slug": "zero_str",
    "name": "zero_str",
    "kind": "関数",
    "group": "初期値ファクトリ",
    "summary": "空文字列を返す初期値ファクトリ",
    "keywords": ["初期値", "ファクトリ", "空文字列", "既定値"],
    "signature": "def zero_str() -> str",
    "intro": [
        "呼ぶたびに空文字列を返す関数です。<a href=\"dict.html\">Dict</a> などの構築に、既定値ファクトリとして渡します。",
    ],
    "sections": [
        ("example", {
            "title": "Dict の既定値にする",
            "code": "d: Dict<str, str> = Dict(zero_str)\n<< d[\"x\"].len",
            "stdout": "0\n",
        }),
    ],
    "related": ["dict", "zero_int", "str"],
},

# ---------------------------------------------------------------- zero_pos2
{
    "slug": "zero_pos2",
    "name": "zero_pos2",
    "kind": "関数",
    "group": "初期値ファクトリ",
    "summary": "原点 (0, 0) を返す初期値ファクトリ",
    "keywords": ["初期値", "ファクトリ", "座標", "原点", "既定値"],
    "signature": "def zero_pos2() -> Pos2",
    "intro": [
        "呼ぶたびに原点 <code>(0, 0)</code> を返す関数です。<a href=\"dict.html\">Dict</a> などの構築に、"
        "既定値ファクトリとして渡します。<code>Deque&lt;Pos2&gt;()</code> のような引数なし構築の埋め草にも自動で使われます。",
    ],
    "sections": [
        ("example", {
            "title": "Dict の既定値にする",
            "code": "d: Dict<str, Pos2> = Dict(zero_pos2)\n<< d[\"x\"]",
            "stdout": "0 0\n",
        }),
    ],
    "related": ["pos2", "dict", "zero_pos3"],
},

# ---------------------------------------------------------------- zero_pos3
{
    "slug": "zero_pos3",
    "name": "zero_pos3",
    "kind": "関数",
    "group": "初期値ファクトリ",
    "summary": "原点 (0, 0, 0) を返す初期値ファクトリ",
    "keywords": ["初期値", "ファクトリ", "座標", "原点", "既定値"],
    "signature": "def zero_pos3() -> Pos3",
    "intro": [
        "呼ぶたびに原点 <code>(0, 0, 0)</code> を返す関数です。<a href=\"dict.html\">Dict</a> などの構築に、"
        "既定値ファクトリとして渡します。",
    ],
    "sections": [
        ("example", {
            "title": "Dict の既定値にする",
            "code": "d: Dict<str, Pos3> = Dict(zero_pos3)\n<< d[\"x\"]",
            "stdout": "0 0 0\n",
        }),
    ],
    "related": ["pos3", "dict", "zero_pos2"],
},

# ---------------------------------------------------------------- zero_array
{
    "slug": "zero_array",
    "name": "zero_array",
    "kind": "関数",
    "group": "初期値ファクトリ",
    "summary": "空配列を返す補助関数",
    "keywords": ["初期値", "空配列", "補助", "既定値"],
    "signature": "def zero_array()",
    "intro": [
        "呼ぶたびに新しい空配列を返す補助関数です。主な用途は、型を引数に渡す構築"
        "（<code>Deque([int])</code> など）で要素型が配列のときに、ゼロ値ファクトリとして自動で使われることです。"
        "戻り値の要素型は「不明」で、どの配列型とも緩く一致します。直接使う場合は、"
        "代入先の注釈で要素型を決めてください。",
    ],
    "sections": [
        ("example", {
            "title": "型を引数に渡す構築での使われ方",
            "code": "q = Deque([int])\nq.push_back([1, 2])\n<< q.front()",
            "stdout": "1 2\n",
        }),
        ("example", {
            "title": "直接使う",
            "code": "a = zero_array()\n<< a.len",
            "stdout": "0\n",
        }),
    ],
    "related": ["array", "deque", "vec", "zero_int"],
},
]
