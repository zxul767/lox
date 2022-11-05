#include <stdarg.h>
#include <stdio.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

static CallFrame* current_frame(VM* vm) {
  return &vm->frames[vm->frames_count - 1];
}

static CallFrame* next_available_frame(VM* vm) {
  return &vm->frames[vm->frames_count++];
}

static void reset_stack(VM* vm) {
  vm->stack_top = vm->stack;
  vm->frames_count = 0;
}

static int get_current_source_line(VM* vm) {
  const CallFrame* frame = current_frame(vm);
  const Bytecode* bytecode = &(frame->function->bytecode);

  // -1 takes into account that the instruction pointer is one instruction past
  // where the actual error happened
  size_t offset = frame->instruction_pointer - bytecode->instructions - 1;
  return bytecode->source_lines[offset];
}

static void runtime_error(VM* vm, const char* format, ...) {
  va_list args;
  va_start(args, format);
  fprintf(stderr, "Runtime Error: ");
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  int line = get_current_source_line(vm);
  fprintf(stderr, "[line %d] in script\n", line);

  reset_stack(vm);
}

static void
callframe__init(CallFrame* frame, ObjectFunction* function, VM* vm) {
  frame->function = function;
  frame->instruction_pointer = function->bytecode.instructions;
  frame->slots = vm->stack;
}

void vm__init(VM* vm) {
  reset_stack(vm);
  vm->objects = NULL;
  vm->mode = VM_SCRIPT_MODE; // default value
  table__init(&vm->interned_strings);
  table__init(&vm->global_vars);
}

void vm__dispose(VM* vm) {
  table__dispose(&vm->interned_strings);
  table__dispose(&vm->global_vars);
  memory__free_objects(vm->objects);
}

void push(Value value, VM* vm) {
  *(vm->stack_top) = value;
  vm->stack_top++;
}

Value pop(VM* vm) {
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
  ObjectString* b = AS_STRING(pop(vm));
  ObjectString* a = AS_STRING(pop(vm));

  int length = a->length + b->length;
  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjectString* result = string__take_ownership(chars, length, vm);

  push(OBJECT_VAL(result), vm);
}

static InterpretResult run(VM* vm) {
  CallFrame* frame = current_frame(vm);

#define READ_BYTE() (*frame->instruction_pointer++)

#define READ_SHORT()                                                           \
  (frame->instruction_pointer += 2,                                            \
   ((frame->instruction_pointer[-2] << 8) | (frame->instruction_pointer[-1])))

#define READ_CONSTANT()                                                        \
  (frame->function->bytecode.constants.values[READ_BYTE()])

#define READ_STRING() AS_STRING(READ_CONSTANT())

#define BINARY_OP(value_type, op)                                              \
  do {                                                                         \
    if (!IS_NUMBER(peek(0, vm)) || !IS_NUMBER(peek(1, vm))) {                  \
      runtime_error(vm, "Operands must be numbers.");                          \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(pop(vm));                                             \
    double a = AS_NUMBER(pop(vm));                                             \
    push(value_type(a op b), vm);                                              \
  } while (false)

#ifdef DEBUG_TRACE_EXECUTION
  printf("TRACED EXECUTION\n");
  debug__print_section_divider();
#endif

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    debug__dump_stack(vm);
    Bytecode* code = &frame->function->bytecode;
    int offset = (int)(frame->instruction_pointer - code->instructions);
    debug__disassemble_instruction(code, offset);
#endif

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_CONSTANT: {
      Value constant = READ_CONSTANT();
      push(constant, vm);
      break;
    }
    case OP_NIL:
      push(NIL_VAL, vm);
      break;
    case OP_TRUE:
      push(BOOL_VAL(true), vm);
      break;
    case OP_FALSE:
      push(BOOL_VAL(false), vm);
      break;
    case OP_POP:
      pop(vm);
      break;
    case OP_GET_LOCAL: {
      uint8_t slot = READ_BYTE();
      push(frame->slots[slot], vm);
      break;
    }
    case OP_SET_LOCAL: {
      uint8_t slot = READ_BYTE();
      frame->slots[slot] = peek(0, vm);
      break;
    }
    case OP_GET_GLOBAL: {
      ObjectString* name = READ_STRING();
      Value value;
      if (!table__get(&vm->global_vars, name, &value)) {
        runtime_error(vm, "Undefined variable '%s'", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      push(value, vm);
      break;
    }
    case OP_DEFINE_GLOBAL: {
      ObjectString* name = READ_STRING();
      table__set(&vm->global_vars, name, peek(0, vm));
      pop(vm);
      break;
    }
    case OP_SET_GLOBAL: {
      ObjectString* name = READ_STRING();
      if (table__set(&vm->global_vars, name, peek(0, vm))) {
        // table__set returns true when assigning for the first time, which
        // means the variable hadn't been declared
        table__delete(&vm->global_vars, name);
        runtime_error(vm, "Undefined variable '%s'", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    // binary operations
    case OP_EQUAL: {
      Value b = pop(vm);
      Value a = pop(vm);
      push(BOOL_VAL(value__equals(a, b)), vm);
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
        double b = AS_NUMBER(pop(vm));
        double a = AS_NUMBER(pop(vm));
        push(NUMBER_VAL(a + b), vm);
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
      push(BOOL_VAL(is_falsey(pop(vm))), vm);
      break;

    case OP_NEGATE: {
      if (!IS_NUMBER(peek(0, vm))) {
        runtime_error(vm, "Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }
      push(NUMBER_VAL(-AS_NUMBER(pop(vm))), vm);
      break;
    }
    case OP_PRINT: {
      value__println(pop(vm));
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t jump_length = READ_SHORT();
      if (is_falsey(peek(0, vm))) {
        frame->instruction_pointer += jump_length;
      }
      break;
    }
    case OP_JUMP: {
      uint16_t jump_length = READ_SHORT();
      frame->instruction_pointer += jump_length;
      break;
    }
    case OP_LOOP: {
      uint16_t jump_length = READ_SHORT();
      frame->instruction_pointer -= jump_length;
      break;
    }
    case OP_RETURN: {
#ifdef DEBUG_TRACE_EXECUTION
      debug__print_section_divider();
#endif
      vm->frames_count--;
      if (vm->frames_count == 0) {
        // pop the sentinel top-level function
        pop(vm);
        return INTERPRET_OK;
      }
      break;
    }
    }
  }
#undef READ_BYTE
#undef READ_SHORT
#undef READ_CONSTANT
#undef READ_STRING
#undef BINARY_OP
}

InterpretResult vm__interpret(const char* source, VM* vm) {
  // we pass `vm` so we can track all heap-allocated
  // objects and dispose them when the VM shuts down
  ObjectFunction* function = compiler__compile(source, vm);
  if (function == NULL) {
    return INTERPRET_COMPILE_ERROR;
  }

  push(OBJECT_VAL(function), vm);

  CallFrame* frame = next_available_frame(vm);
  callframe__init(frame, function, vm);

  InterpretResult result = run(vm);

  return result;
}
