#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

// returns the elapsed time since the program started running, in fractional
// seconds
static Value clock_native(int args_count, Value* args)
{
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

inline int callframe__current_offset(const CallFrame* frame)
{
  return frame->instruction_pointer -
         frame->closure->function->bytecode.instructions;
}

static inline CallFrame* top_frame(VM* vm)
{
  return &vm->frames[vm->frames_count - 1];
}

static inline CallFrame* push_frame(VM* vm)
{
  return &vm->frames[vm->frames_count++];
}

static inline void pop_frame(VM* vm) { vm->frames_count--; }

static inline void pop_frame_args(VM* vm, const CallFrame* frame)
{
  vm->value_stack_top = frame->slots;
}

static void reset_stacks(VM* vm)
{
  vm->value_stack_top = vm->value_stack;
  vm->frames_count = 0;
}

static void runtime_error(VM* vm, const char* format, ...)
{
  va_list args;
  va_start(args, format);
  fprintf(stderr, "Runtime Error: ");
  vfprintf(stderr, format, args);
  va_end(args);
  fputs("\n", stderr);

  debug__dump_stacktrace(vm);
  reset_stacks(vm);
}

static inline void push_value(Value value, VM* vm)
{
  *(vm->value_stack_top) = value;
  vm->value_stack_top++;
}

static inline Value pop_value(VM* vm)
{
  vm->value_stack_top--;
  return *(vm->value_stack_top);
}

static inline Value peek_value(int distance, const VM* vm)
{
  // we add -1 because `value_stack_top` actually points to the next free slot,
  // not to the last occupied slot
  return vm->value_stack_top[-1 - distance];
}

static void
define_native_function(const char* name, NativeFunction function, VM* vm)
{
  // this particular push/push, pop/pop pattern is used to ensure the native
  // function is reachable to the garbage collector (remember the value stack is
  // a root for GC), given that a GC cycle could be triggered by `table__set`
  // (i.e., via a table resizing)
  //
  // see https://craftinginterpreters.com/garbage-collection.html for details
  push_value(OBJECT_VAL(string__copy(name, (int)strlen(name), vm)), vm);
  push_value(OBJECT_VAL(native_function__new(function, vm)), vm);
  table__set(
      &vm->global_vars, AS_STRING(vm->value_stack[0]), vm->value_stack[1]);
  pop_value(vm);
  pop_value(vm);
}

void vm__init(VM* vm)
{
  vm->objects = NULL;
  vm->execution_mode = VM_SCRIPT_MODE;
  vm->trace_execution = false;
  vm->show_bytecode = false;
  reset_stacks(vm);
  table__init(&vm->interned_strings);
  table__init(&vm->global_vars);

  define_native_function("clock", clock_native, vm);
}

void vm__dispose(VM* vm)
{
  table__dispose(&vm->interned_strings);
  table__dispose(&vm->global_vars);

  size_t count = memory__free_objects(vm->objects);

#ifdef DEBUG_TRACE_EXECUTION
  if (vm->trace_execution) {
    fprintf(stderr, "GC: freed %zu heap-allocated objects\n", count);
  }
#endif
}

static bool validate_call_errors(ObjectClosure* closure, int args_count, VM* vm)
{
  if (args_count != closure->function->arity) {
    runtime_error(
        vm, "Expected %d argument%s but got %d", closure->function->arity,
        closure->function->arity == 1 ? "" : "s", args_count);
    return false;
  }
  if (vm->frames_count == FRAMES_MAX) {
    runtime_error(vm, "Stack overflow!");
    return false;
  }
  return true;
}

static bool call(ObjectClosure* closure, int args_count, VM* vm)
{
  if (!validate_call_errors(closure, args_count, vm))
    return false;

  CallFrame* frame = push_frame(vm);
  frame->closure = closure;
  frame->instruction_pointer = closure->function->bytecode.instructions;
  // -1 because the function is right below the arguments on the value_stack
  frame->slots = vm->value_stack_top - args_count - 1;

  return true;
}

static bool call_value(Value callee, int args_count, VM* vm)
{
  if (IS_OBJECT(callee)) {
    switch (OBJECT_TYPE(callee)) {
    case OBJECT_CLOSURE: {
#ifdef DEBUG_TRACE_EXECUTION
      if (vm->trace_execution) {
        debug__print_callframe_divider();
      }
#endif
      return call(AS_CLOSURE(callee), args_count, vm);
    }
    case OBJECT_NATIVE_FUNCTION: {
      NativeFunction native = AS_NATIVE_FUNCTION(callee);
      Value result = native(args_count, vm->value_stack_top - args_count);
      vm->value_stack_top -= args_count + 1;
      push_value(result, vm);
      return true;
    }
    default:
      break; // non-callable object type
    }
  }
  runtime_error(vm, "Can only call functions and classes.");
  return false;
}

// Lox follows Ruby in that `nil` and `false` are falsey and every other
// value behaves like `true` (this may throw off users from other languages
// who may be used to having `0` behave like `false`, e.g., C/C++ users)
static bool is_falsey(Value value)
{
  return IS_NIL(value) || (IS_BOOL(value) && !AS_BOOL(value));
}

// concatenates the two object strings on top of the stack and pushes the result
// back onto it
static void concatenate(VM* vm)
{
  ObjectString* b = AS_STRING(pop_value(vm));
  ObjectString* a = AS_STRING(pop_value(vm));

  int length = a->length + b->length;
  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, a->chars, a->length);
  memcpy(chars + a->length, b->chars, b->length);
  chars[length] = '\0';

  ObjectString* result = string__take_ownership(chars, length, vm);

  push_value(OBJECT_VAL(result), vm);
}

static ObjectUpvalue* capture_upvalue(Value* local, VM* vm)
{
  ObjectUpvalue* upvalue = upvalue__new(local, vm);
  return upvalue;
}

static InterpretResult run(VM* vm)
{
  CallFrame* frame = top_frame(vm);

#define READ_BYTE() (*frame->instruction_pointer++)

#define READ_SHORT()                                                           \
  (frame->instruction_pointer += 2,                                            \
   ((frame->instruction_pointer[-2] << 8) | (frame->instruction_pointer[-1])))

#define READ_CONSTANT()                                                        \
  (frame->closure->function->bytecode.constants.values[READ_BYTE()])

#define READ_STRING() AS_STRING(READ_CONSTANT())

#define BINARY_OP(value_type, op)                                              \
  do {                                                                         \
    if (!IS_NUMBER(peek_value(0, vm)) || !IS_NUMBER(peek_value(1, vm))) {      \
      runtime_error(vm, "Operands must be numbers.");                          \
      return INTERPRET_RUNTIME_ERROR;                                          \
    }                                                                          \
    double b = AS_NUMBER(pop_value(vm));                                       \
    double a = AS_NUMBER(pop_value(vm));                                       \
    push_value(value_type(a op b), vm);                                        \
  } while (false)

#ifdef DEBUG_TRACE_EXECUTION
  if (vm->trace_execution) {
    fprintf(stderr, "TRACED EXECUTION\n");
    debug__print_section_divider();
  }
#endif

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    if (vm->trace_execution) {
      debug__dump_value_stack(vm);
      debug__disassemble_instruction(
          &frame->closure->function->bytecode,
          callframe__current_offset(frame));
      fprintf(stderr, "\n");
    }
#endif

    uint8_t instruction;
    switch (instruction = READ_BYTE()) {
    case OP_LOAD_CONSTANT: {
      Value constant = READ_CONSTANT();
      push_value(constant, vm);
      break;
    }
    case OP_NIL:
      push_value(NIL_VAL, vm);
      break;
    case OP_TRUE:
      push_value(BOOL_VAL(true), vm);
      break;
    case OP_FALSE:
      push_value(BOOL_VAL(false), vm);
      break;
    case OP_POP:
      pop_value(vm);
      break;
    case OP_GET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      push_value(*frame->closure->upvalues[slot]->location, vm);
      break;
    }
    case OP_GET_LOCAL: {
      uint8_t slot = READ_BYTE();
      push_value(frame->slots[slot], vm);
      break;
    }
    case OP_SET_LOCAL: {
      uint8_t slot = READ_BYTE();
      frame->slots[slot] = peek_value(0, vm);
      break;
    }
    case OP_SET_UPVALUE: {
      uint8_t slot = READ_BYTE();
      *frame->closure->upvalues[slot]->location = peek_value(0, vm);
      break;
    }
    case OP_GET_GLOBAL: {
      ObjectString* name = READ_STRING();
      Value value;
      if (!table__get(&vm->global_vars, name, &value)) {
        runtime_error(vm, "Undefined variable '%s'", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      push_value(value, vm);
      break;
    }
    case OP_DEFINE_GLOBAL: {
      ObjectString* name = READ_STRING();
      table__set(&vm->global_vars, name, peek_value(0, vm));
      pop_value(vm);
      break;
    }
    case OP_SET_GLOBAL: {
      ObjectString* name = READ_STRING();
      if (table__set(&vm->global_vars, name, peek_value(0, vm))) {
        // `table__set` returns `true` when assigning for the first time, which
        // means the variable hadn't been declared
        table__delete(&vm->global_vars, name);
        runtime_error(vm, "Undefined variable '%s'", name->chars);
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    // binary operations
    case OP_EQUAL: {
      Value b = pop_value(vm);
      Value a = pop_value(vm);
      push_value(BOOL_VAL(value__equals(a, b)), vm);
      break;
    }
    case OP_GREATER:
      BINARY_OP(BOOL_VAL, >);
      break;
    case OP_LESS:
      BINARY_OP(BOOL_VAL, <);
      break;
    case OP_ADD: {
      if (IS_STRING(peek_value(0, vm)) && IS_STRING(peek_value(1, vm))) {
        concatenate(vm);
      } else if (IS_NUMBER(peek_value(0, vm)) && IS_NUMBER(peek_value(1, vm))) {
        double b = AS_NUMBER(pop_value(vm));
        double a = AS_NUMBER(pop_value(vm));
        push_value(NUMBER_VAL(a + b), vm);
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
      push_value(BOOL_VAL(is_falsey(pop_value(vm))), vm);
      break;

    case OP_NEGATE: {
      if (!IS_NUMBER(peek_value(0, vm))) {
        runtime_error(vm, "Operand must be a number.");
        return INTERPRET_RUNTIME_ERROR;
      }
      push_value(NUMBER_VAL(-AS_NUMBER(pop_value(vm))), vm);
      break;
    }
    case OP_PRINT: {
      value__println(pop_value(vm));
      break;
    }
    case OP_JUMP_IF_FALSE: {
      uint16_t jump_length = READ_SHORT();
      if (is_falsey(peek_value(0, vm))) {
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
    case OP_CALL: {
      int args_count = READ_BYTE();
      // `call_value` just prepares everything to enter the context of the
      // invoked callable
      if (!call_value(peek_value(args_count, vm), args_count, vm)) {
        return INTERPRET_RUNTIME_ERROR;
      }
      frame = top_frame(vm);
      break;
    }
    case OP_CLOSURE: {
      ObjectFunction* function = AS_FUNCTION(READ_CONSTANT());
      ObjectClosure* closure = closure__new(function, vm);
      push_value(OBJECT_VAL(closure), vm);
      for (int i = 0; i < closure->upvalues_count; i++) {
        uint8_t is_local = READ_BYTE();
        uint8_t index = READ_BYTE();
        if (is_local) {
          closure->upvalues[i] = capture_upvalue(frame->slots + index, vm);
        } else {
          closure->upvalues[i] = frame->closure->upvalues[index];
        }
      }
      break;
    }
    case OP_RETURN: {
      Value result = pop_value(vm);
      pop_frame(vm);

      if (vm->frames_count == 0) {
#ifdef DEBUG_TRACE_EXECUTION
        if (vm->trace_execution) {
          debug__print_section_divider();
        }
#endif
        // pop_value the sentinel top-level function wrapper
        pop_value(vm);
        assert(vm->value_stack_top == vm->value_stack);

        return INTERPRET_OK;
      }

      pop_frame_args(vm, frame);
      // make the result available to the caller function
      push_value(result, vm);
      // return control to the caller function
      frame = top_frame(vm);
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

InterpretResult vm__interpret(const char* source, VM* vm)
{
  // we pass `vm` so we can track all heap-allocated
  // objects and dispose them when the VM shuts down
  ObjectFunction* top_level_wrapper = compiler__compile(source, vm);
  if (top_level_wrapper == NULL) {
    return INTERPRET_COMPILE_ERROR;
  }

  // TODO: can we make abstract and hoist this pattern which occurs every time
  // there's a risk of a garbage collection that will reclaim unreachable
  // objects that are in the process of being created? (temporarily pushing them
  // onto the value stack ensures they're reachable)
  //
  push_value(OBJECT_VAL(top_level_wrapper), vm);
  ObjectClosure* closure = closure__new(top_level_wrapper, vm);
  pop_value(vm);
  push_value(OBJECT_VAL(closure), vm);
  call(closure, 0, vm);

  InterpretResult result = run(vm);

  return result;
}
