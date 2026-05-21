#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

class BlockAST;

enum class TypeKind {
  Int,
  Void,
};

inline const char *TypeName(TypeKind type) {
  switch (type) {
    case TypeKind::Int:
      return "int";
    case TypeKind::Void:
      return "void";
  }
  return "<unknown>";
}

inline void PrintIndent(std::ostream &os, int indent) {
  for (int i = 0; i < indent; ++i) {
    os << ' ';
  }
}

class BaseAST {
 public:
  virtual ~BaseAST() = default;
  virtual void Dump(std::ostream &os, int indent = 0) const = 0;
};

class ExprAST : public BaseAST {
 public:
  ~ExprAST() override = default;
};

class ExprListAST final : public BaseAST {
 public:
  std::vector<std::unique_ptr<ExprAST>> exprs;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "ExprListAST {\n";
    for (const auto &expr : exprs) {
      expr->Dump(os, indent + 2);
      os << "\n";
    }
    PrintIndent(os, indent);
    os << "}";
  }
};

class ExprVectorAST final : public BaseAST {
 public:
  std::vector<std::unique_ptr<ExprAST>> exprs;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "ExprVectorAST {\n";
    for (const auto &expr : exprs) {
      expr->Dump(os, indent + 2);
      os << "\n";
    }
    PrintIndent(os, indent);
    os << "}";
  }
};

class InitValAST final : public BaseAST {
 public:
  bool is_list = false;
  std::unique_ptr<ExprAST> expr;
  std::vector<std::unique_ptr<InitValAST>> list;

  static std::unique_ptr<InitValAST> FromExpr(std::unique_ptr<ExprAST> expr) {
    auto node = std::make_unique<InitValAST>();
    node->is_list = false;
    node->expr = std::move(expr);
    return node;
  }

  static std::unique_ptr<InitValAST> FromList(
      std::vector<std::unique_ptr<InitValAST>> list) {
    auto node = std::make_unique<InitValAST>();
    node->is_list = true;
    node->list = std::move(list);
    return node;
  }

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    if (!is_list) {
      os << "InitValAST Expr {\n";
      expr->Dump(os, indent + 2);
      os << "\n";
      PrintIndent(os, indent);
      os << "}";
      return;
    }

    os << "InitValAST List {\n";
    for (const auto &item : list) {
      item->Dump(os, indent + 2);
      os << "\n";
    }
    PrintIndent(os, indent);
    os << "}";
  }
};

class NumberAST final : public ExprAST {
 public:
  explicit NumberAST(std::int64_t value) : value(value) {}

  std::int64_t value;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "NumberAST { value: " << value << " }";
  }
};

class LValAST final : public ExprAST {
 public:
  explicit LValAST(std::string ident) : ident(std::move(ident)) {}

  std::string ident;
  std::vector<std::unique_ptr<ExprAST>> indices;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "LValAST { ident: " << ident;

    if (!indices.empty()) {
      os << "\n";
      for (const auto &index : indices) {
        index->Dump(os, indent + 2);
        os << "\n";
      }
      PrintIndent(os, indent);
    }

    os << "}";
  }
};

enum class UnaryOp {
  Plus,
  Minus,
  Not,
};

inline const char *UnaryOpName(UnaryOp op) {
  switch (op) {
    case UnaryOp::Plus:
      return "+";
    case UnaryOp::Minus:
      return "-";
    case UnaryOp::Not:
      return "!";
  }
  return "<unknown>";
}

class UnaryExprAST final : public ExprAST {
 public:
  UnaryExprAST(UnaryOp op, std::unique_ptr<ExprAST> operand)
      : op(op), operand(std::move(operand)) {}

  UnaryOp op;
  std::unique_ptr<ExprAST> operand;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "UnaryExprAST { op: " << UnaryOpName(op) << "\n";
    operand->Dump(os, indent + 2);
    os << "\n";
    PrintIndent(os, indent);
    os << "}";
  }
};

enum class BinaryOp {
  Add,
  Sub,
  Mul,
  Div,
  Mod,

  Lt,
  Gt,
  Le,
  Ge,
  Eq,
  Ne,

  LAnd,
  LOr,
};

inline const char *BinaryOpName(BinaryOp op) {
  switch (op) {
    case BinaryOp::Add:
      return "+";
    case BinaryOp::Sub:
      return "-";
    case BinaryOp::Mul:
      return "*";
    case BinaryOp::Div:
      return "/";
    case BinaryOp::Mod:
      return "%";
    case BinaryOp::Lt:
      return "<";
    case BinaryOp::Gt:
      return ">";
    case BinaryOp::Le:
      return "<=";
    case BinaryOp::Ge:
      return ">=";
    case BinaryOp::Eq:
      return "==";
    case BinaryOp::Ne:
      return "!=";
    case BinaryOp::LAnd:
      return "&&";
    case BinaryOp::LOr:
      return "||";
  }
  return "<unknown>";
}

class BinaryExprAST final : public ExprAST {
 public:
  BinaryExprAST(BinaryOp op,
                std::unique_ptr<ExprAST> lhs,
                std::unique_ptr<ExprAST> rhs)
      : op(op), lhs(std::move(lhs)), rhs(std::move(rhs)) {}

  BinaryOp op;
  std::unique_ptr<ExprAST> lhs;
  std::unique_ptr<ExprAST> rhs;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "BinaryExprAST { op: " << BinaryOpName(op) << "\n";

    lhs->Dump(os, indent + 2);
    os << "\n";

    rhs->Dump(os, indent + 2);
    os << "\n";

    PrintIndent(os, indent);
    os << "}";
  }
};

class CallExprAST final : public ExprAST {
 public:
  std::string ident;
  std::vector<std::unique_ptr<ExprAST>> args;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "CallExprAST { ident: " << ident << "\n";
    for (const auto &arg : args) {
      arg->Dump(os, indent + 2);
      os << "\n";
    }
    PrintIndent(os, indent);
    os << "}";
  }
};

class BlockItemAST : public BaseAST {
 public:
  ~BlockItemAST() override = default;
};

class DeclAST : public BlockItemAST {
 public:
  ~DeclAST() override = default;
};

class ConstDefAST final : public BaseAST {
 public:
  std::string ident;
  std::vector<std::unique_ptr<ExprAST>> dims;
  std::unique_ptr<InitValAST> init;

  bool IsArray() const {
    return !dims.empty();
  }

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "ConstDefAST { ident: " << ident << "\n";

    if (!dims.empty()) {
      PrintIndent(os, indent + 2);
      os << "dims:\n";

      for (const auto &dim : dims) {
        dim->Dump(os, indent + 4);
        os << "\n";
      }
    }

    if (init) {
      PrintIndent(os, indent + 2);
      os << "init:\n";
      init->Dump(os, indent + 4);
      os << "\n";
    }

    PrintIndent(os, indent);
    os << "}";
  }
};

class ConstDeclAST final : public DeclAST {
 public:
  std::vector<std::unique_ptr<ConstDefAST>> defs;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "ConstDeclAST {\n";
    for (const auto &def : defs) {
      def->Dump(os, indent + 2);
      os << "\n";
    }
    PrintIndent(os, indent);
    os << "}";
  }
};

class VarDefAST final : public BaseAST {
 public:
  std::string ident;
  std::vector<std::unique_ptr<ExprAST>> dims;
  std::unique_ptr<InitValAST> init;  // nullptr 表示无初始化

  bool IsArray() const {
    return !dims.empty();
  }

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "VarDefAST { ident: " << ident << "\n";

    if (!dims.empty()) {
      PrintIndent(os, indent + 2);
      os << "dims:\n";
      for (const auto &dim : dims) {
        dim->Dump(os, indent + 4);
        os << "\n";
      }
    }

    if (init) {
      PrintIndent(os, indent + 2);
      os << "init:\n";
      init->Dump(os, indent + 4);
      os << "\n";
    }

    PrintIndent(os, indent);
    os << "}";
  }
};

class VarDeclAST final : public DeclAST {
 public:
  std::vector<std::unique_ptr<VarDefAST>> defs;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "VarDeclAST {\n";
    for (const auto &def : defs) {
      def->Dump(os, indent + 2);
      os << "\n";
    }
    PrintIndent(os, indent);
    os << "}";
  }
};

class FuncFParamAST final : public BaseAST {
 public:
  TypeKind type = TypeKind::Int;
  std::string ident;
  bool is_array = false;
  std::vector<std::unique_ptr<ExprAST>> dims;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "FuncFParamAST { type: " << TypeName(type)
       << ", ident: " << ident << " }";
  }
};

class FuncFParamListAST final : public BaseAST {
 public:
  std::vector<std::unique_ptr<FuncFParamAST>> params;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "FuncFParamListAST {\n";
    for (const auto &param : params) {
      param->Dump(os, indent + 2);
      os << "\n";
    }
    PrintIndent(os, indent);
    os << "}";
  }
};

class StmtAST : public BlockItemAST {
 public:
  ~StmtAST() override = default;
};

class AssignStmtAST final : public StmtAST {
 public:
  std::unique_ptr<LValAST> lval;
  std::unique_ptr<ExprAST> value;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "AssignStmtAST {\n";

    lval->Dump(os, indent + 2);
    os << "\n";

    value->Dump(os, indent + 2);
    os << "\n";

    PrintIndent(os, indent);
    os << "}";
  }
};

class ExprStmtAST final : public StmtAST {
 public:
  // nullptr 表示空语句 ";"
  std::unique_ptr<ExprAST> expr;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "ExprStmtAST {";

    if (expr) {
      os << "\n";
      expr->Dump(os, indent + 2);
      os << "\n";
      PrintIndent(os, indent);
      os << "}";
    } else {
      os << " empty }";
    }
  }
};


class ReturnStmtAST final : public StmtAST {
 public:
  std::unique_ptr<ExprAST> value;  // nullptr 表示 return;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "ReturnStmtAST {";

    if (value) {
      os << "\n";
      value->Dump(os, indent + 2);
      os << "\n";
      PrintIndent(os, indent);
      os << "}";
    } else {
      os << " void }";
    }
  }
};

class BlockAST final : public BaseAST {
 public:
  std::vector<std::unique_ptr<BlockItemAST>> items;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "BlockAST {\n";
    for (const auto &item : items) {
      item->Dump(os, indent + 2);
      os << "\n";
    }
    PrintIndent(os, indent);
    os << "}";
  }
};

class BlockStmtAST final : public StmtAST {
 public:
  std::unique_ptr<BlockAST> block;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "BlockStmtAST {\n";

    block->Dump(os, indent + 2);
    os << "\n";

    PrintIndent(os, indent);
    os << "}";
  }
};

class IfStmtAST final : public StmtAST {
 public:
  std::unique_ptr<ExprAST> cond;
  std::unique_ptr<StmtAST> then_stmt;
  std::unique_ptr<StmtAST> else_stmt;  // nullptr 表示没有 else

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "IfStmtAST {\n";

    PrintIndent(os, indent + 2);
    os << "cond:\n";
    cond->Dump(os, indent + 4);
    os << "\n";

    PrintIndent(os, indent + 2);
    os << "then:\n";
    then_stmt->Dump(os, indent + 4);
    os << "\n";

    if (else_stmt) {
      PrintIndent(os, indent + 2);
      os << "else:\n";
      else_stmt->Dump(os, indent + 4);
      os << "\n";
    }

    PrintIndent(os, indent);
    os << "}";
  }
};

class WhileStmtAST final : public StmtAST {
 public:
  std::unique_ptr<ExprAST> cond;
  std::unique_ptr<StmtAST> body;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "WhileStmtAST {\n";

    PrintIndent(os, indent + 2);
    os << "cond:\n";
    cond->Dump(os, indent + 4);
    os << "\n";

    PrintIndent(os, indent + 2);
    os << "body:\n";
    body->Dump(os, indent + 4);
    os << "\n";

    PrintIndent(os, indent);
    os << "}";
  }
};

class BreakStmtAST final : public StmtAST {
 public:
  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "BreakStmtAST {}";
  }
};

class ContinueStmtAST final : public StmtAST {
 public:
  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "ContinueStmtAST {}";
  }
};

class FuncDefAST final : public BaseAST {
 public:
  TypeKind ret_type = TypeKind::Int;
  std::string ident;
  std::vector<std::unique_ptr<FuncFParamAST>> params;
  std::unique_ptr<BlockAST> block;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "FuncDefAST {\n";

    PrintIndent(os, indent + 2);
    os << "ret_type: " << TypeName(ret_type) << "\n";

    PrintIndent(os, indent + 2);
    os << "ident: " << ident << "\n";

    PrintIndent(os, indent + 2);
    os << "params:\n";
    for (const auto &param : params) {
      param->Dump(os, indent + 4);
      os << "\n";
    }

    block->Dump(os, indent + 2);
    os << "\n";

    PrintIndent(os, indent);
    os << "}";
  }
};

class CompUnitAST final : public BaseAST {
 public:
  std::vector<std::unique_ptr<BaseAST>> items;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "CompUnitAST {\n";
    for (const auto &item : items) {
      item->Dump(os, indent + 2);
      os << "\n";
    }
    PrintIndent(os, indent);
    os << "}";
  }
};