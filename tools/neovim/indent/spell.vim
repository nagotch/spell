" spell のインデント設定
" `:` 形式と `{}` 形式の両方のブロックで、開いたら1段下げ、
" 閉じ・中間行（} / else / elif / |）で1段戻す。
if exists("b:did_indent")
  finish
endif
let b:did_indent = 1

setlocal indentexpr=GetSpellIndent()
setlocal indentkeys=0{,0},0\|,0=elif,0=else,:
setlocal autoindent
setlocal nosmartindent
setlocal nocindent

if exists("*GetSpellIndent")
  finish
endif

" 文字列を含まない行だけ、末尾の // コメントを取り除く
function! s:StripComment(line)
  if a:line =~ '"'
    return a:line
  endif
  return substitute(a:line, '\/\/.*$', '', '')
endfunction

function! GetSpellIndent()
  let lnum = v:lnum
  let line = getline(lnum)
  let prevlnum = prevnonblank(lnum - 1)
  if prevlnum == 0
    return 0
  endif
  let prevraw = getline(prevlnum)
  let prevline = s:StripComment(prevraw)
  let previndent = indent(prevlnum)
  let sw = shiftwidth()

  " 現在行が閉じ・中間行で始まる場合
  if line =~ '^\s*\(}\||\|elif\>\|else\>\)'
    " 直前行が開き行そのもの（空のブロック）なら、開き行に揃える
    if prevline =~ '[:{]\s*$'
      return previndent
    endif
    return max([previndent - sw, 0])
  endif

  " 直前行がブロックを開いていれば1段下げる
  if prevline =~ '[:{]\s*$'
    return previndent + sw
  endif

  " 直前行が1行スイート（頭の行に本体まで書いてある）なら、
  " 次の行は頭の行の字下げに戻す
  let head = substitute(prevline, '^\s*', '', '')
  if head =~ '^\(if\|?\|while\|for\|def\|class\|elif\|else\||\|@\)\>' && prevline =~ ':\s\+\S'
    return previndent
  endif

  return previndent
endfunction
