#ifndef COMPILER_H_
#define COMPILER_H_

#include "common.h"

typedef struct VM VM;
typedef struct Bytecode Bytecode;
typedef struct ObjectFunction ObjectFunction;

// compiles `source` and returns a function object (a program is always wrapped
// in a sentinel function so we don't have to special case compilation of
// top-level statements.)
//
// post-condition: `vm->objects` points to the linked list of objects
// allocated during compilation (e.g., literal strings)
//
ObjectFunction* compiler__compile(const char* source, VM* vm);

#endif // COMPILER_H_
