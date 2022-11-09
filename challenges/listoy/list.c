#include "list.h"
#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

typedef struct node {
  struct node *previous;
  struct node *next;
  char *value;
} Node;

typedef struct list {
  Node *head;
  Node *tail;
  size_t count;
} List;

typedef struct list_iterator {
  const Node *end;
  const Node *current;
  Node *(*next)(const Node *);
} ListIterator;

#define LIST__FOREACH(list)                                                    \
  for (Node *it = list->head->next; it != list->tail; it = it->next)

#define LIST__FOREACH_REVERSED(list)                                           \
  for (Node *it = list->tail->previous; it != list->head; it = it->previous)

// ----------------------------------------------------------------------------
// List Node
// ----------------------------------------------------------------------------
static Node *node__new(Node *previous, Node *next, char *value) {
  Node *node = (Node *)malloc(sizeof(Node));
  node->previous = previous;
  node->next = next;
  node->value = value;

  return node;
}

static Node *node__new_empty() {
  return node__new(/*previous:*/ NULL, /*next:*/ NULL, /*value:*/ NULL);
}

static void node__dispose(Node *node) {
  free(node->value);
  free(node);
}

// We only use these in the iteration functions to have a single `list__next`
// function regardless of the direction of the iterator
static Node *node__next(const Node *node) { return node->next; }
static Node *node__previous(const Node *node) { return node->previous; }

// ----------------------------------------------------------------------------
// Constructor/Destructor
// ----------------------------------------------------------------------------
List *list__new() {
  List *list = (List *)malloc(sizeof(List));
  list->head = node__new_empty();
  list->tail = node__new_empty();
  list->count = 0;

  // `list->head` and `list->tail` are simply anchor points to the
  // beginning and end of the list that make the code to insert and
  // delete nodes much easier; they should not be deleted except
  // by `list__dispose`
  list->head->next = list->tail;
  list->tail->previous = list->head;

  return list;
}

void list__dispose(List *list) {
  Node *it = list->head->next;
  while (it != list->tail) {
    it = it->next;
    node__dispose(it->previous);
  }
  node__dispose(list->head);
  node__dispose(list->tail);
  free(list);
}

// ----------------------------------------------------------------------------
// Getters
// ----------------------------------------------------------------------------
char *list__first(const List *list) { return list->head->next->value; }
char *list__last(const List *list) { return list->tail->previous->value; }
size_t list__count(const List *list) { return list->count; }

// ----------------------------------------------------------------------------
// Setters
// ----------------------------------------------------------------------------
static void node__link_after(Node *anchor, Node *new_node) {
  Node *next_node = anchor->next;
  new_node->next = next_node;
  next_node->previous = new_node;
  anchor->next = new_node;
  new_node->previous = anchor;
}

static char *string__copy(const char *source) {
  size_t length = strlen(source);
  char *destination = malloc(sizeof(char) * (length + 1));
  memcpy(destination, source, length + 1);

  return destination;
}

void list__append(const char *value, List *list) {
  Node *new_node = node__new(/* previous: */ NULL, /* next: */ NULL,
                             /* value:  */ string__copy(value));
  node__link_after(list->tail->previous, new_node);
  list->count++;
}

void list__prepend(const char *value, List *list) {
  Node *new_node = node__new(/* previous: */ NULL, /* next: */ NULL,
                             /* value:  */ string__copy(value));
  node__link_after(list->head, new_node);
  list->count++;
}

// ----------------------------------------------------------------------------
// Deleters
// ----------------------------------------------------------------------------
static void node__unlink(Node *node) {
  node->previous->next = node->next;
  node->next->previous = node->previous;
}

static Node *list__find(const char *target, const List *list) {
  LIST__FOREACH(list) {
    if (!strcmp(target, it->value))
      return it;
  }
  return NULL;
}

bool list__delete(const char *target, List *list) {
  Node *node = list__find(target, list);
  if (node != NULL) {
    node__unlink(node);
    node__dispose(node);
    list->count--;
    return true;
  }
  return false;
}

// ----------------------------------------------------------------------------
// Predicates
// ----------------------------------------------------------------------------
bool list__contains(const char *text, const List *list) {
  Node *node = list__find(text, list);
  return node != NULL;
}

// ----------------------------------------------------------------------------
// Iteration
// ----------------------------------------------------------------------------
ListIterator *list__iterate(const List *list) {
  ListIterator *iter = (ListIterator *)malloc(sizeof(ListIterator));
  iter->end = list->tail;
  iter->current = list->head->next;
  iter->next = node__next;
  return iter;
}

ListIterator *list__reverse_iterate(const List *list) {
  ListIterator *iter = (ListIterator *)malloc(sizeof(ListIterator));
  iter->end = list->head;
  iter->current = list->tail->previous;
  iter->next = node__previous;
  return iter;
}

void list__dispose_iterator(ListIterator *list) { free(list); }

char *list__next(ListIterator *iterator) {
  if (iterator->current == iterator->end) {
    return NULL;
  }
  char *value = iterator->current->value;
  iterator->current = iterator->next(iterator->current);
  return value;
}

// ----------------------------------------------------------------------------
// Debugging
// ----------------------------------------------------------------------------
void list__dump(const List *list) {
  LIST__FOREACH(list) { printf("%s", it->value); }
  printf("\n");
}

void list__dump_reversed(const List *list) {
  LIST__FOREACH_REVERSED(list) { printf("%s", it->value); }
  printf("\n");
}
