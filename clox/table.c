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
void table__init(Table* table) {
  table->count = 0;
  table->capacity = 0;
  table->entries = NULL;
}

void table__dispose(Table* table) {
  FREE_ARRAY(Entry, table->entries, table->capacity);
  table__init(table);
}

// returns the first entry that either matches `key` or an available* entry if
// there is no match.
//
// * an entry is available either because it's never been used or because it's
// marked for recycling, i.e., it has been previously deleted and marked with a
// tombstone.
static Entry* find_entry(Entry* entries, int capacity, ObjectString* key) {
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
  return NULL; // unreachable
}

static Entry* allocate_entries(int capacity) {
  Entry* entries = ALLOCATE(Entry, capacity);
  for (int i = 0; i < capacity; i++) {
    entries[i].key = NULL;
    entries[i].value = NIL_VAL;
  }
  return entries;
}

static int copy_entries_from(const Table* table, Entry* new_entries,
                             int new_capacity) {
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

static void adjust_capacity(Table* table, int new_capacity) {
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
bool table__set(Table* table, ObjectString* key, Value value) {
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

bool table__get(Table* table, ObjectString* key, Value* out_value) {
  if (table->count == 0)
    return false;

  Entry* entry = find_entry(table->entries, table->capacity, key);
  if (entry->key == NULL)
    return false;

  *out_value = entry->value;
  return true;
}

bool table__delete(Table* table, ObjectString* key) {
  if (table->count == 0)
    return false;

  Entry* entry = find_entry(table->entries, table->capacity, key);
  if (entry->key == NULL)
    return false;

  // place a tombstone in the entry
  entry->key = NULL;
  entry->value = BOOL_VAL(true);
  // notice we don't reduce the count because tombstones are considered
  // full buckets for the purposes of the loading factor (see the OVERVIEW at
  // the top of this file for more details.)

  return true;
}

void table__add_all(Table* from, Table* to) {
  for (int i = 0; i < from->capacity; i++) {
    Entry* entry = &from->entries[i];
    if (entry->key != NULL) {
      table__set(to, entry->key, entry->value);
    }
  }
}
