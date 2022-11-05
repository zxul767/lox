#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "vm.h"

#define ALLOCATE_OBJECT(type, object_type, vm)                                 \
  (type*)object__allocate(sizeof(type), object_type, vm)

static void track_object(Object* object, VM* vm) {
  object->next = vm->objects;
  vm->objects = object;
}

static Object* object__allocate(size_t size, ObjectType type, VM* vm) {
  // We use `size` instead of `sizeof(Object)` because `Object` is always
  // a "base" object that is never allocated on its own, but rather as
  // part of a derived object (e.g., ObjectString). The correct size
  // is therefore known only by the caller, but we want to cast here
  // to `Object` so we can set `type` (only available in `Object`)
  //
  Object* object = (Object*)memory__reallocate(NULL, 0, size);
  object->type = type;

  track_object(object, vm);
  return object;
}

static ObjectString*
string__allocate(char* chars, int length, uint32_t hash, VM* vm) {
  ObjectString* string = ALLOCATE_OBJECT(ObjectString, OBJECT_STRING, vm);
  string->length = length;
  string->chars = chars;
  string->hash = hash;
  table__set(&vm->interned_strings, string, NIL_VAL);

  return string;
}

// FNV-1a hash function. For details, see:
// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
static uint32_t string__hash(const char* key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

ObjectString* string__copy(const char* chars, int length, VM* vm) {
  uint32_t hash = string__hash(chars, length);
  ObjectString* interned =
      table__find_string(&vm->interned_strings, chars, length, hash);
  if (interned != NULL) {
    return interned;
  }
  char* heap_chars = ALLOCATE(char, length + 1);
  memcpy(heap_chars, chars, length);
  heap_chars[length] = '\0';

  return string__allocate(heap_chars, length, hash, vm);
}

ObjectString* string__take_ownership(char* chars, int length, VM* vm) {
  uint32_t hash = string__hash(chars, length);
  ObjectString* interned =
      table__find_string(&vm->interned_strings, chars, length, hash);
  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }
  return string__allocate(chars, length, hash, vm);
}

ObjectFunction* function__new(VM* vm) {
  ObjectFunction* function =
      ALLOCATE_OBJECT(ObjectFunction, OBJECT_FUNCTION, vm);
  function->arity = 0;
  function->name = NULL;
  bytecode__init(&function->bytecode);

  return function;
}

static void print_function(const ObjectFunction* function) {
  if (function->name == NULL) {
    printf("<script>");
    return;
  }
  printf("<fn %s>", function->name->chars);
}

void object__print(Value value) {
  switch (OBJECT_TYPE(value)) {
  case OBJECT_FUNCTION:
    print_function(AS_FUNCTION(value));
  case OBJECT_STRING:
    printf("%s", AS_CSTRING(value));
    break;
  }
}

void object__print_repr(Value value) {
  switch (OBJECT_TYPE(value)) {
  case OBJECT_STRING:
    printf("\"%s\"", AS_CSTRING(value));
    break;
  default:
    object__print(value);
    break;
  }
}
