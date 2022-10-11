#include "debug.h"
#include <stdio.h>

void bytecode__disassemble(const Bytecode *code, const char *name) {
  printf("== %s ==\n", name);
  for (int offset = 0; offset < code->count;) {
    offset = bytecode__disassemble_instruction(code, offset);
  }
}

static int simple_instruction(const char *name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int constant_instruction(const char *name, const Bytecode *code,
                                int offset) {
  uint8_t constant_index = code->instructions[offset + 1];
  printf("%-16s %4d '", name, constant_index);
  value__print(code->constants.values[constant_index]);
  printf("'\n");

  return offset + 2;
}

int bytecode__disassemble_instruction(const Bytecode *code, int offset) {
  printf("%04d ", offset);

  if (offset > 0 &&
      code->source_lines[offset] == code->source_lines[offset - 1]) {
    printf("   | ");
  } else {
    printf("%4d ", code->source_lines[offset]);
  }

  uint8_t instruction = code->instructions[offset];
  switch (instruction) {
  case OP_CONSTANT:
    return constant_instruction("OP_CONSTANT", code, offset);
  case OP_ADD:
    return simple_instruction("OP_ADD", offset);
  case OP_SUBTRACT:
    return simple_instruction("OP_SUBTRACT", offset);
  case OP_MULTIPLY:
    return simple_instruction("OP_MULTIPLY", offset);
  case OP_DIVIDE:
    return simple_instruction("OP_DIVIDE", offset);
  case OP_NEGATE:
    return simple_instruction("OP_NEGATE", offset);
  case OP_RETURN:
    return simple_instruction("OP_RETURN", offset);
  default:
    printf("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}

void vm__dump_stack(const VM *vm) {
  printf("          ");
  for (const Value *slot = vm->stack; slot < vm->stack_top; slot++) {
    printf("[ ");
    value__print(*slot);
    printf(" ]");
  }
  printf("\n");
}
