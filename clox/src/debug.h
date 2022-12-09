#ifndef DEBUG_H_
#define DEBUG_H_

#include "bytecode.h"
#include "table.h"
#include "vm.h"

void debug__disassemble(const Bytecode* code, const char* name);
int debug__disassemble_instruction(const Bytecode* code, int offset);
void debug__dump_value_stack(const VM* vm, const Value* from);
// dump the current call frames stack trace
void debug__dump_stacktrace(const VM* vm);
void debug__show_callframe_names(const VM* vm);

void debug__print_section_divider();
void debug__print_callframe_divider();

void debug__show_entries(const Table* table);

#endif // DEBUG_H_
