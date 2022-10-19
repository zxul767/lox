#include "debug.h"
#include <stdio.h>

static void print_char(char c, int times) {
  for (int i = 0; i < times; i++)
    putchar('#');
}

void debug__print_section_divider() {
  print_char('=', 80);
  putchar('\n');
}

void debug__disassemble(const Bytecode* code, const char* name) {
  printf("%s\n", name);
  debug__print_section_divider();
  for (int offset = 0; offset < code->count;) {
    offset = debug__disassemble_instruction(code, offset);
  }
  debug__print_section_divider();
}

static int simple_instruction(const char* name, int offset) {
  printf("%s\n", name);
  return offset + 1;
}

static int constant_instruction(const char* name, const Bytecode* code,
                                int offset) {
  uint8_t constant_index = code->instructions[offset + 1];
  printf("%-16s %4d '", name, constant_index);
  value__print(code->constants.values[constant_index]);
  printf("'\n");

  return offset + 2;
}

int debug__disassemble_instruction(const Bytecode* code, int offset) {
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
  case OP_NIL:
    return simple_instruction("OP_NIL", offset);
  case OP_TRUE:
    return simple_instruction("OP_TRUE", offset);
  case OP_FALSE:
    return simple_instruction("OP_FALSE", offset);
  case OP_POP:
    return simple_instruction("OP_POP", offset);
  case OP_GET_GLOBAL:
    return constant_instruction("OP_GET_GLOBAL", code, offset);
  case OP_DEFINE_GLOBAL:
    return constant_instruction("OP_DEFINE_GLOBAL", code, offset);
  case OP_EQUAL:
    return simple_instruction("OP_EQUAL", offset);
  case OP_GREATER:
    return simple_instruction("OP_GREATER", offset);
  case OP_LESS:
    return simple_instruction("OP_LESS", offset);
  case OP_ADD:
    return simple_instruction("OP_ADD", offset);
  case OP_SUBTRACT:
    return simple_instruction("OP_SUBTRACT", offset);
  case OP_MULTIPLY:
    return simple_instruction("OP_MULTIPLY", offset);
  case OP_DIVIDE:
    return simple_instruction("OP_DIVIDE", offset);
  case OP_NOT:
    return simple_instruction("OP_NOT", offset);
  case OP_NEGATE:
    return simple_instruction("OP_NEGATE", offset);
  case OP_PRINT:
    return simple_instruction("OP_PRINT", offset);
  case OP_RETURN:
    return simple_instruction("OP_RETURN", offset);
  default:
    printf("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}

void debug__dump_stack(const VM* vm) {
  printf("          ");
  for (const Value* slot = vm->stack; slot < vm->stack_top; slot++) {
    printf("[ ");
    value__print(*slot);
    printf(" ]");
  }
  printf("\n");
}
