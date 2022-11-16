#ifndef OBJECT_H_
#define OBJECT_H_

#include "bytecode.h"
#include "common.h"
#include "table.h"
#include "value.h"

// forward declaration to avoid cyclic references during compilation
typedef struct VM VM;

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

#define IS_CLASS(value) is_object_type(value, OBJECT_CLASS)
#define IS_CLOSURE(value) is_object_type(value, OBJECT_CLOSURE)
#define IS_FUNCTION(value) is_object_type(value, OBJECT_FUNCTION)
#define IS_INSTANCE(value) is_object_type(value, OBJECT_INSTANCE)
#define IS_NATIVE_FUNCTION(value) is_object_type(value, OBJECT_NATIVE_FUNCTION)
#define IS_STRING(value) is_object_type(value, OBJECT_STRING)

#define AS_CLASS(value) ((ObjectClass*)AS_OBJECT(value))
#define AS_CLOSURE(value) ((ObjectClosure*)AS_OBJECT(value))
#define AS_FUNCTION(value) ((ObjectFunction*)AS_OBJECT(value))
#define AS_INSTANCE(value) ((ObjectInstance*)AS_OBJECT(value))
#define AS_NATIVE_FUNCTION(value)                                              \
  (((ObjectNativeFunction*)AS_OBJECT(value))->function)
#define AS_STRING(value) ((ObjectString*)AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjectString*)AS_OBJECT(value))->chars)

// For details on this peculiar technique to be convert enum symbols to strings
// without redundancy, see:
//
// https://stackoverflow.com/questions/9907160/how-to-convert-enum-names-to-string-in-c
#define FOREACH_OBJ_TYPE(TYPE)                                                 \
  TYPE(OBJECT_CLASS)                                                           \
  TYPE(OBJECT_CLOSURE)                                                         \
  TYPE(OBJECT_FUNCTION)                                                        \
  TYPE(OBJECT_NATIVE_FUNCTION)                                                 \
  TYPE(OBJECT_INSTANCE)                                                        \
  TYPE(OBJECT_STRING)                                                          \
  TYPE(OBJECT_UPVALUE)

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

typedef enum { FOREACH_OBJ_TYPE(GENERATE_ENUM) } ObjectType;

// We make it `extern` so it can be used from compiler.c;
// see initialization in scanner.c
extern const char* OBJ_TYPE_TO_STRING[];

struct Object {
  ObjectType type;
  // an "intrusive" linked list to track all heap-allocated objects and
  // so avoid memory leaks (later on to be replaced by full-blown GC)
  struct Object* next;

  // an object is "dead" by default, meaning that it is assumed to not be
  // reachable from the roots during a garbage collection. after the
  // mark phase, it should change its status if it is not actually dead.
  //
  // while this may prone to causing serious errors in the application when the
  // GC algorithm is not correct, i think this is precisely the kind of stress
  // testing that we need for such an important part of the program.
  bool is_alive;
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

typedef struct ObjectClass {
  Object object;

  ObjectString* name;

} ObjectClass;

typedef struct ObjectInstance {
  Object object;

  ObjectClass* _class;
  Table fields;

} ObjectInstance;

// all of these functions need to access `vm->objects` so they can track any
// allocated objects for proper garbage collection
ObjectInstance* instance__new(ObjectClass* _class, VM* vm);
ObjectClass* class__new(ObjectString* name, VM* vm);
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
