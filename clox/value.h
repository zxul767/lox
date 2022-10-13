#ifndef VALUE_H_
#define VALUE_H_

#include "common.h"

typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
  } as;
} Value;

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)

#define BOOL_VAL(value) ((Value){.type = VAL_BOOL, {.boolean = value}})
#define NIL_VAL ((Value){.type = VAL_NIL, {.number = 0}})
#define NUMBER_VAL(value) ((Value){.type = VAL_NUMBER, {.number = value}})

typedef struct {
  int capacity;
  int count;
  Value *values;
} ValueArray;

void value_array__init(ValueArray *array);
void value_array__append(ValueArray *array, Value value);
void value_array__dispose(ValueArray *array);
void value__print(Value value);
void value__println(Value value);

#endif // VALUE_H_
