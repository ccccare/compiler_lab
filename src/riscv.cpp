#include "riscv.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <algorithm>

#include "koopa.h"

namespace {

int AlignTo(int value, int align) {
  return (value + align - 1) / align * align;
}

bool IsImm12(int value) {
  return value >= -2048 && value <= 2047;
}

std::string StripKoopaNamePrefix(const char *name) {
  if (name == nullptr) {
    throw std::runtime_error("Koopa symbol name is null");
  }

  std::string result(name);
  if (!result.empty() && (result[0] == '@' || result[0] == '%')) {
    result.erase(result.begin());
  }
  return result;
}

class RawProgramVisitor {
 public:
  explicit RawProgramVisitor(std::ostream &os) : os_(os) {}

  void Visit(const koopa_raw_program_t &program) {
    Visit(program.values);
    Visit(program.funcs);
  }

 private:
  std::ostream &os_;

  int stack_size_ = 0;
  int outgoing_args_size_ = 0;
  int ra_offset_ = -1;
  bool has_call_ = false;
  std::unordered_map<koopa_raw_value_t, int> stack_offset_;

  void Visit(const koopa_raw_slice_t &slice) {
    for (size_t i = 0; i < slice.len; ++i) {
      const void *ptr = slice.buffer[i];

      switch (slice.kind) {
        case KOOPA_RSIK_FUNCTION:
          Visit(reinterpret_cast<koopa_raw_function_t>(ptr));
          break;

        case KOOPA_RSIK_BASIC_BLOCK:
          Visit(reinterpret_cast<koopa_raw_basic_block_t>(ptr));
          break;

        case KOOPA_RSIK_VALUE:
          Visit(reinterpret_cast<koopa_raw_value_t>(ptr));
          break;

        default:
          throw std::runtime_error("unsupported raw slice kind");
      }
    }
  }

  void Visit(const koopa_raw_function_t &func) {
    // 函数声明没有基本块，例如 decl @getint(): i32，跳过即可。
    if (func->bbs.len == 0) {
      return;
    }

    PrepareStackFrame(func);

    std::string func_name = StripKoopaNamePrefix(func->name);

    os_ << "  .text\n";
    os_ << "  .globl " << func_name << "\n";
    os_ << func_name << ":\n";

    // prologue: 开栈帧
    EmitAddSp(-stack_size_);

    // 如果当前函数内部出现了 call，则必须保存 ra。
    if (has_call_) {
      EmitStoreToStack("ra", ra_offset_);
    }

    for (size_t i = 0; i < func->bbs.len; ++i) {
      auto bb = reinterpret_cast<koopa_raw_basic_block_t>(func->bbs.buffer[i]);

      // 第一个基本块对应函数入口，已经有 func_name:，不额外输出 entry:
    if (i != 0) {
      os_ << StripKoopaNamePrefix(bb->name) << ":\n";
    }

      Visit(bb);
    }
  }

  bool NeedStackSlot(const koopa_raw_value_t &value) const {
    if (value->kind.tag == KOOPA_RVT_ALLOC) {
      return true;
    }

    return value->ty->tag != KOOPA_RTT_UNIT;
  }

  void PrepareStackFrame(const koopa_raw_function_t &func) {
    stack_offset_.clear();
    stack_size_ = 0;
    outgoing_args_size_ = 0;
    ra_offset_ = -1;
    has_call_ = false;

    size_t max_call_args = 0;

    for (size_t i = 0; i < func->bbs.len; ++i) {
      auto bb = reinterpret_cast<koopa_raw_basic_block_t>(func->bbs.buffer[i]);

    for (size_t j = 0; j < bb->insts.len; ++j) {
      auto value = reinterpret_cast<koopa_raw_value_t>(bb->insts.buffer[j]);

      if (value->kind.tag == KOOPA_RVT_CALL) {
        has_call_ = true;
        max_call_args =
            std::max(
          max_call_args,
          static_cast<size_t>(value->kind.data.call.args.len));
      }
    }
  }

  if (max_call_args > 8) {
    outgoing_args_size_ = static_cast<int>((max_call_args - 8) * 4);
  }

  int offset = outgoing_args_size_;

  for (size_t i = 0; i < func->bbs.len; ++i) {
    auto bb = reinterpret_cast<koopa_raw_basic_block_t>(func->bbs.buffer[i]);

    for (size_t j = 0; j < bb->insts.len; ++j) {
      auto value = reinterpret_cast<koopa_raw_value_t>(bb->insts.buffer[j]);

      if (NeedStackSlot(value)) {
        stack_offset_[value] = offset;
        offset += 4;
      }
    }
  }

  if (has_call_) {
    ra_offset_ = offset;
    offset += 4;
  }

    stack_size_ = AlignTo(offset, 16);
  }

  int GetStackOffset(const koopa_raw_value_t &value) const {
    auto it = stack_offset_.find(value);
    if (it == stack_offset_.end()) {
      throw std::runtime_error("value has no stack slot");
    }

    return it->second;
  }

  void EmitAddSp(int delta) {
    if (delta == 0) {
      return;
    }

    if (IsImm12(delta)) {
      os_ << "  addi sp, sp, " << delta << "\n";
    } else {
      os_ << "  li t6, " << delta << "\n";
      os_ << "  add sp, sp, t6\n";
    }
  }

  void EmitLoadFromStack(const std::string &reg, int offset) {
    if (IsImm12(offset)) {
      os_ << "  lw " << reg << ", " << offset << "(sp)\n";
    } else {
      os_ << "  li t6, " << offset << "\n";
      os_ << "  add t6, sp, t6\n";
      os_ << "  lw " << reg << ", 0(t6)\n";
    }
  }

  void EmitStoreToStack(const std::string &reg, int offset) {
    if (IsImm12(offset)) {
      os_ << "  sw " << reg << ", " << offset << "(sp)\n";
    } else {
      os_ << "  li t6, " << offset << "\n";
      os_ << "  add t6, sp, t6\n";
      os_ << "  sw " << reg << ", 0(t6)\n";
    }
  }


  void VisitBranch(const koopa_raw_branch_t &branch) {
    LoadValue(branch.cond, "t0");

    os_ << "  bnez t0, " << StripKoopaNamePrefix(branch.true_bb->name) << "\n";
    os_ << "  j " << StripKoopaNamePrefix(branch.false_bb->name) << "\n";
  }

  void VisitJump(const koopa_raw_jump_t &jump) {
    os_ << "  j " << StripKoopaNamePrefix(jump.target->name) << "\n";
  }

  void VisitCall(const koopa_raw_value_t &value,
               const koopa_raw_call_t &call) {
    for (size_t i = 0; i < call.args.len; ++i) {
      auto arg = reinterpret_cast<koopa_raw_value_t>(call.args.buffer[i]);

      if (i < 8) {
        LoadValue(arg, "a" + std::to_string(i));
      } else {
        LoadValue(arg, "t0");
        EmitStoreToStack("t0", static_cast<int>((i - 8) * 4));
      }
    }

    os_ << "  call " << StripKoopaNamePrefix(call.callee->name) << "\n";

    // 如果 call 有返回值，返回值在 a0，需要保存到当前 call value 的栈槽。
    if (value->ty->tag != KOOPA_RTT_UNIT) {
      StoreValue(value, "a0");
    }
  }

  void Visit(const koopa_raw_basic_block_t &bb) {
    Visit(bb->insts);
  }

  void Visit(const koopa_raw_value_t &value) {
    const auto &kind = value->kind;

    switch (kind.tag) {
      case KOOPA_RVT_ALLOC:
        VisitAlloc(value);
        break;
      
      case KOOPA_RVT_GLOBAL_ALLOC:
        VisitGlobalAlloc(value, kind.data.global_alloc);
        break;

      case KOOPA_RVT_LOAD:
        VisitLoad(value, kind.data.load);
        break;

      case KOOPA_RVT_STORE:
        VisitStore(kind.data.store);
        break;

      case KOOPA_RVT_RETURN:
        VisitReturn(kind.data.ret);
        break;

      case KOOPA_RVT_BINARY:
        VisitBinary(value, kind.data.binary);
        break;
      
      case KOOPA_RVT_CALL:
        VisitCall(value, kind.data.call);
        break;

      case KOOPA_RVT_BRANCH:
        VisitBranch(kind.data.branch);
        break;

      case KOOPA_RVT_JUMP:
        VisitJump(kind.data.jump);
        break;

      default:
        throw std::runtime_error("unsupported Koopa value kind in Lv4");
    }
  }

  void VisitAlloc(const koopa_raw_value_t &value) {
      (void)value;
    // alloc 只代表一块栈内存。栈槽已经在 PrepareStackFrame 中分配好了。
    // 因此真正生成汇编时，这里不需要输出任何指令。
  }

  void VisitGlobalAlloc(const koopa_raw_value_t &value,
                      const koopa_raw_global_alloc_t &global_alloc) {
    std::string name = StripKoopaNamePrefix(value->name);

    os_ << "  .data\n";
    os_ << "  .globl " << name << "\n";
    os_ << name << ":\n";

    auto init = global_alloc.init;

    if (init->kind.tag == KOOPA_RVT_INTEGER) {
      os_ << "  .word " << init->kind.data.integer.value << "\n";
    } else if (init->kind.tag == KOOPA_RVT_ZERO_INIT) {
      os_ << "  .zero 4\n";
    } else {
      throw std::runtime_error("unsupported global initializer in Lv8");
    }
  }

  void LoadValue(const koopa_raw_value_t &value, const std::string &reg) {
    switch (value->kind.tag) {
      case KOOPA_RVT_INTEGER:
        os_ << "  li " << reg << ", " << value->kind.data.integer.value
            << "\n";
        break;

      case KOOPA_RVT_BINARY:
      case KOOPA_RVT_LOAD:
      case KOOPA_RVT_CALL:
        EmitLoadFromStack(reg, GetStackOffset(value));
        break;

      case KOOPA_RVT_FUNC_ARG_REF: {
        size_t index = value->kind.data.func_arg_ref.index;

        if (index < 8) {
          os_ << "  mv " << reg << ", a" << index << "\n";
        } else {
          // 第 9 个及之后的参数在 caller 的栈上传入。
          // callee 开栈帧后，old_sp 变成 new_sp + stack_size_。
          int offset = stack_size_ + static_cast<int>((index - 8) * 4);
          EmitLoadFromStack(reg, offset);
        }

        break;
      }

      default:
        throw std::runtime_error("unsupported value used as operand");
    }
  }

  void StoreValue(const koopa_raw_value_t &value, const std::string &reg) {
    EmitStoreToStack(reg, GetStackOffset(value));
  }

  void VisitLoad(const koopa_raw_value_t &value,
               const koopa_raw_load_t &load) {
    if (load.src->kind.tag == KOOPA_RVT_GLOBAL_ALLOC) {
      std::string name = StripKoopaNamePrefix(load.src->name);
      os_ << "  la t0, " << name << "\n";
      os_ << "  lw t0, 0(t0)\n";
      StoreValue(value, "t0");
      return;
    }

    int src_offset = GetStackOffset(load.src);
    EmitLoadFromStack("t0", src_offset);
    StoreValue(value, "t0");
  }

  void VisitStore(const koopa_raw_store_t &store) {
    LoadValue(store.value, "t0");

    if (store.dest->kind.tag == KOOPA_RVT_GLOBAL_ALLOC) {
      std::string name = StripKoopaNamePrefix(store.dest->name);
      os_ << "  la t1, " << name << "\n";
      os_ << "  sw t0, 0(t1)\n";
      return;
    }

    int dest_offset = GetStackOffset(store.dest);
    EmitStoreToStack("t0", dest_offset);
  }

  void VisitReturn(const koopa_raw_return_t &ret) {
    if (ret.value != nullptr) {
      LoadValue(ret.value, "a0");
    }

    // epilogue: 如果保存过 ra，这里恢复。
    if (has_call_) {
      EmitLoadFromStack("ra", ra_offset_);
    }

    // 恢复 sp。
    EmitAddSp(stack_size_);

    os_ << "  ret\n";
  }

  void VisitBinary(const koopa_raw_value_t &value,
                   const koopa_raw_binary_t &binary) {
    LoadValue(binary.lhs, "t0");
    LoadValue(binary.rhs, "t1");

    switch (binary.op) {
      case KOOPA_RBO_ADD:
        os_ << "  add t0, t0, t1\n";
        break;

      case KOOPA_RBO_SUB:
        os_ << "  sub t0, t0, t1\n";
        break;

      case KOOPA_RBO_MUL:
        os_ << "  mul t0, t0, t1\n";
        break;

      case KOOPA_RBO_DIV:
        os_ << "  div t0, t0, t1\n";
        break;

      case KOOPA_RBO_MOD:
        os_ << "  rem t0, t0, t1\n";
        break;

      case KOOPA_RBO_EQ:
        os_ << "  xor t0, t0, t1\n";
        os_ << "  seqz t0, t0\n";
        break;

      case KOOPA_RBO_NOT_EQ:
        os_ << "  xor t0, t0, t1\n";
        os_ << "  snez t0, t0\n";
        break;

      case KOOPA_RBO_LT:
        os_ << "  slt t0, t0, t1\n";
        break;

      case KOOPA_RBO_GT:
        os_ << "  slt t0, t1, t0\n";
        break;

      case KOOPA_RBO_LE:
        os_ << "  slt t0, t1, t0\n";
        os_ << "  seqz t0, t0\n";
        break;

      case KOOPA_RBO_GE:
        os_ << "  slt t0, t0, t1\n";
        os_ << "  seqz t0, t0\n";
        break;

      case KOOPA_RBO_AND:
        os_ << "  and t0, t0, t1\n";
        break;

      case KOOPA_RBO_OR:
        os_ << "  or t0, t0, t1\n";
        break;

      default:
        throw std::runtime_error("unsupported binary operator in Lv4");
    }

    StoreValue(value, "t0");
  }
};

}  // namespace

void RiscvGenerator::Generate(const std::string &koopa_ir) {
  koopa_program_t program;
  koopa_error_code_t parse_ret =
      koopa_parse_from_string(koopa_ir.c_str(), &program);

  if (parse_ret != KOOPA_EC_SUCCESS) {
    throw std::runtime_error("failed to parse Koopa IR");
  }

  koopa_raw_program_builder_t builder = koopa_new_raw_program_builder();
  koopa_raw_program_t raw = koopa_build_raw_program(builder, program);

  koopa_delete_program(program);

  try {
    RawProgramVisitor visitor(os_);
    visitor.Visit(raw);
  } catch (...) {
    koopa_delete_raw_program_builder(builder);
    throw;
  }

  koopa_delete_raw_program_builder(builder);
}