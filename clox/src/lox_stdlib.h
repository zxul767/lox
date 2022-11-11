#ifndef LOX_STDLIB_H_
#define LOX_STDLIB_H_

#include "value.h"

Value print(int args_count, Value* args);
Value println(int args_count, Value* args);
Value clock_native(int args_count, Value* args);

#endif // LOX_STDLIB_H_
