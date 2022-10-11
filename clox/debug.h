#ifndef DEBUG_H_
#define DEBUG_H_

#include "bytecode.h"

void bytecode__disassemble(Bytecode *code, const char *name);
int bytecode__disassemble_instruction(Bytecode *code, int offset);

#endif // DEBUG_H_
