#include <time.h>

#include "lox_stdlib.h"

// TODO: check function arity against arguments passed in every function

// returns the elapsed time since the program started running, in fractional
// seconds
Value clock_native(int args_count, Value* args)
{
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

Value print(int args_count, Value* args)
{
  value__print(args[0]);
  return NIL_VAL;
}

Value println(int args_count, Value* args)
{
  value__println(args[0]);
  return NIL_VAL;
}
