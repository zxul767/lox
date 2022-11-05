#include <stdio.h>
#include <stdlib.h>

#include "memory.h"
#include "object.h"

void* memory__reallocate(void* pointer, size_t old_size, size_t new_size) {
  if (new_size == 0) {
    free(pointer);
    return NULL;
  }
  void* result = realloc(pointer, new_size);
  if (result == NULL) {
    // TODO: replace `1` with appropriate symbolic constant
    exit(1);
  }
  return result;
}

static void free_object(Object* object) {
  switch (object->type) {
  case OBJECT_FUNCTION: {
    ObjectFunction* function = (ObjectFunction*)object;
    bytecode__dispose(&function->bytecode);
    FREE(ObjectFunction, object);
    break;
  }
  case OBJECT_STRING: {
    ObjectString* string = (ObjectString*)object;
    FREE_ARRAY(char, string->chars, string->length + 1);
    FREE(ObjectString, object);
    break;
  }
  }
}

void memory__free_objects(Object* objects) {
  int count = 0;
  Object* object = objects;
  while (object != NULL) {
    Object* next = object->next;
    free_object(object);
    object = next;
    count++;
  }
  fprintf(stderr, "GC: freed %d heap-allocated objects\n", count);
}
