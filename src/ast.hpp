#pragma once

#include <cstdint>
#include <iostream>
#include <memory>
#include <ostream>
#include <string>
#include <vector>

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

class BlockItemAST : public BaseAST {
 public:
  ~BlockItemAST() override = default;
};

class StmtAST : public BlockItemAST {
 public:
  ~StmtAST() override = default;
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