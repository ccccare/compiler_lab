#include "riscv.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>
#include <unordered_map>

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
    if (func->bbs.len == 0) {
      return;
    }

    PrepareStackFrame(func);

    std::string func_name = StripKoopaNamePrefix(func->name);

    os_ << "  .text\n";
    os_ << "  .globl " << func_name << "\n";
    os_ << func_name << ":\n";

    EmitAddSp(-stack_size_);

    Visit(func->bbs);
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

    int offset = 0;

    for (size_t i = 0; i < func->bbs.len; ++i) {
      auto bb =
          reinterpret_cast<koopa_raw_basic_block_t>(func->bbs.buffer[i]);

      for (size_t j = 0; j < bb->insts.len; ++j) {
        auto value =
            reinterpret_cast<koopa_raw_value_t>(bb->insts.buffer[j]);

        if (NeedStackSlot(value)) {
          stack_offset_[value] = offset;
          offset += 4;
        }
      }
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

  void Visit(const koopa_raw_basic_block_t &bb) {
    Visit(bb->insts);
  }

  void Visit(const koopa_raw_value_t &value) {
    const auto &kind = value->kind;

    switch (kind.tag) {
      case KOOPA_RVT_ALLOC:
        VisitAlloc(value);
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

      default:
        throw std::runtime_error("unsupported Koopa value kind in Lv4");
    }
  }

  void VisitAlloc(const koopa_raw_value_t &value) {
      (void)value;
    // alloc 只代表一块栈内存。栈槽已经在 PrepareStackFrame 中分配好了。
    // 因此真正生成汇编时，这里不需要输出任何指令。
  }

  void LoadValue(const koopa_raw_value_t &value, const std::string &reg) {
    switch (value->kind.tag) {
      case KOOPA_RVT_INTEGER:
        os_ << "  li " << reg << ", " << value->kind.data.integer.value
            << "\n";
        break;

      case KOOPA_RVT_BINARY:
      case KOOPA_RVT_LOAD:
        EmitLoadFromStack(reg, GetStackOffset(value));
        break;

      default:
        throw std::runtime_error("unsupported value used as operand");
    }
  }

  void StoreValue(const koopa_raw_value_t &value, const std::string &reg) {
    EmitStoreToStack(reg, GetStackOffset(value));
  }

  void VisitLoad(const koopa_raw_value_t &value,
                 const koopa_raw_load_t &load) {
    int src_offset = GetStackOffset(load.src);
    EmitLoadFromStack("t0", src_offset);
    StoreValue(value, "t0");
  }

  void VisitStore(const koopa_raw_store_t &store) {
    LoadValue(store.value, "t0");

    int dest_offset = GetStackOffset(store.dest);
    EmitStoreToStack("t0", dest_offset);
  }

  void VisitReturn(const koopa_raw_return_t &ret) {
    if (ret.value != nullptr) {
      LoadValue(ret.value, "a0");
    }

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