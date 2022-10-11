#include "vm.h"
#include "common.h"
#include "compiler.h"
#include "debug.h"
#include <stdio.h>

static void reset_stack(VM *vm) { vm->stack_top = vm->stack; }

void vm__init(VM *vm) { reset_stack(vm); }

void vm__dispose(VM *vm) {}

static InterpretResult run(VM *vm) {
#define READ_BYTE() (*vm->instruction_pointer++)
#define READ_CONSTANT() (vm->bytecode->constants.values[READ_BYTE()])
#define BINARY_OP(op)                                                          \
  do {                                                                         \
    double b = vm__pop(vm);                                                    \
    double a = vm__pop(vm);                                                    \
    vm__push(a op b, vm);                                                      \
  } while (false)

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    vm__dump_stack(vm);
    bytecode__disassemble_instruction(
        vm->bytecode,
        (int)(vm->instruction_pointer - vm->bytecode->instructions));
#endif
    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      vm__push(constant, vm);
      break;
    }
      // binary operations
    case OP_ADD:
      BINARY_OP(+);
      break;
    case OP_SUBTRACT:
      BINARY_OP(-);
      break;
    case OP_MULTIPLY:
      BINARY_OP(*);
      break;
    case OP_DIVIDE:
      BINARY_OP(/);
      break;

      // unary operations
    case OP_NEGATE: {
      vm__push(-vm__pop(vm), vm);
      break;
    }
    case OP_RETURN: {
      value__println(vm__pop(vm));
      return INTERPRET_OK;
    }
    }
  }
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult vm__interpret_bytecode(const Bytecode *code, VM *vm) {
  vm->bytecode = code;
  vm->instruction_pointer = vm->bytecode->instructions;
  return run(vm);
}

InterpretResult vm__interpret(const char *source, VM *vm) {
  compiler__compile(source, vm);
  return INTERPRET_OK;
}

void vm__push(Value value, VM *vm) {
  *(vm->stack_top) = value;
  vm->stack_top++;
}

Value vm__pop(VM *vm) {
  vm->stack_top--;
  return *(vm->stack_top);
}
