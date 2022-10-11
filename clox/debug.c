#include "debug.h"
#include <stdio.h>

void bytecode__disassemble(Bytecode *code, const char *name) {
  printf("== %s ==\n", name);
  for (int offset = 0; offset < code->count;) {
    offset = bytecode__disassemble_instruction(code, offset);
  }
}

static int simple_instruction(const char *name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int constant_instruction(const char *name, Bytecode *code, int offset) {
  uint8_t constant_index = code->instructions[offset + 1];
  printf("%-16s %4d '", name, constant_index);
  value__print(code->constants.values[constant_index]);
  printf("'\n");

  return offset + 2;
}

int bytecode__disassemble_instruction(Bytecode *code, int offset) {
  printf("%04d ", offset);

  if (offset > 0 &&
      code->source_lines[offset] == code->source_lines[offset - 1]) {
    printf("   | ");
  } else {
    printf("%4d ", code->source_lines[offset]);
  }

  uint8_t instruction = code->instructions[offset];
  switch (instruction) {
  case OP_RETURN:
    return simple_instruction("OP_RETURN", offset);
  case OP_CONSTANT:
    return constant_instruction("OP_CONSTANT", code, offset);
  default:
    printf("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}
