#ifndef VALUE_H_
#define VALUE_H_

#include "common.h"

typedef double Value;

typedef struct {
  int capacity;
  int count;
  Value *values;
} ValueArray;

void value_array__init(ValueArray *array);
void value_array__append(ValueArray *array, Value value);
void value_array__dispose(ValueArray *array);
void value__print(Value value);

#endif // VALUE_H_
