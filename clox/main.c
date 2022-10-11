#include "bytecode.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

int main(int argc, const char *argv[]) {
  VM vm;
  vm__init(&vm);

  Bytecode code;
  bytecode__init(&code);

  int line = 100;
  // simulate the instructions for:
  // 100   ((1.2 + (-2.1))
  // 101    / 2.0)
  //
  // load constant: 1.2
  int index1 = bytecode__store_constant(&code, 1.2);
  // the OP_CONSTANT instruction takes one argument: the index of the
  // constant to load (in the code->constants array)
  bytecode__append(&code, OP_CONSTANT, line);
  bytecode__append(&code, index1, line);
  // load constant: 2.1
  int index2 = bytecode__store_constant(&code, 2.1);
  bytecode__append(&code, OP_CONSTANT, line);
  bytecode__append(&code, index2, line);
  // negation: -2.1
  bytecode__append(&code, OP_NEGATE, line);
  // addition: 1.2 + (-2.1)
  bytecode__append(&code, OP_ADD, line);
  // load constant: 2.0
  int index3 = bytecode__store_constant(&code, 2.0);
  bytecode__append(&code, OP_CONSTANT, line);
  bytecode__append(&code, index3, line);
  line++;
  // division: -0.9 / 2.0
  bytecode__append(&code, OP_DIVIDE, line);
  bytecode__append(&code, OP_RETURN, line);

  vm__interpret(&code, &vm);
  /* bytecode__disassemble(&code, "clox bytecode"); */

  bytecode__dispose(&code);
  vm__dispose(&vm);

  return 0;
}
