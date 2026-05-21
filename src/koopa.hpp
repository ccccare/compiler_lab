#pragma once

#include <cstddef>
#include <cstdint>
#include <memory>
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

  struct ParamTypeInfo {
    bool is_array = false;

    // 对数组参数，dims 保存“省略第一维之后”的剩余维度。
    // 例如：
    //   int a[]       -> dims = {}
    //   int a[][3]    -> dims = {3}
    //   int a[][3][4] -> dims = {3, 4}
    std::vector<int> dims;
  };

  struct SymbolInfo {
    SymbolKind kind = SymbolKind::Const;

    // const 标记：
    //   scalar const: kind = Const, is_const = true
    //   const array : kind = Var,   is_const = true, is_array = true
    bool is_const = false;

    // 标量 const 的编译期值。
    std::int32_t const_value = 0;

    // Var / Array / ArrayParam 对应的 Koopa 地址。
    // 例如：
    //   局部变量: %v0
    //   全局变量: @g
    //   数组形参保存指针的局部槽: %v1
    std::string ir_name;

    // 数组信息。
    bool is_array = false;

    // true 表示这是函数数组形参。
    // 例如 int a[] / int a[][3]。
    // 它在 Koopa 中实际是一个“保存指针的局部变量槽”：
    //   %v0 = alloc *i32
    //   store %p0, %v0
    bool is_array_param = false;

    // 普通数组：
    //   int a[2][3] -> dims = {2, 3}
    //
    // 数组形参：
    //   int a[]     -> dims = {}
    //   int a[][3]  -> dims = {3}
    std::vector<int> dims;

    // const 数组的编译期展开值，按行优先 flatten。
    // 例如：
    //   const int a[2][3] = {{1,2,3},{4,5,6}};
    // 保存为：
    //   {1,2,3,4,5,6}
    //
    // 用途：支持 ConstExp 中访问 const 数组元素，例如：
    //   const int a[3] = {1,2,3};
    //   const int x = a[1];
    std::vector<int> const_array_values;

    // 函数信息。
    TypeKind return_type = TypeKind::Int;
    std::vector<ParamTypeInfo> param_types;
  };

  struct LoopInfo {
    std::string continue_label;
    std::string break_label;
  };

  std::ostream &os_;

  int temp_id_ = 0;
  int var_id_ = 0;
  int block_id_ = 0;
  bool current_block_terminated_ = false;

  TypeKind current_func_ret_type_ = TypeKind::Int;

  std::vector<std::unordered_map<std::string, SymbolInfo>> scopes_;
  std::vector<LoopInfo> loop_stack_;

  std::string NewTemp();
  std::string NewVar();
  std::string NewBlock(const std::string &prefix);

  void EmitBlockLabel(const std::string &label);
  void EmitJumpIfNeeded(const std::string &target);
  std::string GenerateBoolValue(const std::string &value);

  void EnterScope();
  void ExitScope();
  void InsertSymbol(const std::string &name, const SymbolInfo &info);
  const SymbolInfo &LookupSymbol(const std::string &name) const;

  int Product(const std::vector<int> &dims, size_t start = 0) const;

  std::vector<int> EvalDims(
      const std::vector<std::unique_ptr<ExprAST>> &dims) const;

  std::string KoopaArrayType(const std::vector<int> &dims,
                             size_t start = 0) const;

  std::string KoopaPointerParamType(const std::vector<int> &dims) const;

  void FlattenInit(const InitValAST &init,
                   const std::vector<int> &dims,
                   size_t dim,
                   int base,
                   std::vector<const ExprAST *> &out) const;

  std::vector<int> ConstInitToValues(const InitValAST &init,
                                     const std::vector<int> &dims) const;

  std::string BuildAggregateInit(const std::vector<int> &values,
                                 const std::vector<int> &dims,
                                 size_t dim,
                                 int &pos) const;

  void GenerateLocalArrayInit(const std::string &array_addr,
                              const std::vector<int> &dims,
                              const InitValAST &init);

  std::string GenerateLValAddr(const LValAST &ast,
                               size_t *remaining_dims = nullptr);

  std::string GenerateLValForCallArg(const LValAST &ast,
                                     const ParamTypeInfo &expected);

  void EmitLibraryDecls();
  void InsertLibraryFunctions();
  void PredeclareFunctions(const CompUnitAST &ast);

  void GenerateCompUnit(const CompUnitAST &ast);
  void GenerateCompUnitItem(const BaseAST &ast);

  void GenerateGlobalDecl(const DeclAST &ast);
  void GenerateGlobalConstDecl(const ConstDeclAST &ast);
  void GenerateGlobalVarDecl(const VarDeclAST &ast);

  void GenerateFuncDef(const FuncDefAST &ast);
  void GenerateBlock(const BlockAST &ast);

  void GenerateBlockItem(const BlockItemAST &ast);
  void GenerateDecl(const DeclAST &ast);
  void GenerateConstDecl(const ConstDeclAST &ast);
  void GenerateVarDecl(const VarDeclAST &ast);

  void GenerateStmt(const StmtAST &ast);

  void GenerateIfStmt(const IfStmtAST &ast);
  void GenerateWhileStmt(const WhileStmtAST &ast);
  void GenerateBreakStmt(const BreakStmtAST &ast);
  void GenerateContinueStmt(const ContinueStmtAST &ast);

  std::string GenerateLogicalAnd(const BinaryExprAST &ast);
  std::string GenerateLogicalOr(const BinaryExprAST &ast);

  std::string GenerateCallExpr(const CallExprAST &ast);

  std::string GenerateExpr(const ExprAST &ast);

  std::int32_t EvalConstExpr(const ExprAST &ast) const;
};
