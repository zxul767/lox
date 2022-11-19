#include <assert.h>
#include <stdlib.h>

#include "compiler.h"
#include "lox_list.h"
#include "memory.h"
#include "object.h"
#include "vm.h"

#include <stdio.h>

#ifdef DEBUG_STRESS_GC
#include "debug.h"
#endif

#define GC_HEAP_GROW_FACTOR 2

GC __gc;

void memory__print_gc_stats()
{
  // TODO: format numeric output with thousands separators
  fprintf(stderr, "bytes allocated: %zu.\n", __gc.bytes_allocated);
  fprintf(
      stderr, "next gc run after %zu bytes allocated.\n",
      __gc.next_gc_in_bytes);
}

void memory__init_gc()
{
  __gc.vms_count = 0;

  __gc.gray_capacity = 0;
  __gc.gray_count = 0;
  __gc.gray_stack = NULL;

  __gc.bytes_allocated = 0;
  __gc.next_gc_in_bytes = 1024 * 1024;
}

void memory__shutdown_gc() { free(__gc.gray_stack); }

void memory__register_for_gc(VM* vm)
{
  assert(__gc.vms_count == 0);

  __gc.vms[__gc.vms_count++] = vm;
}

void* memory__reallocate(void* pointer, size_t old_size, size_t new_size)
{
  __gc.bytes_allocated += new_size - old_size;

  if (new_size > old_size) {
#ifdef DEBUG_STRESS_GC
    memory__run_gc();
#endif

    if (__gc.bytes_allocated > __gc.next_gc_in_bytes) {
      memory__run_gc();
    }
  }

  if (new_size == 0) {
    free(pointer);
    return NULL;
  }
  void* result = realloc(pointer, new_size);
  if (result == NULL) {
    exit(EXIT_FAILURE);
  }
  return result;
}

static void free_object(Object* object)
{
#ifdef DEBUG_LOG_GC_DETAILED
  fprintf(
      stderr, "%p disposing (%s)\n", (void*)object,
      OBJ_TYPE_TO_STRING[object->type]);
#endif

  switch (object->type) {
  case OBJECT_CLASS: {
    ObjectClass* _class = (ObjectClass*)object;
    table__dispose(&_class->methods);
    FREE(ObjectClass, object);
    break;
  }
  case OBJECT_CLOSURE: {
    ObjectClosure* closure = (ObjectClosure*)object;
    FREE_ARRAY(ObjectUpvalue*, closure->upvalues, closure->upvalues_count);
    FREE(ObjectClosure, object);
    break;
  }
  case OBJECT_FUNCTION: {
    ObjectFunction* function = (ObjectFunction*)object;
    bytecode__dispose(&function->bytecode);
    FREE(ObjectFunction, object);
    break;
  }
  case OBJECT_BOUND_METHOD: {
    FREE(ObjectBoundMethod, object);
    break;
  }
  case OBJECT_INSTANCE: {
    ObjectInstance* instance = (ObjectInstance*)object;
    table__dispose(&instance->fields);
    FREE(ObjectInstance, object);
    break;
  }
  case OBJECT_NATIVE_FUNCTION: {
    FREE(ObjectNativeFunction, object);
    break;
  }
  case OBJECT_STRING: {
    ObjectString* string = (ObjectString*)object;
    FREE_ARRAY(char, string->chars, string->length + 1);
    FREE(ObjectString, object);
    break;
  }
  case OBJECT_LIST: {
    // `ObjectList` is a kind of instance (it "inherits" from `ObjectInstance`)
    ObjectInstance* instance = (ObjectInstance*)object;
    table__dispose(&instance->fields);

    ObjectList* list = (ObjectList*)object;
    value_array__dispose(&list->array);
    FREE(ObjectList, object);
    break;
  }
  case OBJECT_UPVALUE: {
    FREE(ObjectUpvalue, object);
    break;
  }
  default:
    assert(false);
  }
}

size_t memory__free_objects(Object* objects)
{
  size_t count = 0;
  Object* object = objects;
  while (object != NULL) {
    Object* next = object->next;
    free_object(object);
    object = next;
    count++;
  }
  return count;
}

static void grow_gray_capacity(GC* gc)
{
  gc->gray_capacity = GROW_CAPACITY(gc->gray_capacity);
  gc->gray_stack =
      (Object**)realloc(gc->gray_stack, sizeof(Object*) * gc->gray_capacity);

  if (gc->gray_stack == NULL) {
    fprintf(stderr, "Critical failure: cannot allocate gray list during GC.\n");
    exit(EXIT_FAILURE);
  }
}

// see
// https://craftinginterpreters.com/garbage-collection.html#the-tricolor-abstraction
// for details on the tricolor abstraction and the meaning of white/gray/black
// objects during garbage collection
//
static void add_to_gray_list(GC* gc, Object* object)
{
  if (gc->gray_count + 1 > gc->gray_capacity) {
    grow_gray_capacity(gc);
  }
  gc->gray_stack[gc->gray_count++] = object;
}

void memory__mark_object_as_alive(Object* object)
{
  if (object == NULL || object->is_alive)
    return;

#ifdef DEBUG_LOG_GC_DETAILED
  fprintf(stderr, "%p marked as alive\n", (void*)object);
#endif

  object->is_alive = true;
  add_to_gray_list(&__gc, object);
}

void memory__mark_value_as_alive(Value value)
{
  if (IS_OBJECT(value))
    memory__mark_object_as_alive(AS_OBJECT(value));
}

static void mark_roots(VM* vm)
{
  // VM constants
  memory__mark_object_as_alive((Object*)vm->init_string);

  // the value stack
  for (Value* slot = vm->value_stack; slot < vm->stack_free_slot; slot++) {
    memory__mark_value_as_alive(*slot);
  }

  // objects allocated during compilation
  compiler__mark_roots(vm->current_fn_compiler);

  // global variables
  table__mark_as_alive(&vm->global_vars);

  // closures in current frames
  for (int i = 0; i < vm->frames_count; i++) {
    memory__mark_object_as_alive((Object*)vm->frames[i].closure);
  }

  // open upvalues
  for (ObjectUpvalue* upvalue = vm->open_upvalues; upvalue != NULL;
       upvalue = upvalue->next) {
    memory__mark_object_as_alive((Object*)upvalue);
  }
}

static void mark_array_as_alive(ValueArray* array)
{
  for (int i = 0; i < array->count; i++) {
    memory__mark_value_as_alive(array->values[i]);
  }
}

static void mark_object_references(Object* object)
{
#ifdef DEBUG_LOG_GC_DETAILED
  printf("%p marked as processed\n", (void*)object);
#endif

  switch (object->type) {
  case OBJECT_BOUND_METHOD: {
    ObjectBoundMethod* bound_method = (ObjectBoundMethod*)object;
    memory__mark_value_as_alive(bound_method->instance);
    memory__mark_value_as_alive(bound_method->method);
    break;
  }
  case OBJECT_CLASS: {
    ObjectClass* _class = (ObjectClass*)object;
    memory__mark_object_as_alive((Object*)_class->name);
    table__mark_as_alive(&_class->methods);
    break;
  }
  case OBJECT_CLOSURE: {
    ObjectClosure* closure = (ObjectClosure*)object;
    memory__mark_object_as_alive((Object*)closure->function);
    for (int i = 0; i < closure->upvalues_count; i++) {
      memory__mark_object_as_alive((Object*)closure->upvalues[i]);
    }
    break;
  }
  case OBJECT_FUNCTION: {
    ObjectFunction* function = (ObjectFunction*)object;
    memory__mark_object_as_alive((Object*)CALLABLE_CAST(function)->name);
    mark_array_as_alive(&function->bytecode.constants);
    break;
  }
    // `ObjectList` is a kind of instance (it "inherits" from `ObjectInstance`)
  case OBJECT_LIST:
  case OBJECT_INSTANCE: {
    ObjectInstance* instance = (ObjectInstance*)object;
    memory__mark_object_as_alive((Object*)instance->_class);
    table__mark_as_alive(&instance->fields);
    break;
  }
  case OBJECT_UPVALUE:
    memory__mark_value_as_alive(((ObjectUpvalue*)object)->closed);
    break;
  case OBJECT_NATIVE_FUNCTION: {
    ObjectFunction* function = (ObjectFunction*)object;
    memory__mark_object_as_alive((Object*)CALLABLE_CAST(function)->name);
  } break;
  case OBJECT_STRING:
  case OBJECT_CALLABLE:
    // a callable is never used on its own; it is always a "super-class" of
    // `ObjectFunction` or others, which are handled above
    break;
  }
}

static void trace_references(GC* gc)
{
  while (gc->gray_count > 0) {
    Object* object = gc->gray_stack[--gc->gray_count];
    mark_object_references(object);
  }
}

static void sweep(VM* vm)
{
  Object* previous = NULL;
  Object* object = vm->objects;

  while (object != NULL) {
    if (object->is_alive) {
      object->is_alive = false;

      previous = object;
      object = object->next;

    } else {
      Object* unreachable = object;

      object = object->next;
      if (previous != NULL) {
        previous->next = object;
      } else {
        vm->objects = object;
      }

      free_object(unreachable);
    }
  }
}

void memory__run_gc()
{
#if defined(DEBUG_LOG_GC) || defined(DEBUG_LOG_GC_DETAILED)
  fprintf(stderr, "-- GC begins\n");
  size_t before = __gc.bytes_allocated;
#endif

  // TODO: design a good policy to strike a balance between throughout and
  // latency
  for (int i = 0; i < __gc.vms_count; ++i) {
    assert(__gc.vms[i] != NULL);

    mark_roots(__gc.vms[i]);
    trace_references(&__gc);
    table__remove_dead_objects(&(__gc.vms[i]->interned_strings));
    sweep(__gc.vms[i]);

    __gc.next_gc_in_bytes = __gc.bytes_allocated * GC_HEAP_GROW_FACTOR;
  }

#if defined(DEBUG_LOG_GC) || defined(DEBUG_LOG_GC_DETAILED)
  fprintf(stderr, "-- GC ends\n");

  fprintf(
      stderr, "\tcollected %zu bytes (from %zu to %zu).\n",
      before - __gc.bytes_allocated, before, __gc.bytes_allocated);

  fprintf(stderr, "\tnext collection at %zu bytes\n", __gc.next_gc_in_bytes);
#endif
}
