" Vim syntax file
" Language: spell
" Maintainer: nagotch

if exists("b:current_syntax")
  finish
endif

" コメント
syntax keyword spellTodo contained TODO FIXME XXX
syntax match spellComment "//.*$" contains=spellTodo
syntax region spellComment start="/\*" end="\*/" contains=spellTodo

" キーワード（tokenize.c の keywords[] と一致させる）
" Python の python.vim に寄せる: 文キーワードは Statement、in は Operator、
" 組み込み型は pythonBuiltin と同じ Function、True/False は Boolean
syntax keyword spellKeyword return if else elif while for break continue def class fn
syntax keyword spellKeyword in_str in_line put yn
syntax keyword spellOperatorWord in
syntax keyword spellConstant true false

" 関数定義・呼び出し
syntax match spellFuncDef "def\s\+\zs\h\w*"
syntax match spellFuncCall "\<\h\w*\ze\s*("

" dunder メソッド
syntax match spellDunder "__\h\w*__"

" 型（組み込み型は Python の builtin と同じ扱い、クラス名は Type）
syntax keyword spellBuiltinType int bool str
syntax match spellType "\<[A-Z][A-Za-z0-9_]*\>"

" 数値
syntax match spellNumber "\<\d\+\>"

" 文字列
syntax match spellEscape contained "\\."
syntax region spellString start=+"+ skip=+\\"+ end=+"+ contains=spellEscape

" 演算子・糖衣記号
syntax match spellOperator "->\|\.\.=\|\.\.\|<<\|>>\|++\|--\|&&\|||\|==\|!=\|<=\|>=\|+=\|-=\|\*=\|/=\|%=\|[-+*/%<>=!@^]"

highlight default link spellComment Comment
highlight default link spellTodo Todo
highlight default link spellKeyword Statement
highlight default link spellOperatorWord Operator
highlight default link spellConstant Boolean
highlight default link spellBuiltinType Function
highlight default link spellType Type
highlight default link spellNumber Number
highlight default link spellString String
highlight default link spellEscape SpecialChar
highlight default link spellFuncDef Function
highlight default link spellFuncCall Function
highlight default link spellDunder Special
highlight default link spellOperator Operator

let b:current_syntax = "spell"
