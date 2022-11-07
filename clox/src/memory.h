#ifndef MEMORY_H_
#define MEMORY_H_

#include "common.h"

typedef struct Object Object;

#define GROW_CAPACITY(capacity) ((capacity) < 8 ? 8 : (capacity)*2)

#define GROW_ARRAY(type, pointer, old_count, new_count)                        \
  (type*)memory__reallocate(                                                   \
      pointer, sizeof(type) * (old_count), sizeof(type) * (new_count))

#define FREE_ARRAY(type, pointer, old_count)                                   \
  memory__reallocate(pointer, sizeof(type) * (old_count), 0)

// Freeing a single object is just a special case of freeing a collection
#define FREE(type, pointer) FREE_ARRAY(type, pointer, 1)

#define ALLOCATE(type, count)                                                  \
  (type*)memory__reallocate(NULL, 0, sizeof(type) * (count))

void* memory__reallocate(void* pointer, size_t old_size, size_t new_size);
size_t memory__free_objects(Object* objects);

#endif // MEMORY_H_
