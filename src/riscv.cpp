#include "riscv.hpp"

#include <cstdint>
#include <stdexcept>
#include <string>

#include "koopa.h"

namespace {

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
          throw std::runtime_error("unsupported raw slice kind in Lv2");
      }
    }
  }

  void Visit(const koopa_raw_function_t &func) {
    // 函数声明没有基本块，例如后续可能出现的 decl @getint(): i32
    if (func->bbs.len == 0) {
      return;
    }

    std::string func_name = StripKoopaNamePrefix(func->name);

    os_ << "  .text\n";
    os_ << "  .globl " << func_name << "\n";
    os_ << func_name << ":\n";

    Visit(func->bbs);
  }

  void Visit(const koopa_raw_basic_block_t &bb) {
    Visit(bb->insts);
  }

  void Visit(const koopa_raw_value_t &value) {
    const auto &kind = value->kind;

    switch (kind.tag) {
      case KOOPA_RVT_RETURN:
        VisitReturn(kind.data.ret);
        break;

      default:
        throw std::runtime_error("unsupported Koopa value kind in Lv2");
    }
  }

  void VisitReturn(const koopa_raw_return_t &ret) {
    if (ret.value == nullptr) {
      os_ << "  ret\n";
      return;
    }

    int32_t ret_value = GetInteger(ret.value);

    os_ << "  li a0, " << ret_value << "\n";
    os_ << "  ret\n";
  }

  int32_t GetInteger(const koopa_raw_value_t &value) {
    if (value->kind.tag != KOOPA_RVT_INTEGER) {
      throw std::runtime_error("Lv2 only supports returning integer constants");
    }

    return value->kind.data.integer.value;
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