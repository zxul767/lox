#ifndef BYTECODE_H_
#define BYTECODE_H_

#include "common.h"
#include "value.h"

typedef enum {
  // push a constant (read as two bytes: [opcode, index]) onto
  // the stack
  OP_CONSTANT,
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  OP_POP,
  OP_GET_LOCAL,
  OP_SET_LOCAL,
  OP_GET_GLOBAL,
  OP_SET_GLOBAL,
  OP_DEFINE_GLOBAL,
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  // all binary operations take their two operands from the top
  // of the stack
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  // logically negate the current value at the top of the stack
  // (pop and push, or in-place modification)
  OP_NOT,
  // numerically negate the current value at the top of the stack
  // (pop and push, or in-place modification)
  OP_NEGATE,
  OP_PRINT,
  // unconditional jump
  OP_JUMP,
  // conditional jump when the last condition was false
  OP_JUMP_IF_FALSE,
  OP_LOOP,
  OP_RETURN,
  OP_CALL,

} OpCode;

typedef struct Bytecode {
  int count;
  int capacity;
  uint8_t* instructions;
  // `to_source_line[offset]` maps `instructions[offset]` to its corresponding
  // source code line
  int* to_source_line;
  ValueArray constants;

} Bytecode;

void bytecode__init(Bytecode*);
void bytecode__dispose(Bytecode*);
void bytecode__append(Bytecode*, uint8_t byte, int source_line);
// returns the slot index into which `value` was inserted
// in the code->constants array
int bytecode__store_constant(Bytecode* code, Value value);

#endif // BYTECODE_H_
