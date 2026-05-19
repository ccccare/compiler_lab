#pragma once

#include <ostream>
#include <string>

#include "ast.hpp"

class KoopaGenerator {
 public:
  explicit KoopaGenerator(std::ostream &os) : os_(os) {}

  void Generate(const BaseAST &ast);

 private:
  std::ostream &os_;
  int temp_id_ = 0;

  std::string NewTemp();

  void GenerateCompUnit(const CompUnitAST &ast);
  void GenerateFuncDef(const FuncDefAST &ast);
  void GenerateBlock(const BlockAST &ast);

  void GenerateBlockItem(const BlockItemAST &ast);
  void GenerateStmt(const StmtAST &ast);

  std::string GenerateExpr(const ExprAST &ast);
};