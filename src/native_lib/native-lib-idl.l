%option noyywrap
%option c++
%option yyClass="arancini::native_lib::Scanner"
%option prefix="nativeLib"

%{
#include <string>
#include <arancini/native_lib/native-lib-idl.h>

#pragma GCC diagnostic ignored "-Wunused-function"

using Token = arancini::native_lib::Parser::token;

#define yyterminate() return( Token::YYEOF )

#define YY_USER_ACTION yylloc->step(); yylloc->columns(yyleng);
#define YY_VERBOSE
%}


%x comment str

%%

"#" BEGIN(comment);
"\"" BEGIN(str);

"library" { return Token::T_LIBRARY; }
"callconv" { return Token::T_CALLCONV; }

"i8" { return Token::T_I8; }
"i16" { return Token::T_I16; }
"i32" { return Token::T_I32; }
"i64" { return Token::T_I64; }
"ilong" { return Token::T_ILONG; }
"u1" { return Token::T_U1; }
"u8" { return Token::T_U8; }
"u16" { return Token::T_U16; }
"u32" { return Token::T_U32; }
"u64" { return Token::T_U64; }
"ulong" { return Token::T_ULONG; }
"f32" { return Token::T_F32; }
"f64" { return Token::T_F64; }

"string" { return Token::T_STRING; }
"fd" { return Token::T_FD; }
"cplx" { return Token::T_CPLX; }
"const" { return Token::T_CONST; }
"void" { return Token::T_VOID; }
"ptr" { return Token::T_PTR; }

"(" { return Token::T_LPAREN; }
")" { return Token::T_RPAREN; }
"[" { return Token::T_LBRACKET; }
"]" { return Token::T_RBRACKET; }
";" { return Token::T_SEMI; }
"," { return Token::T_COMMA; }
"*" { return Token::T_STAR; }

[a-zA-Z_][a-zA-Z0-9_]* { yylval->emplace<std::string>(yytext); return Token::IDENTIFIER; }
[ \t\r]+ {}
\n { yylloc->lines(); yylloc->step(); }

<comment>[^\n]* {};
<comment>\n { BEGIN(INITIAL); yylloc->lines(); yylloc->step();; }

<str>[^\"]* { yylval->emplace<std::string>(yytext); return Token::STRING_LITERAL; }
<str>\" { BEGIN(INITIAL); }
%%
