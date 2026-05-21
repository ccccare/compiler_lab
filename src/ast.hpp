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
};

inline const char *TypeName(TypeKind type) {
  switch (type) {
    case TypeKind::Int:
      return "int";
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

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "LValAST { ident: " << ident << " }";
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
  std::unique_ptr<ExprAST> init;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "ConstDefAST { ident: " << ident << "\n";
    init->Dump(os, indent + 2);
    os << "\n";
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
  std::unique_ptr<ExprAST> init;  // nullptr 表示无初始化

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "VarDefAST { ident: " << ident;
    if (init) {
      os << "\n";
      init->Dump(os, indent + 2);
      os << "\n";
      PrintIndent(os, indent);
      os << "}";
    } else {
      os << ", no init }";
    }
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

class StmtAST : public BlockItemAST {
 public:
  ~StmtAST() override = default;
};

class AssignStmtAST final : public StmtAST {
 public:
  std::string ident;
  std::unique_ptr<ExprAST> value;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "AssignStmtAST { ident: " << ident << "\n";
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
  std::unique_ptr<ExprAST> value;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "ReturnStmtAST {\n";
    value->Dump(os, indent + 2);
    os << "\n";
    PrintIndent(os, indent);
    os << "}";
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

class FuncDefAST final : public BaseAST {
 public:
  TypeKind ret_type = TypeKind::Int;
  std::string ident;
  std::unique_ptr<BlockAST> block;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "FuncDefAST {\n";

    PrintIndent(os, indent + 2);
    os << "ret_type: " << TypeName(ret_type) << "\n";

    PrintIndent(os, indent + 2);
    os << "ident: " << ident << "\n";

    block->Dump(os, indent + 2);
    os << "\n";

    PrintIndent(os, indent);
    os << "}";
  }
};

class CompUnitAST final : public BaseAST {
 public:
  std::unique_ptr<FuncDefAST> func_def;

  void Dump(std::ostream &os, int indent = 0) const override {
    PrintIndent(os, indent);
    os << "CompUnitAST {\n";
    func_def->Dump(os, indent + 2);
    os << "\n";
    PrintIndent(os, indent);
    os << "}";
  }
};