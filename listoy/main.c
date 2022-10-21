#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "list.h"

#define LIST_TEST(body)                                                        \
  {                                                                            \
    putchar('.');                                                              \
    List *list = list__new();                                                  \
    body list__dispose(list);                                                  \
  }

void test_can_preprend_single_element() {
  LIST_TEST({
    assert(!list__contains("first", list));
    list__prepend("first", list);

    assert(list__contains("first", list));
    assert(1 == list__count(list));
    assert(!strcmp("first", list__first(list)));
    assert(!strcmp("first", list__last(list)));
  })
}

void test_can_append_single_element() {
  LIST_TEST({
    assert(!list__contains("last", list));
    list__append("last", list);

    assert(list__contains("last", list));
    assert(1 == list__count(list));
    assert(!strcmp("last", list__first(list)));
    assert(!strcmp("last", list__last(list)));
  })
}

void test_can_delete_any_element() {
  LIST_TEST({
    list__prepend("first", list);
    list__append("last", list);

    list__delete("first", list);
    assert(list__contains("last", list));
    assert(!list__contains("first", list));
    assert(1 == list__count(list));
  })
}

void test_can_get_first_element() {
  LIST_TEST({
    list__prepend("first", list);
    list__append("last", list);

    assert(!strcmp("first", list__first(list)));
  })
}

void test_can_get_last_element() {
  LIST_TEST({
    list__prepend("first", list);
    list__append("last", list);

    assert(!strcmp("last", list__last(list)));
  })
}

void test_first_element_should_be_null_on_empty_list() {
  LIST_TEST({ assert(list__first(list) == NULL); })
}

void test_last_element_should_be_null_on_empty_list() {
  LIST_TEST({ assert(list__last(list) == NULL); })
}

void test_empty_list_should_have_zero_elements() {
  LIST_TEST({ assert(0 == list__count(list)); })
}

void test_list_count_is_correct_after_mixed_operations() {
  LIST_TEST({
    list__append("first", list);
    assert(1 == list__count(list));
    list__prepend("last", list);
    assert(2 == list__count(list));
    list__delete("first", list);
    list__delete("last", list);
    assert(0 == list__count(list));
  })
}

void test_can_iterate_list() {
  LIST_TEST({
    list__append("first", list);
    list__append("second", list);
    list__append("third", list);

    ListIterator *it = list__iterate(list);
    assert(!strcmp("first", list__next(it)));
    assert(!strcmp("second", list__next(it)));
    assert(!strcmp("third", list__next(it)));

    list__dispose_iterator(it);
  })
}

void test_can_iterate_same_list_in_two_directions() {
  LIST_TEST({
    list__append("one", list);
    list__append("two", list);
    list__append("two", list);
    list__append("one", list);

    ListIterator *forward = list__iterate(list);
    ListIterator *backward = list__reverse_iterate(list);

    char *s1 = list__next(forward);
    while (s1) {
      assert(!strcmp(s1, list__next(backward)));
      s1 = list__next(forward);
    }

    list__dispose_iterator(forward);
    list__dispose_iterator(backward);
  })
}

int main() {
  // smoke tests
  test_first_element_should_be_null_on_empty_list();
  test_last_element_should_be_null_on_empty_list();
  test_empty_list_should_have_zero_elements();

  // setters/getters tests
  test_can_get_first_element();
  test_can_get_last_element();
  test_can_append_single_element();
  test_can_preprend_single_element();

  // deleters tests
  test_can_delete_any_element();

  // iteration
  test_can_iterate_list();
  test_can_iterate_same_list_in_two_directions();

  // mixed tests
  test_list_count_is_correct_after_mixed_operations();

  return 0;
}
