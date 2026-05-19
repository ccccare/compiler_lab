#include "koopa.hpp"

#include <stdexcept>
#include <string>

std::string KoopaGenerator::NewTemp() {
  return "%t" + std::to_string(temp_id_++);
}

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
    throw std::runtime_error("Lv3 only supports function named main");
  }

  if (ast.ret_type != TypeKind::Int) {
    throw std::runtime_error("Lv3 only supports int main()");
  }

  temp_id_ = 0;

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
    throw std::runtime_error("Lv3 only supports statements as block items");
  }

  GenerateStmt(*stmt);
}

void KoopaGenerator::GenerateStmt(const StmtAST &ast) {
  const auto *ret_stmt = dynamic_cast<const ReturnStmtAST *>(&ast);
  if (ret_stmt == nullptr) {
    throw std::runtime_error("Lv3 only supports return statements");
  }

  std::string ret_value = GenerateExpr(*ret_stmt->value);
  os_ << "  ret " << ret_value << "\n";
}

std::string KoopaGenerator::GenerateExpr(const ExprAST &ast) {
  if (const auto *number = dynamic_cast<const NumberAST *>(&ast)) {
    return std::to_string(number->value);
  }

  if (const auto *unary = dynamic_cast<const UnaryExprAST *>(&ast)) {
    std::string value = GenerateExpr(*unary->operand);

    switch (unary->op) {
      case UnaryOp::Plus:
        return value;

      case UnaryOp::Minus: {
        std::string result = NewTemp();
        os_ << "  " << result << " = sub 0, " << value << "\n";
        return result;
      }

      case UnaryOp::Not: {
        std::string result = NewTemp();
        os_ << "  " << result << " = eq " << value << ", 0\n";
        return result;
      }
    }
  }

  if (const auto *binary = dynamic_cast<const BinaryExprAST *>(&ast)) {
    std::string lhs = GenerateExpr(*binary->lhs);
    std::string rhs = GenerateExpr(*binary->rhs);

    if (binary->op == BinaryOp::LAnd) {
      std::string lhs_bool = NewTemp();
      os_ << "  " << lhs_bool << " = ne " << lhs << ", 0\n";

      std::string rhs_bool = NewTemp();
      os_ << "  " << rhs_bool << " = ne " << rhs << ", 0\n";

      std::string result = NewTemp();
      os_ << "  " << result << " = and " << lhs_bool << ", " << rhs_bool << "\n";
      return result;
    }

    if (binary->op == BinaryOp::LOr) {
      std::string lhs_bool = NewTemp();
      os_ << "  " << lhs_bool << " = ne " << lhs << ", 0\n";

      std::string rhs_bool = NewTemp();
      os_ << "  " << rhs_bool << " = ne " << rhs << ", 0\n";

      std::string result = NewTemp();
      os_ << "  " << result << " = or " << lhs_bool << ", " << rhs_bool << "\n";
      return result;
    }

    std::string op;

    switch (binary->op) {
      case BinaryOp::Add:
        op = "add";
        break;
      case BinaryOp::Sub:
        op = "sub";
        break;
      case BinaryOp::Mul:
        op = "mul";
        break;
      case BinaryOp::Div:
        op = "div";
        break;
      case BinaryOp::Mod:
        op = "mod";
        break;
      case BinaryOp::Lt:
        op = "lt";
        break;
      case BinaryOp::Gt:
        op = "gt";
        break;
      case BinaryOp::Le:
        op = "le";
        break;
      case BinaryOp::Ge:
        op = "ge";
        break;
      case BinaryOp::Eq:
        op = "eq";
        break;
      case BinaryOp::Ne:
        op = "ne";
        break;
      case BinaryOp::LAnd:
      case BinaryOp::LOr:
        throw std::runtime_error("logical operators should be handled earlier");
    }

    std::string result = NewTemp();
    os_ << "  " << result << " = " << op << " " << lhs << ", " << rhs << "\n";
    return result;
  }

  throw std::runtime_error("unsupported expression AST node in Lv3");
}