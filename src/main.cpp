#include <cstdio>
#include <fstream>
#include <iostream>
#include <memory>
#include <stdexcept>
#include <string>

#include "ast.hpp"
#include "koopa.hpp"

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
        "usage: compiler -koopa input.sy -o output.koopa");
  }

  CmdArgs args;
  args.mode = argv[1];
  args.input = argv[2];

  std::string option_o = argv[3];
  if (option_o != "-o") {
    throw std::runtime_error(
        "usage: compiler -koopa input.sy -o output.koopa");
  }

  args.output = argv[4];

  if (args.mode != "-koopa") {
    throw std::runtime_error("Lv1 only supports -koopa mode");
  }

  return args;
}

int main(int argc, const char *argv[]) {
  try {
    CmdArgs args = ParseArgs(argc, argv);

    yyin = std::fopen(args.input.c_str(), "r");
    if (yyin == nullptr) {
      throw std::runtime_error("failed to open input file: " + args.input);
    }

    std::unique_ptr<BaseAST> ast;
    int parse_ret = yyparse(ast);
    std::fclose(yyin);

    if (parse_ret != 0 || !ast) {
      throw std::runtime_error("failed to parse input file");
    }

    std::ofstream output(args.output);
    if (!output.is_open()) {
      throw std::runtime_error("failed to open output file: " + args.output);
    }

    KoopaGenerator generator(output);
    generator.Generate(*ast);

    return 0;
  } catch (const std::exception &e) {
    std::cerr << "compiler error: " << e.what() << std::endl;
    return 1;
  }
}