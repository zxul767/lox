#ifndef MEMORY_H_
#define MEMORY_H_

#include "common.h"
#include "value.h"

#define MAX_VMS 32

typedef struct VM VM;
typedef struct Object Object;

typedef struct GC {
  VM* vms[MAX_VMS];
  int vms_count;

  int gray_count;
  int gray_capacity;
  Object** gray_stack;

  size_t bytes_allocated;
  size_t next_gc_in_bytes;

} GC;

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

#define WITH_OBJECTS_NURSERY(vm, block)                                        \
  {                                                                            \
    memory__open_object_nursery(vm);                                           \
    block;                                                                     \
    memory__close_object_nursery(vm);                                          \
  }

void* memory__reallocate(void* pointer, size_t old_size, size_t new_size);
size_t memory__free_objects(Object* objects);

void memory__init_gc();
void memory__shutdown_gc();
void memory__run_gc();
void memory__register_for_gc(VM* vm);
void memory__mark_value_as_alive(Value value);
void memory__mark_object_as_alive(Object* object);
void memory__print_gc_stats();

void memory__open_object_nursery(VM* vm);
void memory__close_object_nursery(VM* vm);

#endif // MEMORY_H_
