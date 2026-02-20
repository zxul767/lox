#ifndef LOX_STRING_H_
#define LOX_STRING_H_

#include <assert.h>

#include "object.h"
#include "value.h"

#define REQUIRE_STRING(value) (assert(IS_STRING(value)), AS_STRING(value))

ObjectClass* lox_string__new_class(const char* name, VM* vm);

#endif // LOX_STRING_H_
