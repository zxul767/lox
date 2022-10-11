#include "bytecode.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

int main(int argc, const char *argv[]) {
  VM vm;
  vm__init(&vm);

  Bytecode code;
  bytecode__init(&code);

  int index1 = bytecode__store_constant(&code, 1.2);
  int index2 = bytecode__store_constant(&code, 2.1);
  // the OP_CONSTANT instruction takes one argument: the index of the
  // constant to load (in the code->constants array)
  int dummy_line = 101;
  bytecode__append(&code, OP_CONSTANT, dummy_line);
  bytecode__append(&code, index1, dummy_line++);
  bytecode__append(&code, OP_CONSTANT, dummy_line);
  bytecode__append(&code, index2, dummy_line);
  bytecode__append(&code, OP_NEGATE, dummy_line++);
  bytecode__append(&code, OP_RETURN, dummy_line);

  /* bytecode__disassemble(&code, "clox bytecode"); */
  vm__interpret(&code, &vm);

  bytecode__dispose(&code);
  vm__dispose(&vm);

  return 0;
}
