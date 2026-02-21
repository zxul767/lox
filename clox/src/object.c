#include <assert.h>
#include <stdio.h>
#include <string.h>

#include "cstring.h"
#include "lox_list.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"
#include "table.h"
#include "vm.h"

const char* OBJ_TYPE_TO_STRING[] = {FOREACH_OBJ_TYPE(GENERATE_STRING)};

static void track_object_for_gc(Object* object, VM* vm)
{
  object->is_alive = false;
  object->next = vm->objects;
  vm->objects = object;
}

Object* object__allocate(size_t size, ObjectType type, VM* vm)
{
  // We use `size` instead of `sizeof(Object)` because `Object` is always
  // a "base" object that is never allocated on its own, but rather as
  // part of a derived object (e.g., ObjectString). The correct size
  // is therefore known only by the caller, but we want to cast here
  // to `Object` so we can set `type` (only available in `Object`)
  //
  Object* object = (Object*)memory__reallocate(NULL, 0, size);
  object->type = type;

  track_object_for_gc(object, vm);

#ifdef DEBUG_LOG_GC_DETAILED
  fprintf(
      stderr,
      "%p allocate %zu bytes for %s\n",
      (void*)object,
      size,
      OBJ_TYPE_TO_STRING[type]
  );
#endif

  return object;
}

static ObjectString* string__allocate(char* chars, int length, uint32_t hash, VM* vm)
{
  ObjectString* string = NULL;
  WITH_OBJECTS_NURSERY(vm, {
    string = ALLOCATE_OBJECT(ObjectString, OBJECT_STRING, vm);
    string->length = length;
    string->chars = chars;
    string->hash = hash;

    table__set(&vm->interned_strings, string, NIL_VALUE);
  });
  return string;
}

static char get_escaped_char(const char* it, const char* end)
{
  if (*it == '\\' && (it + 1 < end)) {
    switch (*(it + 1)) {
    case 'n':
      return '\n';
    case 't':
      return '\t';
      break;
    case '\\':
      return '\\';
      break;
    default:
      return '\0';
    }
  }
  return *it;
}

static inline int advance_chars_count(const char* iter, const char* end)
{
  // for almost all escape sequences ("\n", "\t") the resulting escaped
  // character will be different than "\", except of course for the escape
  // sequence "\\"
  return (*iter == get_escaped_char(iter, end) && *iter != '\\') ? 1 : 2;
}

static int count_effective_chars(const char* string, int length)
{
  int count = 0;

  const char* iter = string;
  const char* end = string + length;
  while (iter < end) {
    iter += advance_chars_count(iter, end);
    count++;
  }
  return count;
}

// pre-condition: `source` contains, if any, only valid escape sequences
static void
copy_with_translated_escapes(char* destination, const char* source, int length)
{
  const char* iter = source;
  const char* end = source + length;
  while (iter < end) {
    char c = get_escaped_char(iter, end);
    iter += advance_chars_count(iter, end);
    *destination++ = c;
  }
}

ObjectString* string__copy(const char* chars, int length, VM* vm)
{
  uint32_t hash = cstr__hash(chars, length);
  ObjectString* interned = table__find_string(&vm->interned_strings, chars, length, hash);
  if (interned != NULL) {
    return interned;
  }

  int actual_length = count_effective_chars(chars, length);
  char* heap_chars = ALLOCATE(char, actual_length + 1);
  copy_with_translated_escapes(heap_chars, chars, length);
  heap_chars[actual_length] = '\0';

  return string__allocate(heap_chars, actual_length, hash, vm);
}

ObjectString* string__take_ownership(char* chars, int length, VM* vm)
{
  uint32_t hash = cstr__hash(chars, length);
  ObjectString* interned = table__find_string(&vm->interned_strings, chars, length, hash);
  if (interned != NULL) {
    FREE_ARRAY(char, chars, length + 1);
    return interned;
  }
  return string__allocate(chars, length, hash, vm);
}

bool string__equals_token(ObjectString* string, const Token* token)
{
  return string->length == token->length &&
         memcmp(string->chars, token->start, string->length) == 0;
}

ObjectUpvalue* upvalue__new(Value* slot, VM* vm)
{
  ObjectUpvalue* upvalue = ALLOCATE_OBJECT(ObjectUpvalue, OBJECT_UPVALUE, vm);
  upvalue->location = slot;
  upvalue->closed = NIL_VALUE;
  upvalue->next = NULL;

  return upvalue;
}

ObjectClass* class__new(ObjectString* name, VM* vm)
{
  ObjectClass* _class = ALLOCATE_OBJECT(ObjectClass, OBJECT_CLASS, vm);
  _class->name = name;
  _class->new_instance = instance__new;

  table__init(&_class->methods);

  return _class;
}

ObjectInstance* instance__new(ObjectClass* _class, VM* vm)
{
  ObjectInstance* instance = ALLOCATE_OBJECT(ObjectInstance, OBJECT_INSTANCE, vm);
  instance->_class = _class;
  table__init(&instance->fields);

  return instance;
}

ObjectBoundMethod* bound_method__new(Value instance, Value method, VM* vm)
{
  ObjectBoundMethod* bound = ALLOCATE_OBJECT(ObjectBoundMethod, OBJECT_BOUND_METHOD, vm);
  bound->instance = instance;
  bound->method = method;

  return bound;
}

ObjectClosure* closure__new(ObjectFunction* function, VM* vm)
{
  ObjectUpvalue** upvalues = ALLOCATE(ObjectUpvalue*, function->upvalues_count);
  for (int i = 0; i < function->upvalues_count; i++) {
    upvalues[i] = NULL;
  }
  ObjectClosure* closure = ALLOCATE_OBJECT(ObjectClosure, OBJECT_CLOSURE, vm);
  closure->function = function;
  closure->upvalues = upvalues;
  closure->upvalues_count = function->upvalues_count;

  return closure;
}

ObjectFunction* function__new(VM* vm)
{
  ObjectFunction* function = ALLOCATE_OBJECT(ObjectFunction, OBJECT_FUNCTION, vm);

  ObjectCallable* callable = AS_CALLABLE(function);
  callable->signature.name = NULL;
  callable->signature.arity = 0;
  callable->signature.parameters = NULL;
  callable->signature.return_type = "any";
  callable->docstring = NULL;

  function->upvalues_count = 0;
  bytecode__init(&function->bytecode);

  return function;
}

static void assert_defaults_are_trailing(const CallableSignature* signature)
{
  assert(signature != NULL && "signature cannot be NULL");
  assert(
      signature->arity == 0 ||
      (signature->parameters != NULL && "parameters cannot be NULL when arity > 0")
  );

  bool seen_default = false;
  for (int i = 0; i < signature->arity; i++) {
    bool has_default = signature->parameters[i].default_value_repr != NULL;
    if (has_default) {
      seen_default = true;
    } else {
      assert(!seen_default && "parameters with default values must be trailing");
    }
  }
}

ObjectNativeFunction* native_function__new(
    NativeFunction primitive,
    ObjectString* name,
    const CallableSignature* signature,
    const char* docstring,
    VM* vm
)
{
  assert_defaults_are_trailing(signature);

  ObjectNativeFunction* native =
      ALLOCATE_OBJECT(ObjectNativeFunction, OBJECT_NATIVE_FUNCTION, vm);

  ObjectCallable* callable = AS_CALLABLE(native);
  callable->signature = *signature;
  callable->signature.name = name;
  callable->docstring =
      docstring != NULL ? string__copy(docstring, (int)strlen(docstring), vm) : NULL;

  native->function = primitive;
  native->is_method = false;

  return native;
}

static void print_function(const ObjectFunction* function)
{
  ObjectCallable* callable = AS_CALLABLE(function);
  if (callable->signature.name == NULL) {
    fprintf(stderr, "<script>");
    return;
  }
  fprintf(stderr, "<fn %s>", callable->signature.name->chars);
}

void object__print(Value value)
{
  switch (OBJECT_TYPE(value)) {
  case OBJECT_CLASS:
    fprintf(stderr, "<class %s>", AS_CLASS(value)->name->chars);
    break;
  case OBJECT_CALLABLE:
    fprintf(stderr, "<callable>");
    break;
  case OBJECT_BOUND_METHOD:
    fprintf(stderr, "bound:");
    object__print(AS_BOUND_METHOD(value)->method);
    break;
  case OBJECT_CLOSURE:
    print_function(AS_CLOSURE(value)->function);
    break;
  case OBJECT_FUNCTION:
    print_function(AS_FUNCTION(value));
    break;
  case OBJECT_NATIVE_FUNCTION:
    fprintf(stderr, "<native fn>");
    break;
  case OBJECT_INSTANCE:
    fprintf(stderr, "<%s instance>", AS_INSTANCE(value)->_class->name->chars);
    break;
  case OBJECT_STRING:
    fprintf(stderr, "%s", AS_CSTRING(value));
    break;
  case OBJECT_LIST:
    lox_list__print(AS_LIST(value));
    /* fprintf(stderr, "<native list>"); */
    break;
  case OBJECT_UPVALUE:
    fprintf(stderr, "upvalue");
    break;
  }
}

void object__print_repr(Value value)
{
  switch (OBJECT_TYPE(value)) {
  case OBJECT_STRING:
    fprintf(stderr, "\"%s\"", AS_CSTRING(value));
    break;
  default:
    value__print(value);
    break;
  }
}
