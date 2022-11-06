#include "debug.h"
#include <stdio.h>

// TODO: parameterize justification values for all helper functions that print
// bytecode in a table
//
static void print_char(char c, int times) {
  for (int i = 0; i < times; i++)
    putchar(c);
}

void debug__print_section_divider() {
  print_char('=', 80);
  putchar('\n');
}

void debug__print_callframe_divider() {
  print_char('-', 80);
  putchar('\n');
}

void debug__disassemble(const Bytecode* code, const char* name) {
  printf(
      "BYTECODE for '%s' %s\n", name != NULL ? name : "<script>",
      name != NULL ? "function" : ""
  );
  debug__print_section_divider();
  for (int offset = 0; offset < code->count;) {
    offset = debug__disassemble_instruction(code, offset);
  }
  debug__print_section_divider();
}

static int simple_instruction(const char* name, int offset) {
  // simple instructions are encoded as:
  // [OP_CODE]...
  //     ^
  //   offset
  printf("%s\n", name);

  return offset + 1;
}

static int byte_instruction(
    const char* name, const Bytecode* code, int offset, const char* value_name
) {
  // byte instructions are encoded as:
  // [OP_CODE][value]...
  //     ^
  //   offset
  uint8_t value = code->instructions[offset + 1];
  printf("%-20s %s:%-4d\n", name, value_name, value);

  return offset + 2;
}

static int jump_instruction(
    const char* name, int direction, const Bytecode* code, int offset
) {
  // jump instructions are encoded as:
  // [JUMP_OP_CODE][jump_length_high_bits][jump_length_low_bits]...
  //       ^
  //     offset
  uint16_t jump_length = (uint16_t)(code->instructions[offset + 1] << 8);
  jump_length |= code->instructions[offset + 2];
  printf("%-20s -> offset:%04d\n", name, offset + 3 + direction * jump_length);

  return offset + 3;
}

static int
constant_instruction(const char* name, const Bytecode* code, int offset) {
  // constant instructions are encoded as:
  // [OP_CODE][index]...
  //     ^
  //  offset
  uint8_t constant_location = code->instructions[offset + 1];
  printf("%-20s index:%d (", name, constant_location);
  value__print_repr(code->constants.values[constant_location]);
  printf(")\n");

  return offset + 2;
}

int debug__disassemble_instruction(const Bytecode* code, int offset) {
  printf("%04d ", offset);

  if (offset > 0 &&
      code->source_lines[offset] == code->source_lines[offset - 1]) {
    // reduce visual clutter by not printing repeated source line numbers
    // but still add an indicator to let the user know source line number is the
    // same as the previous instruction
    printf("   |   ");
  } else {
    printf("%4d   ", code->source_lines[offset]);
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
  case OP_DEFINE_GLOBAL:
    return constant_instruction("OP_DEFINE_GLOBAL", code, offset);
  case OP_GET_GLOBAL:
    return constant_instruction("OP_GET_GLOBAL", code, offset);
  case OP_SET_GLOBAL:
    return constant_instruction("OP_SET_GLOBAL", code, offset);
  case OP_GET_LOCAL:
    return byte_instruction("OP_GET_LOCAL", code, offset, "index");
  case OP_SET_LOCAL:
    return byte_instruction("OP_SET_LOCAL", code, offset, "index");
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
  case OP_LOOP:
    return jump_instruction("OP_JUMP", /*direction:*/ -1, code, offset);
  case OP_JUMP:
    return jump_instruction("OP_JUMP", /*direction:*/ +1, code, offset);
  case OP_JUMP_IF_FALSE:
    return jump_instruction(
        "OP_JUMP_IF_FALSE", /*direction:*/ +1, code, offset
    );
  case OP_CALL:
    return byte_instruction("OP_CALL", code, offset, "#args");
  case OP_RETURN:
    return simple_instruction("OP_RETURN", offset);
  default:
    printf("Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}

void debug__dump_stack(const VM* vm) {
  printf("            stack:");
  for (const Value* value = vm->stack; value < vm->stack_top; value++) {
    printf("[ ");
    // `value__print` would print the string `"true"` in the same way as the
    // literal `true` but during debugging we want to distinguish between the
    // two, hence the need for `value__print_repr`
    value__print_repr(*value);
    printf(" ]");
  }
  printf("\n");
}
