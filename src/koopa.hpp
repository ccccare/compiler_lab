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
    Func,
  };

  struct SymbolInfo {
    SymbolKind kind = SymbolKind::Const;

    std::int32_t const_value = 0;

    // Var: 变量对应的 Koopa 地址，例如 %v0 或 @g
    std::string ir_name;

    // Func:
    TypeKind return_type = TypeKind::Int;
    std::vector<TypeKind> param_types;
  };

  struct LoopInfo {
    std::string continue_label;
    std::string break_label;
  };

  std::ostream &os_;

  int temp_id_ = 0;
  int var_id_ = 0;
  bool current_block_terminated_ = false;
  int block_id_ = 0;
  TypeKind current_func_ret_type_ = TypeKind::Int;

  std::vector<std::unordered_map<std::string, SymbolInfo>> scopes_;
  std::vector<LoopInfo> loop_stack_;

  std::string NewTemp();
  std::string NewVar();
  std::string NewBlock(const std::string &prefix);

  void EmitBlockLabel(const std::string &label);
  void EmitJumpIfNeeded(const std::string &target);

  std::string GenerateBoolValue(const std::string &value);
  std::string GenerateLogicalAnd(const BinaryExprAST &ast);
  std::string GenerateLogicalOr(const BinaryExprAST &ast);
  void GenerateIfStmt(const IfStmtAST &ast);
  void GenerateWhileStmt(const WhileStmtAST &ast);
  void GenerateBreakStmt(const BreakStmtAST &ast);
  void GenerateContinueStmt(const ContinueStmtAST &ast);

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

  void EmitLibraryDecls();
  void InsertLibraryFunctions();
  void PredeclareFunctions(const CompUnitAST &ast);

  void GenerateCompUnitItem(const BaseAST &ast);
  void GenerateGlobalDecl(const DeclAST &ast);
  void GenerateGlobalConstDecl(const ConstDeclAST &ast);
  void GenerateGlobalVarDecl(const VarDeclAST &ast);

  std::string GenerateCallExpr(const CallExprAST &ast);

  std::string GenerateExpr(const ExprAST &ast);

  std::int32_t EvalConstExpr(const ExprAST &ast) const;
};