#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "indexing.h"
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
      fprintf(stderr, ", ");
    }
  }
  fprintf(stderr, "]");
}

// in all native methods, the first argument is always `this` (i.e., the
// instance)
static Value lox_list__length(int args_count, Value* args, VM* vm)
{
  (void)vm;
  (void)args_count;

  ObjectList* list = REQUIRE_LIST(args[0]);

  return NUMBER_VALUE(list->array.count);
}

static Value lox_list__append(int args_count, Value* args, VM* vm)
{
  (void)vm;
  (void)args_count;

  ObjectList* list = REQUIRE_LIST(args[0]);
  value_array__append(&list->array, args[1]);

  return NIL_VALUE;
}

static Value lox_list__at(int args_count, Value* args, VM* vm)
{
  (void)vm;
  (void)args_count;

  ObjectList* list = REQUIRE_LIST(args[0]);

  int index = AS_INT(args[1]);
  int normed_index = 0;
  if (!index__normalize_index(index, list->array.count, &normed_index)) {
    return ERROR_VALUE;
  }
  return list->array.values[normed_index];
}

static Value lox_list__set(int args_count, Value* args, VM* vm)
{
  (void)vm;
  (void)args_count;

  ObjectList* list = REQUIRE_LIST(args[0]);

  int index = AS_INT(args[1]);
  int normed_index = 0;
  if (!index__normalize_index(index, list->array.count, &normed_index)) {
    return ERROR_VALUE;
  }

  return (list->array.values[normed_index] = args[2]);
}

static Value lox_list__slice(int args_count, Value* args, VM* vm)
{
  (void)args_count;

  ObjectList* list = REQUIRE_LIST(args[0]);
  int start = AS_INT(args[1]);
  int end = list->array.count;
  if (args_count == 2 && !IS_NIL(args[2])) {
    end = AS_INT(args[2]);
  }

  int normed_start = 0;
  int normed_end = 0;
  if (!index__normalize_slice_bounds(
          start,
          end,
          list->array.count,
          "list",
          &normed_start,
          &normed_end
      )) {
    return ERROR_VALUE;
  }

  Value sliced = NIL_VALUE;
  WITH_OBJECTS_NURSERY(vm, {
    ObjectList* result = (ObjectList*)lox_list__new(list->instance._class, vm);
    for (int i = normed_start; i < normed_end; i++) {
      value_array__append(&result->array, list->array.values[i]);
    }
    sliced = OBJECT_VALUE(result);
  });
  return sliced;
}

static Value lox_list__clear(int args_count, Value* args, VM* vm)
{
  (void)vm;
  (void)args_count;

  ObjectList* list = REQUIRE_LIST(args[0]);

  value_array__dispose(&list->array);
  return NIL_VALUE;
}

static Value lox_list__pop(int args_count, Value* args, VM* vm)
{
  (void)vm;
  (void)args_count;

  ObjectList* list = REQUIRE_LIST(args[0]);
  if (list->array.count == 0) {
    fprintf(stderr, "Error: Cannot remove elements from an empty list.\n");
    return NIL_VALUE;
  }
  return value_array__pop(&list->array);
}

static void define_method(
    ObjectClass* _class,
    const char* name,
    NativeFunction native,
    const CallableSignature* signature,
    const char* docstring,
    VM* vm
)
{
  WITH_OBJECTS_NURSERY(vm, {
    ObjectString* method_name = string__copy(name, (int)strlen(name), vm);

    ObjectNativeFunction* native_fn =
        native_function__new(native, method_name, signature, docstring, vm);
    native_fn->is_method = true;

    table__set(&_class->methods, method_name, OBJECT_VALUE(native_fn));
  });
}

static void define_list_methods(ObjectClass* _class, VM* vm)
{
  // self.length() -> int
  static const CallableSignature length_signature =
      {.name = NULL, .arity = 0, .parameters = NULL, .return_type = "int"};
  define_method(
      _class,
      "length",
      lox_list__length,
      &length_signature,
      "Returns the number of elements in the list.",
      vm
  );

  // self.append(arg:any) -> nil
  static const CallableParameter append_parameters[] = {{"value", "any"}};
  static const CallableSignature append_signature =
      {.name = NULL, .arity = 1, .parameters = append_parameters, .return_type = "nil"};
  define_method(
      _class,
      "append",
      lox_list__append,
      &append_signature,
      "Appends a value to the end of the list.",
      vm
  );

  // self.at(index:int) -> any
  static const CallableParameter at_parameters[] = {{"index", "int"}};
  static const CallableSignature at_signature =
      {.name = NULL, .arity = 1, .parameters = at_parameters, .return_type = "any"};
  define_method(
      _class,
      "at",
      lox_list__at,
      &at_signature,
      "Returns the element at index (negative indexes are supported).",
      vm
  );

  // self.set(index:int, value:any) -> any
  static const CallableParameter set_parameters[] = {{"index", "int"}, {"value", "any"}};
  static const CallableSignature set_signature =
      {.name = NULL, .arity = 2, .parameters = set_parameters, .return_type = "any"};
  define_method(
      _class,
      "set",
      lox_list__set,
      &set_signature,
      "Sets the element at index and returns the assigned value.",
      vm
  );

  // self.slice(start:int, end:int=nil) -> list
  static const CallableParameter slice_parameters[] = {
      {"start", "int"},
      {"end", "int", "nil"}
  };
  static const CallableSignature slice_signature =
      {.name = NULL, .arity = 2, .parameters = slice_parameters, .return_type = "list"};
  define_method(
      _class,
      "slice",
      lox_list__slice,
      &slice_signature,
      "Returns sublist in [start, end).",
      vm
  );

  // self.clear() -> nil
  static const CallableSignature clear_signature =
      {.name = NULL, .arity = 0, .parameters = NULL, .return_type = "nil"};
  define_method(
      _class,
      "clear",
      lox_list__clear,
      &clear_signature,
      "Removes all elements from the list.",
      vm
  );

  // self.pop() -> any
  static const CallableSignature pop_signature =
      {.name = NULL, .arity = 0, .parameters = NULL, .return_type = "any"};
  define_method(
      _class,
      "pop",
      lox_list__pop,
      &pop_signature,
      "Removes and returns the last element.",
      vm
  );
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
