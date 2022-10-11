#include "bytecode.h"
#include "common.h"
#include "debug.h"

int main(int argc, const char *argv[]) {
  Bytecode code;
  bytecode__init(&code);

  int index = bytecode__add_constant(&code, 1.2);
  // the OP_CONSTANT instruction takes one argument: the index of the
  // constant to load (in the code->constants array)
  bytecode__write_byte(&code, OP_CONSTANT);
  bytecode__write_byte(&code, index);

  bytecode__write_byte(&code, OP_RETURN);

  bytecode__disassemble(&code, "test bytecode");
  bytecode__dispose(&code);

  return 0;
}
