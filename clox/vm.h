#ifndef VM_H_
#define VM_H_

#include "bytecode.h"
#include "value.h"

#define STACK_MAX 256

// Virtual Machine for Lox
typedef struct {
  const Bytecode *bytecode;
  uint8_t *instruction_pointer;
  Value stack[STACK_MAX];
  Value *stack_top;
} VM;

typedef enum {
  INTERPRET_OK,
  INTERPRET_COMPILE_ERROR,
  INTERPRET_RUNTIME_ERROR,
} InterpretResult;

void vm__init(VM *vm);
void vm__dispose(VM *vm);

InterpretResult vm__interpret_bytecode(const Bytecode *code, VM *vm);
InterpretResult vm__interpret(const char *source, VM *vm);

void vm__push(Value value, VM *vm);
Value vm__pop(VM *vm);

#endif // VM_H_
