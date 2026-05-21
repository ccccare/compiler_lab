#include "koopa.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <utility>
#include <vector>

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

int KoopaGenerator::Product(const std::vector<int> &dims, size_t start) const {
  int result = 1;
  for (size_t i = start; i < dims.size(); ++i) {
    result *= dims[i];
  }
  return result;
}

std::vector<int> KoopaGenerator::EvalDims(
    const std::vector<std::unique_ptr<ExprAST>> &dims) const {
  std::vector<int> result;
  result.reserve(dims.size());

  for (const auto &dim : dims) {
    int value = EvalConstExpr(*dim);
    if (value <= 0) {
      throw std::runtime_error("array dimension must be positive");
    }
    result.push_back(value);
  }

  return result;
}

std::string KoopaGenerator::KoopaArrayType(const std::vector<int> &dims,
                                           size_t start) const {
  if (start >= dims.size()) {
    return "i32";
  }

  return "[" + KoopaArrayType(dims, start + 1) + ", " +
         std::to_string(dims[start]) + "]";
}

std::string KoopaGenerator::KoopaPointerParamType(
    const std::vector<int> &dims) const {
  if (dims.empty()) {
    return "*i32";
  }
  return "*" + KoopaArrayType(dims);
}

void KoopaGenerator::FlattenInit(const InitValAST &init,
                                 const std::vector<int> &dims,
                                 size_t dim,
                                 int base,
                                 std::vector<const ExprAST *> &out) const {
  int total = Product(dims, dim);

  if (!init.is_list) {
    if (base < 0 || base >= static_cast<int>(out.size())) {
      throw std::runtime_error("too many initializers");
    }
    out[base] = init.expr.get();
    return;
  }

  int cur = base;
  int end = base + total;

  for (const auto &item : init.list) {
    if (cur >= end) {
      throw std::runtime_error("too many initializers");
    }

    if (!item->is_list) {
      out[cur++] = item->expr.get();
      continue;
    }

    size_t sub_dim = dim + 1;
    int offset = cur - base;

    while (sub_dim < dims.size()) {
      int sub_size = Product(dims, sub_dim);
      if (offset % sub_size == 0) {
        break;
      }
      ++sub_dim;
    }

    if (sub_dim >= dims.size()) {
      throw std::runtime_error("invalid nested initializer alignment");
    }

    FlattenInit(*item, dims, sub_dim, cur, out);
    cur += Product(dims, sub_dim);

    if (cur > end) {
      throw std::runtime_error("too many initializers");
    }
  }
}

std::string KoopaGenerator::BuildAggregateInit(
    const std::vector<int> &values,
    const std::vector<int> &dims,
    size_t dim,
    int &pos) const {
  if (dim == dims.size()) {
    return std::to_string(values[pos++]);
  }

  std::string result = "{";
  for (int i = 0; i < dims[dim]; ++i) {
    if (i != 0) {
      result += ", ";
    }
    result += BuildAggregateInit(values, dims, dim + 1, pos);
  }
  result += "}";

  return result;
}

std::vector<int> KoopaGenerator::ConstInitToValues(
    const InitValAST &init,
    const std::vector<int> &dims) const {
  int total = Product(dims);
  std::vector<const ExprAST *> exprs(total, nullptr);

  FlattenInit(init, dims, 0, 0, exprs);

  std::vector<int> values(total, 0);
  for (int i = 0; i < total; ++i) {
    if (exprs[i] != nullptr) {
      values[i] = EvalConstExpr(*exprs[i]);
    }
  }

  return values;
}

void KoopaGenerator::GenerateLocalArrayInit(const std::string &array_addr,
                                            const std::vector<int> &dims,
                                            const InitValAST &init) {
  int total = Product(dims);
  std::vector<const ExprAST *> values(total, nullptr);

  FlattenInit(init, dims, 0, 0, values);

  for (int flat = 0; flat < total; ++flat) {
    std::string ptr = array_addr;

    int remain = flat;
    for (size_t d = 0; d < dims.size(); ++d) {
      int stride = Product(dims, d + 1);
      int index = remain / stride;
      remain %= stride;

      std::string next = NewTemp();
      os_ << "  " << next << " = getelemptr " << ptr << ", "
          << index << "\n";
      ptr = next;
    }

    std::string value = "0";
    if (values[flat] != nullptr) {
      value = GenerateExpr(*values[flat]);
    }

    os_ << "  store " << value << ", " << ptr << "\n";
  }
}

std::string KoopaGenerator::GenerateLValAddr(const LValAST &ast,
                                             size_t *remaining_dims) {
  const SymbolInfo &symbol = LookupSymbol(ast.ident);

  if (symbol.kind == SymbolKind::Const && !symbol.is_array) {
    throw std::runtime_error("scalar const has no address: " + ast.ident);
  }

  if (symbol.kind != SymbolKind::Var) {
    throw std::runtime_error("lval is not a variable: " + ast.ident);
  }

  std::string ptr = symbol.ir_name;
  const size_t used_indices = ast.indices.size();

  if (!symbol.is_array) {
    if (used_indices != 0) {
      throw std::runtime_error("indexing non-array variable: " + ast.ident);
    }

    if (remaining_dims) {
      *remaining_dims = 0;
    }
    return ptr;
  }

  bool first_index = true;

  // 数组形参在 Koopa 中是一个保存指针值的局部槽：alloc *T。
  // 使用前必须先 load 出真正的数组指针。
  if (symbol.is_array_param) {
    std::string loaded = NewTemp();
    os_ << "  " << loaded << " = load " << ptr << "\n";
    ptr = loaded;
  }

  for (size_t i = 0; i < ast.indices.size(); ++i) {
    std::string index = GenerateExpr(*ast.indices[i]);
    std::string next = NewTemp();

    if (symbol.is_array_param && first_index) {
      os_ << "  " << next << " = getptr " << ptr << ", " << index << "\n";
    } else {
      os_ << "  " << next << " = getelemptr " << ptr << ", " << index << "\n";
    }

    ptr = next;
    first_index = false;
  }

  size_t rem = 0;
  if (symbol.is_array_param) {
    if (used_indices == 0) {
      rem = symbol.dims.size();
    } else if (used_indices - 1 <= symbol.dims.size()) {
      rem = symbol.dims.size() - (used_indices - 1);
    } else {
      rem = 0;
    }
  } else {
    if (used_indices <= symbol.dims.size()) {
      rem = symbol.dims.size() - used_indices;
    } else {
      rem = 0;
    }
  }

  if (remaining_dims) {
    *remaining_dims = rem;
  }

  return ptr;
}

std::string KoopaGenerator::GenerateLValForCallArg(
    const LValAST &ast,
    const ParamTypeInfo &expected) {
  size_t remaining = 0;
  std::string ptr = GenerateLValAddr(ast, &remaining);

  while (remaining > expected.dims.size()) {
    std::string zero_ptr = NewTemp();
    os_ << "  " << zero_ptr << " = getelemptr " << ptr << ", 0\n";
    ptr = zero_ptr;
    --remaining;
  }

  if (remaining != expected.dims.size()) {
    throw std::runtime_error("array argument dimension mismatch: " + ast.ident);
  }

  return ptr;
}

void KoopaGenerator::EmitLibraryDecls() {
  os_ << "decl @getint(): i32\n";
  os_ << "decl @getch(): i32\n";
  os_ << "decl @getarray(*i32): i32\n";
  os_ << "decl @putint(i32)\n";
  os_ << "decl @putch(i32)\n";
  os_ << "decl @putarray(i32, *i32)\n";
  os_ << "decl @starttime()\n";
  os_ << "decl @stoptime()\n\n";
}

void KoopaGenerator::InsertLibraryFunctions() {
  auto scalar_param = []() {
    ParamTypeInfo param;
    param.is_array = false;
    return param;
  };

  auto array_param = [](std::vector<int> dims = {}) {
    ParamTypeInfo param;
    param.is_array = true;
    param.dims = std::move(dims);
    return param;
  };

  auto insert_func = [this](const std::string &name,
                            TypeKind ret,
                            std::vector<ParamTypeInfo> params) {
    SymbolInfo info;
    info.kind = SymbolKind::Func;
    info.return_type = ret;
    info.param_types = std::move(params);
    InsertSymbol(name, info);
  };

  insert_func("getint", TypeKind::Int, {});
  insert_func("getch", TypeKind::Int, {});
  insert_func("getarray", TypeKind::Int, {array_param()});
  insert_func("putint", TypeKind::Void, {scalar_param()});
  insert_func("putch", TypeKind::Void, {scalar_param()});
  insert_func("putarray", TypeKind::Void, {scalar_param(), array_param()});
  insert_func("starttime", TypeKind::Void, {});
  insert_func("stoptime", TypeKind::Void, {});
}

void KoopaGenerator::PredeclareFunctions(const CompUnitAST &ast) {
  for (const auto &item : ast.items) {
    const auto *func = dynamic_cast<const FuncDefAST *>(item.get());
    if (!func) {
      continue;
    }

    SymbolInfo info;
    info.kind = SymbolKind::Func;
    info.return_type = func->ret_type;

    for (const auto &param : func->params) {
      ParamTypeInfo param_info;
      param_info.is_array = param->is_array;
      if (param->is_array) {
        param_info.dims = EvalDims(param->dims);
      }
      info.param_types.push_back(std::move(param_info));
    }

    InsertSymbol(func->ident, info);
  }
}

void KoopaGenerator::Generate(const BaseAST &ast) {
  const auto *comp_unit = dynamic_cast<const CompUnitAST *>(&ast);
  if (comp_unit == nullptr) {
    throw std::runtime_error("root AST node must be CompUnitAST");
  }

  GenerateCompUnit(*comp_unit);
}

void KoopaGenerator::GenerateCompUnit(const CompUnitAST &ast) {
  scopes_.clear();
  EnterScope();  // 全局作用域

  EmitLibraryDecls();
  InsertLibraryFunctions();
  PredeclareFunctions(ast);

  for (const auto &item : ast.items) {
    GenerateCompUnitItem(*item);
  }
}

void KoopaGenerator::GenerateCompUnitItem(const BaseAST &ast) {
  if (const auto *decl = dynamic_cast<const DeclAST *>(&ast)) {
    GenerateGlobalDecl(*decl);
    return;
  }

  if (const auto *func = dynamic_cast<const FuncDefAST *>(&ast)) {
    GenerateFuncDef(*func);
    return;
  }

  throw std::runtime_error("unsupported comp unit item");
}

void KoopaGenerator::GenerateGlobalDecl(const DeclAST &ast) {
  if (const auto *const_decl = dynamic_cast<const ConstDeclAST *>(&ast)) {
    GenerateGlobalConstDecl(*const_decl);
    return;
  }

  if (const auto *var_decl = dynamic_cast<const VarDeclAST *>(&ast)) {
    GenerateGlobalVarDecl(*var_decl);
    return;
  }

  throw std::runtime_error("unsupported global declaration");
}

void KoopaGenerator::GenerateGlobalConstDecl(const ConstDeclAST &ast) {
  for (const auto &def : ast.defs) {
    std::vector<int> dims = EvalDims(def->dims);

    if (dims.empty()) {
      if (!def->init || def->init->is_list) {
        throw std::runtime_error("invalid scalar const initializer");
      }

      SymbolInfo info;
      info.kind = SymbolKind::Const;
      info.is_const = true;
      info.const_value = EvalConstExpr(*def->init->expr);

      InsertSymbol(def->ident, info);
      continue;
    }

    if (!def->init) {
      throw std::runtime_error("const array must have initializer");
    }

    std::vector<int> values = ConstInitToValues(*def->init, dims);

    std::string global_name = "@" + def->ident;
    int pos = 0;
    std::string init = BuildAggregateInit(values, dims, 0, pos);

    os_ << "global " << global_name << " = alloc "
        << KoopaArrayType(dims) << ", " << init << "\n";

    SymbolInfo info;
    info.kind = SymbolKind::Var;
    info.is_const = true;
    info.ir_name = global_name;
    info.is_array = true;
    info.is_array_param = false;
    info.dims = dims;
    info.const_array_values = std::move(values);

    InsertSymbol(def->ident, info);
  }

  os_ << "\n";
}

void KoopaGenerator::GenerateGlobalVarDecl(const VarDeclAST &ast) {
  for (const auto &def : ast.defs) {
    std::vector<int> dims = EvalDims(def->dims);

    std::string global_name = "@" + def->ident;

    if (dims.empty()) {
      if (def->init) {
        if (def->init->is_list) {
          throw std::runtime_error("scalar variable initialized by list");
        }

        int value = EvalConstExpr(*def->init->expr);
        os_ << "global " << global_name << " = alloc i32, " << value << "\n";
      } else {
        os_ << "global " << global_name << " = alloc i32, zeroinit\n";
      }

      SymbolInfo info;
      info.kind = SymbolKind::Var;
      info.is_const = false;
      info.ir_name = global_name;

      InsertSymbol(def->ident, info);
      continue;
    }

    if (def->init) {
      std::vector<int> values = ConstInitToValues(*def->init, dims);
      int pos = 0;
      std::string init = BuildAggregateInit(values, dims, 0, pos);
      os_ << "global " << global_name << " = alloc "
          << KoopaArrayType(dims) << ", " << init << "\n";
    } else {
      os_ << "global " << global_name << " = alloc "
          << KoopaArrayType(dims) << ", zeroinit\n";
    }

    SymbolInfo info;
    info.kind = SymbolKind::Var;
    info.is_const = false;
    info.ir_name = global_name;
    info.is_array = true;
    info.is_array_param = false;
    info.dims = dims;

    InsertSymbol(def->ident, info);
  }

  os_ << "\n";
}

void KoopaGenerator::GenerateFuncDef(const FuncDefAST &ast) {
  temp_id_ = 0;
  var_id_ = 0;
  block_id_ = 0;
  current_block_terminated_ = false;
  loop_stack_.clear();
  current_func_ret_type_ = ast.ret_type;

  os_ << "fun @" << ast.ident << "(";

  for (size_t i = 0; i < ast.params.size(); ++i) {
    if (i != 0) {
      os_ << ", ";
    }

    const auto &param = ast.params[i];

    if (param->is_array) {
      std::vector<int> dims = EvalDims(param->dims);
      os_ << "%p" << i << ": " << KoopaPointerParamType(dims);
    } else {
      os_ << "%p" << i << ": i32";
    }
  }

  os_ << ")";

  if (ast.ret_type == TypeKind::Int) {
    os_ << ": i32";
  }

  os_ << " {\n";
  os_ << "%entry:\n";

  // 函数参数作用域。函数体 Block 会再开一个内层作用域。
  EnterScope();

  for (size_t i = 0; i < ast.params.size(); ++i) {
    const auto &param = ast.params[i];

    std::string var_name = NewVar();

    SymbolInfo info;
    info.kind = SymbolKind::Var;
    info.is_const = false;
    info.ir_name = var_name;
    info.is_array = param->is_array;
    info.is_array_param = param->is_array;

    if (param->is_array) {
      info.dims = EvalDims(param->dims);

      os_ << "  " << var_name << " = alloc "
          << KoopaPointerParamType(info.dims) << "\n";
      os_ << "  store %p" << i << ", " << var_name << "\n";
    } else {
      os_ << "  " << var_name << " = alloc i32\n";
      os_ << "  store %p" << i << ", " << var_name << "\n";
    }

    InsertSymbol(param->ident, info);
  }

  GenerateBlock(*ast.block);

  ExitScope();

  if (!current_block_terminated_) {
    if (ast.ret_type == TypeKind::Void) {
      os_ << "  ret\n";
    } else {
      os_ << "  ret 0\n";
    }
  }

  os_ << "}\n\n";
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
    std::vector<int> dims = EvalDims(def->dims);

    if (dims.empty()) {
      if (!def->init || def->init->is_list) {
        throw std::runtime_error("invalid scalar const initializer");
      }

      SymbolInfo info;
      info.kind = SymbolKind::Const;
      info.is_const = true;
      info.const_value = EvalConstExpr(*def->init->expr);

      InsertSymbol(def->ident, info);
      continue;
    }

    if (!def->init) {
      throw std::runtime_error("const array must have initializer");
    }

    std::vector<int> values = ConstInitToValues(*def->init, dims);

    std::string var_name = NewVar();
    os_ << "  " << var_name << " = alloc " << KoopaArrayType(dims) << "\n";

    SymbolInfo info;
    info.kind = SymbolKind::Var;
    info.is_const = true;
    info.ir_name = var_name;
    info.is_array = true;
    info.is_array_param = false;
    info.dims = dims;
    info.const_array_values = std::move(values);

    InsertSymbol(def->ident, info);

    GenerateLocalArrayInit(var_name, dims, *def->init);
  }
}

void KoopaGenerator::GenerateVarDecl(const VarDeclAST &ast) {
  for (const auto &def : ast.defs) {
    std::vector<int> dims = EvalDims(def->dims);

    if (dims.empty()) {
      std::string var_name = NewVar();

      os_ << "  " << var_name << " = alloc i32\n";

      SymbolInfo info;
      info.kind = SymbolKind::Var;
      info.is_const = false;
      info.ir_name = var_name;

      InsertSymbol(def->ident, info);

      if (def->init) {
        if (def->init->is_list) {
          throw std::runtime_error("scalar variable initialized by list");
        }

        std::string init_value = GenerateExpr(*def->init->expr);
        os_ << "  store " << init_value << ", " << var_name << "\n";
      }

      continue;
    }

    std::string var_name = NewVar();
    os_ << "  " << var_name << " = alloc " << KoopaArrayType(dims) << "\n";

    SymbolInfo info;
    info.kind = SymbolKind::Var;
    info.is_const = false;
    info.ir_name = var_name;
    info.is_array = true;
    info.is_array_param = false;
    info.dims = dims;

    InsertSymbol(def->ident, info);

    if (def->init) {
      GenerateLocalArrayInit(var_name, dims, *def->init);
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

void KoopaGenerator::GenerateWhileStmt(const WhileStmtAST &ast) {
  std::string entry_label = NewBlock("while_entry");
  std::string body_label = NewBlock("while_body");
  std::string end_label = NewBlock("while_end");

  EmitJumpIfNeeded(entry_label);

  EmitBlockLabel(entry_label);
  std::string cond_value = GenerateExpr(*ast.cond);
  os_ << "  br " << cond_value << ", " << body_label << ", " << end_label
      << "\n";
  current_block_terminated_ = true;

  EmitBlockLabel(body_label);

  loop_stack_.push_back(LoopInfo{entry_label, end_label});

  GenerateStmt(*ast.body);

  loop_stack_.pop_back();

  EmitJumpIfNeeded(entry_label);

  EmitBlockLabel(end_label);
}

void KoopaGenerator::GenerateBreakStmt(const BreakStmtAST &ast) {
  (void)ast;

  if (loop_stack_.empty()) {
    throw std::runtime_error("break statement not within a loop");
  }

  os_ << "  jump " << loop_stack_.back().break_label << "\n";
  current_block_terminated_ = true;
}

void KoopaGenerator::GenerateContinueStmt(const ContinueStmtAST &ast) {
  (void)ast;

  if (loop_stack_.empty()) {
    throw std::runtime_error("continue statement not within a loop");
  }

  os_ << "  jump " << loop_stack_.back().continue_label << "\n";
  current_block_terminated_ = true;
}

void KoopaGenerator::GenerateStmt(const StmtAST &ast) {
  if (const auto *assign_stmt = dynamic_cast<const AssignStmtAST *>(&ast)) {
    const SymbolInfo &symbol = LookupSymbol(assign_stmt->lval->ident);

    if (symbol.is_const) {
      throw std::runtime_error("cannot assign to const symbol: " +
                               assign_stmt->lval->ident);
    }

    size_t remaining = 0;
    std::string addr = GenerateLValAddr(*assign_stmt->lval, &remaining);

    if (remaining != 0) {
      throw std::runtime_error("cannot assign to array object directly");
    }

    std::string value = GenerateExpr(*assign_stmt->value);
    os_ << "  store " << value << ", " << addr << "\n";
    return;
  }

  if (const auto *expr_stmt = dynamic_cast<const ExprStmtAST *>(&ast)) {
    if (expr_stmt->expr) {
      (void)GenerateExpr(*expr_stmt->expr);
    }
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

  if (const auto *while_stmt = dynamic_cast<const WhileStmtAST *>(&ast)) {
    GenerateWhileStmt(*while_stmt);
    return;
  }

  if (const auto *break_stmt = dynamic_cast<const BreakStmtAST *>(&ast)) {
    GenerateBreakStmt(*break_stmt);
    return;
  }

  if (const auto *continue_stmt = dynamic_cast<const ContinueStmtAST *>(&ast)) {
    GenerateContinueStmt(*continue_stmt);
    return;
  }

  if (const auto *ret_stmt = dynamic_cast<const ReturnStmtAST *>(&ast)) {
    if (ret_stmt->value) {
      std::string ret_value = GenerateExpr(*ret_stmt->value);
      os_ << "  ret " << ret_value << "\n";
    } else {
      os_ << "  ret\n";
    }

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

std::string KoopaGenerator::GenerateCallExpr(const CallExprAST &ast) {
  const SymbolInfo &symbol = LookupSymbol(ast.ident);

  if (symbol.kind != SymbolKind::Func) {
    throw std::runtime_error("called object is not a function: " + ast.ident);
  }

  if (symbol.param_types.size() != ast.args.size()) {
    throw std::runtime_error("wrong number of arguments in call: " + ast.ident);
  }

  std::vector<std::string> args;
  args.reserve(ast.args.size());

  for (size_t i = 0; i < ast.args.size(); ++i) {
    const auto &expected = symbol.param_types[i];

    if (expected.is_array) {
      const auto *lval = dynamic_cast<const LValAST *>(ast.args[i].get());
      if (!lval) {
        throw std::runtime_error("array argument must be lval");
      }

      args.push_back(GenerateLValForCallArg(*lval, expected));
    } else {
      args.push_back(GenerateExpr(*ast.args[i]));
    }
  }

  std::string call_text = "call @" + ast.ident + "(";
  for (size_t i = 0; i < args.size(); ++i) {
    if (i != 0) {
      call_text += ", ";
    }
    call_text += args[i];
  }
  call_text += ")";

  if (symbol.return_type == TypeKind::Void) {
    os_ << "  " << call_text << "\n";
    return "";
  }

  std::string result = NewTemp();
  os_ << "  " << result << " = " << call_text << "\n";
  return result;
}

std::string KoopaGenerator::GenerateExpr(const ExprAST &ast) {
  if (const auto *number = dynamic_cast<const NumberAST *>(&ast)) {
    return std::to_string(number->value);
  }

  if (const auto *lval = dynamic_cast<const LValAST *>(&ast)) {
    const SymbolInfo &symbol = LookupSymbol(lval->ident);

    if (symbol.kind == SymbolKind::Const && !symbol.is_array) {
      if (!lval->indices.empty()) {
        throw std::runtime_error("indexing scalar const: " + lval->ident);
      }
      return std::to_string(symbol.const_value);
    }

    size_t remaining = 0;
    std::string addr = GenerateLValAddr(*lval, &remaining);

    if (remaining != 0) {
      // 合法 SysY 中，未完全解引用的数组值只会作为函数实参。
      return addr;
    }

    std::string result = NewTemp();
    os_ << "  " << result << " = load " << addr << "\n";
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

  if (const auto *call = dynamic_cast<const CallExprAST *>(&ast)) {
    return GenerateCallExpr(*call);
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

  throw std::runtime_error("unsupported expression AST node");
}

std::int32_t KoopaGenerator::EvalConstExpr(const ExprAST &ast) const {
  if (const auto *number = dynamic_cast<const NumberAST *>(&ast)) {
    return static_cast<std::int32_t>(number->value);
  }

  if (const auto *lval = dynamic_cast<const LValAST *>(&ast)) {
    const SymbolInfo &symbol = LookupSymbol(lval->ident);

    if (symbol.kind == SymbolKind::Const && !symbol.is_array) {
      if (!lval->indices.empty()) {
        throw std::runtime_error("indexing scalar const: " + lval->ident);
      }
      return symbol.const_value;
    }

    if (symbol.is_const && symbol.is_array) {
      if (lval->indices.size() != symbol.dims.size()) {
        throw std::runtime_error(
            "const array in ConstExp must be fully indexed: " + lval->ident);
      }

      int offset = 0;
      for (size_t i = 0; i < lval->indices.size(); ++i) {
        int index = EvalConstExpr(*lval->indices[i]);
        offset += index * Product(symbol.dims, i + 1);
      }

      if (offset < 0 ||
          offset >= static_cast<int>(symbol.const_array_values.size())) {
        throw std::runtime_error("const array index out of range: " +
                                 lval->ident);
      }

      return symbol.const_array_values[offset];
    }

    throw std::runtime_error("non-constant symbol in ConstExp: " +
                             lval->ident);
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
    if (binary->op == BinaryOp::LAnd) {
      std::int32_t lhs = EvalConstExpr(*binary->lhs);
      if (lhs == 0) {
        return 0;
      }
      std::int32_t rhs = EvalConstExpr(*binary->rhs);
      return rhs != 0 ? 1 : 0;
    }

    if (binary->op == BinaryOp::LOr) {
      std::int32_t lhs = EvalConstExpr(*binary->lhs);
      if (lhs != 0) {
        return 1;
      }
      std::int32_t rhs = EvalConstExpr(*binary->rhs);
      return rhs != 0 ? 1 : 0;
    }

    std::int32_t lhs = EvalConstExpr(*binary->lhs);
    std::int32_t rhs = EvalConstExpr(*binary->rhs);

    switch (binary->op) {
      case BinaryOp::Add:
        return lhs + rhs;
      case BinaryOp::Sub:
        return lhs - rhs;
      case BinaryOp::Mul:
        return lhs * rhs;
      case BinaryOp::Div:
        return lhs / rhs;
      case BinaryOp::Mod:
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

  throw std::runtime_error("unsupported ConstExp AST node");
}
