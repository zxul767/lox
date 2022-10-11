#include "vm.h"
#include "common.h"
#include "debug.h"
#include <stdio.h>

static void reset_stack(VM *vm) { vm->stack_top = vm->stack; }

void vm__init(VM *vm) { reset_stack(vm); }

void vm__dispose(VM *vm) {}

static void dump_stack(const VM *vm) {
  printf("          ");
  for (const Value *slot = vm->stack; slot < vm->stack_top; slot++) {
    printf("[ ");
    value__print(*slot);
    printf(" ]");
  }
  printf("\n");
}

static InterpretResult run(VM *vm) {
#define READ_BYTE() (*vm->instruction_pointer++)
#define READ_CONSTANT() (vm->bytecode->constants.values[READ_BYTE()])

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    dump_stack(vm);
    bytecode__disassemble_instruction(
        vm->bytecode,
        (int)(vm->instruction_pointer - vm->bytecode->instructions));
#endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      vm__push(constant, vm);
      printf("\n");
      break;
    }
    case OP_NEGATE: {
      vm__push(-vm__pop(vm), vm);
      break;
    }
    case OP_RETURN: {
      value__print(vm__pop(vm));
      printf("\n");
      return INTERPRET_OK;
    }
    }
  }
#undef READ_BYTE
#undef READ_CONSTANT
}

InterpretResult vm__interpret(const Bytecode *code, VM *vm) {
  vm->bytecode = code;
  vm->instruction_pointer = vm->bytecode->instructions;
  return run(vm);
}

void vm__push(Value value, VM *vm) {
  *(vm->stack_top) = value;
  vm->stack_top++;
}

Value vm__pop(VM *vm) {
  vm->stack_top--;
  return *(vm->stack_top);
}
