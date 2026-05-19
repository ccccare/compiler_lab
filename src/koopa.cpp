#include "koopa.hpp"

#include <stdexcept>
#include <string>

void KoopaGenerator::Generate(const BaseAST &ast) {
  const auto *comp_unit = dynamic_cast<const CompUnitAST *>(&ast);
  if (comp_unit == nullptr) {
    throw std::runtime_error("root AST node must be CompUnitAST");
  }

  GenerateCompUnit(*comp_unit);
}

void KoopaGenerator::GenerateCompUnit(const CompUnitAST &ast) {
  if (!ast.func_def) {
    throw std::runtime_error("CompUnitAST has no function definition");
  }

  GenerateFuncDef(*ast.func_def);
}

void KoopaGenerator::GenerateFuncDef(const FuncDefAST &ast) {
  if (ast.ident != "main") {
    throw std::runtime_error("Lv1 only supports function named main");
  }

  if (ast.ret_type != TypeKind::Int) {
    throw std::runtime_error("Lv1 only supports int main()");
  }

  os_ << "fun @" << ast.ident << "(): i32 {\n";
  os_ << "%entry:\n";

  GenerateBlock(*ast.block);

  os_ << "}\n";
}

void KoopaGenerator::GenerateBlock(const BlockAST &ast) {
  for (const auto &item : ast.items) {
    GenerateBlockItem(*item);
  }
}

void KoopaGenerator::GenerateBlockItem(const BlockItemAST &ast) {
  const auto *stmt = dynamic_cast<const StmtAST *>(&ast);
  if (stmt == nullptr) {
    throw std::runtime_error("Lv1 only supports statements as block items");
  }

  GenerateStmt(*stmt);
}

void KoopaGenerator::GenerateStmt(const StmtAST &ast) {
  const auto *ret_stmt = dynamic_cast<const ReturnStmtAST *>(&ast);
  if (ret_stmt == nullptr) {
    throw std::runtime_error("Lv1 only supports return statements");
  }

  os_ << "  ret " << GenerateExpr(*ret_stmt->value) << "\n";
}

std::string KoopaGenerator::GenerateExpr(const ExprAST &ast) {
  const auto *number = dynamic_cast<const NumberAST *>(&ast);
  if (number == nullptr) {
    throw std::runtime_error("Lv1 only supports number expressions");
  }

  return std::to_string(number->value);
}