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

static std::unique_ptr<LValAST> TakeLVal(BaseAST *ptr) {
  return std::unique_ptr<LValAST>(static_cast<LValAST *>(ptr));
}

static std::unique_ptr<BlockAST> TakeBlock(BaseAST *ptr) {
  return std::unique_ptr<BlockAST>(static_cast<BlockAST *>(ptr));
}

static std::unique_ptr<BlockItemAST> TakeBlockItem(BaseAST *ptr) {
  return std::unique_ptr<BlockItemAST>(static_cast<BlockItemAST *>(ptr));
}

static std::unique_ptr<ConstDefAST> TakeConstDef(BaseAST *ptr) {
  return std::unique_ptr<ConstDefAST>(static_cast<ConstDefAST *>(ptr));
}

static std::unique_ptr<VarDefAST> TakeVarDef(BaseAST *ptr) {
  return std::unique_ptr<VarDefAST>(static_cast<VarDefAST *>(ptr));
}

static std::unique_ptr<FuncFParamAST> TakeFuncFParam(BaseAST *ptr) {
  return std::unique_ptr<FuncFParamAST>(static_cast<FuncFParamAST *>(ptr));
}

static std::unique_ptr<FuncFParamListAST> TakeFuncFParamList(BaseAST *ptr) {
  return std::unique_ptr<FuncFParamListAST>(
      static_cast<FuncFParamListAST *>(ptr));
}

static std::unique_ptr<ExprListAST> TakeExprList(BaseAST *ptr) {
  return std::unique_ptr<ExprListAST>(static_cast<ExprListAST *>(ptr));
}

static std::unique_ptr<InitValAST> TakeInitVal(BaseAST *ptr) {
  return std::unique_ptr<InitValAST>(static_cast<InitValAST *>(ptr));
}

static std::unique_ptr<ExprVectorAST> TakeExprVector(BaseAST *ptr) {
  return std::unique_ptr<ExprVectorAST>(static_cast<ExprVectorAST *>(ptr));
}

%}

%parse-param { std::unique_ptr<BaseAST> &ast }

%union {
  std::string *str_val;
  std::int64_t int_val;
  BaseAST *ast_val;
}

%token CONST INT VOID RETURN IF ELSE WHILE BREAK CONTINUE
%token LE GE EQ NE LAND LOR

%token <str_val> IDENT
%token <int_val> INT_CONST

%nonassoc LOWER_THAN_ELSE
%nonassoc ELSE

%type <ast_val> FuncDef Block BlockItemList BlockItem
%type <ast_val> Decl ConstDecl ConstDefList ConstDef ConstInitVal ConstExp
%type <ast_val> VarDecl VarDefList VarDef InitVal
%type <ast_val> Stmt LVal
%type <ast_val> Exp LOrExp LAndExp EqExp RelExp AddExp MulExp UnaryExp PrimaryExp Number
%type <ast_val> CompUnitItem CompUnitItemList
%type <ast_val> FuncFParams FuncFParam
%type <ast_val> FuncRParams
%type <ast_val> ArrayDims
%type <ast_val> InitValList ConstInitValList
%type <ast_val> ArrayIndices

%%

CompUnit
  : CompUnitItemList {
      ast = std::unique_ptr<BaseAST>(static_cast<BaseAST *>($1));
    }
  ;

CompUnitItemList
  : CompUnitItem {
      auto comp_unit = std::make_unique<CompUnitAST>();
      comp_unit->items.emplace_back(
          std::unique_ptr<BaseAST>(static_cast<BaseAST *>($1)));
      $$ = comp_unit.release();
    }
  | CompUnitItemList CompUnitItem {
      auto comp_unit = static_cast<CompUnitAST *>($1);
      comp_unit->items.emplace_back(
          std::unique_ptr<BaseAST>(static_cast<BaseAST *>($2)));
      $$ = comp_unit;
    }
  ;

CompUnitItem
  : Decl {
      $$ = $1;
    }
  | FuncDef {
      $$ = $1;
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
  | INT IDENT '(' FuncFParams ')' Block {
      auto func = std::make_unique<FuncDefAST>();

      func->ret_type = TypeKind::Int;

      std::unique_ptr<std::string> ident($2);
      func->ident = *ident;

      auto params = TakeFuncFParamList($4);
      func->params = std::move(params->params);

      func->block = TakeBlock($6);

      $$ = func.release();
    }
  | VOID IDENT '(' ')' Block {
      auto func = std::make_unique<FuncDefAST>();

      func->ret_type = TypeKind::Void;

      std::unique_ptr<std::string> ident($2);
      func->ident = *ident;

      func->block = TakeBlock($5);

      $$ = func.release();
    }
  | VOID IDENT '(' FuncFParams ')' Block {
      auto func = std::make_unique<FuncDefAST>();

      func->ret_type = TypeKind::Void;

      std::unique_ptr<std::string> ident($2);
      func->ident = *ident;

      auto params = TakeFuncFParamList($4);
      func->params = std::move(params->params);

      func->block = TakeBlock($6);

      $$ = func.release();
    }
  ;

FuncFParams
  : FuncFParam {
      auto list = std::make_unique<FuncFParamListAST>();
      list->params.emplace_back(TakeFuncFParam($1));
      $$ = list.release();
    }
  | FuncFParams ',' FuncFParam {
      auto list = static_cast<FuncFParamListAST *>($1);
      list->params.emplace_back(TakeFuncFParam($3));
      $$ = list;
    }
  ;

FuncFParam
  : INT IDENT {
      auto param = std::make_unique<FuncFParamAST>();
      param->type = TypeKind::Int;

      std::unique_ptr<std::string> ident($2);
      param->ident = *ident;

      $$ = param.release();
    }
  | INT IDENT '[' ']' {
      auto param = std::make_unique<FuncFParamAST>();
      param->type = TypeKind::Int;
      param->is_array = true;

      std::unique_ptr<std::string> ident($2);
      param->ident = *ident;

      $$ = param.release();
    }
  | INT IDENT '[' ']' ArrayDims {
      auto param = std::make_unique<FuncFParamAST>();
      param->type = TypeKind::Int;
      param->is_array = true;

      std::unique_ptr<std::string> ident($2);
      param->ident = *ident;

      auto dims = TakeExprVector($5);
      param->dims = std::move(dims->exprs);

      $$ = param.release();
    }
  ;

FuncRParams
  : Exp {
      auto list = std::make_unique<ExprListAST>();
      list->exprs.emplace_back(TakeExpr($1));
      $$ = list.release();
    }
  | FuncRParams ',' Exp {
      auto list = static_cast<ExprListAST *>($1);
      list->exprs.emplace_back(TakeExpr($3));
      $$ = list;
    }
  ;

Block
  : '{' BlockItemList '}' {
      $$ = $2;
    }
  ;

BlockItemList
  : /* empty */ {
      $$ = new BlockAST();
    }
  | BlockItemList BlockItem {
      auto block = static_cast<BlockAST *>($1);
      block->items.emplace_back(TakeBlockItem($2));
      $$ = block;
    }
  ;

BlockItem
  : Decl {
      $$ = $1;
    }
  | Stmt {
      $$ = $1;
    }
  ;

Decl
  : ConstDecl {
      $$ = $1;
    }
  | VarDecl {
      $$ = $1;
    }
  ;

ConstDecl
  : CONST INT ConstDefList ';' {
      $$ = $3;
    }
  ;

BType
  : INT
  ;

ConstDefList
  : ConstDef {
      auto decl = std::make_unique<ConstDeclAST>();
      decl->defs.emplace_back(TakeConstDef($1));
      $$ = decl.release();
    }
  | ConstDefList ',' ConstDef {
      auto decl = static_cast<ConstDeclAST *>($1);
      decl->defs.emplace_back(TakeConstDef($3));
      $$ = decl;
    }
  ;

ConstDef
  : IDENT '=' ConstInitVal {
      auto def = std::make_unique<ConstDefAST>();

      std::unique_ptr<std::string> ident($1);
      def->ident = *ident;

      def->init = TakeInitVal($3);

      $$ = def.release();
    }
  | IDENT ArrayDims '=' ConstInitVal {
      auto def = std::make_unique<ConstDefAST>();

      std::unique_ptr<std::string> ident($1);
      def->ident = *ident;

      auto dims = TakeExprVector($2);
      def->dims = std::move(dims->exprs);

      def->init = TakeInitVal($4);

      $$ = def.release();
    }
  ;

ConstInitVal
  : ConstExp {
      $$ = InitValAST::FromExpr(TakeExpr($1)).release();
    }
  | '{' '}' {
      auto list = std::vector<std::unique_ptr<InitValAST>>();
      $$ = InitValAST::FromList(std::move(list)).release();
    }
  | '{' ConstInitValList '}' {
      $$ = $2;
    }
  ;

ConstInitValList
  : ConstInitVal {
      std::vector<std::unique_ptr<InitValAST>> items;
      items.emplace_back(TakeInitVal($1));
      $$ = InitValAST::FromList(std::move(items)).release();
    }
  | ConstInitValList ',' ConstInitVal {
      auto list = static_cast<InitValAST *>($1);
      list->list.emplace_back(TakeInitVal($3));
      $$ = list;
    }
  ;

ConstExp
  : Exp {
      $$ = $1;
    }
  ;

VarDecl
  : INT VarDefList ';' {
      $$ = $2;
    }
  ;

VarDefList
  : VarDef {
      auto decl = std::make_unique<VarDeclAST>();
      decl->defs.emplace_back(TakeVarDef($1));
      $$ = decl.release();
    }
  | VarDefList ',' VarDef {
      auto decl = static_cast<VarDeclAST *>($1);
      decl->defs.emplace_back(TakeVarDef($3));
      $$ = decl;
    }
  ;

VarDef
  : IDENT {
      auto def = std::make_unique<VarDefAST>();

      std::unique_ptr<std::string> ident($1);
      def->ident = *ident;

      $$ = def.release();
    }
  | IDENT '=' InitVal {
      auto def = std::make_unique<VarDefAST>();

      std::unique_ptr<std::string> ident($1);
      def->ident = *ident;

      def->init = TakeInitVal($3);

      $$ = def.release();
    }
  | IDENT ArrayDims {
      auto def = std::make_unique<VarDefAST>();

      std::unique_ptr<std::string> ident($1);
      def->ident = *ident;

      auto dims = TakeExprVector($2);
      def->dims = std::move(dims->exprs);

      $$ = def.release();
    }
  | IDENT ArrayDims '=' InitVal {
      auto def = std::make_unique<VarDefAST>();

      std::unique_ptr<std::string> ident($1);
      def->ident = *ident;

      auto dims = TakeExprVector($2);
      def->dims = std::move(dims->exprs);

      def->init = TakeInitVal($4);

      $$ = def.release();
    }
  ;

ArrayDims
  : '[' ConstExp ']' {
      auto dims = std::make_unique<ExprVectorAST>();
      dims->exprs.emplace_back(TakeExpr($2));
      $$ = dims.release();
    }
  | ArrayDims '[' ConstExp ']' {
      auto dims = static_cast<ExprVectorAST *>($1);
      dims->exprs.emplace_back(TakeExpr($3));
      $$ = dims;
    }
  ;

InitVal
  : Exp {
      $$ = InitValAST::FromExpr(TakeExpr($1)).release();
    }
  | '{' '}' {
      auto list = std::vector<std::unique_ptr<InitValAST>>();
      $$ = InitValAST::FromList(std::move(list)).release();
    }
  | '{' InitValList '}' {
      $$ = $2;
    }
  ;

InitValList
  : InitVal {
      std::vector<std::unique_ptr<InitValAST>> items;
      items.emplace_back(TakeInitVal($1));
      $$ = InitValAST::FromList(std::move(items)).release();
    }
  | InitValList ',' InitVal {
      auto list = static_cast<InitValAST *>($1);
      list->list.emplace_back(TakeInitVal($3));
      $$ = list;
    }
  ;

Stmt
  : LVal '=' Exp ';' {
      auto stmt = std::make_unique<AssignStmtAST>();
      stmt->lval = TakeLVal($1);
      stmt->value = TakeExpr($3);

      $$ = stmt.release();
    }
  | Exp ';' {
      auto stmt = std::make_unique<ExprStmtAST>();
      stmt->expr = TakeExpr($1);
      $$ = stmt.release();
    }
  | ';' {
      auto stmt = std::make_unique<ExprStmtAST>();
      $$ = stmt.release();
    }
  | Block {
      auto stmt = std::make_unique<BlockStmtAST>();
      stmt->block = TakeBlock($1);
      $$ = stmt.release();
    }
  | IF '(' Exp ')' Stmt %prec LOWER_THAN_ELSE {
      auto stmt = std::make_unique<IfStmtAST>();
      stmt->cond = TakeExpr($3);
      stmt->then_stmt = std::unique_ptr<StmtAST>(static_cast<StmtAST *>($5));
      $$ = stmt.release();
    }
  | IF '(' Exp ')' Stmt ELSE Stmt {
      auto stmt = std::make_unique<IfStmtAST>();
      stmt->cond = TakeExpr($3);
      stmt->then_stmt = std::unique_ptr<StmtAST>(static_cast<StmtAST *>($5));
      stmt->else_stmt = std::unique_ptr<StmtAST>(static_cast<StmtAST *>($7));
      $$ = stmt.release();
    }
  | WHILE '(' Exp ')' Stmt {
    auto stmt = std::make_unique<WhileStmtAST>();
    stmt->cond = TakeExpr($3);
    stmt->body = std::unique_ptr<StmtAST>(static_cast<StmtAST *>($5));
    $$ = stmt.release();
  }
  | BREAK ';' {
    $$ = new BreakStmtAST();
  }
  | CONTINUE ';' {
    $$ = new ContinueStmtAST();
  }
  | RETURN Exp ';' {
    auto stmt = std::make_unique<ReturnStmtAST>();
    stmt->value = TakeExpr($2);
    $$ = stmt.release();
  }
  | RETURN ';' {
    auto stmt = std::make_unique<ReturnStmtAST>();
    $$ = stmt.release();
  }
  ;

LVal
  : IDENT {
      std::unique_ptr<std::string> ident($1);
      $$ = new LValAST(*ident);
    }
  | IDENT ArrayIndices {
      std::unique_ptr<std::string> ident($1);
      auto lval = std::make_unique<LValAST>(*ident);

      auto indices = TakeExprVector($2);
      lval->indices = std::move(indices->exprs);

      $$ = lval.release();
    }
  ;

ArrayIndices
  : '[' Exp ']' {
      auto indices = std::make_unique<ExprVectorAST>();
      indices->exprs.emplace_back(TakeExpr($2));
      $$ = indices.release();
    }
  | ArrayIndices '[' Exp ']' {
      auto indices = static_cast<ExprVectorAST *>($1);
      indices->exprs.emplace_back(TakeExpr($3));
      $$ = indices;
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
  | IDENT '(' ')' {
      auto call = std::make_unique<CallExprAST>();

      std::unique_ptr<std::string> ident($1);
      call->ident = *ident;

      $$ = call.release();
    }
  | IDENT '(' FuncRParams ')' {
      auto call = std::make_unique<CallExprAST>();

      std::unique_ptr<std::string> ident($1);
      call->ident = *ident;

      auto args = TakeExprList($3);
      call->args = std::move(args->exprs);

      $$ = call.release();
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
  | LVal {
      $$ = $1;
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