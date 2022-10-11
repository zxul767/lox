#ifndef BYTECODE_H_
#define BYTECODE_H_

#include "common.h"
#include "value.h"

typedef enum {
  OP_RETURN,
  OP_CONSTANT,

} OpCode;

typedef struct {
  int count;
  int capacity;
  uint8_t *instructions;
  // sources_lines[i] maps position instruction `i` to its corresponding
  int *source_lines;
  ValueArray constants;

} Bytecode;

void bytecode__init(Bytecode *code);
void bytecode__dispose(Bytecode *code);
void bytecode__append(Bytecode *code, uint8_t byte, int source_line);
// returns the slot index into which `value` was inserted
// in the code->constants array
int bytecode__store_constant(Bytecode *code, Value value);

#endif // BYTECODE_H_
