#ifndef LOX_STDLIB_H_
#define LOX_STDLIB_H_

#include "value.h"
typedef struct VM VM;

Value print(int args_count, Value* args, VM* vm);
Value println(int args_count, Value* args, VM* vm);
Value clock_native(int args_count, Value* args, VM* vm);
Value help(int args_count, Value* args, VM* vm);

#endif // LOX_STDLIB_H_
