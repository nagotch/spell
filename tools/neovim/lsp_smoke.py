#!/usr/bin/env python3
"""spell-lsp の手動確認用スモークテスト。

サーバーを起動し、生の JSON-RPC メッセージを流して、診断の
publishDiagnostics が期待どおり返ることを確認する。

使い方: python3 lsp_smoke.py [spell-lsp のパス]
（省略時はリポジトリの src/spell-lsp を使う）
"""
import json
import os
import select
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SERVER = sys.argv[1] if len(sys.argv) > 1 else os.path.join(HERE, "..", "..", "src", "spell-lsp")
URI = "file:///tmp/spell_lsp_smoke.sp"

proc = subprocess.Popen([SERVER], stdin=subprocess.PIPE, stdout=subprocess.PIPE)
buffer = b""


def send(obj):
    body = json.dumps(obj).encode("utf-8")
    proc.stdin.write(b"Content-Length: %d\r\n\r\n" % len(body) + body)
    proc.stdin.flush()


def read_message(timeout=30):
    global buffer
    deadline = os.times().elapsed + timeout
    while True:
        header, sep, rest = buffer.partition(b"\r\n\r\n")
        if sep:
            length = None
            for line in header.split(b"\r\n"):
                if line.lower().startswith(b"content-length:"):
                    length = int(line.split(b":", 1)[1].strip())
            if length is not None and len(rest) >= length:
                body, buffer = rest[:length], rest[length:]
                return json.loads(body.decode("utf-8"))
        if os.times().elapsed > deadline:
            raise TimeoutError("サーバーから応答がありません")
        ready, _, _ = select.select([proc.stdout], [], [], 1)
        if ready:
            chunk = os.read(proc.stdout.fileno(), 65536)
            if not chunk:
                raise EOFError("サーバーが終了しました")
            buffer += chunk


def wait_for(predicate, what):
    while True:
        msg = read_message()
        if predicate(msg):
            return msg
        print("  （読み飛ばし: %s）" % json.dumps(msg, ensure_ascii=False)[:120])


def wait_diagnostics():
    msg = wait_for(lambda m: m.get("method") == "textDocument/publishDiagnostics", "診断")
    return msg["params"]["diagnostics"]


failures = 0


def check(name, cond, detail=""):
    global failures
    if cond:
        print("PASS: %s" % name)
    else:
        failures += 1
        print("FAIL: %s %s" % (name, detail))


OK_PROGRAM = "def main() { return 0; }\n"
TYPE_ERROR_PROGRAM = "def main() {\n    x: int = 1 < 2;\n    return 0;\n}\n"
SYNTAX_ERROR_PROGRAM = "def main() { return (1 + 2; }\n"

# 1. initialize
send({"jsonrpc": "2.0", "id": 1, "method": "initialize",
      "params": {"processId": None, "rootUri": None, "capabilities": {}}})
resp = wait_for(lambda m: m.get("id") == 1, "initialize 応答")
caps = resp.get("result", {}).get("capabilities", {})
check("initialize: textDocumentSync が Full(1)", caps.get("textDocumentSync") == 1, json.dumps(caps))
send({"jsonrpc": "2.0", "method": "initialized", "params": {}})

# 2. didOpen: 正しいプログラム → 診断ゼロ
send({"jsonrpc": "2.0", "method": "textDocument/didOpen",
      "params": {"textDocument": {"uri": URI, "languageId": "spell", "version": 1, "text": OK_PROGRAM}}})
diags = wait_diagnostics()
check("didOpen（正常）: 診断ゼロ", diags == [], json.dumps(diags, ensure_ascii=False))

# 3. didChange: 型エラー → 正しい行・列とメッセージ
send({"jsonrpc": "2.0", "method": "textDocument/didChange",
      "params": {"textDocument": {"uri": URI, "version": 2},
                 "contentChanges": [{"text": TYPE_ERROR_PROGRAM}]}})
diags = wait_diagnostics()
check("didChange（型エラー）: 診断1件", len(diags) == 1, json.dumps(diags, ensure_ascii=False))
if len(diags) == 1:
    d = diags[0]
    r = d["range"]
    check("型エラー: 位置は x の場所（line=1, char=4-5）",
          r["start"] == {"line": 1, "character": 4} and r["end"] == {"line": 1, "character": 5},
          json.dumps(r))
    check("型エラー: メッセージに「が代入されています」",
          "が代入されています" in d["message"], d["message"])
    check("型エラー: severity は Error(1)", d.get("severity") == 1)

# 4. didChange: 構文エラー → 診断
send({"jsonrpc": "2.0", "method": "textDocument/didChange",
      "params": {"textDocument": {"uri": URI, "version": 3},
                 "contentChanges": [{"text": SYNTAX_ERROR_PROGRAM}]}})
diags = wait_diagnostics()
check("didChange（構文エラー）: 診断1件", len(diags) == 1, json.dumps(diags, ensure_ascii=False))
if len(diags) == 1:
    check("構文エラー: メッセージに「が必要ですが」",
          "が必要ですが" in diags[0]["message"], diags[0]["message"])

# 5. didChange: 直すと診断が消える
send({"jsonrpc": "2.0", "method": "textDocument/didChange",
      "params": {"textDocument": {"uri": URI, "version": 4},
                 "contentChanges": [{"text": OK_PROGRAM}]}})
diags = wait_diagnostics()
check("didChange（修正後）: 診断ゼロ", diags == [], json.dumps(diags, ensure_ascii=False))

# 5.5 prelude.sp 自身を開く: 組み込み prelude と二重定義にならず診断ゼロ
PRELUDE_PATH = os.path.join(HERE, "..", "..", "src", "prelude.sp")
if os.path.exists(PRELUDE_PATH):
    with open(PRELUDE_PATH, encoding="utf-8") as f:
        prelude_text = f.read()
    PRELUDE_URI = "file:///tmp/prelude.sp"
    send({"jsonrpc": "2.0", "method": "textDocument/didOpen",
          "params": {"textDocument": {"uri": PRELUDE_URI, "languageId": "spell", "version": 1, "text": prelude_text}}})
    diags = wait_diagnostics()
    check("didOpen（prelude.sp 自身）: 診断ゼロ", diags == [], json.dumps(diags, ensure_ascii=False))
    send({"jsonrpc": "2.0", "method": "textDocument/didClose",
          "params": {"textDocument": {"uri": PRELUDE_URI}}})
    diags = wait_diagnostics()
    check("didClose（prelude.sp）: 空の診断", diags == [], json.dumps(diags, ensure_ascii=False))

# 5.6 複数診断（型エラー×3）: 別々の関数の独立した型エラーが全部返る
MULTI_TYPE_PROGRAM = (
    "def fa() -> int {\n"
    "    x: int = 1 < 2;\n"
    "    return 0;\n"
    "}\n"
    "def fb() -> int {\n"
    "    y: str = 42;\n"
    "    return 0;\n"
    "}\n"
    "def fc() -> int {\n"
    "    return \"abc\";\n"
    "}\n"
)
send({"jsonrpc": "2.0", "method": "textDocument/didChange",
      "params": {"textDocument": {"uri": URI, "version": 5},
                 "contentChanges": [{"text": MULTI_TYPE_PROGRAM}]}})
diags = wait_diagnostics()
check("didChange（型エラー×3）: 診断3件", len(diags) == 3, json.dumps(diags, ensure_ascii=False))
if len(diags) == 3:
    lines = sorted(d["range"]["start"]["line"] for d in diags)
    check("型エラー×3: 行は 1, 5, 9", lines == [1, 5, 9], json.dumps(lines))
    msgs = [d["message"] for d in diags]
    check("型エラー×3: 代入エラー2件と戻り値エラー1件",
          sum("が代入されています" in m for m in msgs) == 2
          and sum("が返されています" in m for m in msgs) == 1,
          json.dumps(msgs, ensure_ascii=False))
    check("型エラー×3: severity は全て Error(1)", all(d.get("severity") == 1 for d in diags))

# 5.7 複数診断（構文エラー×2＋型エラー）: 文単位リカバリで全部返る
MIXED_ERROR_PROGRAM = (
    "def main() {\n"
    "    a = (1 + 2;\n"
    "    b: int = 1 < 2;\n"
    "    c = 3 + ;\n"
    "    return 0;\n"
    "}\n"
)
send({"jsonrpc": "2.0", "method": "textDocument/didChange",
      "params": {"textDocument": {"uri": URI, "version": 6},
                 "contentChanges": [{"text": MIXED_ERROR_PROGRAM}]}})
diags = wait_diagnostics()
check("didChange（構文×2＋型×1）: 診断3件", len(diags) == 3, json.dumps(diags, ensure_ascii=False))
if len(diags) == 3:
    msgs = [d["message"] for d in diags]
    check("構文×2＋型×1: 構文エラー2件と型エラー1件",
          sum("が必要ですが" in m for m in msgs) == 2
          and sum("が代入されています" in m for m in msgs) == 1,
          json.dumps(msgs, ensure_ascii=False))
    lines = sorted(d["range"]["start"]["line"] for d in diags)
    check("構文×2＋型×1: 行は 1, 2, 3", lines == [1, 2, 3], json.dumps(lines))

# 5.8 カスケード抑制: 悪い代入1件の下流利用では追加の診断が出ない。
# 代入先 x は注釈の int として記録されるため、x を使う後続の文は
# そのまま検査が通り、診断は元の1件だけになるのが正しい
CASCADE_PROGRAM = (
    "def main() {\n"
    "    x: int = 1 < 2;\n"
    "    y = x + 1;\n"
    "    z = y * 2;\n"
    "    return z;\n"
    "}\n"
)
send({"jsonrpc": "2.0", "method": "textDocument/didChange",
      "params": {"textDocument": {"uri": URI, "version": 7},
                 "contentChanges": [{"text": CASCADE_PROGRAM}]}})
diags = wait_diagnostics()
check("didChange（カスケード）: 診断1件だけ", len(diags) == 1, json.dumps(diags, ensure_ascii=False))
if len(diags) == 1:
    check("カスケード: 元の代入エラーだけ",
          "が代入されています" in diags[0]["message"], diags[0]["message"])

# 5.9 EOF 位置の診断範囲（step 62）: ファイル末尾を指すエラーは最終行の
# 最後の1文字を指す。本文が改行で終わっていても、エディタの行モデルに
# 存在しない「最終行の次の行」へ出してはいけない
EOF_PAREN_PROGRAM = "def main() {\n    x = (1 + 2"
send({"jsonrpc": "2.0", "method": "textDocument/didChange",
      "params": {"textDocument": {"uri": URI, "version": 8},
                 "contentChanges": [{"text": EOF_PAREN_PROGRAM}]}})
diags = wait_diagnostics()
check("didChange（EOF・未閉じ括弧）: 診断1件", len(diags) == 1, json.dumps(diags, ensure_ascii=False))
if len(diags) == 1:
    r = diags[0]["range"]
    check("EOF・未閉じ括弧: 範囲は最終行の最後の文字（line=1, char=13-14）",
          r["start"] == {"line": 1, "character": 13} and r["end"] == {"line": 1, "character": 14},
          json.dumps(r))
    check("EOF・未閉じ括弧: メッセージに「が必要ですが」",
          "が必要ですが" in diags[0]["message"], diags[0]["message"])

EOF_PAREN_NL_PROGRAM = "def main() {\n    x = (1 + 2\n"
send({"jsonrpc": "2.0", "method": "textDocument/didChange",
      "params": {"textDocument": {"uri": URI, "version": 9},
                 "contentChanges": [{"text": EOF_PAREN_NL_PROGRAM}]}})
diags = wait_diagnostics()
check("didChange（EOF・末尾改行つき）: 診断1件", len(diags) == 1, json.dumps(diags, ensure_ascii=False))
if len(diags) == 1:
    r = diags[0]["range"]
    check("EOF・末尾改行つき: 範囲は改行なしと同じ（line=1, char=13-14）",
          r["start"] == {"line": 1, "character": 13} and r["end"] == {"line": 1, "character": 14},
          json.dumps(r))

EOF_LATE_LINE_PROGRAM = "def main() {\n    a = 1;\n    b\n"
send({"jsonrpc": "2.0", "method": "textDocument/didChange",
      "params": {"textDocument": {"uri": URI, "version": 10},
                 "contentChanges": [{"text": EOF_LATE_LINE_PROGRAM}]}})
diags = wait_diagnostics()
check("didChange（EOF・後ろの行）: 診断1件", len(diags) == 1, json.dumps(diags, ensure_ascii=False))
if len(diags) == 1:
    r = diags[0]["range"]
    check("EOF・後ろの行: 範囲は b の位置（line=2, char=4-5）",
          r["start"] == {"line": 2, "character": 4} and r["end"] == {"line": 2, "character": 5},
          json.dumps(r))
    check("EOF・後ろの行: メッセージに「数値が必要ですが」",
          "数値が必要ですが" in diags[0]["message"], diags[0]["message"])

# 5.10 矢印演算子（step 66）: 正しい矢印は診断なし、dunder を持たない
# クラスへの矢印はキャプチャモードでも位置つき診断になる
ARROW_OK_PROGRAM = (
    "def main() {\n"
    "    q: Deque<int> = Deque(zero_int);\n"
    "    q <- 1;\n"
    "    x <- q;\n"
    "    return x;\n"
    "}\n"
)
send({"jsonrpc": "2.0", "method": "textDocument/didChange",
      "params": {"textDocument": {"uri": URI, "version": 11},
                 "contentChanges": [{"text": ARROW_OK_PROGRAM}]}})
diags = wait_diagnostics()
check("didChange（矢印・正常）: 診断なし", diags == [], json.dumps(diags, ensure_ascii=False))

ARROW_ERR_PROGRAM = (
    "class P { x: int; def __init__(self, x: int) { self.x = x; } }\n"
    "def main() {\n"
    "    p = P(1);\n"
    "    p <- 1;\n"
    "    return 0;\n"
    "}\n"
)
send({"jsonrpc": "2.0", "method": "textDocument/didChange",
      "params": {"textDocument": {"uri": URI, "version": 12},
                 "contentChanges": [{"text": ARROW_ERR_PROGRAM}]}})
diags = wait_diagnostics()
check("didChange（矢印・dunder なし）: 診断1件", len(diags) == 1, json.dumps(diags, ensure_ascii=False))
if len(diags) == 1:
    check("矢印・dunder なし: メッセージに「は使えません」",
          "は使えません" in diags[0]["message"], diags[0]["message"])
    r = diags[0]["range"]
    check("矢印・dunder なし: 範囲は <- の位置（line=3, char=6-8）",
          r["start"] == {"line": 3, "character": 6} and r["end"] == {"line": 3, "character": 8},
          json.dumps(r))
# 5.11 colon 形式（step 67）: インデント形式のプログラムは診断ゼロ
COLON_PROGRAM = (
    "def main():\n"
    "    x = 0\n"
    "    if (x == 0):\n"
    "        x = 42\n"
    "    return x\n"
)
send({"jsonrpc": "2.0", "method": "textDocument/didChange",
      "params": {"textDocument": {"uri": URI, "version": 13},
                 "contentChanges": [{"text": COLON_PROGRAM}]}})
diags = wait_diagnostics()
check("didChange（colon 形式）: 診断ゼロ", diags == [], json.dumps(diags, ensure_ascii=False))

# 5.12 dedent エラー（step 67）: どの段にも合わないインデントは、
# そのトークンの位置に診断が出る
DEDENT_PROGRAM = (
    "def main():\n"
    "    if (1 < 2):\n"
    "        x = 1\n"
    "      y = 2\n"
    "    return 0\n"
)
send({"jsonrpc": "2.0", "method": "textDocument/didChange",
      "params": {"textDocument": {"uri": URI, "version": 14},
                 "contentChanges": [{"text": DEDENT_PROGRAM}]}})
diags = wait_diagnostics()
check("didChange（dedent エラー）: 診断1件", len(diags) == 1, json.dumps(diags, ensure_ascii=False))
if len(diags) == 1:
    r = diags[0]["range"]
    check("dedent エラー: 範囲は y の位置（line=3, char=6-7）",
          r["start"] == {"line": 3, "character": 6} and r["end"] == {"line": 3, "character": 7},
          json.dumps(r))
    check("dedent エラー: メッセージに「インデントが揃っていません」",
          "インデントが揃っていません" in diags[0]["message"], diags[0]["message"])

# 5.13 パイプ else / elif（step 74）: `|` の糖衣を含むプログラムは診断ゼロ
PIPE_PROGRAM = (
    "def main():\n"
    "    x = 2\n"
    "    r = 0\n"
    "    if x == 1:\n"
    "        r = 1\n"
    "    | x == 2:\n"
    "        r = 2\n"
    "    |:\n"
    "        r = 3\n"
    "    return r\n"
)
send({"jsonrpc": "2.0", "method": "textDocument/didChange",
      "params": {"textDocument": {"uri": URI, "version": 15},
                 "contentChanges": [{"text": PIPE_PROGRAM}]}})
diags = wait_diagnostics()
check("didChange（パイプ else / elif）: 診断ゼロ", diags == [], json.dumps(diags, ensure_ascii=False))

# 5.14 構築の簡略化（step 76）: 引数なし構築と型引数つき構築は診断ゼロ、
# クラス要素の引数なし構築は構築位置に診断が出る
CTOR_OK_PROGRAM = (
    "def main() {\n"
    "    q = Deque();\n"
    "    q.push_back(1);\n"
    "    s = Deque<str>();\n"
    "    s.push_back(\"x\");\n"
    "    return q.len + s.len;\n"
    "}\n"
)
send({"jsonrpc": "2.0", "method": "textDocument/didChange",
      "params": {"textDocument": {"uri": URI, "version": 16},
                 "contentChanges": [{"text": CTOR_OK_PROGRAM}]}})
diags = wait_diagnostics()
check("didChange（引数なし構築・型引数）: 診断ゼロ", diags == [], json.dumps(diags, ensure_ascii=False))

CTOR_ERR_PROGRAM = (
    "class P { x: int; def __init__(self, x: int) { self.x = x; } }\n"
    "def main() {\n"
    "    q = Deque<P>();\n"
    "    return 0;\n"
    "}\n"
)
send({"jsonrpc": "2.0", "method": "textDocument/didChange",
      "params": {"textDocument": {"uri": URI, "version": 17},
                 "contentChanges": [{"text": CTOR_ERR_PROGRAM}]}})
diags = wait_diagnostics()
check("didChange（クラス要素の引数なし構築）: 診断1件", len(diags) == 1, json.dumps(diags, ensure_ascii=False))
if len(diags) == 1:
    check("クラス要素の引数なし構築: メッセージに「ファクトリを渡してください」",
          "ファクトリを渡してください" in diags[0]["message"], diags[0]["message"])
    r = diags[0]["range"]
    check("クラス要素の引数なし構築: 範囲は Deque の位置（line=2, char=8-13）",
          r["start"] == {"line": 2, "character": 8} and r["end"] == {"line": 2, "character": 13},
          json.dumps(r))

# 5.15 Pos と型を引数に渡す構築（step 77）: リテラル・a[p]・型渡しは
# 診断ゼロ、次元の合わない a[p] は添字の位置に診断が出る
POS_OK_PROGRAM = (
    "def main() {\n"
    "    a = [[1, 2], [3, 4]];\n"
    "    p = (0, 1);\n"
    "    q = Deque(Pos2);\n"
    "    q <- p;\n"
    "    return a[p] + q.len;\n"
    "}\n"
)
send({"jsonrpc": "2.0", "method": "textDocument/didChange",
      "params": {"textDocument": {"uri": URI, "version": 18},
                 "contentChanges": [{"text": POS_OK_PROGRAM}]}})
diags = wait_diagnostics()
check("didChange（Pos リテラル・a[p]・型渡し構築）: 診断ゼロ", diags == [], json.dumps(diags, ensure_ascii=False))

POS_ERR_PROGRAM = (
    "def main() {\n"
    "    a = [[1, 2], [3, 4]];\n"
    "    return a[(1, 0, 1)];\n"
    "}\n"
)
send({"jsonrpc": "2.0", "method": "textDocument/didChange",
      "params": {"textDocument": {"uri": URI, "version": 19},
                 "contentChanges": [{"text": POS_ERR_PROGRAM}]}})
diags = wait_diagnostics()
check("didChange（a[p] の次元不一致）: 診断1件", len(diags) == 1, json.dumps(diags, ensure_ascii=False))
if len(diags) == 1:
    check("a[p] の次元不一致: メッセージに「3次元ですが」",
          "3次元ですが" in diags[0]["message"], diags[0]["message"])

# 6. didClose: 空の診断が送られて消える
send({"jsonrpc": "2.0", "method": "textDocument/didClose",
      "params": {"textDocument": {"uri": URI}}})
diags = wait_diagnostics()
check("didClose: 空の診断", diags == [], json.dumps(diags, ensure_ascii=False))

# 7. shutdown / exit
send({"jsonrpc": "2.0", "id": 2, "method": "shutdown", "params": None})
resp = wait_for(lambda m: m.get("id") == 2, "shutdown 応答")
check("shutdown: result は null", resp.get("result") is None and "error" not in resp, json.dumps(resp))
send({"jsonrpc": "2.0", "method": "exit"})
code = proc.wait(timeout=10)
check("exit: 終了コード 0", code == 0, "code=%s" % code)

print("---")
if failures:
    print("%d 件失敗" % failures)
    sys.exit(1)
print("すべて成功")
