#ifndef VM_H_
#define VM_H_

#include "table.h"
#include "value.h"

typedef struct Bytecode Bytecode;

#define STACK_MAX 256

// stack-based, virtual machine
typedef struct VM {
  const Bytecode* bytecode;
  uint8_t* instruction_pointer;

  // all instructions take their operands from this stack
  Value stack[STACK_MAX];
  Value* stack_top;

  // all heap-allocated objects are linked in a list whose head is pointed at by
  // `objects`
  Object* objects;

  // all strings (including those needed to represent variables), are "interned"
  // (i.e., a single copy is kept and reused when necessary) to minimize memory
  // and make string comparison much faster (essentially a pointer comparison
  // regardless of the string's length)
  Table interned_strings;

  Table global_vars;

} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void vm__init(VM* vm);
void vm__dispose(VM* vm);

InterpretResult vm__interpret(const char* source, VM* vm);

#endif // VM_H_
