#ifndef OBJECT_H_
#define OBJECT_H_

#include "bytecode.h"
#include "common.h"
#include "value.h"

// forward declaration to avoid cyclic references during compilation
typedef struct VM VM;

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

#define IS_CLOSURE(value) is_object_type(value, OBJECT_CLOSURE)
#define IS_FUNCTION(value) is_object_type(value, OBJECT_FUNCTION)
#define IS_NATIVE_FUNCTION(value) is_object_type(value, OBJECT_NATIVE_FUNCTION)
#define IS_STRING(value) is_object_type(value, OBJECT_STRING)

#define AS_CLOSURE(value) ((ObjectClosure*)AS_OBJECT(value))
#define AS_FUNCTION(value) ((ObjectFunction*)AS_OBJECT(value))
#define AS_NATIVE_FUNCTION(value)                                              \
  (((ObjectNativeFunction*)AS_OBJECT(value))->function)
#define AS_STRING(value) ((ObjectString*)AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjectString*)AS_OBJECT(value))->chars)

typedef enum {
  OBJECT_CLOSURE,
  OBJECT_FUNCTION,
  OBJECT_NATIVE_FUNCTION,
  OBJECT_STRING,
  OBJECT_UPVALUE,

} ObjectType;

struct Object {
  ObjectType type;
  // an "intrusive" linked list to track all heap-allocated objects and
  // so avoid memory leaks (later on to be replaced by full-blown GC)
  struct Object* next;
};

typedef Value (*NativeFunction)(int args_count, Value* args);

typedef struct ObjectNative {
  Object object;
  NativeFunction function;

} ObjectNativeFunction;

typedef struct ObjectFunction {
  Object object;

  int arity;
  ObjectString* name;
  Bytecode bytecode;

  int upvalues_count;

} ObjectFunction;

struct ObjectString {
  Object object;

  int length;
  char* chars;
  uint32_t hash;
};

typedef struct ObjectUpvalue {
  Object object;

  Value* location;
  Value closed;
  struct ObjectUpvalue* next;

} ObjectUpvalue;

typedef struct ObjectClosure {
  Object object;

  ObjectFunction* function;
  ObjectUpvalue** upvalues;
  int upvalues_count;

} ObjectClosure;

// TODO: replace `vm` with garbage collector object when we implement it so it
// is more clear why we're passing such an object
//
// all of these functions need to access `vm->objects` so they can track any
// allocated objects for proper garbage collection
ObjectClosure* closure__new(ObjectFunction*, VM* vm);
ObjectFunction* function__new(VM* vm);
ObjectNativeFunction* native_function__new(NativeFunction function, VM* vm);

ObjectUpvalue* upvalue__new(Value* slot, VM* vm);
ObjectString* string__copy(const char* chars, int length, VM* vm);
ObjectString* string__take_ownership(char* chars, int length, VM* vm);

void object__print(Value value);
void object__print_repr(Value value);

static inline bool is_object_type(Value value, ObjectType type)
{
  return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#endif // OBJECT_H_
