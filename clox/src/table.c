#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "memory.h"
#include "object.h"
#include "table.h"
#include "value.h"

// OVERVIEW
//
// this hash table implementation uses linear probing as its collision
// resolution strategy and tombstones as a way to handle deletions.
//
// a tombstone is an entry that has been previously deleted but wasn't
// marked as an empty entry to avoid breaking one or more probe sequences;
// it is, therefore, an entry that can be recycled.
//
// we consider tombstones to be full buckets for the purposes of computing the
// effective count of the table. this risks the possibility of ending with a
// bigger array than we probably need because it artificially inflates the load
// factor, but the alternative risks ending with no actual empty buckets to
// terminate a lookup, thus falling into an infinite loop.
//
// the effective count of the table (for the purposes of computing the load
// factor) is therefore: # of used entries + # of tombstones.
//
// for full details, see https://craftinginterpreters.com/hash-tables.html
//
void table__init(Table* table)
{
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void table__dispose(Table* table)
{
  FREE_ARRAY(Entry, table->entries, table->capacity);
  table__init(table);
}

// returns the first entry that either matches `key` or an available* entry if
// there is no match.
//
// * an entry is available either because it's never been used or because it's
// marked for recycling, i.e., it has been previously deleted and marked with a
// tombstone.
static Entry* find_entry(Entry* entries, int capacity, ObjectString* key)
{
  uint32_t index = key->hash % capacity;
  Entry* tombstone = NULL;

  for (;;) {
    Entry* entry = &entries[index];
    // an available entry (never used or a tombstone)
    if (entry->key == NULL) {
      // we found an empty entry (i.e., a [key:NULL, value:NIL_VALUE] entry)
      if (IS_NIL(entry->value)) {
        // ...but we recycle a tombstone if possible.
        return tombstone != NULL ? tombstone : entry;

      } else {
        // we found a tombstone (i.e., a [key:NULL, value:TRUE] entry);
        // let's ready it for recycling (unless another tombstone has already
        // been appointed) and continue the current probe sequence*.
        if (tombstone == NULL)
          tombstone = entry;
        // * we could return immediately if all we were looking for was an
        // available entry; however, since we're also looking for an entry
        // matching `key`, we can't just stop (after all, tombstones are just
        // part of probe sequences.)
      }
    } else if (entry->key == key) {
      // we found the key, we're done
      return entry;
    }
    index = (index + 1) % capacity;
  }
  // in theory, as long as we adjust the table's size before it gets too loaded,
  // we should always find an entry, so this statement should never be reached
  assert(false);
  return NULL; // unreachable
}

static Entry* allocate_entries(int capacity)
{
  Entry* entries = ALLOCATE(Entry, capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].value = NIL_VALUE;
  }
  return entries;
}

static int
copy_entries_from(const Table* table, Entry* new_entries, int new_capacity)
{
  // since we're not copying tombstones, the count will need to be recomputed
  int new_count = 0;
  for (int i = 0; i < table->capacity; i++) {
    Entry* old_entry = &table->entries[i];
    // this excludes copying empty new_entries but also tombstones (copying
    // tombstones doesn't add any value since we're rebuilding the probe
    // sequences anyway)
    if (old_entry->key == NULL)
      continue;

    Entry* new_entry = find_entry(new_entries, new_capacity, old_entry->key);
    new_entry->key = old_entry->key;
    new_entry->value = old_entry->value;
    new_count++;
  }
  return new_count;
}

static void adjust_capacity(Table* table, int new_capacity)
{
  Entry* new_entries = allocate_entries(new_capacity);
  // there is a new count because we're not copying tombstones
  int new_count = copy_entries_from(table, new_entries, new_capacity);
  FREE_ARRAY(Entry, table->entries, table->capacity);

  table->count = new_count;
  table->entries = new_entries;
  table->capacity = new_capacity;
}

// returns true if a new entry was set; otherwise false (i.e., when an
// existing entry was simply updated)
bool table__set(Table* table, ObjectString* key, Value value)
{
  // grow table if overly loaded
  if (table->count + 1 > table->capacity * TABLE_MAX_LOAD) {
    int capacity = GROW_CAPACITY(table->capacity);
    adjust_capacity(table, capacity);
  }

  Entry* entry = find_entry(table->entries, table->capacity, key);
  bool is_new_key = entry->key == NULL;

  // we only increment the count for never-before used entries because
  // tombstones are considered full buckets for the purposes of the loading
  // factor (see the OVERVIEW at the top of this file for more details.)
  if (is_new_key && IS_NIL(entry->value))
    table->count++;

  entry->key = key;
  entry->value = value;

  return is_new_key;
}

bool table__get(const Table* table, ObjectString* key, Value* out_value)
{
  if (table->count == 0)
    return false;

  Entry* entry = find_entry(table->entries, table->capacity, key);
  if (entry->key == NULL)
    return false;

  *out_value = entry->value;
  return true;
}

bool table__delete(Table* table, ObjectString* key)
{
  if (table->count == 0)
    return false;

  Entry* entry = find_entry(table->entries, table->capacity, key);
  if (entry->key == NULL)
    return false;

  // place a tombstone in the entry
  entry->key = NULL;
  entry->value = BOOL_VALUE(true);
  // notice we don't reduce the count because tombstones are considered
  // full buckets for the purposes of the loading factor (see the OVERVIEW at
  // the top of this file for more details.)
  return true;
}

void table__add_all(Table* from, Table* to)
{
  for (int i = 0; i < from->capacity; i++) {
    Entry* entry = &from->entries[i];
    if (entry->key != NULL) {
      table__set(to, entry->key, entry->value);
    }
  }
}

// The implementation of `find_entry` relies on comparing `entry->key == key`,
// but this comparison only returns true when both strings are the exact same
// object in memory, not when they are different objects with the same
// characters. One way to fix this would be to change the comparison to be
// character-based, but that would be slow. Instead, by "interning" all strings
// in the VM, we ensure that any two strings with the same characters actually
// point to the same object in memory, thereby fixing `find_entry`.
//
// This function is here to help us implement "string interning", so it's quite
// similar to `find_entry`. Unlike `find_entry` though, we only need it to
// either find an existing string or return NULL when the string hasn't been
// interned yet, so the loop can be a bit simpler.
//
// See https://craftinginterpreters.com/hash-tables.html#string-interning for
// details.
//
ObjectString* table__find_string(
    const Table* table, const char* chars, int length, uint32_t hash)
{
  if (table->count == 0)
    return NULL;

  uint32_t index = hash % table->capacity;
  for (;;) {
    Entry* entry = &table->entries[index];
    if (entry->key == NULL) {
      // stop if we found an empty non-tombstone entry (probe sequences always
      // terminate at [key:NULL, value:NIL] entries)
      if (IS_NIL(entry->value))
        return NULL;

      // because string interning will be a frequent operation in every program
      // (remember that they're used not just for string literals, but also for
      // variables), it's very important that it is as efficient as possible,
      // hence why we do simpler comparisons (length and hash) first so we can
      // rule out negative cases faster.
    } else if (
        entry->key->length == length && entry->key->hash == hash &&
        !memcmp(entry->key->chars, chars, length)) {
      // we found it
      return entry->key;
    }
    index = (index + 1) % table->capacity;
  }
  // in theory, we should never get here, since the table will always have
  // empty entries that terminate probe sequences.
  assert(false);
  return NULL; // unreachable
}

void table__mark_as_alive(const Table* table)
{
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    memory__mark_object_as_alive((Object*)entry->key);
    memory__mark_value_as_alive(entry->value);
  }
}

void table__remove_dead_objects(Table* table)
{
  for (int i = 0; i < table->capacity; i++) {
    Entry* entry = &table->entries[i];
    if (entry->key != NULL && !entry->key->object.is_alive) {
      table__delete(table, entry->key);
    }
  }
}
