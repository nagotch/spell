-- spell-lsp を NeoVim から起動する最小スニペット。
-- このファイルの中身を自分の init.lua（またはプラグイン設定）へ貼る。
-- 前提: spell-lsp が PATH に入っていること（prelude.sp が
-- spell-lsp と同じフォルダに置かれていること）。

-- *.sp を spell ファイルタイプとして認識させる
vim.filetype.add({
  extension = {
    sp = 'spell',
  },
})

-- spell バッファを開いたら言語サーバーを起動する
vim.api.nvim_create_autocmd('FileType', {
  pattern = 'spell',
  callback = function(args)
    vim.lsp.start({
      name = 'spell-lsp',
      cmd = { 'spell-lsp' },
      root_dir = vim.fs.root(0, { '.git' }) or vim.fn.getcwd(),
    }, { bufnr = args.buf })
  end,
})
