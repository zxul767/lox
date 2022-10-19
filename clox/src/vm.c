#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

static void reset_stack(VM* vm) { vm->stack_top = vm->stack; }

static void runtime_error(VM* vm, const char* format, ...) {
  va_list args;
  va_start(args, format);
  fprintf(stderr, "Runtime Error: ");
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  const Bytecode* bc = vm->bytecode;

  size_t instruction_index = vm->instruction_pointer - bc->instructions - 1;
  int line = vm->bytecode->source_lines[instruction_index];
  fprintf(stderr, "[line %d] in script\n", line);

  reset_stack(vm);
}

void vm__init(VM* vm) {
  reset_stack(vm);
  vm->objects = NULL;
  table__init(&vm->interned_strings);
}

void vm__dispose(VM* vm) {
  table__dispose(&vm->interned_strings);
  memory__free_objects(vm->objects);
}

void vm__push(Value value, VM* vm) {
  *(vm->stack_top) = value;
  vm->stack_top++;
}

Value vm__pop(VM* vm) {
  vm->stack_top--;
  return *(vm->stack_top);
}

static Value peek(int distance, const VM* vm) {
  // we add -1 because `stack_top` actually points to the next free slot,
  // not to the last occupied slot
  return vm->stack_top[-1 - distance];
}

// Lox follows Ruby in that `nil` and `false` are falsey and every other
// value behaves like `true` (this may throw off users from other languages
// who may be used to having `0` behave like `false`, e.g., C/C++ users)
static bool is_falsey(Value value) {
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

static void concatenate(VM* vm) {
  ObjectString* b = AS_STRING(vm__pop(vm));
  ObjectString* a = AS_STRING(vm__pop(vm));

  int length = a->length + b->length;
  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjectString* result = string__take_ownership(chars, length, vm);

  vm__push(OBJECT_VAL(result), vm);
}

static InterpretResult run(VM* vm) {

#define READ_BYTE() (*vm->instruction_pointer++)
#define READ_CONSTANT() (vm->bytecode->constants.values[READ_BYTE()])
#define BINARY_OP(value_type, op)                                              \
  do {                                                                         \
    if (!IS_NUMBER(peek(0, vm)) || !IS_NUMBER(peek(1, vm))) {                  \
      runtime_error(vm, "Operands must be numbers.");                          \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(vm__pop(vm));                                         \
    double a = AS_NUMBER(vm__pop(vm));                                         \
    vm__push(value_type(a op b), vm);                                          \
  } while (false)

#ifdef DEBUG_TRACE_EXECUTION
  printf("TRACED EXECUTION\n");
  debug__print_section_divider();
#endif

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    debug__dump_stack(vm);
    debug__disassemble_instruction(
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
    case OP_NIL:
      vm__push(NIL_VAL, vm);
      break;
    case OP_TRUE:
      vm__push(BOOL_VAL(true), vm);
      break;
    case OP_FALSE:
      vm__push(BOOL_VAL(false), vm);
      break;
    // binary operations
    case OP_EQUAL: {
      Value b = vm__pop(vm);
      Value a = vm__pop(vm);
      vm__push(BOOL_VAL(value__equals(a, b)), vm);
      break;
    }
    case OP_GREATER:
      BINARY_OP(BOOL_VAL, >);
      break;
    case OP_LESS:
      BINARY_OP(BOOL_VAL, <);
      break;
    case OP_ADD: {
      if (IS_STRING(peek(0, vm)) && IS_STRING(peek(1, vm))) {
        concatenate(vm);
      } else if (IS_NUMBER(peek(0, vm)) && IS_NUMBER(peek(1, vm))) {
        double b = AS_NUMBER(vm__pop(vm));
        double a = AS_NUMBER(vm__pop(vm));
        vm__push(NUMBER_VAL(a + b), vm);
      } else {
        runtime_error(vm, "Operands must be two numbers or two strings.");
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_SUBTRACT:
      BINARY_OP(NUMBER_VAL, -);
      break;
    case OP_MULTIPLY:
      BINARY_OP(NUMBER_VAL, *);
      break;
    case OP_DIVIDE:
      BINARY_OP(NUMBER_VAL, /);
      break;

      // unary operations
    case OP_NOT:
      vm__push(BOOL_VAL(is_falsey(vm__pop(vm))), vm);
      break;

    case OP_NEGATE: {
      if (!IS_NUMBER(peek(0, vm))) {
        runtime_error(vm, "Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }
      vm__push(NUMBER_VAL(-AS_NUMBER(vm__pop(vm))), vm);
      break;
    }
    case OP_PRINT: {
      printf(">> ");
      value__println(vm__pop(vm));
      break;
    }
    case OP_RETURN: {
      debug__print_section_divider();
      return INTERPRET_OK;
    }
    }
  }
#undef READ_BYTE
#undef READ_CONSTANT
#undef BINARY_OP
}

InterpretResult vm__interpret(const char* source, VM* vm) {
  Bytecode bytecode;
  bytecode__init(&bytecode);

  // we pass `vm` so we can track all heap-allocated
  // objects and dispose them when the VM shuts down
  if (!compiler__compile(source, &bytecode, vm)) {
    bytecode__dispose(&bytecode);
    return INTERPRET_COMPILE_ERROR;
  }

  // bytecode execution
  vm->bytecode = &bytecode;
  vm->instruction_pointer = vm->bytecode->instructions;
  InterpretResult result = run(vm);

  bytecode__dispose(&bytecode);

  return result;
}