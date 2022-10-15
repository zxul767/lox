#include <stdio.h>
#include <string.h>

#include "memory.h"
#include "object.h"
/* #include "value.h" */
/* #include "vm.h" */

#define ALLOCATE_OBJECT(type, object_type)                                     \
  (type *)object__allocate(sizeof(type), object_type)

static Object *object__allocate(size_t size, ObjectType type) {
  Object *object = (Object *)reallocate(NULL, 0, size);
  object->type = type;

  return object;
}

static ObjectString *string__allocate(char *chars, int length) {
  ObjectString *string = ALLOCATE_OBJECT(ObjectString, OBJECT_STRING);
  string->length = length;
  string->chars = chars;

  return string;
}

ObjectString *string__copy(const char *chars, int length) {
  char *heap_chars = ALLOCATE(char, length + 1);
  memcpy(heap_chars, chars, length);
  heap_chars[length] = '\0';

  return string__allocate(heap_chars, length);
}

void object__print(Value value) {
  switch (OBJECT_TYPE(value)) {
  case OBJECT_STRING:
    printf("%s", AS_CSTRING(value));
    break;
  }
}
