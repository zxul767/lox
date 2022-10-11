#include "value.h"
#include "memory.h"
#include <stdio.h>

void value_array__init(ValueArray *array) {
  array->values = NULL;
  array->count = 0;
  array->capacity = 0;
}

static void grow_capacity(ValueArray *array) {
  int old_capacity = array->capacity;
  array->capacity = GROW_CAPACITY(old_capacity);
  array->values =
      GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
}

void value_array__append(ValueArray *array, Value value) {
  if (array->count + 1 > array->capacity) {
    grow_capacity(array);
  }
  array->values[array->count] = value;
  array->count++;
}

void value_array__dispose(ValueArray *array) {
  FREE_ARRAY(Value, array->values, array->capacity);
  value_array__init(array);
}

void value__print(Value value) { printf("%g", value); }
void value__println(Value value) { printf("%g\n", value); }
