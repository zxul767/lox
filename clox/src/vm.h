#ifndef VM_H_
#define VM_H_

#include "table.h"
#include "value.h"

typedef struct Bytecode Bytecode;
typedef struct ObjectFunction ObjectFunction;

// we need this information to compile a few things differently when running
// inside a REPL (e.g., statement expressions are expected to print their value
// automatically).
//
// this is our only option given that we don't have access to an intermediate
// representation that we could modify after parsing in the REPL call frame (the
// way we do in jlox)
//
typedef enum {
  VM_REPL_MODE,
  VM_SCRIPT_MODE,

} ExecutionMode;

#define FRAMES_MAX 64
#define STACK_MAX (FRAMES_MAX * UINT8_COUNT)

typedef struct CallFrame {
  ObjectFunction* function;
  uint8_t* instruction_pointer;
  // slots is a "window" onto the values stack of the VM which contains
  // all local information for the function call (i.e., arguments and local
  // variables)
  //
  // this makes the implementation of function calls quite simple and efficient
  // see https://craftinginterpreters.com/calls-and-functions.html for details
  Value* slots;

} CallFrame;

// stack-based, virtual machine
typedef struct VM {
  CallFrame frames[FRAMES_MAX];
  int frames_count;

  // this stack holds all local variables and temporaries in the whole stack of
  // call frames.
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

  ExecutionMode mode;

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
