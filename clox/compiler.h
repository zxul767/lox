#ifndef COMPILER_H_
#define COMPILER_H_

#include "bytecode.h"

bool compiler__compile(const char *source, Bytecode *bytecode);

#endif // COMPILER_H_
