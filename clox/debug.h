#ifndef DEBUG_H_
#define DEBUG_H_

#include "bytecode.h"
#include "vm.h"

void bytecode__disassemble(const Bytecode *code, const char *name);
int bytecode__disassemble_instruction(const Bytecode *code, int offset);
void vm__dump_stack(const VM *vm);

#endif // DEBUG_H_
