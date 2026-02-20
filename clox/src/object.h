#ifndef OBJECT_H_
#define OBJECT_H_

#include "bytecode.h"
#include "common.h"
#include "table.h"
#include "value.h"

// forward declaration for cases where we just need the pointer
typedef struct Token Token;

// forward declaration to avoid cyclic references during compilation
typedef struct VM VM;

#define OBJECT_TYPE(value) (AS_OBJECT(value)->type)

#define IS_CLASS(value) is_object_type(value, OBJECT_CLASS)

#define IS_CALLABLE(value)                                                               \
  (is_object_type(value, OBJECT_CALLABLE) || is_object_type(value, OBJECT_FUNCTION) ||   \
   is_object_type(value, OBJECT_NATIVE_FUNCTION) ||                                      \
   is_object_type(value, OBJECT_BOUND_METHOD))
#define IS_CLOSURE(value) is_object_type(value, OBJECT_CLOSURE)
#define IS_FUNCTION(value) is_object_type(value, OBJECT_FUNCTION)
#define IS_BOUND_METHOD(value) is_object_type(value, OBJECT_BOUND_METHOD)
#define IS_NATIVE_FUNCTION(value) is_object_type(value, OBJECT_NATIVE_FUNCTION)

#define IS_INSTANCE(value)                                                               \
  (is_object_type(value, OBJECT_INSTANCE) || is_object_type(value, OBJECT_LIST))

#define IS_STRING(value) is_object_type(value, OBJECT_STRING)

#define AS_CLASS(value) ((ObjectClass*)AS_OBJECT(value))

#define AS_CALLABLE(object) ((ObjectCallable*)object)

#define AS_CLOSURE(value) ((ObjectClosure*)AS_OBJECT(value))
#define AS_FUNCTION(value) ((ObjectFunction*)AS_OBJECT(value))
#define AS_BOUND_METHOD(value) ((ObjectBoundMethod*)AS_OBJECT(value))
#define AS_NATIVE_FUNCTION(value) (((ObjectNativeFunction*)AS_OBJECT(value)))

#define AS_INSTANCE(value) ((ObjectInstance*)AS_OBJECT(value))

#define AS_STRING(value) ((ObjectString*)AS_OBJECT(value))
#define AS_CSTRING(value) (((ObjectString*)AS_OBJECT(value))->chars)

// For details on this peculiar technique to be convert enum symbols to strings
// without redundancy, see:
//
// https://stackoverflow.com/questions/9907160/how-to-convert-enum-names-to-string-in-c
#define FOREACH_OBJ_TYPE(TYPE)                                                           \
  TYPE(OBJECT_CLASS)                                                                     \
  TYPE(OBJECT_CLOSURE)                                                                   \
  TYPE(OBJECT_FUNCTION)                                                                  \
  TYPE(OBJECT_CALLABLE)                                                                  \
  TYPE(OBJECT_BOUND_METHOD)                                                              \
  TYPE(OBJECT_NATIVE_FUNCTION)                                                           \
  TYPE(OBJECT_INSTANCE)                                                                  \
  TYPE(OBJECT_UPVALUE)                                                                   \
  TYPE(OBJECT_STRING)                                                                    \
  TYPE(OBJECT_LIST) // native class

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

typedef Value (*NativeFunction)(int args_count, Value* args, VM* vm);

typedef struct CallableParameter {
  const char* name;
  const char* type;
} CallableParameter;

typedef struct CallableSignature {
  ObjectString* name;
  int arity;
  const CallableParameter* parameters;
  const char* return_type;
} CallableSignature;

typedef struct ObjectCallable {
  Object object;

  CallableSignature signature;
  ObjectString* docstring;

} ObjectCallable;

typedef struct ObjectNativeFunction {
  ObjectCallable callable;

  NativeFunction function;
  bool is_method;

} ObjectNativeFunction;

typedef struct ObjectFunction {
  ObjectCallable callable;

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

typedef struct ObjectInstance ObjectInstance;
typedef struct ObjectClass ObjectClass;
typedef ObjectInstance* (*ClassConstructor)(ObjectClass*, VM* vm);

typedef struct ObjectClass {
  Object object;

  ObjectString* name;
  Table methods;

  ClassConstructor new_instance;

} ObjectClass;

typedef struct ObjectInstance {
  Object object;

  ObjectClass* _class;
  Table fields;

} ObjectInstance;

typedef struct ObjectBoundMethod {
  Object object;

  Value instance;
  // the bound method is wrapped in a value so we can use both
  // `ObjectClosure` and `ObjectNativeFunction` objects
  Value method;

} ObjectBoundMethod;

#define ALLOCATE_OBJECT(type, object_type, vm)                                           \
  (type*)object__allocate(sizeof(type), object_type, vm)

Object* object__allocate(size_t size, ObjectType type, VM* vm);

// all of these functions need to access `vm->objects` so they can track any
// allocated objects for proper garbage collection
ObjectInstance* instance__new(ObjectClass* _class, VM* vm);
ObjectClass* class__new(ObjectString* name, VM* vm);
ObjectBoundMethod* bound_method__new(Value instance, Value method, VM* vm);
ObjectClosure* closure__new(ObjectFunction*, VM* vm);
ObjectFunction* function__new(VM* vm);
ObjectNativeFunction* native_function__new(
    NativeFunction function,
    ObjectString* name,
    const CallableSignature* signature,
    const char* docstring,
    VM* vm
);
ObjectUpvalue* upvalue__new(Value* slot, VM* vm);

ObjectString* string__copy(const char* chars, int length, VM* vm);
ObjectString* string__take_ownership(char* chars, int length, VM* vm);

bool string__equals_token(ObjectString* string, const Token* token);

void object__print(Value value);
void object__print_repr(Value value);

static inline bool is_object_type(Value value, ObjectType type)
{
  return IS_OBJECT(value) && AS_OBJECT(value)->type == type;
}

#endif // OBJECT_H_
