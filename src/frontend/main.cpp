#include <sys/resource.h>

#include <cstdlib>
#include <iostream>

#include "frontend/SysYLexer.h"
#include "frontend/SysYParser.h"
#include "frontend/ast_visitor.hpp"
#include "common/common.hpp"
#include "common/errors.hpp"
#include "optimizer/ir.hpp"
#include "optimizer/pass.hpp"
#include "backend/armv7/backend_passes.hpp"
#include "backend/armv7/program.hpp"
#include "backend/rv32/program.hpp"
#include "backend/rv32/inst.hpp"
#include "backend/rv32/backend_passes.hpp"

using namespace antlr4;
using namespace std;

int main(int argc, char *argv[]) {
  // increase stack size
  const rlim_t kStackSize = 64L * 1024L * 1024L;
  struct rlimit rl;
  int result;
  result = getrlimit(RLIMIT_STACK, &rl);
  if (result == 0) {
    if (rl.rlim_cur < kStackSize) {
      rl.rlim_cur = kStackSize;
      result = setrlimit(RLIMIT_STACK, &rl);
      if (result != 0)
        std::cerr << "setrlimit failed, use ulimit -s unlimited instead."
                  << std::endl;
    }
  }

  pair<string, string> filename = parse_arg(argc, argv);
  ifstream source{filename.first};
  if (!source.is_open()) {
    cerr << "cannot open input file\n";
    return EXIT_FAILURE;
  }

  ANTLRInputStream input(source);
  SysYLexer lexer(&input);
  CommonTokenStream tokens(&lexer);
  SysYParser parser(&tokens);
  parser.setErrorHandler(make_shared<BailErrorStrategy>());

  SysYParser::CompUnitContext *root = parser.compUnit();
  try {
    IR::CompileUnit ir;
    ASTVisitor visitor(ir);
    bool found_main = visitor.visitCompUnit(root);
    if (!found_main) throw MainFuncNotFound();
    dbg << "```cpp\n" << ir << "```\n";
    optimize_passes(ir);
    string arch = global_config.get_arg("arch", "armv7");
    if (arch == "armv7") {
      ARMv7::Program prog(&ir);
      ARMv7::optimize_before_reg_alloc(&prog);
      ofstream asm_out{filename.second};
      prog.gen_asm(asm_out);
    } else if (arch == "rv32") {
      if (global_config.disabled_passes.find("loop-parallel") == global_config.disabled_passes.end()) {
        cerr << "loop parallel is not supported by rv32 backend yet\n";
        return EXIT_FAILURE;
      }
      RV32::Program prog(&ir);
      RV32::optimize_before_reg_alloc(&prog);
      ofstream asm_out{filename.second};
      prog.gen_asm(asm_out);
    } else {
      cerr << "unrecognized architecture\n";
      return EXIT_FAILURE;
    }
    return 0;
  } catch (SyntaxError &e) {
    cout << "error: " << e.what() << '\n';
    return EXIT_FAILURE;
  }
}
