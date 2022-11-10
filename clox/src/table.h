#ifndef TABLE_H_
#define TABLE_H_

#include "common.h"
#include "value.h"

#define TABLE_MAX_LOAD 0.75

typedef struct Entry {
  ObjectString* key;
  Value value;
} Entry;

typedef struct Table {
  int count;
  int capacity;
  Entry* entries;
} Table;

void table__init(Table* table);
void table__dispose(Table* table);

bool table__set(Table* table, ObjectString* key, Value value);
bool table__get(const Table* table, ObjectString* key, Value* out_value);
bool table__delete(Table* table, ObjectString* key);

ObjectString* table__find_string(
    const Table* table, const char* chars, int length, uint32_t hash);

void table__add_all(Table* from, Table* to);

void table__mark_as_alive(const Table* table);
void table__remove_unvisited_objects(Table* table);

#endif // TABLE_H_
