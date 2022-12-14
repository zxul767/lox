#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>
#include <time.h>

#include "common.h"
#include "compiler.h"
#include "debug.h"
#include "lox_list.h"
#include "lox_stdlib.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

inline int callframe__current_offset(const CallFrame* frame)
{
  return frame->instruction_pointer -
         frame->closure->function->bytecode.instructions;
}

static inline CallFrame* top_frame(VM* vm)
{
  return &vm->frames[vm->frames_count - 1];
}

static void push_new_frame(ObjectClosure* closure, int args_count, VM* vm)
{
  CallFrame* frame = &vm->frames[vm->frames_count++];
  frame->closure = closure;
  frame->instruction_pointer = closure->function->bytecode.instructions;
  // vm value stack:
  // [<script>]...[ some-value ][ function ][ arg1 ]...[ arg k ][ . ]
  //                                 ^                            ^
  //                                 |                            |
  //                            frame->slots            vm->stack_free_slot
  // where k = args_count
  frame->slots = vm->stack_free_slot - (args_count + 1);
}

static inline void pop_frame(VM* vm) { vm->frames_count--; }

static inline void discard_frame_window(VM* vm, const CallFrame* frame)
{
  vm->stack_free_slot = frame->slots;
  // after a call is done, `vm->stack_free_slot` is pointing back at where the
  // callee was (and still is, since we don't bother to "erase" it or its
  // arguments):
  //
  // [<script>]...[ some-value ][ function ][ arg1 ]...[ arg k ]...
  //                                 ^
  //                                 |
  //                          vm->stack_free_slot
}

static void reset_for_execution(VM* vm)
{
  vm->stack_free_slot = vm->value_stack;
  vm->frames_count = 0;
  vm->open_upvalues = NULL;
  vm->current_fn_compiler = NULL;
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
  reset_for_execution(vm);
}

static inline void push_value(Value value, VM* vm)
{
  *(vm->stack_free_slot) = value;
  vm->stack_free_slot++;
}

void vm__push(Value value, VM* vm) { push_value(value, vm); }

static inline Value pop_value(VM* vm)
{
  vm->stack_free_slot--;
  return *(vm->stack_free_slot);
}

void vm__pop(VM* vm) { pop_value(vm); }

static inline Value peek_value(int distance, const VM* vm)
{
  // -1 because `stack_free_slot` always points right after the top of the stack
  return vm->stack_free_slot[-1 - distance];
}

Value vm__peek(int distance, VM* vm) { return peek_value(distance, vm); }

static void define_native_class(const char* name, ObjectClass* _class, VM* vm)
{
  WITH_OBJECTS_NURSERY(vm, {
    ObjectString* class_name = string__copy(name, (int)strlen(name), vm);
    table__set(&vm->global_vars, class_name, OBJECT_VAL(_class));
  });
}

static void define_native_function(
    const char* name, int arity, NativeFunction function, VM* vm)
{
  WITH_OBJECTS_NURSERY(vm, {
    ObjectString* function_name = string__copy(name, (int)strlen(name), vm);
    table__set(
        &vm->global_vars, function_name,
        OBJECT_VAL(native_function__new(function, function_name, arity, vm)));
  });
}

// whenever `vm->objects` is reset (e.g., in `memory.c::sweep`),
// `vm->object_nursery_start` needs to reset to the same value so as to keep the
// invariant that the nursery must alway be a prefix of the objects list.
void vm__reset_objects_list_head(VM* vm, Object* start)
{
  vm->objects = start;
  vm->object_nursery_start = start;
  // NOTE: `vm->object_nursery_nested_scopes` is not reset because that would
  // cause trouble when `memory__close_object_nursery` is called to close open
  // "nursery scopes"
}

void vm__init(VM* vm)
{
  vm->execution_mode = VM_SCRIPT_MODE;
  vm->trace_execution = false;
  vm->show_bytecode = false;

  vm__reset_objects_list_head(vm, NULL);
  vm->object_nursery_nested_scopes = 0;

  reset_for_execution(vm);
  table__init(&vm->interned_strings);
  table__init(&vm->global_vars);

  // `string__copy` can indirectly trigger a GC run, so let's make sure
  // that the collector will ignore `vm->init_string` until we actually
  // have a value on it
  vm->init_string = NULL;
  vm->init_string = string__copy("__init__", strlen("__init__"), vm);

  // standard library
  define_native_function("clock", 0, clock_native, vm);
  define_native_function("print", 1, print, vm);
  define_native_function("println", 1, println, vm);

  WITH_OBJECTS_NURSERY(vm, {
    define_native_class("list", lox_list__new_class("list", vm), vm);
  });
}

void vm__dispose(VM* vm)
{
  table__dispose(&vm->interned_strings);
  table__dispose(&vm->global_vars);
  // we don't need to explicitly dispose it, since it's tracked by `vm->objects`
  vm->init_string = NULL;
  vm->object_nursery_start = NULL;
  vm->object_nursery_nested_scopes = 0;

  size_t count = memory__free_objects(vm->objects);

#ifdef DEBUG_TRACE_EXECUTION
  if (vm->trace_execution) {
    fprintf(stderr, "GC: freed %zu heap-allocated objects\n", count);
  }
#endif
}

static bool is_valid_call(ObjectCallable* callable, int args_count, VM* vm)
{
  if (args_count != callable->arity) {
    runtime_error(
        vm, "Expected %d argument%s but got %d", callable->arity,
        callable->arity == 1 ? "" : "s", args_count);
    return false;
  }
  if (vm->frames_count == FRAMES_MAX) {
    runtime_error(vm, "Stack overflow!");
    return false;
  }
  return true;
}

static bool call_closure(ObjectClosure* closure, int args_count, VM* vm)
{
  if (!is_valid_call(AS_CALLABLE(closure->function), args_count, vm))
    return false;

#ifdef DEBUG_TRACE_EXECUTION
  if (vm->trace_execution) {
    debug__print_callframe_divider();
    debug__show_callframe_names(vm);
  }
#endif

  push_new_frame(closure, args_count, vm);

  return true;
}

static bool call_native(ObjectNativeFunction* native, int args_count, VM* vm)
{
  if (!is_valid_call(AS_CALLABLE(native), args_count, vm))
    return false;

  // for regular native function calls:
  //
  //                                           native function frame
  // VM value stack:                           _________________
  //                                          /                 |
  // [<script>]...[ some-value ][ native-fn ][ arg1 ]...[ arg k ][ . ]
  //                                  ^                            ^
  //                                  |                            |
  //                                  |                 `vm->stack_free_slot`
  //                                  |
  //                       `vm->stack_free_slot` after call
  //
  // where k = args_count

  // for native method calls (e.g., for methods of the `list` class):
  //
  //                                 native method frame
  // VM value stack:              ___________________________
  //                             /                           |
  // [<script>]...[ some-value ][ `this` ][ arg1 ]...[ arg k ][ . ]
  //                                 ^                          ^
  //                                 |                          |
  //                                 |                   vm->stack_free_slot
  //                                 |
  //                     `vm->stack_free_slot` after call
  //
  // where k = args_count, and `this` is the method's instance

  int include_this_arg = native->is_method ? 1 : 0;
  Value result = native->function(
      args_count, vm->stack_free_slot - args_count - include_this_arg);

  // +1 to clobber the native function or instance with the call's result
  vm->stack_free_slot -= args_count + 1;
  push_value(result, vm);

  return true;
}

static bool call_value(Value callee, int args_count, VM* vm)
{
  if (IS_OBJECT(callee)) {
    switch (OBJECT_TYPE(callee)) {

    case OBJECT_BOUND_METHOD: {
      ObjectBoundMethod* bound = AS_BOUND_METHOD(callee);
      vm->stack_free_slot[-args_count - 1] = bound->instance;

      return call_value(bound->method, args_count, vm);
    }
    case OBJECT_CLASS: {
      ObjectClass* _class = AS_CLASS(callee);
      vm->stack_free_slot[-args_count - 1] =
          OBJECT_VAL(_class->new_instance(_class, vm));

      Value initializer;
      if (table__get(&_class->methods, vm->init_string, &initializer)) {
        return call_closure(AS_CLOSURE(initializer), args_count, vm);

      } else if (args_count != 0) {
        runtime_error(vm, "Expected 0 arguments but got %d.", args_count);
        return false;
      }
      return true;
    }
    case OBJECT_CLOSURE: {
      return call_closure(AS_CLOSURE(callee), args_count, vm);
    }
    case OBJECT_NATIVE_FUNCTION: {
      return call_native(AS_NATIVE_FUNCTION(callee), args_count, vm);
    }
    default:
      break; // non-callable object type
    }
  }
  runtime_error(vm, "Can only call functions and classes.");
  return false;
}

static bool bind_method(ObjectClass* _class, ObjectString* name, VM* vm)
{
  Value method;
  if (!table__get(&_class->methods, name, &method)) {
    runtime_error(vm, "Undefined property '%s'.", name->chars);
    return false;
  }
  ObjectBoundMethod* bound = bound_method__new(
      /* instance: */ peek_value(0, vm), method, vm);
  pop_value(vm); // we no longer need the instance around
  push_value(OBJECT_VAL(bound), vm);

  return true;
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
  ObjectString* b = AS_STRING(peek_value(0, vm));
  ObjectString* a = AS_STRING(peek_value(1, vm));

  WITH_OBJECTS_NURSERY(vm, {
    int length = a->length + b->length;
    char* result_chars = ALLOCATE(char, length + 1);
    memcpy(result_chars, a->chars, a->length);
    memcpy(result_chars + a->length, b->chars, b->length);
    result_chars[length] = '\0';

    pop_value(vm);
    pop_value(vm);
    ObjectString* result = string__take_ownership(result_chars, length, vm);
    push_value(OBJECT_VAL(result), vm);
  });
}

static ObjectUpvalue* capture_upvalue(Value* local, VM* vm)
{
  ObjectUpvalue* previous_upvalue = NULL;
  ObjectUpvalue* upvalue = vm->open_upvalues;
  while (upvalue != NULL && upvalue->location > local) {
    previous_upvalue = upvalue;
    upvalue = upvalue->next;
  }
  if (upvalue != NULL && upvalue->location == local) {
    return upvalue;
  }

  ObjectUpvalue* new_upvalue = upvalue__new(local, vm);
  new_upvalue->next = upvalue;

  if (previous_upvalue == NULL) {
    vm->open_upvalues = new_upvalue;
  } else {
    previous_upvalue->next = new_upvalue;
  }
  return new_upvalue;
}

static void close_upvalues(Value* last, VM* vm)
{
  // for closures whose lifetime will outlive that of its parent function, it is
  // necessary to make sure that non-local variables are migrated from the stack
  // to a more permanent place. that place is the "upvalues" arrays associated
  // with each closure. the simplest way to accomplish this is to store each
  // closed variable in the upvalue, and simply redirect the upvalue's location
  // to it, as follows:
  while (vm->open_upvalues != NULL && vm->open_upvalues->location >= last) {
    ObjectUpvalue* upvalue = vm->open_upvalues;
    upvalue->closed = *upvalue->location;
    upvalue->location = &upvalue->closed;
    vm->open_upvalues = upvalue->next;
  }
}

static void define_method(ObjectString* name, VM* vm)
{
  Value closure = peek_value(0, vm);
  ObjectClass* _class = AS_CLASS(peek_value(1, vm));
  table__set(&_class->methods, name, closure);
  pop_value(vm); // we no longer need the closure on the stack
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

  for (;;) {
#ifdef DEBUG_TRACE_EXECUTION
    if (vm->trace_execution) {
      debug__dump_value_stack(vm, /* from: */ frame->slots);
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
    case OP_GET_PROPERTY: {
      if (!IS_INSTANCE(peek_value(0, vm))) {
        runtime_error(vm, "Only instances have properties");
        return INTERPRET_RUNTIME_ERROR;
      }
      ObjectInstance* instance = AS_INSTANCE(peek_value(0, vm));
      ObjectString* name = READ_STRING();

      Value value;
      if (table__get(&instance->fields, name, &value)) {
        pop_value(vm); // instance
        push_value(value, vm);
        break;
      }

      if (!bind_method(instance->_class, name, vm)) {
        return INTERPRET_RUNTIME_ERROR;
      }
      break;
    }
    case OP_SET_PROPERTY: {
      ObjectInstance* instance = AS_INSTANCE(peek_value(1, vm));
      ObjectString* name = READ_STRING();
      table__set(&instance->fields, name, peek_value(0, vm));
      Value value = pop_value(vm); // pop the assigned value
      pop_value(vm);               // pop the instance
      // push the assigned value because assignments are expressions
      push_value(value, vm);
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
      // these two instructions are here for the exclusive benefit of the REPL
      // (as it is much more convenient to emit these simple instructions than
      // to prepare the equivalent `OP_CALL`)
      //
    case OP_PRINT: {
      Value value = pop_value(vm);
      // in Python tradition, by default we don't display `nil` in the REPL
      if (!IS_NIL(value)) {
        value__print(value);
      }
      break;
    }
    case OP_PRINTLN: {
      Value value = pop_value(vm);
      if (!IS_NIL(value)) {
        value__println(value);
      }
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
      // update local frame to last on call stack
      frame = top_frame(vm);

      break;
    }
    case OP_NEW_CLOSURE: {
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
    case OP_NEW_CLASS: {
      push_value(OBJECT_VAL(class__new(READ_STRING(), vm)), vm);
      break;
    }
    case OP_NEW_METHOD: {
      define_method(READ_STRING(), vm);
      break;
    }
    case OP_CLOSE_UPVALUE: {
      // "close" the single local variable on top of the stack
      close_upvalues(vm->stack_free_slot - 1, vm);
      // we can get rid of since it is no longer needed by anyone (keep in mind
      // that this operation only happens when a scope is "popped" so the only
      // remaining references could by capturing closures)
      pop_value(vm);
      break;
    }
    case OP_RETURN: {
      Value result = pop_value(vm);
      // before local values are "lost", let's "close" all local variables that
      // were captured by inner closures (remember that all locals start at
      // `frame->slots`)
      close_upvalues(frame->slots, vm);

      pop_frame(vm);
      if (vm->frames_count == 0) {
#ifdef DEBUG_TRACE_EXECUTION
        if (vm->trace_execution) {
          debug__print_section_divider();
        }
#endif
        // pop_value the sentinel top-level function wrapper
        pop_value(vm);
        // if there are no bugs, we should always end with an empty stack
        assert(vm->stack_free_slot == vm->value_stack);

        return INTERPRET_OK;
      }

      discard_frame_window(vm, frame);
      // make the result available to the caller function
      push_value(result, vm);
      // return control to the caller function
      frame = top_frame(vm);

#ifdef DEBUG_TRACE_EXECUTION
      // after a call returns, it's hard to know what was the caller, so
      // let's reprint that information
      if (vm->trace_execution) {
        debug__print_callframe_divider();
        debug__show_callframe_names(vm);
      }
#endif
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
  reset_for_execution(vm);

  // we pass `vm` so we can track all heap-allocated
  // objects and dispose them when the VM shuts down
  ObjectFunction* top_level_wrapper = compiler__compile(source, vm);
  if (top_level_wrapper == NULL) {
    return INTERPRET_COMPILE_ERROR;
  }

  // TODO: can we make abstract and hoist this pattern which occurs every time
  // there's a risk of a garbage collection that will reclaim unreachable
  // objects that are in the process of being created? (temporarily pushing
  // them onto the value stack ensures they're reachable)
  //
  push_value(OBJECT_VAL(top_level_wrapper), vm);
  ObjectClosure* closure = closure__new(top_level_wrapper, vm);
  pop_value(vm);

#ifdef DEBUG_TRACE_EXECUTION
  if (vm->trace_execution) {
    fprintf(stderr, "TRACED EXECUTION\n");
    debug__print_section_divider();
  }
#endif

  push_value(OBJECT_VAL(closure), vm);
  call_closure(closure, 0, vm);

  InterpretResult result = run(vm);

  return result;
}
