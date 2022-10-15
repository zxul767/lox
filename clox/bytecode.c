#include "bytecode.h"
#include "memory.h"

void bytecode__init(Bytecode* code) {
  code->count = 0;
  code->capacity = 0;
  code->instructions = NULL;
  code->source_lines = NULL;
  value_array__init(&code->constants);
}

void bytecode__dispose(Bytecode* code) {
  FREE_ARRAY(uint8_t, code->instructions, code->capacity);
  FREE_ARRAY(int, code->source_lines, code->capacity);
  value_array__dispose(&code->constants);
  bytecode__init(code);
}

static void grow_capacity(Bytecode* code) {
  int old_capacity = code->capacity;
  code->capacity = GROW_CAPACITY(old_capacity);
  code->instructions =
      GROW_ARRAY(uint8_t, code->instructions, old_capacity, code->capacity);
  code->source_lines =
      GROW_ARRAY(int, code->source_lines, old_capacity, code->capacity);
}

void bytecode__append(Bytecode* code, uint8_t byte, int source_line) {
  if (code->count + 1 > code->capacity) {
    grow_capacity(code);
  }
  code->instructions[code->count] = byte;
  code->source_lines[code->count] = source_line;
  code->count++;
}

int bytecode__store_constant(Bytecode* code, Value value) {
  value_array__append(&code->constants, value);
  return code->constants.count - 1;
}
