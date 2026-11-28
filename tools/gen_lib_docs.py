#!/usr/bin/env python3
"""spell ライブラリリファレンスの生成器。

tools/lib_docs_data.py の PAGES を唯一の情報源として、次を生成する。

  docs/lib/<slug>.html      クラス・関数ごとの個別ページ
  docs/lib/index.html       ライブラリ目次（全項目の一覧と検索ボックス）
  docs/lib/search-index.json サイト内検索のインデックス

生成の前に、データ中の全コード例を実際に src/spell でコンパイル・
実行し、期待出力（標準出力・標準エラー・終了コード）と一致する
ことを確認する。1件でも失敗したら何も書かず、終了コード 1 で終わる。
生成の後には docs/ 全体の内部リンク（ファイルと #アンカー）を検査し、
切れているものがあれば終了コード 1 で終わる。

使い方:
  python3 tools/gen_lib_docs.py           検証 → 生成 → リンク検査
  python3 tools/gen_lib_docs.py --links   リンク検査だけ
"""

import html
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
sys.dont_write_bytecode = True  # tools/__pycache__ を作らない
sys.path.insert(0, str(ROOT / "tools"))

from lib_docs_data import GROUPS, PAGES  # noqa: E402

LIB_DIR = ROOT / "docs" / "lib"
FOOTER = """<footer>
  <span>spell — <a href="https://github.com/nagotch/spell">github.com/nagotch/spell</a></span>
  <span>ドキュメントは <a href="https://creativecommons.org/licenses/by/4.0/deed.ja">CC BY 4.0</a> で公開しています</span>
</footer>"""


def esc(s):
    return html.escape(s, quote=False)


def plain(s):
    """HTML 断片からタグを落とし、エンティティを復号して検索用の素テキストにする。"""
    return html.unescape(re.sub(r"<[^>]+>", "", s))


# ---------------------------------------------------------------- データ検証

NAME_RE = re.compile(r"^[a-z0-9_]+$")
BLOCK_KINDS = {"h2", "h3", "p", "note", "ul", "table", "methods", "example"}


def validate_data():
    """データの形を生成前にまとめて検証する。

    ここで落ちる誤り（slug の重複・不正な文字、存在しない group や
    related、メソッドアンカーの重複など）を放置すると、目次から
    黙って消えるページや、途中まで書かれた生成物が残るため、
    検証と生成の前に必ず通す。
    """
    errors = []
    slugs = set()
    for page in PAGES:
        slug = page.get("slug", "")
        if not NAME_RE.match(slug):
            errors.append(f"slug が不正です: {slug!r}")
        if slug in slugs:
            errors.append(f"slug が重複しています: {slug}")
        slugs.add(slug)
        if page.get("group") not in GROUPS:
            errors.append(f"[{slug}] group が GROUPS にありません: {page.get('group')!r}")
        for key in ("name", "kind", "summary"):
            if not page.get(key):
                errors.append(f"[{slug}] {key} が空です")
        anchors = set()
        for block in page.get("sections", []):
            if block[0] not in BLOCK_KINDS:
                errors.append(f"[{slug}] 未知のブロック種別: {block[0]!r}")
                continue
            if block[0] == "methods":
                for entry in block[1]:
                    if len(entry) >= 3:
                        if not NAME_RE.match(entry[2]):
                            errors.append(f"[{slug}] メソッドアンカーが不正です: {entry[2]!r}")
                        if entry[2] in anchors:
                            errors.append(f"[{slug}] メソッドアンカーが重複しています: {entry[2]}")
                        anchors.add(entry[2])
            if block[0] == "example":
                ex = block[1]
                if "code" not in ex or "stdout" not in ex:
                    errors.append(f"[{slug}] 例に code / stdout がありません: {ex.get('title')!r}")
        for label, href in page.get("sugar", []):
            if not href.startswith("../"):
                errors.append(f"[{slug}] sugar の href は ../ 始まりにしてください: {href!r}")
    for page in PAGES:
        for rel in page.get("related", []):
            if rel not in slugs:
                errors.append(f"[{page['slug']}] related の参照先がありません: {rel}")
    return errors


# ---------------------------------------------------------------- 例の検証

def iter_examples(page):
    for block in page.get("sections", []):
        if block[0] == "example":
            yield block[1]


def run_example(spell, funcs, ex):
    with tempfile.TemporaryDirectory() as td:
        td = Path(td)
        (td / "prog.sp").write_text(ex["code"] + "\n", encoding="utf-8")
        r = subprocess.run(
            [str(spell), "-S", "-o", "prog", "prog.sp"],
            cwd=td, capture_output=True, text=True)
        if r.returncode != 0:
            return False, "コンパイル失敗: " + r.stderr.strip()
        r = subprocess.run(
            ["cc", "-o", "prog", "prog.s", str(funcs)],
            cwd=td, capture_output=True, text=True)
        if r.returncode != 0:
            return False, "リンク失敗: " + r.stderr.strip()
        try:
            r = subprocess.run(
                ["./prog", *ex.get("argv", [])],
                cwd=td, input=ex.get("stdin", ""),
                capture_output=True, text=True, timeout=10)
        except subprocess.TimeoutExpired:
            return False, "実行がタイムアウトしました"
        want_exit = ex.get("exit", 0)
        if r.returncode != want_exit:
            return False, f"終了コードが {r.returncode}（期待 {want_exit}）"
        if r.stdout != ex["stdout"]:
            return False, (f"標準出力が一致しません\n"
                           f"  期待: {ex['stdout']!r}\n  実際: {r.stdout!r}")
        want_err = ex.get("stderr", "")
        if r.stderr != want_err:
            return False, (f"標準エラーが一致しません\n"
                           f"  期待: {want_err!r}\n  実際: {r.stderr!r}")
        return True, ""


def verify_examples():
    subprocess.run(["make", "-C", str(ROOT / "src")],
                   check=True, capture_output=True, text=True)
    spell = ROOT / "src" / "spell"
    funcs = ROOT / "tests" / "funcs.c"
    total = 0
    failures = []
    for page in PAGES:
        for ex in iter_examples(page):
            total += 1
            ok, msg = run_example(spell, funcs, ex)
            if not ok:
                failures.append((page["slug"], ex.get("title", "（無題）"), msg))
    return total, failures


# ---------------------------------------------------------------- HTML 部品

def head(title, here_lib):
    nav = [
        ("../index.html", "概要", False),
        ("../guide.html", "言語ツアー", False),
        ("../reference.html", "リファレンス", False),
        ("index.html", "ライブラリ", here_lib),
        ("../sugar.html", "糖衣集", False),
        ("../editor.html", "エディタ連携", False),
        ("https://github.com/nagotch/spell", "GitHub", False),
    ]
    def nav_item(item):
        href, label, here = item
        cls = ' class="here"' if here else ""
        return f'    <a href="{href}"{cls}>{label}</a>'

    items = "\n".join(nav_item(item) for item in nav)
    return f"""<!DOCTYPE html>
<html lang="ja">
<head>
<meta charset="utf-8">
<meta name="viewport" content="width=device-width, initial-scale=1">
<title>{esc(title)}</title>
<link rel="preconnect" href="https://fonts.googleapis.com">
<link rel="preconnect" href="https://fonts.gstatic.com" crossorigin>
<link href="https://fonts.googleapis.com/css2?family=Shippori+Mincho+B1:wght@600;800&family=Zen+Kaku+Gothic+New:wght@400;500;700&family=IBM+Plex+Mono:wght@400;600&display=swap" rel="stylesheet">
<link rel="stylesheet" href="../style.css">
</head>
<body>
<div class="wrap">

<header class="mast">
  <a class="brand" href="../index.html">spell<span class="dot">.</span></a>
  <nav>
{items}
  </nav>
</header>"""


SEARCH_BOX = """<div class="lib-search" data-lib-search>
  <input type="search" placeholder="名前・メソッド・キーワードで検索" aria-label="ライブラリ内検索">
  <ul class="lib-search-results"></ul>
</div>"""


def sidebar(current_slug):
    parts = ['<aside class="side">', SEARCH_BOX]
    for group in GROUPS:
        members = [p for p in PAGES if p["group"] == group]
        if not members:
            continue
        parts.append(f'  <p class="side-h">{esc(group)}</p>')
        parts.append('  <ul class="side-list">')
        for p in members:
            here = ' class="here"' if p["slug"] == current_slug else ""
            parts.append(
                f'    <li><a href="{p["slug"]}.html"{here}>{esc(p["name"])}</a></li>')
        parts.append('  </ul>')
    parts.append('  <p class="side-h">ドキュメント</p>')
    parts.append('  <ul class="side-list">')
    for href, label in [("../index.html", "概要"), ("../guide.html", "言語ツアー"),
                        ("../reference.html", "リファレンス"),
                        ("../sugar.html", "糖衣集"), ("../editor.html", "エディタ連携")]:
        parts.append(f'    <li><a href="{href}">{label}</a></li>')
    parts.append('  </ul>')
    parts.append('</aside>')
    return "\n".join(parts)


def disp(s):
    """表示用に末尾の改行を1つだけ落とす（検証は元の文字列で完全一致を見る）。"""
    return s[:-1] if s.endswith("\n") else s


def render_example(ex):
    out = ['<figure class="code">']
    out.append(f'  <figcaption>例 — {esc(ex["title"])}</figcaption>')
    out.append(f'<pre>{esc(ex["code"])}</pre>')
    out.append('</figure>')
    if "stdin" in ex:
        out.append('<figure class="out">')
        out.append('  <figcaption>入力</figcaption>')
        out.append(f'<pre>{esc(disp(ex["stdin"]))}</pre>')
        out.append('</figure>')
    if ex.get("argv"):
        out.append('<figure class="out">')
        out.append('  <figcaption>コマンドライン引数</figcaption>')
        out.append(f'<pre>{esc(" ".join(ex["argv"]))}</pre>')
        out.append('</figure>')
    out.append('<figure class="out">')
    out.append('  <figcaption>出力</figcaption>')
    out.append(f'<pre>{esc(disp(ex["stdout"]))}</pre>')
    out.append('</figure>')
    if "stderr" in ex:
        out.append('<figure class="out">')
        out.append('  <figcaption>標準エラー出力</figcaption>')
        out.append(f'<pre>{esc(disp(ex["stderr"]))}</pre>')
        out.append('</figure>')
    if ex.get("exit"):
        out.append(f'<p class="note">終了コードは {ex["exit"]} です。</p>')
    return "\n".join(out)


def render_block(block):
    kind = block[0]
    if kind == "h2":
        return f'<h2>{block[1]}</h2>'
    if kind == "h3":
        return f'<h3>{block[1]}</h3>'
    if kind == "p":
        return f'<p>{block[1]}</p>'
    if kind == "note":
        return f'<p class="note">{block[1]}</p>'
    if kind == "ul":
        items = "\n".join(f'  <li>{it}</li>' for it in block[1])
        return f'<ul>\n{items}\n</ul>'
    if kind == "table":
        heads, rows = block[1], block[2]
        th = "".join(f'<th>{h}</th>' for h in heads)
        trs = []
        for row in rows:
            tds = "".join(f'<td>{c}</td>' for c in row)
            trs.append(f'  <tr>{tds}</tr>')
        return f'<table>\n  <tr>{th}</tr>\n' + "\n".join(trs) + '\n</table>'
    if kind == "methods":
        trs = []
        for entry in block[1]:
            sig, desc = entry[0], entry[1]
            anchor = entry[2] if len(entry) > 2 else None
            idattr = f' id="m-{anchor}"' if anchor else ""
            trs.append(f'  <tr{idattr}><td><code>{esc(sig)}</code></td><td>{desc}</td></tr>')
        return ('<table>\n  <tr><th>シグネチャ</th><th>内容</th></tr>\n'
                + "\n".join(trs) + '\n</table>')
    if kind == "example":
        return render_example(block[1])
    raise ValueError(f"unknown block kind: {kind}")


def render_page(page):
    parts = [head(f'{page["name"]} — spell ライブラリ', True)]
    parts.append('<div class="layout">')
    parts.append(sidebar(page["slug"]))
    parts.append('<main class="main">')
    parts.append(f'<p class="eyebrow">ライブラリ — {esc(page["kind"])}</p>')
    parts.append(f'<h1 class="page">{esc(page["name"])}</h1>')
    parts.append(f'<p class="lede">{esc(page["summary"])}</p>')
    if page.get("signature"):
        parts.append('<figure class="code">')
        parts.append('  <figcaption>シグネチャ</figcaption>')
        parts.append(f'<pre>{esc(page["signature"])}</pre>')
        parts.append('</figure>')
    for para in page.get("intro", []):
        parts.append(f'<p>{para}</p>')
    for block in page.get("sections", []):
        parts.append(render_block(block))
    if page.get("sugar"):
        parts.append('<h2>関係する糖衣構文</h2>')
        items = "\n".join(
            f'  <li><a href="{href}">{esc(label)}</a></li>'
            for label, href in page["sugar"])
        parts.append(f'<ul>\n{items}\n</ul>')
    if page.get("related"):
        by_slug = {p["slug"]: p for p in PAGES}
        parts.append('<h2>関連項目</h2>')
        items = "\n".join(
            f'  <li><a href="{s}.html">{esc(by_slug[s]["name"])}</a>'
            f' — {esc(by_slug[s]["summary"])}</li>'
            for s in page["related"])
        parts.append(f'<ul>\n{items}\n</ul>')
    parts.append('</main>')
    parts.append('</div>')
    parts.append(FOOTER)
    parts.append('<script src="search.js"></script>')
    parts.append('</div>')
    parts.append('</body>')
    parts.append('</html>')
    return "\n".join(parts) + "\n"


def render_index():
    parts = [head('ライブラリ — spell', True)]
    parts.append('<div class="layout">')
    parts.append(sidebar(None))
    parts.append('<main class="main">')
    parts.append('<p class="eyebrow">ライブラリ</p>')
    parts.append('<h1 class="page">ライブラリ</h1>')
    parts.append('<p class="lede">'
                 '起動時に読み込まれる prelude が用意する型と関数、およびコンパイラ組み込みの'
                 '一覧です。項目ごとに個別ページがあり、使い方と実行確認済みの例を載せています。'
                 '名前やメソッド名、日本語のキーワードで検索できます。</p>')
    parts.append(SEARCH_BOX)
    for group in GROUPS:
        members = [p for p in PAGES if p["group"] == group]
        if not members:
            continue
        parts.append(f'<h2>{esc(group)}</h2>')
        parts.append('<div class="toc">')
        for p in members:
            parts.append(f'  <a href="{p["slug"]}.html">')
            parts.append(f'    <span class="tt">{esc(p["name"])}</span>')
            parts.append(f'    <span class="td">{esc(p["summary"])}</span>')
            parts.append('  </a>')
        parts.append('</div>')
    parts.append('<h2>個別ページのないもの</h2>')
    parts.append('<p><code>__println</code> / <code>__eprintln</code>（<code>print</code> / '
                 '<code>eprint</code> の実体）、<code>__count_range</code>（<code>@n</code> の実体）、'
                 '<code>__dict_is_prime</code> / <code>__dict_next_cap</code>（<code>Dict</code> の内部用）は、'
                 '利用側から直接使うものではないため個別ページはありません。'
                 '一覧は<a href="../reference.html#s16">リファレンス §16</a>にあります。</p>')
    parts.append('</main>')
    parts.append('</div>')
    parts.append(FOOTER)
    parts.append('<script src="search.js"></script>')
    parts.append('</div>')
    parts.append('</body>')
    parts.append('</html>')
    return "\n".join(parts) + "\n"


def render_search_index():
    entries = []
    for p in PAGES:
        words = [p["name"], p["summary"], *p.get("keywords", [])]
        entries.append({
            "t": p["name"],
            "u": f'{p["slug"]}.html',
            "k": p["kind"],
            "s": p["summary"],
            "w": " ".join(words),
        })
        for block in p.get("sections", []):
            if block[0] != "methods":
                continue
            for entry in block[1]:
                if len(entry) < 3:
                    continue
                sig, desc, name = entry
                entries.append({
                    "t": f'{p["name"]}.{name}',
                    "u": f'{p["slug"]}.html#m-{name}',
                    "k": "メソッド",
                    "s": plain(desc),
                    "w": " ".join([name, sig, plain(desc), p["name"]]),
                })
    return json.dumps(entries, ensure_ascii=False, indent=1) + "\n"


# ---------------------------------------------------------------- リンク検査

def check_links():
    docs = ROOT / "docs"
    files = sorted(docs.rglob("*.html"))
    ids = {}
    for f in files:
        ids[f.resolve()] = set(
            re.findall(r"""id=["']([^"']+)["']""", f.read_text(encoding="utf-8")))
    broken = []
    for f in files:
        text = f.read_text(encoding="utf-8")
        refs = re.findall(r"""href=["']([^"']+)["']""", text)
        refs += re.findall(r"""src=["']([^"']+)["']""", text)
        for href in refs:
            if href.startswith(("http://", "https://", "mailto:")):
                continue
            target, _, frag = href.partition("#")
            tpath = (f.parent / target).resolve() if target else f.resolve()
            if target and not tpath.exists():
                broken.append(f"{f.relative_to(ROOT)}: {href}（ファイルが無い）")
            elif frag and tpath in ids and frag not in ids[tpath]:
                broken.append(f"{f.relative_to(ROOT)}: {href}（アンカーが無い）")
    return broken


# ---------------------------------------------------------------- 本体

def main():
    if "--links" in sys.argv:
        broken = check_links()
        if broken:
            print("リンク切れ:")
            print("\n".join("  " + b for b in broken))
            return 1
        print("リンク検査: 切れはありません")
        return 0

    errors = validate_data()
    if errors:
        print("データ検証に失敗しました:")
        print("\n".join("  " + e for e in errors))
        print("生成は中止しました。")
        return 1
    print("データ検証: 通過")

    total, failures = verify_examples()
    if failures:
        print(f"例の検証に失敗しました（{len(failures)} / {total} 件）:")
        for slug, title, msg in failures:
            print(f"  [{slug}] {title}: {msg}")
        print("生成は中止しました。")
        return 1
    print(f"例の検証: {total} / {total} 件通過")

    LIB_DIR.mkdir(exist_ok=True)
    for page in PAGES:
        (LIB_DIR / f'{page["slug"]}.html').write_text(
            render_page(page), encoding="utf-8")
    (LIB_DIR / "index.html").write_text(render_index(), encoding="utf-8")
    (LIB_DIR / "search-index.json").write_text(
        render_search_index(), encoding="utf-8")
    print(f"生成: 個別ページ {len(PAGES)} 件 + 目次 + 検索インデックス")

    broken = check_links()
    if broken:
        print("リンク切れ:")
        print("\n".join("  " + b for b in broken))
        return 1
    print("リンク検査: 切れはありません")
    return 0


if __name__ == "__main__":
    sys.exit(main())
