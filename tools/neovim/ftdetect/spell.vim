augroup SpellFtdetect
  autocmd!
  " *.sp は NeoVim 組み込みの spice（回路シミュレーション）と競合する。
  " setfiletype では組み込み側が先に決まってしまうため、強制的に上書きする。
  autocmd BufNewFile,BufRead *.sp set filetype=spell
augroup END
