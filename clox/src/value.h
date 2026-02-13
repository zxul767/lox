#ifndef VALUE_H_
#define VALUE_H_

#include "common.h"

typedef struct Object Object;
typedef struct ObjectString ObjectString;

typedef enum {
  VALUE_BOOL,
  VALUE_NIL,
  VALUE_NUMBER,
  VALUE_OBJECT,
  VALUE_ERROR,

} ValueType;

typedef struct {
  ValueType type;
  union {
    bool boolean;
    double number;
    Object* object;
  } as;

} Value;

#define IS_BOOL(value) ((value).type == VALUE_BOOL)
#define IS_NIL(value) ((value).type == VALUE_NIL)
#define IS_NUMBER(value) ((value).type == VALUE_NUMBER)
#define IS_OBJECT(value) ((value).type == VALUE_OBJECT)

#define AS_BOOL(value) ((value).as.boolean)
#define AS_NUMBER(value) ((value).as.number)
#define AS_INT(value) ((int)(value).as.number)
#define AS_OBJECT(value) ((value).as.object)

#define BOOL_VALUE(value) ((Value){.type = VALUE_BOOL, {.boolean = value}})
#define NIL_VALUE ((Value){.type = VALUE_NIL, {.number = 0}})
#define ERROR_VALUE ((Value){.type = VALUE_ERROR, {.boolean = false}})
#define NUMBER_VALUE(value) ((Value){.type = VALUE_NUMBER, {.number = value}})
#define OBJECT_VALUE(value) ((Value){.type = VALUE_OBJECT, {.object = (Object*)value}})

typedef struct {
  int capacity;
  int count;
  Value* values;

} ValueArray;

void value_array__init(ValueArray* array);
void value_array__append(ValueArray* array, Value value);
Value value_array__pop(ValueArray* array);
void value_array__dispose(ValueArray* array);
void value_array__mark_as_alive(ValueArray* array);

bool value__equals(Value a, Value b);
void value__print(Value value);
void value__println(Value value);
void value__print_repr(Value value);

#endif // VALUE_H_
