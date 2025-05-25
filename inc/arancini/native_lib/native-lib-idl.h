#pragma once

#ifndef __FLEX_LEXER_H // Not sure why this is necessary but it is
#undef yyFlexLexer
#define yyFlexLexer nativeLibFlexLexer
#include <FlexLexer.h>
#endif
#undef YY_DECL
#define YY_DECL                                                                \
    arancini::native_lib::Parser::token_type                                   \
    arancini::native_lib::Scanner::lex(                                        \
        arancini::native_lib::Parser::semantic_type *yylval,                   \
        arancini::native_lib::Parser::location_type *yylloc)

#include "location.hh"
#include "native-lib-idl.tab.h"

namespace arancini::native_lib {
class Scanner : public nativeLibFlexLexer {
  public:
    Scanner() = default;

    virtual Parser::token_type lex(Parser::semantic_type *yylval,
                                   Parser::location_type *yylloc);
};
} // namespace arancini::native_lib
