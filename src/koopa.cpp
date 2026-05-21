#include "koopa.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

std::string KoopaGenerator::NewTemp() {
  return "%t" + std::to_string(temp_id_++);
}

std::string KoopaGenerator::NewVar() {
  return "%v" + std::to_string(var_id_++);
}

void KoopaGenerator::EnterScope() {
  scopes_.emplace_back();
}

void KoopaGenerator::ExitScope() {
  if (scopes_.empty()) {
    throw std::runtime_error("internal error: no scope to exit");
  }
  scopes_.pop_back();
}

void KoopaGenerator::InsertSymbol(const std::string &name,
                                  const SymbolInfo &info) {
  if (scopes_.empty()) {
    throw std::runtime_error("internal error: no active scope");
  }

  auto &scope = scopes_.back();
  if (scope.count(name) != 0) {
    throw std::runtime_error("redefinition of symbol: " + name);
  }

  scope.emplace(name, info);
}

const KoopaGenerator::SymbolInfo &KoopaGenerator::LookupSymbol(
    const std::string &name) const {
  for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
    auto found = it->find(name);
    if (found != it->end()) {
      return found->second;
    }
  }

  throw std::runtime_error("undefined symbol: " + name);
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
    throw std::runtime_error("Lv4 only supports function named main");
  }

  if (ast.ret_type != TypeKind::Int) {
    throw std::runtime_error("Lv4 only supports int main()");
  }

  temp_id_ = 0;
  var_id_ = 0;
  current_block_terminated_ = false;
  scopes_.clear();

  os_ << "fun @" << ast.ident << "(): i32 {\n";
  os_ << "%entry:\n";

  GenerateBlock(*ast.block);

  // 合法测试一般会有 return。这里补一个 ret 0，避免生成没有终结指令的 Koopa 基本块。
  if (!current_block_terminated_) {
    os_ << "  ret 0\n";
  }

  os_ << "}\n";
}

void KoopaGenerator::GenerateBlock(const BlockAST &ast) {
  EnterScope();

  for (const auto &item : ast.items) {
    if (current_block_terminated_) {
      break;
    }
    GenerateBlockItem(*item);
  }

  ExitScope();
}

void KoopaGenerator::GenerateBlockItem(const BlockItemAST &ast) {
  if (const auto *decl = dynamic_cast<const DeclAST *>(&ast)) {
    GenerateDecl(*decl);
    return;
  }

  if (const auto *stmt = dynamic_cast<const StmtAST *>(&ast)) {
    GenerateStmt(*stmt);
    return;
  }

  throw std::runtime_error("unsupported block item AST node");
}

void KoopaGenerator::GenerateDecl(const DeclAST &ast) {
  if (const auto *const_decl = dynamic_cast<const ConstDeclAST *>(&ast)) {
    GenerateConstDecl(*const_decl);
    return;
  }

  if (const auto *var_decl = dynamic_cast<const VarDeclAST *>(&ast)) {
    GenerateVarDecl(*var_decl);
    return;
  }

  throw std::runtime_error("unsupported declaration AST node");
}

void KoopaGenerator::GenerateConstDecl(const ConstDeclAST &ast) {
  for (const auto &def : ast.defs) {
    std::int32_t value = EvalConstExpr(*def->init);

    SymbolInfo info;
    info.kind = SymbolKind::Const;
    info.const_value = value;

    InsertSymbol(def->ident, info);
  }
}

void KoopaGenerator::GenerateVarDecl(const VarDeclAST &ast) {
  for (const auto &def : ast.defs) {
    std::string var_name = NewVar();

    os_ << "  " << var_name << " = alloc i32\n";

    SymbolInfo info;
    info.kind = SymbolKind::Var;
    info.ir_name = var_name;

    InsertSymbol(def->ident, info);

    if (def->init) {
      std::string init_value = GenerateExpr(*def->init);
      os_ << "  store " << init_value << ", " << var_name << "\n";
    }
  }
}

void KoopaGenerator::GenerateStmt(const StmtAST &ast) {
  if (const auto *assign_stmt = dynamic_cast<const AssignStmtAST *>(&ast)) {
    const SymbolInfo &symbol = LookupSymbol(assign_stmt->ident);
    if (symbol.kind != SymbolKind::Var) {
      throw std::runtime_error("cannot assign to const symbol: " +
                               assign_stmt->ident);
    }

    std::string value = GenerateExpr(*assign_stmt->value);
    os_ << "  store " << value << ", " << symbol.ir_name << "\n";
    return;
  }

  if (const auto *ret_stmt = dynamic_cast<const ReturnStmtAST *>(&ast)) {
    std::string ret_value = GenerateExpr(*ret_stmt->value);
    os_ << "  ret " << ret_value << "\n";
    current_block_terminated_ = true;
    return;
  }

  throw std::runtime_error("unsupported statement AST node");
}

std::string KoopaGenerator::GenerateExpr(const ExprAST &ast) {
  if (const auto *number = dynamic_cast<const NumberAST *>(&ast)) {
    return std::to_string(number->value);
  }

  if (const auto *lval = dynamic_cast<const LValAST *>(&ast)) {
    const SymbolInfo &symbol = LookupSymbol(lval->ident);

    if (symbol.kind == SymbolKind::Const) {
      return std::to_string(symbol.const_value);
    }

    std::string result = NewTemp();
    os_ << "  " << result << " = load " << symbol.ir_name << "\n";
    return result;
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
      os_ << "  " << result << " = and " << lhs_bool << ", " << rhs_bool
          << "\n";
      return result;
    }

    if (binary->op == BinaryOp::LOr) {
      std::string lhs_bool = NewTemp();
      os_ << "  " << lhs_bool << " = ne " << lhs << ", 0\n";

      std::string rhs_bool = NewTemp();
      os_ << "  " << rhs_bool << " = ne " << rhs << ", 0\n";

      std::string result = NewTemp();
      os_ << "  " << result << " = or " << lhs_bool << ", " << rhs_bool
          << "\n";
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

  throw std::runtime_error("unsupported expression AST node in Lv4");
}

std::int32_t KoopaGenerator::EvalConstExpr(const ExprAST &ast) const {
  if (const auto *number = dynamic_cast<const NumberAST *>(&ast)) {
    return static_cast<std::int32_t>(number->value);
  }

  if (const auto *lval = dynamic_cast<const LValAST *>(&ast)) {
    const SymbolInfo &symbol = LookupSymbol(lval->ident);

    if (symbol.kind != SymbolKind::Const) {
      throw std::runtime_error("variable used in constant expression: " +
                               lval->ident);
    }

    return symbol.const_value;
  }

  if (const auto *unary = dynamic_cast<const UnaryExprAST *>(&ast)) {
    std::int32_t value = EvalConstExpr(*unary->operand);

    switch (unary->op) {
      case UnaryOp::Plus:
        return value;
      case UnaryOp::Minus:
        return -value;
      case UnaryOp::Not:
        return value == 0 ? 1 : 0;
    }
  }

  if (const auto *binary = dynamic_cast<const BinaryExprAST *>(&ast)) {
    std::int32_t lhs = EvalConstExpr(*binary->lhs);

    if (binary->op == BinaryOp::LAnd) {
      if (lhs == 0) {
        return 0;
      }
      std::int32_t rhs = EvalConstExpr(*binary->rhs);
      return rhs != 0 ? 1 : 0;
    }

    if (binary->op == BinaryOp::LOr) {
      if (lhs != 0) {
        return 1;
      }
      std::int32_t rhs = EvalConstExpr(*binary->rhs);
      return rhs != 0 ? 1 : 0;
    }

    std::int32_t rhs = EvalConstExpr(*binary->rhs);

    switch (binary->op) {
      case BinaryOp::Add:
        return lhs + rhs;
      case BinaryOp::Sub:
        return lhs - rhs;
      case BinaryOp::Mul:
        return lhs * rhs;
      case BinaryOp::Div:
        if (rhs == 0) {
          throw std::runtime_error("division by zero in constant expression");
        }
        return lhs / rhs;
      case BinaryOp::Mod:
        if (rhs == 0) {
          throw std::runtime_error("modulo by zero in constant expression");
        }
        return lhs % rhs;
      case BinaryOp::Lt:
        return lhs < rhs ? 1 : 0;
      case BinaryOp::Gt:
        return lhs > rhs ? 1 : 0;
      case BinaryOp::Le:
        return lhs <= rhs ? 1 : 0;
      case BinaryOp::Ge:
        return lhs >= rhs ? 1 : 0;
      case BinaryOp::Eq:
        return lhs == rhs ? 1 : 0;
      case BinaryOp::Ne:
        return lhs != rhs ? 1 : 0;
      case BinaryOp::LAnd:
      case BinaryOp::LOr:
        throw std::runtime_error("logical operators should be handled earlier");
    }
  }

  throw std::runtime_error("unsupported constant expression AST node");
}