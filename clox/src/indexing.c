#include <stdio.h>

#include "indexing.h"

static int normalize_index(int index, int length)
{
  if (index < 0) {
    return length + index;
  }
  return index;
}

bool index__normalize_index(int index, int length, int* result)
{
  if (length == 0) {
    fprintf(stderr, "Index Error: Cannot access elements in empty list.\n");
    return false;
  }

  int normed_index = normalize_index(index, length);
  if (normed_index < 0 || normed_index >= length) {
    fprintf(
        stderr,
        "Index Error: tried to access index %d, but valid range is [0..%d] or "
        "[-%d..-1].\n",
        index,
        length - 1,
        length
    );
    return false;
  }
  *result = normed_index;
  return true;
}

static bool normalize_slice_index(
    int index, int length, bool allow_endpoint, const char* index_name, int* result
)
{
  int normed_index = normalize_index(index, length);
  int upper_bound = allow_endpoint ? length : length - 1;
  if (normed_index < 0 || normed_index > upper_bound) {
    fprintf(
        stderr,
        "Index Error: %s index %d is out of range [0..%d].\n",
        index_name,
        index,
        upper_bound
    );
    return false;
  }
  *result = normed_index;
  return true;
}

bool index__normalize_slice_bounds(
    int start,
    int end,
    int length,
    const char* container_name,
    int* normed_start,
    int* normed_end
)
{
  if (length == 0) {
    fprintf(stderr, "Index Error: Cannot slice an empty %s.\n", container_name);
    return false;
  }
  if (!normalize_slice_index(
          start,
          length,
          /* allow_endpoint: */ false,
          "start",
          normed_start
      )) {
    return false;
  }
  if (!normalize_slice_index(
          end,
          length,
          /* allow_endpoint: */ true,
          "end",
          normed_end
      )) {
    return false;
  }
  if (*normed_start > *normed_end) {
    fprintf(
        stderr,
        "Index Error: start index %d cannot be greater than end index %d.\n",
        start,
        end
    );
    return false;
  }
  return true;
}
