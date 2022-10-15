#ifndef OBJECT_H_
#define OBJECT_H_

#include "common.h"
#include "value.h"

// forward declaration to avoid cyclic references during compilation
typedef struct VM VM;

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

#define IS_STRING(value) is_object_type(value, OBJECT_STRING)
#define AS_STRING(value) ((ObjectString*)AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjectString*)AS_OBJECT(value))->chars)

typedef enum {
  OBJECT_STRING,
} ObjectType;

struct Object {
  ObjectType type;
  // an implicit linked list to track all heap-allocated objects and
  // so avoid memory leaks (later on to be replaced by full-blown GC)
  struct Object* next;
};

struct ObjectString {
  Object object;
  int length;
  char* chars;
};

ObjectString* string__copy(const char* chars, int length, VM* vm);
ObjectString* string__take_ownership(char* chars, int length, VM* vm);
void object__print(Value value);

static inline bool is_object_type(Value value, ObjectType type) {
  return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#endif // OBJECT_H_
