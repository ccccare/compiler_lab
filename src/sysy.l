%option noyywrap
%option nounput
%option noinput

%x COMMENT

%{
#include <cerrno>
#include <climits>
#include <cstdint>
#include <cstdlib>
#include <iostream>
#include <string>

#include "sysy.tab.hpp"

static std::int64_t ParseInteger(const char *text) {
  errno = 0;
  char *end = nullptr;
  long long value = std::strtoll(text, &end, 0);

  if (errno != 0 || end == text || *end != '\0') {
    std::cerr << "invalid integer literal: " << text << std::endl;
    std::exit(1);
  }

  if (value < 0 || value > INT_MAX) {
    std::cerr << "integer literal out of range: " << text << std::endl;
    std::exit(1);
  }

  return static_cast<std::int64_t>(value);
}
%}

WhiteSpace    [ \t\r\n]+
Identifier    [a-zA-Z_][a-zA-Z0-9_]*
Decimal       [1-9][0-9]*
Octal         0[0-7]*
Hexadecimal   0[xX][0-9a-fA-F]+

%%

{WhiteSpace}        { }

"//".*              { }

"/*"                { BEGIN(COMMENT); }
<COMMENT>"*/"       { BEGIN(INITIAL); }
<COMMENT>\n         { }
<COMMENT>.          { }
<COMMENT><<EOF>>    {
                       std::cerr << "unterminated block comment" << std::endl;
                       std::exit(1);
                     }

"int"               { return INT; }
"return"            { return RETURN; }

"<="                { return LE; }
">="                { return GE; }
"=="                { return EQ; }
"!="                { return NE; }
"&&"                { return LAND; }
"||"                { return LOR; }

{Identifier}        {
                       yylval.str_val = new std::string(yytext);
                       return IDENT;
                     }

{Decimal}           {
                       yylval.int_val = ParseInteger(yytext);
                       return INT_CONST;
                     }

{Octal}             {
                       yylval.int_val = ParseInteger(yytext);
                       return INT_CONST;
                     }

{Hexadecimal}       {
                       yylval.int_val = ParseInteger(yytext);
                       return INT_CONST;
                     }

.                   { return yytext[0]; }

%%