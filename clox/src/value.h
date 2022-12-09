#ifndef VALUE_H_
#define VALUE_H_

#include "common.h"

typedef struct Object Object;
typedef struct ObjectString ObjectString;

typedef enum {
  VAL_BOOL,
  VAL_NIL,
  VAL_NUMBER,
  VAL_OBJECT,
  VAL_ERROR,

} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
    Object* object;
  } as;

} Value;

#define IS_BOOL(value) ((value).type == VAL_BOOL)
#define IS_NIL(value) ((value).type == VAL_NIL)
#define IS_NUMBER(value) ((value).type == VAL_NUMBER)
#define IS_OBJECT(value) ((value).type == VAL_OBJECT)

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_INT(value) ((int)(value).as.number)
#define AS_OBJECT(value) ((value).as.object)

#define BOOL_VAL(value) ((Value){.type = VAL_BOOL, {.boolean = value}})
#define NIL_VAL ((Value){.type = VAL_NIL, {.number = 0}})
#define ERROR_VAL ((Value){.type = VAL_ERROR, {.boolean = false}})
#define NUMBER_VAL(value) ((Value){.type = VAL_NUMBER, {.number = value}})
#define OBJECT_VAL(value)                                                      \
  ((Value){.type = VAL_OBJECT, {.object = (Object*)value}})

typedef struct {
  int capacity;
  int count;
  Value* values;
} ValueArray;

void value_array__init(ValueArray* array);
void value_array__append(ValueArray* array, Value value);
Value value_array__pop(ValueArray* array);
void value_array__dispose(ValueArray* array);
void value_array__clear(ValueArray* array);

bool value__equals(Value a, Value b);
void value__print(Value value);
void value__println(Value value);
void value__print_repr(Value value);

#endif // VALUE_H_
