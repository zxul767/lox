#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "lox_string.h"
#include "memory.h"

// in all native methods, the first argument is always `this` (i.e., the
// instance/receiver)
static Value lox_string__length(int args_count, Value* args, VM* vm)
{
  (void)vm;
  (void)args_count;
  ObjectString* self = REQUIRE_STRING(args[0]);
  return NUMBER_VALUE(self->length);
}

static Value lox_string__starts_with(int args_count, Value* args, VM* vm)
{
  (void)vm;
  (void)args_count;
  ObjectString* self = REQUIRE_STRING(args[0]);
  ObjectString* prefix = REQUIRE_STRING(args[1]);

  if (prefix->length > self->length) {
    return BOOL_VALUE(false);
  }
  return BOOL_VALUE(strncmp(self->chars, prefix->chars, prefix->length) == 0);
}

static Value lox_string__ends_with(int args_count, Value* args, VM* vm)
{
  (void)vm;
  (void)args_count;
  ObjectString* self = REQUIRE_STRING(args[0]);
  ObjectString* suffix = REQUIRE_STRING(args[1]);

  if (suffix->length > self->length) {
    return BOOL_VALUE(false);
  }

  return BOOL_VALUE(
      strncmp(
          self->chars + (self->length - suffix->length),
          suffix->chars,
          suffix->length
      ) == 0
  );
}

static Value lox_string__index_of(int args_count, Value* args, VM* vm)
{
  (void)vm;
  (void)args_count;
  ObjectString* self = REQUIRE_STRING(args[0]);
  ObjectString* target = REQUIRE_STRING(args[1]);

  if (target->length == 0) {
    return NUMBER_VALUE(0);
  }
  if (target->length > self->length) {
    return NUMBER_VALUE(-1);
  }

  for (int i = 0; i <= self->length - target->length; i++) {
    if (strncmp(self->chars + i, target->chars, target->length) == 0) {
      return NUMBER_VALUE(i);
    }
  }
  return NUMBER_VALUE(-1);
}

static Value lox_string__slice(int args_count, Value* args, VM* vm)
{
  (void)args_count;

  ObjectString* self = REQUIRE_STRING(args[0]);
  int start = AS_INT(args[1]);
  int end = AS_INT(args[2]);

  if (self->length == 0) {
    fprintf(stderr, "Index Error: Cannot slice an empty string.\n");
    return ERROR_VALUE;
  }
  if (start < 0 || start >= self->length) {
    fprintf(
        stderr,
        "Index Error: start index %d is out of range [0..%d].\n",
        start,
        self->length - 1
    );
    return ERROR_VALUE;
  }
  if (end < 0 || end > self->length) {
    fprintf(
        stderr,
        "Index Error: end index %d is out of range [0..%d].\n",
        end,
        self->length
    );
    return ERROR_VALUE;
  }
  if (start > end) {
    fprintf(
        stderr,
        "Index Error: start index %d cannot be greater than end index %d.\n",
        start,
        end
    );
    return ERROR_VALUE;
  }

  int length = end - start;
  char* chars = ALLOCATE(char, length + 1);
  memcpy(chars, self->chars + start, length);
  chars[length] = '\0';

  return OBJECT_VALUE(string__take_ownership(chars, length, vm));
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

static void define_string_methods(ObjectClass* _class, VM* vm)
{
  // self.length() -> int
  static const CallableSignature length_signature =
      {.name = NULL, .arity = 0, .parameters = NULL, .return_type = "int"};
  define_method(
      _class,
      "length",
      lox_string__length,
      &length_signature,
      "Returns the string length.",
      vm
  );

  // self.starts_with(prefix:str) -> bool
  static const CallableParameter starts_with_parameters[] = {{"prefix", "str"}};
  static const CallableSignature starts_with_signature = {
      .name = NULL,
      .arity = 1,
      .parameters = starts_with_parameters,
      .return_type = "bool"
  };
  define_method(
      _class,
      "starts_with",
      lox_string__starts_with,
      &starts_with_signature,
      "Returns true if string starts with prefix.",
      vm
  );

  // self.ends_with(suffix:str) -> bool
  static const CallableParameter ends_with_parameters[] = {{"suffix", "str"}};
  static const CallableSignature ends_with_signature =
      {.name = NULL, .arity = 1, .parameters = ends_with_parameters, .return_type = "bool"
      };
  define_method(
      _class,
      "ends_with",
      lox_string__ends_with,
      &ends_with_signature,
      "Returns true if string ends with suffix.",
      vm
  );

  // self.index_of(target:str) -> int
  static const CallableParameter index_of_parameters[] = {{"target", "str"}};
  static const CallableSignature index_of_signature =
      {.name = NULL, .arity = 1, .parameters = index_of_parameters, .return_type = "int"};
  define_method(
      _class,
      "index_of",
      lox_string__index_of,
      &index_of_signature,
      "Returns first index of target, or -1 if not found.",
      vm
  );

  // self.slice(start:int, end:int) -> str
  static const CallableParameter slice_parameters[] = {{"start", "int"}, {"end", "int"}};
  static const CallableSignature slice_signature =
      {.name = NULL, .arity = 2, .parameters = slice_parameters, .return_type = "str"};
  define_method(
      _class,
      "slice",
      lox_string__slice,
      &slice_signature,
      "Returns substring in [start, end).",
      vm
  );
}

ObjectClass* lox_string__new_class(const char* name, VM* vm)
{
  ObjectClass* _class = NULL;
  WITH_OBJECTS_NURSERY(vm, {
    ObjectString* class_name = string__copy(name, strlen(name), vm);
    _class = class__new(class_name, vm);
    define_string_methods(_class, vm);
  });

  return _class;
}
