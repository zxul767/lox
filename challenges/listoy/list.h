#ifndef LIST_H_
#define LIST_H_

#include <stdbool.h>
#include <stdlib.h>

typedef struct list List;
typedef struct list_iterator ListIterator;

// Constructor/Destructor
List *list__new();
void list__dispose(List *list);

// Getters
char *list__first(const List *list);
char *list__last(const List *list);
size_t list__count(const List *list);

// Setters
void list__prepend(const char *text, List *list);
void list__append(const char *text, List *list);

// Deleters
bool list__delete(const char *text, List *list);

// Predicates
bool list__contains(const char *text, const List *list);

// Iteration
ListIterator *list__iterate(const List *list);
ListIterator *list__reverse_iterate(const List *list);
void list__dispose_iterator(ListIterator *list);

char *list__next(ListIterator *iterator);

// Debugging
void list__dump(const List *list);
void list__dump_reversed(const List *list);

#endif // LIST_H_
