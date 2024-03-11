%language "C++"
%skeleton "lalr1.cc"
%require "3.2"

%locations

%define api.namespace {arancini::native_lib}
%define api.parser.class {Parser}
%define api.token.raw
%define api.value.type variant

%define			parse.assert
%define			parse.trace
%define			parse.error			detailed
%define			parse.lac			full


%code requires {
	#include <string>

    #include <arancini/native_lib/idl-ast-node.h>


	namespace arancini::native_lib {
		class Scanner;
		class NativeLibs;
	}


}


%parse-param	{  arancini::native_lib::Scanner& scanner }
%parse-param { arancini::native_lib::NativeLibs& driver }

%code {

	#include <arancini/native_lib/native-lib.h>
	#include <arancini/native_lib/native-lib-idl.h>


	#define yylex scanner.lex

}

/*
 * idl  : defs
 * defs : def defs
 *      |
 * def  : libdef
 *      | fndef
 *      | cmndef
 * libdef : `library` libname `;`
 * libname : `"` .* `"`
 * fndef : ctypedef fname `(` params `)` `;`
 * typedef : `i8` | `i16` | `i32` | `i64` | `ilong` | `u1` | `u8` | `u16` | `u32` | `u64` | `ulong` | `f32` | `f64` | `fd` | `string` | ptrdef
 * ctypedef: typedef | `const` typedef
 * ptrdef : `ptr` | `ptr` `(` paramname `)`
 * params : params `,` param
 *        | param
 *        |
 * param : ctypedef paramname
 * paramname: [A-Za-z]+[A-Za-z0-9]*
 */


%token T_LIBRARY T_CALLCONV T_STRING T_FD T_CPLX T_CONST T_PTR T_VOID
%token T_I8 T_I16 T_I32 T_I64 T_ILONG T_U1 T_U8 T_U16 T_U32 T_U64 T_ULONG T_F32 T_F64
%token T_STAR T_SEMI T_LPAREN T_RPAREN T_LBRACKET T_RBRACKET T_COMMA
%token <std::string> STRING_LITERAL IDENTIFIER

%start idl

%type <idl_ast_node> idl defs def libdef fndef const_typedef typedef ptrdef params param attrs attr maybe_attrlist ccdef full_typedef

%%

idl: defs { $$ = idl_ast_node(IANT_ROOT); $$.add_child(std::move($1)); driver.idl_commit($$); };

defs: defs def { $$ = $1; $$.add_child(std::move($2)); }
    | def { $$ = idl_ast_node(IANT_DEFS);$$.add_child(std::move($1)); }
    ;

def: libdef { $$ = $1; }
   | ccdef { $$ = $1; }
   | fndef { $$ = $1; }
   ;

libdef: T_LIBRARY STRING_LITERAL T_SEMI { $$ = idl_ast_node(IANT_LIBDEF); $$.value = $2; };

ccdef: T_CALLCONV IDENTIFIER T_SEMI { $$ = idl_ast_node(IANT_CCDEF); $$.value = $2; };

fndef: maybe_attrlist full_typedef IDENTIFIER T_LPAREN params T_RPAREN T_SEMI { $$ = idl_ast_node(IANT_FNDEF); $$.value = $3; $$.add_child(std::move($2));  $$.add_child(std::move($5)); };

maybe_attrlist: { $$ = idl_ast_node(IANT_ATTRS); }
              | T_LBRACKET attrs T_RBRACKET { $$ = $2; }
              ;

attrs: attr { $$ = idl_ast_node(IANT_ATTRS);  $$.add_child(std::move($1)); }
     | attrs T_COMMA attr { $$ = $1;  $$.add_child(std::move($3)); }
     ;

attr: IDENTIFIER { $$ = idl_ast_node(IANT_ATTR); $$.value = $1; }
    ;

full_typedef: maybe_attrlist const_typedef { $$ = $2; };

const_typedef: typedef          { $$ = $1; $$.is_const = 0; }
             | T_CONST typedef  { $$ = $2; $$.is_const = 1; }
             ;

typedef: ptrdef { $$ = $1; }
    | T_I8      { $$ = idl_ast_node(IANT_TYPEDEF); $$.width = 8; $$.tc = 1; }
    | T_I16     { $$ = idl_ast_node(IANT_TYPEDEF); $$.width = 16; $$.tc = 1; }
    | T_I32     { $$ = idl_ast_node(IANT_TYPEDEF); $$.width = 32; $$.tc = 1; }
    | T_I64     { $$ = idl_ast_node(IANT_TYPEDEF); $$.width = 64; $$.tc = 1; }
    | T_ILONG   { $$ = idl_ast_node(IANT_TYPEDEF); $$.width = 64; $$.tc = 1; }
    | T_U1      { $$ = idl_ast_node(IANT_TYPEDEF); $$.width = 1; $$.tc = 2; }
    | T_U8      { $$ = idl_ast_node(IANT_TYPEDEF); $$.width = 8; $$.tc = 2; }
    | T_U16     { $$ = idl_ast_node(IANT_TYPEDEF); $$.width = 16; $$.tc = 2; }
    | T_U32     { $$ = idl_ast_node(IANT_TYPEDEF); $$.width = 32; $$.tc = 2; }
    | T_U64     { $$ = idl_ast_node(IANT_TYPEDEF); $$.width = 64; $$.tc = 2; }
    | T_ULONG   { $$ = idl_ast_node(IANT_TYPEDEF); $$.width = 64; $$.tc = 2; }
    | T_F32     { $$ = idl_ast_node(IANT_TYPEDEF); $$.width = 32; $$.tc = 3; }
    | T_F64     { $$ = idl_ast_node(IANT_TYPEDEF); $$.width = 64; $$.tc = 3; }
    | T_STRING  { $$ = idl_ast_node(IANT_TYPEDEF); $$.width = 64; $$.tc = 4; }
    | T_VOID    { $$ = idl_ast_node(IANT_TYPEDEF); $$.width = 0; $$.tc = 0; }
    | T_CPLX    { $$ = idl_ast_node(IANT_TYPEDEF); $$.width = 0; $$.tc = 8; }
    ;

ptrdef: T_PTR { $$ = idl_ast_node(IANT_TYPEDEF); $$.width = 64; $$.tc = 5; }
      | T_PTR T_LPAREN IDENTIFIER T_RPAREN { $$ = idl_ast_node(IANT_TYPEDEF); $$.width = 64; $$.tc = 5; }
      ;

params: params T_COMMA param    { $$ = $1;  $$.add_child(std::move($3)); }
      | param                   { $$ = idl_ast_node(IANT_PARAMS);  $$.add_child(std::move($1)); }
      |                         { $$ = idl_ast_node(IANT_PARAMS); }
      ;

param: full_typedef IDENTIFIER { $$ = idl_ast_node(IANT_PARAM); $$.value = $2;  $$.add_child(std::move($1)); };

%%
// Bison expects us to provide implementation - otherwise linker complains
void arancini::native_lib::Parser::error(const arancini::native_lib::location &loc , const std::string &message) {

		std::cerr << "Error: " << message << "\nLocation: " << loc << '\n';
}