#pragma once

#include <cstdint>
#include <ostream>
#include <string>
#include <unordered_map>
#include <vector>

#include "ast.hpp"

class KoopaGenerator {
 public:
  explicit KoopaGenerator(std::ostream &os) : os_(os) {}

  void Generate(const BaseAST &ast);

 private:
  enum class SymbolKind {
    Const,
    Var,
  };

  struct SymbolInfo {
    SymbolKind kind = SymbolKind::Const;
    std::int32_t const_value = 0;
    std::string ir_name;
  };

  std::ostream &os_;

  int temp_id_ = 0;
  int var_id_ = 0;
  bool current_block_terminated_ = false;

  std::vector<std::unordered_map<std::string, SymbolInfo>> scopes_;

  std::string NewTemp();
  std::string NewVar();

  void EnterScope();
  void ExitScope();
  void InsertSymbol(const std::string &name, const SymbolInfo &info);
  const SymbolInfo &LookupSymbol(const std::string &name) const;

  void GenerateCompUnit(const CompUnitAST &ast);
  void GenerateFuncDef(const FuncDefAST &ast);
  void GenerateBlock(const BlockAST &ast);

  void GenerateBlockItem(const BlockItemAST &ast);
  void GenerateDecl(const DeclAST &ast);
  void GenerateConstDecl(const ConstDeclAST &ast);
  void GenerateVarDecl(const VarDeclAST &ast);
  void GenerateStmt(const StmtAST &ast);

  std::string GenerateExpr(const ExprAST &ast);

  std::int32_t EvalConstExpr(const ExprAST &ast) const;
};