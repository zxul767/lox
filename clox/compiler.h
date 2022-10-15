#ifndef COMPILER_H_
#define COMPILER_H_

#include "common.h"

typedef struct VM VM;
typedef struct Bytecode Bytecode;

// compiles `source` and writes the result in `bytecode`
//
// post-condition: `vm->objects` points to the linked list of objects
// allocated during compilation (e.g., literal strings)
//
bool compiler__compile(const char *source, Bytecode *bytecode, VM *vm);

#endif // COMPILER_H_
