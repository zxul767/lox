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

// the global garbage collector for the interpreter
GC __gc;

void memory__print_gc_stats()
{
  // TODO: format numeric output with thousands separators
  fprintf(stderr, "bytes allocated: %zu.\n", __gc.bytes_allocated);
  fprintf(stderr, "next gc run after %zu bytes allocated.\n", __gc.next_gc_in_bytes);
}

void memory__init_gc(VM* vm)
{
  __gc.gray_capacity = 0;
  __gc.gray_count = 0;
  __gc.gray_stack = NULL;
  __gc.vm = vm;

  __gc.bytes_allocated = 0;
  __gc.next_gc_in_bytes = 1024 * 1024;
}

void memory__shutdown_gc()
{
  free(__gc.gray_stack);
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
    // TODO: print an OOM error message
    exit(EXIT_FAILURE);
  }
  return result;
}

static void dispose_object(Object* object)
{
  // if an object contains heap-allocated structures which are not objects
  // it's important that they get disposed here (e.g., an `ObjectList` must call
  // `value_array__dispose` on its underlying `array` member, since the memory
  // for it is not tracked as an object.)
  //
  // if an object contains references to other objects, DO NOT dispose of them
  // here, since that could cause referencing of "freed" memory later when that
  // objects is disposed (e.g., an `ObjectFunction` contains indirectly an
  // `ObjectString` thru its `ObjectCallable` instance, but that string is being
  // tracked in `vm->objects` and will get deleted on another call to this
  // function.)
  //
#ifdef DEBUG_LOG_GC_DETAILED
  fprintf(stderr, "%p disposing (%s)\n", (void*)object, OBJ_TYPE_TO_STRING[object->type]);
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
  case OBJECT_CALLABLE:
    // a callable is never used on its own; it is always a "super-class" of
    // `ObjectClosure` or `ObjectNativeFunction`, so there is no need to dispose
    // it directly.
    break;
  }
}

size_t memory__free_objects(Object* objects)
{
  // Garbage collection always leaves reachable objects (from the roots)
  // untouched, so this is the last opportunity to dispose of them, via
  // `memory__free_objects(vm->objects)`
  //
  size_t count = 0;
  Object* object = objects;
  while (object != NULL) {
    Object* next = object->next;
    dispose_object(object);
    object = next;
    count++;
  }
  return count;
}

static void grow_gray_capacity(GC* gc)
{
  gc->gray_capacity = GROW_CAPACITY(gc->gray_capacity);
  gc->gray_stack = (Object**)realloc(gc->gray_stack, sizeof(Object*) * gc->gray_capacity);

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
  memory__mark_object_as_alive((Object*)vm->string_class);

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

  // the object nursery
  if (vm->object_nursery_end != NULL) {
    // if the nursery points to non-null, then `vm->objects` should necessarily
    // be non-null
    assert(vm->objects != NULL);

    for (Object* object = vm->objects; object != vm->object_nursery_end;
         object = object->next) {
      memory__mark_object_as_alive(object);
    }
  }
}

static void mark_object_instance_as_alive(ObjectInstance* instance)
{
  memory__mark_object_as_alive((Object*)instance->_class);
  table__mark_as_alive(&instance->fields);
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
    memory__mark_object_as_alive((Object*)AS_CALLABLE(function)->signature.name);
    memory__mark_object_as_alive((Object*)AS_CALLABLE(function)->docstring);
    value_array__mark_as_alive(&function->bytecode.constants);
    break;
  }
  case OBJECT_INSTANCE: {
    mark_object_instance_as_alive((ObjectInstance*)object);
    break;
  }
  case OBJECT_LIST: {
    mark_object_instance_as_alive((ObjectInstance*)object);
    // `list` is a native instance class and its contained items aren't tracked
    // by the `fields` table, so we need to manually mark them.
    lox_list__mark_as_alive((ObjectList*)object);
    break;
  }
  case OBJECT_UPVALUE:
    memory__mark_value_as_alive(((ObjectUpvalue*)object)->closed);
    break;
  case OBJECT_NATIVE_FUNCTION: {
    ObjectNativeFunction* native = (ObjectNativeFunction*)object;
    memory__mark_object_as_alive((Object*)AS_CALLABLE(native)->signature.name);
    memory__mark_object_as_alive((Object*)AS_CALLABLE(native)->docstring);
  } break;
  case OBJECT_STRING:
    // a string has no references to other objects which are only reachable
    // through it
  case OBJECT_CALLABLE:
    // a callable is never used on its own; it is always a "super-class" of
    // `ObjectFunction` or `ObjectNativeFunction`, which are handled above
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
        // doing `vm->objects = object` is not enough because we also need to
        // keep the objects "nursery" in sync.
        vm__reset_objects_list_head(vm, /* new_head: */ object);
      }
#ifdef DEBUG_LOG_GC
      printf("%p about to be disposed\n", (void*)object);
#endif
      dispose_object(unreachable);
    }
  }
}

void memory__open_object_nursery(VM* vm)
{
  assert(vm != NULL);

  if (vm->object_nursery_end == NULL) {
#ifdef DEBUG_LOG_GC_DETAILED
    fprintf(stderr, "object nursery just opened\n");
#endif
    vm->object_nursery_end = vm->objects;
  }
  vm->object_nursery_nested_scopes++;
}

void memory__close_object_nursery(VM* vm)
{
  assert(vm != NULL);

  if (vm->object_nursery_nested_scopes == 0) {
    fprintf(stderr, "WARNING: trying to close a closed object nursery!\n");
    assert(vm->object_nursery_end == NULL);
    return;
  }
  // only close the nursery after all nested scopes have been closed
  if (--(vm->object_nursery_nested_scopes) == 0) {
#ifdef DEBUG_LOG_GC_DETAILED
    fprintf(stderr, "object nursery just closed\n");
#endif
    vm->object_nursery_end = NULL;

  } else {
#ifdef DEBUG_LOG_GC_DETAILED
    fprintf(stderr, "object nursery nested scope closed\n");
#endif
  }
}

void memory__run_gc()
{
#if defined(DEBUG_LOG_GC) || defined(DEBUG_LOG_GC_DETAILED)
  fprintf(stderr, "-- GC begins\n");
  size_t before = __gc.bytes_allocated;
#endif

  assert(__gc.vm != NULL);

  mark_roots(__gc.vm);
  trace_references(&__gc);
  table__remove_dead_objects(&(__gc.vm->interned_strings));
  sweep(__gc.vm);

  __gc.next_gc_in_bytes = __gc.bytes_allocated * GC_HEAP_GROW_FACTOR;

#if defined(DEBUG_LOG_GC) || defined(DEBUG_LOG_GC_DETAILED)
  fprintf(stderr, "-- GC ends\n");

  fprintf(
      stderr,
      "\tcollected %zu bytes (from %zu to %zu).\n",
      before - __gc.bytes_allocated,
      before,
      __gc.bytes_allocated
  );

  fprintf(stderr, "\tnext collection at %zu bytes\n", __gc.next_gc_in_bytes);
#endif
}
