if exists("b:current_syntax")
  finish
endif

syntax keyword blCast cast
syntax keyword blAuto auto

syntax keyword blStruct struct
syntax keyword blUnion union  
syntax keyword blEnum enum
syntax keyword blFn fn 

syntax keyword blIf if
syntax keyword blThen then
syntax keyword blElse else
syntax keyword blFor loop
syntax keyword blBreak break 
syntax keyword blContinue continue 
syntax keyword blUnreachable unreachable
syntax keyword blSwitch switch ->

syntax keyword blDataType void string u8 u16 u32 u64 s8 s16 s32 s64 bool usize type f32 f64
syntax keyword blBool true false
syntax keyword blNull null

syntax keyword blReturn return
syntax keyword blDefer defer
syntax keyword blAuto auto
syntax keyword blSizeof sizeof 
syntax keyword blTypeInfo typeinfo
syntax keyword blTestCases testcases 

syntax region blString start=/\v"/ skip=/\v\\./ end=/\v"/

syntax match blFunction "\v<\w*>(\s*::\s*)@="
syntax match blDynamicFunction "\v<\w*(\s*:\=\s*\(.*\))@="

syntax match blClass "\v<[A-Z]\w+>" display
syntax match blConstant "\v<[A-Z0-9,_]+>" display

syntax match blInteger "\<\d\+\>" display
syntax match blFloat "\<[0-9][0-9_]*\%(\.[0-9][0-9_]*\)\%([eE][+-]\=[0-9_]\+\)\=" display
syntax match blHex "\<0x[0-9A-Fa-f]\+\>" display

syntax match blMacro "#\<\w\+\>" display

syntax match blTemplate "$\<\w\+\>"

syntax match blCommentNote "@\<\w\+\>" contained display
syntax match blLineComment "//.*" contains=blCommentNote
syntax region blBlockComment start=/\v\/\*/ end=/\v\*\// contains=blBlockComment, blCommentNote

highlight link blCast Keyword
highlight link blAuto Keyword
highlight link blReturn Keyword
highlight link blBreak Keyword
highlight link blContinue Keyword
highlight link blUnreachable Keyword
highlight link blDefer Operator
highlight link blAuto Operator 
highlight link blSizeof Operator
highlight link blTypeInfo Operator
highlight link blTestCases Operator

highlight link blString String

highlight link blStruct Structure
highlight link blUnion Structure
highlight link blEnum Structure

highlight link blFunction Function
highlight link blFn Function
highlight link blDynamicFunction Function

highlight link blMacro Macro
highlight link blIf Conditional
highlight link blThen Conditional
highlight link blSwitch Conditional
highlight link blElse Conditional
highlight link blFor Repeat

highlight link blLineComment Comment
highlight link blBlockComment Comment
highlight link blCommentNote Todo

highlight link blClass Type

highlight link blTemplate Constant

highlight link blDataType Type
highlight link blBool Boolean
highlight link blConstant Constant
highlight link blNull Type
highlight link blInteger Number
highlight link blFloat Float
highlight link blHex Number

let b:current_syntax = "bl"
