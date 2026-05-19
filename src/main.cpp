#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <sstream>
#include <stdexcept>
#include <string>

#include "ast.hpp"
#include "koopa.hpp"
#include "riscv.hpp"

extern FILE *yyin;
extern int yyparse(std::unique_ptr<BaseAST> &ast);

struct CmdArgs {
  std::string mode;
  std::string input;
  std::string output;
};

static CmdArgs ParseArgs(int argc, const char *argv[]) {
  if (argc != 5) {
    throw std::runtime_error(
        "usage: compiler (-koopa | -riscv) input.sy -o output");
  }

  CmdArgs args;
  args.mode = argv[1];
  args.input = argv[2];

  std::string option_o = argv[3];
  if (option_o != "-o") {
    throw std::runtime_error(
        "usage: compiler (-koopa | -riscv) input.sy -o output");
  }

  args.output = argv[4];

  if (args.mode != "-koopa" && args.mode != "-riscv") {
    throw std::runtime_error("mode must be either -koopa or -riscv");
  }

  return args;
}

static std::unique_ptr<BaseAST> ParseSysY(const std::string &input_path) {
  yyin = std::fopen(input_path.c_str(), "r");
  if (yyin == nullptr) {
    throw std::runtime_error("failed to open input file: " + input_path);
  }

  std::unique_ptr<BaseAST> ast;
  int parse_ret = yyparse(ast);

  std::fclose(yyin);

  if (parse_ret != 0 || !ast) {
    throw std::runtime_error("failed to parse input file");
  }

  return ast;
}

static std::string GenerateKoopaIR(const BaseAST &ast) {
  std::ostringstream koopa_stream;
  KoopaGenerator generator(koopa_stream);
  generator.Generate(ast);
  return koopa_stream.str();
}

int main(int argc, const char *argv[]) {
  try {
    CmdArgs args = ParseArgs(argc, argv);

    std::unique_ptr<BaseAST> ast = ParseSysY(args.input);
    std::string koopa_ir = GenerateKoopaIR(*ast);

    std::ofstream output(args.output);
    if (!output.is_open()) {
      throw std::runtime_error("failed to open output file: " + args.output);
    }

    if (args.mode == "-koopa") {
      output << koopa_ir;
    } else if (args.mode == "-riscv") {
      RiscvGenerator generator(output);
      generator.Generate(koopa_ir);
    }

    return 0;
  } catch (const std::exception &e) {
    std::cerr << "compiler error: " << e.what() << std::endl;
    return 1;
  }
}