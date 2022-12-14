#ifndef LOX_LIST_H_
#define LOX_LIST_H_

#include "object.h"
#include "value.h"

typedef struct ObjectList {
  ObjectInstance instance;

  ValueArray array;

} ObjectList;

#define IS_LIST(value) is_object_type(value, OBJECT_LIST)
#define AS_LIST(value) ((ObjectList*)AS_OBJECT(value))
#define REQUIRE_LIST(value) (assert(IS_LIST(value)), AS_LIST(value))

ObjectClass* lox_list__new_class(const char* name, VM* vm);
void lox_list__print(const ObjectList*);

void lox_list__mark_as_alive(ObjectList*);

#endif // LOX_LIST_H_
