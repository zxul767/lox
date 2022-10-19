#ifndef VM_H_
#define VM_H_

#include "table.h"
#include "value.h"

typedef struct Bytecode Bytecode;

// Virtual Machine for Lox
#define STACK_MAX 256
typedef struct VM {
  const Bytecode* bytecode;
  uint8_t* instruction_pointer;

  Value stack[STACK_MAX];
  Value* stack_top;

  Object* objects;
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
