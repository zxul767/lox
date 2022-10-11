#include "bytecode.h"
#include "common.h"
#include "debug.h"

int main(int argc, const char *argv[]) {
  Bytecode code;
  bytecode__init(&code);

  int index = bytecode__store_constant(&code, 1.2);
  // the OP_CONSTANT instruction takes one argument: the index of the
  // constant to load (in the code->constants array)
  int dummy_line = 101;
  bytecode__append(&code, OP_CONSTANT, dummy_line);
  bytecode__append(&code, index, dummy_line);
  bytecode__append(&code, OP_RETURN, dummy_line);

  bytecode__disassemble(&code, "clox bytecode");
  bytecode__dispose(&code);

  return 0;
}
