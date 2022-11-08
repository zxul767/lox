#ifndef BYTECODE_H_
#define BYTECODE_H_

#include "common.h"
#include "value.h"

//
// our interpreter is a stack-based virtual machine, with instructions that may
// have arguments stored either in the bytecode itself, in the constants table,
// or in the value stack
//
typedef enum {
  // push a constant (from the constants array) onto the stack
  OP_LOAD_CONSTANT, // [opcode, constant_location]
  // push a singleton constant as a single byte onto the stack
  OP_NIL,
  OP_TRUE,
  OP_FALSE,
  // pop value off of the stack
  OP_POP,

  // push a local argument (which itself is stored on the stack) onto the top of
  // the stack
  OP_GET_LOCAL, // [opcode, local_stack_slot]
  // set a local argument (in-place modification of the value on the stack) to
  // the value on top of the stack
  OP_SET_LOCAL, // [opcode, local_stack_slot]

  // push a non-local argument (which itself is stored on the stack) onto the
  // top of the stack
  OP_GET_UPVALUE,
  // set a non-local argument (in-place modification of the value on the stack)
  // to the value on top of the stack
  OP_SET_UPVALUE,

  // push the value of a global variable (read from the VM's "globals" table)
  OP_GET_GLOBAL,
  // set the value of a global variable (in the VM's "globals" table)
  OP_SET_GLOBAL,
  // add an entry to the VM's "globals" table (popping off of the stack)
  OP_DEFINE_GLOBAL,

  // all binary operations take their two operands from the top of the stack
  OP_EQUAL,
  OP_GREATER,
  OP_LESS,
  OP_ADD,
  OP_SUBTRACT,
  OP_MULTIPLY,
  OP_DIVIDE,
  // logically negate the top of the stack
  OP_NOT,

  // numerically negate the top of the stack
  OP_NEGATE,

  // pop value off of the stack and print it
  OP_PRINT,

  // unconditional forward jump
  OP_JUMP, // [opcode, jump-high-bits, jump-low-bits]

  // conditional jump when the top of the stack is false
  OP_JUMP_IF_FALSE, // [opcode, jump-high-bits, jump-low-bits]

  // unconditional backward jump
  OP_LOOP, // [opcode, jump-high-bits, jump-low-bits]

  // all functions are wrapped in closures
  OP_CLOSURE, // [opcode, function_constant_location]
  OP_CALL,    // [opcode, args_count]

  // pop result value off of the stack, pop last call frame (including value
  // stack args), and push result back onto the stack (for the caller to use)
  OP_RETURN,

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
