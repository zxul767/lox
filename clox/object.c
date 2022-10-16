#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
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
  // TODO: add to list of heap-allocated objects
  return object;
}

static ObjectString* string__allocate(char* chars, int length, uint32_t hash,
                                      VM* vm) {
  ObjectString* string = ALLOCATE_OBJECT(ObjectString, OBJECT_STRING, vm);
  string->length = length;
  string->chars = chars;
  string->hash = hash;

  return string;
}

static uint32_t string__hash(const char* key, int length) {
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

ObjectString* string__copy(const char* chars, int length, VM* vm) {
  char* heap_chars = ALLOCATE(char, length + 1);
  memcpy(heap_chars, chars, length);
  heap_chars[length] = '\0';
  uint32_t hash = string__hash(chars, length);

  return string__allocate(heap_chars, length, hash, vm);
}

ObjectString* string__take_ownership(char* chars, int length, VM* vm) {
  uint32_t hash = string__hash(chars, length);
  return string__allocate(chars, length, hash, vm);
}

void object__print(Value value) {
  switch (OBJECT_TYPE(value)) {
  case OBJECT_STRING:
    printf("%s", AS_CSTRING(value));
    break;
  }
}
