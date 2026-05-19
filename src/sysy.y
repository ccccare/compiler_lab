%code requires {
  #include <cstdint>
  #include <memory>
  #include <string>

  #include "ast.hpp"
}

%{
#include <iostream>
#include <memory>
#include <string>

#include "ast.hpp"

int yylex();
void yyerror(std::unique_ptr<BaseAST> &ast, const char *s);
%}

%parse-param { std::unique_ptr<BaseAST> &ast }

%union {
  std::string *str_val;
  std::int64_t int_val;
  BaseAST *ast_val;
}

%token INT RETURN
%token <str_val> IDENT
%token <int_val> INT_CONST

%type <ast_val> FuncDef Block Stmt Exp Number

%%

CompUnit
  : FuncDef {
      auto comp_unit = std::make_unique<CompUnitAST>();
      comp_unit->func_def =
          std::unique_ptr<FuncDefAST>(static_cast<FuncDefAST *>($1));
      ast = std::move(comp_unit);
    }
  ;

FuncDef
  : INT IDENT '(' ')' Block {
      auto func = std::make_unique<FuncDefAST>();

      func->ret_type = TypeKind::Int;

      std::unique_ptr<std::string> ident($2);
      func->ident = *ident;

      func->block = std::unique_ptr<BlockAST>(static_cast<BlockAST *>($5));

      $$ = func.release();
    }
  ;

Block
  : '{' Stmt '}' {
      auto block = std::make_unique<BlockAST>();
      block->items.emplace_back(
          std::unique_ptr<BlockItemAST>(static_cast<BlockItemAST *>($2)));
      $$ = block.release();
    }
  ;

Stmt
  : RETURN Exp ';' {
      auto stmt = std::make_unique<ReturnStmtAST>();
      stmt->value = std::unique_ptr<ExprAST>(static_cast<ExprAST *>($2));
      $$ = stmt.release();
    }
  ;

Exp
  : Number {
      $$ = $1;
    }
  ;

Number
  : INT_CONST {
      $$ = new NumberAST($1);
    }
  ;

%%

void yyerror(std::unique_ptr<BaseAST> &ast, const char *s) {
  (void)ast;
  std::cerr << "parse error: " << s << std::endl;
}