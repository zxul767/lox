#include "value.h"
#include "memory.h"
#include "object.h"

#include <assert.h>
#include <stdio.h>
#include <string.h>

void value_array__init(ValueArray* array)
{
  array->values = NULL;
  array->count = 0;
  array->capacity = 0;
}

static void grow_capacity(ValueArray* array)
{
  int old_capacity = array->capacity;
  array->capacity = GROW_CAPACITY(old_capacity);
  array->values =
      GROW_ARRAY(Value, array->values, old_capacity, array->capacity);
}

void value_array__append(ValueArray* array, Value value)
{
  if (array->count + 1 > array->capacity) {
    grow_capacity(array);
  }
  array->values[array->count] = value;
  array->count++;
}

Value value_array__pop(ValueArray* array)
{
  assert(array->count > 0);
  return array->values[--array->count];
}

void value_array__dispose(ValueArray* array)
{
  FREE_ARRAY(Value, array->values, array->capacity);
  value_array__init(array);
}

void value_array__mark_as_alive(ValueArray* array)
{
  for (int i = 0; i < array->count; i++) {
    memory__mark_value_as_alive(array->values[i]);
  }
}

bool value__equals(Value a, Value b)
{
  if (a.type != b.type)
    return false;
  switch (a.type) {
  case VAL_BOOL:
    return AS_BOOL(a) == AS_BOOL(b);
  case VAL_NIL:
    return true;
  case VAL_NUMBER:
    return AS_NUMBER(a) == AS_NUMBER(b);
  case VAL_OBJECT:
    return AS_OBJECT(a) == AS_OBJECT(b);
  default:
    return false; // unreachable
  }
}

void value__print(Value value)
{
  switch (value.type) {
  case VAL_BOOL:
    fprintf(stderr, AS_BOOL(value) ? "true" : "false");
    break;
  case VAL_NIL:
    fprintf(stderr, "nil");
    break;
  case VAL_NUMBER:
    fprintf(stderr, "%g", AS_NUMBER(value));
    break;
  case VAL_OBJECT:
    object__print(value);
    break;
  case VAL_ERROR:
    // whenever an error value is returned, the precise error is reported
    // in the call site where the error originated
    break;
  default:
    fprintf(stderr, "Cannot print value of unknown type: %d", value.type);
  }
}

void value__print_repr(Value value)
{
  if (value.type == VAL_OBJECT) {
    object__print_repr(value);
  } else {
    value__print(value);
  }
}

void value__println(Value value)
{
  value__print(value);
  fprintf(stderr, "\n");
}
