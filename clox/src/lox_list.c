#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "lox_list.h"
#include "memory.h"
#include "value.h"
#include "vm.h"

static ObjectInstance* lox_list__new(ObjectClass* _class, VM* vm)
{
  ObjectList* list = ALLOCATE_OBJECT(ObjectList, OBJECT_LIST, vm);
  // every instance has these properties
  list->instance._class = _class;
  table__init(&list->instance.fields);

  // the underlying list implementation
  value_array__init(&list->array);

  return (ObjectInstance*)list;
}

void lox_list__print(const ObjectList* list)
{
  fprintf(stderr, "[");
  for (int i = 0; i < list->array.count; ++i) {
    Value value = list->array.values[i];
    // don't print lists recursively because in the worst case we can get into
    // cycles, e.g. var l = list() l.append(l) println(l)
    if (IS_LIST(value)) {
      if (AS_LIST(value) == list) {
        fprintf(stderr, "@");

      } else {
        fprintf(stderr, "[...]");
      }

    } else {
      value__print_repr(value);
    }
    if (i + 1 < list->array.count) {
      fprintf(stderr, ",");
    }
  }
  fprintf(stderr, "]");
}

// in all native methods, the first argument is always `this` (i.e., the
// instance)
static Value lox_list__length(int args_count, Value* args)
{
  ObjectList* list = REQUIRE_LIST(args[0]);

  return NUMBER_VAL(list->array.count);
}

static Value lox_list__append(int args_count, Value* args)
{
  ObjectList* list = REQUIRE_LIST(args[0]);
  value_array__append(&list->array, args[1]);

  return NIL_VAL;
}

static int normalize_index(int index, const ObjectList* list)
{
  int normed_index = index;
  if (index < 0) {
    normed_index = list->array.count + index;
  }
  if (normed_index < 0 || normed_index >= list->array.count) {
    fprintf(
        stderr,
        "Index Error: tried to access index %d, but valid range is [0..%d] or "
        "[-%d..-1].\n",
        index, list->array.count - 1, list->array.count);
    return -1;
  }
  return normed_index;
}

static Value lox_list__at(int args_count, Value* args)
{
  ObjectList* list = REQUIRE_LIST(args[0]);

  if (list->array.count == 0) {
    fprintf(stderr, "Index Error: Cannot access elements in empty list.\n");
    return ERROR_VAL;
  }

  int index = AS_INT(args[1]);
  index = normalize_index(index, list);
  if (index == -1) {
    return ERROR_VAL;
  }
  return list->array.values[index];
}

static Value lox_list__clear(int args_count, Value* args)
{
  ObjectList* list = REQUIRE_LIST(args[0]);

  value_array__dispose(&list->array);
  return NIL_VAL;
}

static Value lox_list__pop(int args_count, Value* args)
{
  ObjectList* list = REQUIRE_LIST(args[0]);
  if (list->array.count == 0) {
    fprintf(stderr, "Error: Cannot remove elements from an empty list.\n");
    return NIL_VAL;
  }
  return value_array__pop(&list->array);
}

static void define_method(
    ObjectClass* _class, const char* name, int arity, NativeFunction native,
    VM* vm)
{
  WITH_OBJECTS_NURSERY(vm, {
    ObjectString* method_name = string__copy(name, (int)strlen(name), vm);

    ObjectNativeFunction* native_fn =
        native_function__new(native, method_name, arity, vm);
    native_fn->is_method = true;

    table__set(&_class->methods, method_name, OBJECT_VAL(native_fn));
  });
}

static void define_list_methods(ObjectClass* _class, VM* vm)
{
  define_method(_class, "length", 0, lox_list__length, vm);
  define_method(_class, "append", 1, lox_list__append, vm);
  define_method(_class, "at", 1, lox_list__at, vm);
  define_method(_class, "clear", 0, lox_list__clear, vm);
  define_method(_class, "pop", 0, lox_list__pop, vm);
}

ObjectClass* lox_list__new_class(const char* name, VM* vm)
{
  ObjectClass* _class = NULL;
  WITH_OBJECTS_NURSERY(vm, {
    ObjectString* class_name = string__copy(name, strlen(name), vm);
    _class = class__new(class_name, vm);
    _class->new_instance = lox_list__new;

    define_list_methods(_class, vm);
  });

  return _class;
}

void lox_list__mark_as_alive(ObjectList* list)
{
  value_array__mark_as_alive(&list->array);
}
