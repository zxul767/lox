#include <stdio.h>
#include <string.h>
#include <time.h>

#include "lox_stdlib.h"
#include "object.h"
#include "table.h"

// TODO: check function arity against arguments passed in every function

// returns the elapsed time since the program started running, in fractional
// seconds
Value clock_native(int args_count, Value* args)
{
  return NUMBER_VAL((double)clock() / CLOCKS_PER_SEC);
}

Value print(int args_count, Value* args)
{
  value__print(args[0]);
  return NIL_VAL;
}

Value println(int args_count, Value* args)
{
  value__println(args[0]);
  return NIL_VAL;
}

static void print_signature_parameter(const CallableSignature* signature, int index)
{
  char fallback_parameter_name[16];
  snprintf(fallback_parameter_name, sizeof(fallback_parameter_name), "arg%d", index + 1);

  const char* parameter_name = fallback_parameter_name;
  const char* parameter_type = "any";
  if (signature->parameters != NULL) {
    if (signature->parameters[index].name != NULL) {
      parameter_name = signature->parameters[index].name;
    }
    if (signature->parameters[index].type != NULL) {
      parameter_type = signature->parameters[index].type;
    }
  }
  fprintf(stderr, "%s:%s", parameter_name, parameter_type);
}

static void print_signature_parameters(const CallableSignature* signature)
{
  for (int i = 0; i < signature->arity; i++) {
    if (i > 0) {
      fprintf(stderr, ", ");
    }
    print_signature_parameter(signature, i);
  }
}

static void print_signature(const CallableSignature* signature, const char* fallback_name)
{
  const char* name = signature->name != NULL ? signature->name->chars : fallback_name;

  fprintf(stderr, "%s(", name);
  print_signature_parameters(signature);
  fprintf(stderr, ") -> %s", signature->return_type);
}

static void print_docstring(ObjectString* docstring)
{
  if (docstring != NULL) {
    fprintf(stderr, " | %s", docstring->chars);
  }
}

static void
print_callable_details(const ObjectCallable* callable, const char* fallback_name)
{
  print_signature(&callable->signature, fallback_name);
  print_docstring(callable->docstring);
}

static ObjectCallable* get_callable_from_value(Value value)
{
  if (!IS_OBJECT(value)) {
    return NULL;
  }

  switch (OBJECT_TYPE(value)) {
  case OBJECT_CLOSURE:
    return AS_CALLABLE(AS_CLOSURE(value)->function);
  case OBJECT_FUNCTION:
    return AS_CALLABLE(AS_FUNCTION(value));
  case OBJECT_NATIVE_FUNCTION:
    return AS_CALLABLE(AS_NATIVE_FUNCTION(value));
  case OBJECT_BOUND_METHOD:
    return get_callable_from_value(AS_BOUND_METHOD(value)->method);
  case OBJECT_CALLABLE:
    return AS_CALLABLE(AS_OBJECT(value));
  default:
    return NULL;
  }
}

static const char* type_description(Value value)
{
  switch (value.type) {
  case VAL_BOOL:
    return "boolean";
  case VAL_NIL:
    return "nil";
  case VAL_NUMBER:
    return "number";
  case VAL_ERROR:
    return "error";
  case VAL_OBJECT:
    break;
  }

  switch (OBJECT_TYPE(value)) {
  case OBJECT_CLASS:
    return "class";
  case OBJECT_CLOSURE:
  case OBJECT_FUNCTION:
    return "function";
  case OBJECT_NATIVE_FUNCTION:
    return "native function";
  case OBJECT_BOUND_METHOD:
    return "bound method";
  case OBJECT_INSTANCE:
  case OBJECT_LIST:
    return AS_INSTANCE(value)->_class->name->chars;
  case OBJECT_STRING:
    return "string";
  case OBJECT_UPVALUE:
    return "upvalue";
  case OBJECT_CALLABLE:
    return "callable";
  }

  return "unknown";
}

static bool is_initializer_name(const ObjectString* name)
{
  return !strcmp(name->chars, "__init__");
}

static ObjectCallable* find_initializer(const ObjectClass* _class)
{
  for (int i = 0; i < _class->methods.capacity; i++) {
    Entry* entry = &_class->methods.entries[i];
    if (entry->key == NULL || !is_initializer_name(entry->key)) {
      continue;
    }
    return get_callable_from_value(entry->value);
  }
  return NULL;
}

static int count_non_initializer_methods(const ObjectClass* _class)
{
  int count = 0;
  for (int i = 0; i < _class->methods.capacity; i++) {
    Entry* entry = &_class->methods.entries[i];
    if (entry->key != NULL && !is_initializer_name(entry->key)) {
      count++;
    }
  }
  return count;
}

static CallableSignature
build_constructor_signature(const ObjectClass* _class, const ObjectCallable* initializer)
{
  CallableSignature constructor_signature = {
      .name = _class->name,
      .arity = initializer != NULL ? initializer->signature.arity : 0,
      .parameters = initializer != NULL ? initializer->signature.parameters : NULL,
      .return_type = _class->name->chars
  };

  return constructor_signature;
}

static void print_constructor_help(const ObjectClass* _class)
{
  ObjectCallable* initializer = find_initializer(_class);
  CallableSignature constructor_signature =
      build_constructor_signature(_class, initializer);

  fprintf(stderr, "constructor: ");
  print_signature(&constructor_signature, _class->name->chars);
  print_docstring(initializer != NULL ? initializer->docstring : NULL);
  fprintf(stderr, "\n");
}

static void print_method_entry(const Entry* entry)
{
  ObjectCallable* callable = get_callable_from_value(entry->value);
  fprintf(stderr, "  - ");
  if (callable != NULL) {
    print_callable_details(callable, entry->key->chars);
  } else {
    fprintf(stderr, "%s(?)", entry->key->chars);
  }
  fprintf(stderr, "\n");
}

static void print_method_signatures(const ObjectClass* _class)
{
  print_constructor_help(_class);
  fprintf(stderr, "methods: %d\n", count_non_initializer_methods(_class));
  for (int i = 0; i < _class->methods.capacity; i++) {
    const Entry* entry = &_class->methods.entries[i];
    if (entry->key == NULL || is_initializer_name(entry->key)) {
      continue;
    }
    print_method_entry(entry);
  }
}

static Value help_class(const Value value)
{
  ObjectClass* _class = AS_CLASS(value);
  fprintf(stderr, "[class] <class %s>\n", _class->name->chars);
  print_method_signatures(_class);
  return NIL_VAL;
}

static Value help_instance(const Value value)
{
  ObjectInstance* instance = AS_INSTANCE(value);
  fprintf(
      stderr,
      "[%s] <%s instance>\n",
      instance->_class->name->chars,
      instance->_class->name->chars
  );
  fprintf(
      stderr,
      "Use help(%s) to inspect constructor and methods.\n",
      instance->_class->name->chars
  );
  return NIL_VAL;
}

static bool try_help_callable(const Value value)
{
  ObjectCallable* callable = get_callable_from_value(value);
  if (callable == NULL || callable->signature.name == NULL) {
    return false;
  }

  fprintf(stderr, "[%s] ", type_description(value));
  print_callable_details(callable, callable->signature.name->chars);
  fprintf(stderr, "\n");
  return true;
}

Value help(int args_count, Value* args)
{
  // `help` always takes exactly one argument; we keep this to satisfy the
  // shared native-function signature and avoid unused-parameter warnings.
  (void)args_count;

  Value value = args[0];

  if (IS_OBJECT(value)) {
    switch (OBJECT_TYPE(value)) {
    case OBJECT_CLASS: {
      return help_class(value);
    }
    case OBJECT_INSTANCE:
    case OBJECT_LIST: {
      return help_instance(value);
    }
    default: {
      if (try_help_callable(value)) {
        return NIL_VAL;
      }
      break;
    }
    }
  }

  value__print_repr(value);
  fprintf(stderr, " [%s]\n", type_description(value));

  return NIL_VAL;
}
