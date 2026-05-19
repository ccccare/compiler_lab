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

static std::unique_ptr<ExprAST> TakeExpr(BaseAST *ptr) {
  return std::unique_ptr<ExprAST>(static_cast<ExprAST *>(ptr));
}

static std::unique_ptr<BlockAST> TakeBlock(BaseAST *ptr) {
  return std::unique_ptr<BlockAST>(static_cast<BlockAST *>(ptr));
}

static std::unique_ptr<BlockItemAST> TakeBlockItem(BaseAST *ptr) {
  return std::unique_ptr<BlockItemAST>(static_cast<BlockItemAST *>(ptr));
}
%}

%parse-param { std::unique_ptr<BaseAST> &ast }

%union {
  std::string *str_val;
  std::int64_t int_val;
  BaseAST *ast_val;
}

%token INT RETURN
%token LE GE EQ NE LAND LOR

%token <str_val> IDENT
%token <int_val> INT_CONST

%type <ast_val> FuncDef Block Stmt
%type <ast_val> Exp LOrExp LAndExp EqExp RelExp AddExp MulExp UnaryExp PrimaryExp Number

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

      func->block = TakeBlock($5);

      $$ = func.release();
    }
  ;

Block
  : '{' Stmt '}' {
      auto block = std::make_unique<BlockAST>();
      block->items.emplace_back(TakeBlockItem($2));
      $$ = block.release();
    }
  ;

Stmt
  : RETURN Exp ';' {
      auto stmt = std::make_unique<ReturnStmtAST>();
      stmt->value = TakeExpr($2);
      $$ = stmt.release();
    }
  ;

Exp
  : LOrExp {
      $$ = $1;
    }
  ;

LOrExp
  : LAndExp {
      $$ = $1;
    }
  | LOrExp LOR LAndExp {
      $$ = new BinaryExprAST(BinaryOp::LOr, TakeExpr($1), TakeExpr($3));
    }
  ;

LAndExp
  : EqExp {
      $$ = $1;
    }
  | LAndExp LAND EqExp {
      $$ = new BinaryExprAST(BinaryOp::LAnd, TakeExpr($1), TakeExpr($3));
    }
  ;

EqExp
  : RelExp {
      $$ = $1;
    }
  | EqExp EQ RelExp {
      $$ = new BinaryExprAST(BinaryOp::Eq, TakeExpr($1), TakeExpr($3));
    }
  | EqExp NE RelExp {
      $$ = new BinaryExprAST(BinaryOp::Ne, TakeExpr($1), TakeExpr($3));
    }
  ;

RelExp
  : AddExp {
      $$ = $1;
    }
  | RelExp '<' AddExp {
      $$ = new BinaryExprAST(BinaryOp::Lt, TakeExpr($1), TakeExpr($3));
    }
  | RelExp '>' AddExp {
      $$ = new BinaryExprAST(BinaryOp::Gt, TakeExpr($1), TakeExpr($3));
    }
  | RelExp LE AddExp {
      $$ = new BinaryExprAST(BinaryOp::Le, TakeExpr($1), TakeExpr($3));
    }
  | RelExp GE AddExp {
      $$ = new BinaryExprAST(BinaryOp::Ge, TakeExpr($1), TakeExpr($3));
    }
  ;

AddExp
  : MulExp {
      $$ = $1;
    }
  | AddExp '+' MulExp {
      $$ = new BinaryExprAST(BinaryOp::Add, TakeExpr($1), TakeExpr($3));
    }
  | AddExp '-' MulExp {
      $$ = new BinaryExprAST(BinaryOp::Sub, TakeExpr($1), TakeExpr($3));
    }
  ;

MulExp
  : UnaryExp {
      $$ = $1;
    }
  | MulExp '*' UnaryExp {
      $$ = new BinaryExprAST(BinaryOp::Mul, TakeExpr($1), TakeExpr($3));
    }
  | MulExp '/' UnaryExp {
      $$ = new BinaryExprAST(BinaryOp::Div, TakeExpr($1), TakeExpr($3));
    }
  | MulExp '%' UnaryExp {
      $$ = new BinaryExprAST(BinaryOp::Mod, TakeExpr($1), TakeExpr($3));
    }
  ;

UnaryExp
  : PrimaryExp {
      $$ = $1;
    }
  | '+' UnaryExp {
      $$ = new UnaryExprAST(UnaryOp::Plus, TakeExpr($2));
    }
  | '-' UnaryExp {
      $$ = new UnaryExprAST(UnaryOp::Minus, TakeExpr($2));
    }
  | '!' UnaryExp {
      $$ = new UnaryExprAST(UnaryOp::Not, TakeExpr($2));
    }
  ;

PrimaryExp
  : '(' Exp ')' {
      $$ = $2;
    }
  | Number {
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