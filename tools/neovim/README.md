# spell-lsp（NeoVim 連携）

spell の言語サーバー `spell-lsp` を NeoVim で使うための連携物です。
v1 の対応範囲は **診断（エラー表示）だけ** です。補完・ホバー・定義ジャンプ等は未対応です。

## ビルドと配置

```sh
cd src
make            # spell と spell-lsp ができる
```

`spell-lsp` と `prelude.sp` は **同じフォルダに置いて** PATH の通った場所へコピーしてください
（コンパイラ本体と同じく、prelude は実行ファイル相対で探します）。

```sh
cp src/spell-lsp src/prelude.sp ~/.local/bin/
```

## NeoVim の設定

1. ファイルタイプ検出: `ftdetect/spell.vim` を自分の `runtimepath` 配下の
   `ftdetect/` へコピーするか、`spell-lsp.lua` の冒頭にある
   `vim.filetype.add` の形で `*.sp` を登録します。
2. サーバー起動: `spell-lsp.lua` の中身を `init.lua` 等へ貼ります。
   `FileType spell` で `vim.lsp.start` により `spell-lsp` が起動します。
3. インデント: `indent/spell.vim` を自分の `runtimepath` 配下の `indent/` へ
   コピーします。`:` 形式・`{}` 形式のどちらのブロックでも、開いた行の
   次行が1段下がり、`}` / `else` / `elif` / `|` の行で1段戻ります。

## 動作

- ファイルを開く・編集するたびに全文をチェックし、見つかったエラーを
  すべて `publishDiagnostics` で表示します（上限50件）。
- 構文エラー・型エラーの両方が対象です。位置情報のあるエラーは該当箇所に、
  ないものはファイル先頭に出ます。
- チェックは変更のたびに子プロセスで実行されます（コンパイラの状態を持ち越さないため）。

## 制限事項

- 位置の列は「文字（コードポイント）単位」で返します。BMP の文字（日本語を含む）
  では LSP 標準の UTF-16 単位と一致しますが、BMP 外の文字（絵文字等）が
  エラー位置より前にある行では列がずれることがあります。
- 同期方式は Full（全文送信）です。
- 診断は最大50件までです。

## 動作確認

```sh
python3 tools/neovim/lsp_smoke.py
```

サーバーを起動して JSON-RPC を流し、正常系・型エラー・構文エラー・
修正で診断が消えること・shutdown/exit を確認します。
