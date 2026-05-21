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

std::string KoopaGenerator::NewBlock(const std::string &prefix) {
  return "%" + prefix + std::to_string(block_id_++);
}

void KoopaGenerator::EmitBlockLabel(const std::string &label) {
  os_ << "\n" << label << ":\n";
  current_block_terminated_ = false;
}

void KoopaGenerator::EmitJumpIfNeeded(const std::string &target) {
  if (!current_block_terminated_) {
    os_ << "  jump " << target << "\n";
    current_block_terminated_ = true;
  }
}

std::string KoopaGenerator::GenerateBoolValue(const std::string &value) {
  std::string result = NewTemp();
  os_ << "  " << result << " = ne " << value << ", 0\n";
  return result;
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
    throw std::runtime_error("Lv6 only supports function named main");
  }

  if (ast.ret_type != TypeKind::Int) {
    throw std::runtime_error("Lv6 only supports int main()");
  }

  temp_id_ = 0;
  var_id_ = 0;
  block_id_ = 0;
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

void KoopaGenerator::GenerateIfStmt(const IfStmtAST &ast) {
  std::string then_label = NewBlock("then");
  std::string else_label = ast.else_stmt ? NewBlock("else") : "";
  std::string end_label = NewBlock("ifend");

  std::string cond_value = GenerateExpr(*ast.cond);

  if (ast.else_stmt) {
    os_ << "  br " << cond_value << ", " << then_label << ", " << else_label
        << "\n";
  } else {
    os_ << "  br " << cond_value << ", " << then_label << ", " << end_label
        << "\n";
  }
  current_block_terminated_ = true;

  EmitBlockLabel(then_label);
  GenerateStmt(*ast.then_stmt);
  EmitJumpIfNeeded(end_label);

  if (ast.else_stmt) {
    EmitBlockLabel(else_label);
    GenerateStmt(*ast.else_stmt);
    EmitJumpIfNeeded(end_label);
  }

  EmitBlockLabel(end_label);
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

  if (const auto *expr_stmt = dynamic_cast<const ExprStmtAST *>(&ast)) {
    if (expr_stmt->expr) {
      // 表达式语句虽然不使用结果，但表达式本身仍然要被计算。
      // 例如之后出现函数调用时，函数调用可能有副作用。
      (void)GenerateExpr(*expr_stmt->expr);
    }

    // 空语句 ; 不生成任何 IR。
    return;
  }

  if (const auto *block_stmt = dynamic_cast<const BlockStmtAST *>(&ast)) {
    GenerateBlock(*block_stmt->block);
    return;
  }

  if (const auto *if_stmt = dynamic_cast<const IfStmtAST *>(&ast)) {
    GenerateIfStmt(*if_stmt);
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

std::string KoopaGenerator::GenerateLogicalAnd(const BinaryExprAST &ast) {
  std::string result_ptr = NewVar();
  os_ << "  " << result_ptr << " = alloc i32\n";

  std::string lhs = GenerateExpr(*ast.lhs);
  std::string lhs_bool = GenerateBoolValue(lhs);

  std::string rhs_label = NewBlock("land_rhs");
  std::string end_label = NewBlock("land_end");

  // 默认结果为 0。只有 lhs 和 rhs 都为真时，才会改成 1。
  os_ << "  store 0, " << result_ptr << "\n";
  os_ << "  br " << lhs_bool << ", " << rhs_label << ", " << end_label << "\n";
  current_block_terminated_ = true;

  EmitBlockLabel(rhs_label);
  std::string rhs = GenerateExpr(*ast.rhs);
  std::string rhs_bool = GenerateBoolValue(rhs);
  os_ << "  store " << rhs_bool << ", " << result_ptr << "\n";
  EmitJumpIfNeeded(end_label);

  EmitBlockLabel(end_label);
  std::string result = NewTemp();
  os_ << "  " << result << " = load " << result_ptr << "\n";
  return result;
}

std::string KoopaGenerator::GenerateLogicalOr(const BinaryExprAST &ast) {
  std::string result_ptr = NewVar();
  os_ << "  " << result_ptr << " = alloc i32\n";

  std::string lhs = GenerateExpr(*ast.lhs);
  std::string lhs_bool = GenerateBoolValue(lhs);

  std::string rhs_label = NewBlock("lor_rhs");
  std::string end_label = NewBlock("lor_end");

  // 默认结果为 1。只有 lhs 为假时，才需要计算 rhs。
  os_ << "  store 1, " << result_ptr << "\n";
  os_ << "  br " << lhs_bool << ", " << end_label << ", " << rhs_label << "\n";
  current_block_terminated_ = true;

  EmitBlockLabel(rhs_label);
  std::string rhs = GenerateExpr(*ast.rhs);
  std::string rhs_bool = GenerateBoolValue(rhs);
  os_ << "  store " << rhs_bool << ", " << result_ptr << "\n";
  EmitJumpIfNeeded(end_label);

  EmitBlockLabel(end_label);
  std::string result = NewTemp();
  os_ << "  " << result << " = load " << result_ptr << "\n";
  return result;
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
    

    if (binary->op == BinaryOp::LAnd) {
      return GenerateLogicalAnd(*binary);
    }

    if (binary->op == BinaryOp::LOr) {
      return GenerateLogicalOr(*binary);
    }

    std::string lhs = GenerateExpr(*binary->lhs);
    std::string rhs = GenerateExpr(*binary->rhs);

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