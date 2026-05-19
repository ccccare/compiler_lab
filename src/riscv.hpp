#pragma once

#include <ostream>
#include <string>

class RiscvGenerator {
 public:
  explicit RiscvGenerator(std::ostream &os) : os_(os) {}

  void Generate(const std::string &koopa_ir);

 private:
  std::ostream &os_;
};