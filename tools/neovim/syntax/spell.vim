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
" 制御系は C の cStatement、宣言系は C++ の cppStructure に寄せる
syntax keyword spellKeyword return if else elif while for break continue in fn
syntax keyword spellKeyword in_str in_line put yn
syntax keyword spellDecl def class
syntax keyword spellBuiltin self
syntax keyword spellConstant true false

" 関数定義・呼び出し
syntax match spellFuncDef "def\s\+\zs\h\w*"
syntax match spellFuncCall "\<\h\w*\ze\s*("

" dunder メソッド
syntax match spellDunder "__\h\w*__"

" 型（組み込み型と、大文字始まりのクラス名）
syntax keyword spellType int bool str
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
highlight default link spellDecl Structure
highlight default link spellBuiltin Statement
highlight default link spellConstant Constant
highlight default link spellType Type
highlight default link spellNumber Number
highlight default link spellString String
highlight default link spellEscape SpecialChar
highlight default link spellFuncDef Function
highlight default link spellFuncCall Function
highlight default link spellDunder Special
highlight default link spellOperator Operator

let b:current_syntax = "spell"
