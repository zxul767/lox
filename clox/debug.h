#ifndef DEBUG_H_
#define DEBUG_H_

#include "bytecode.h"
#include "vm.h"

void debug__disassemble(const Bytecode *code, const char *name);
int debug__disassemble_instruction(const Bytecode *code, int offset);
void debug__dump_stack(const VM *vm);

void debug__print_section_divider();

#endif // DEBUG_H_
