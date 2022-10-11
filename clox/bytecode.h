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
  ValueArray constants;

} Bytecode;

void bytecode__init(Bytecode *code);
void bytecode__dispose(Bytecode *code);
void bytecode__write_byte(Bytecode *code, uint8_t byte);
// returns the slot index into which `value` was inserted
// in the code->constants array
int bytecode__add_constant(Bytecode *code, Value value);

#endif // BYTECODE_H_
