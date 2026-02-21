#ifndef INDEXING_H_
#define INDEXING_H_

#include <stdbool.h>

bool index__normalize_index(int index, int length, int* normalized_index);

bool index__normalize_slice_bounds(
    int start,
    int end,
    int length,
    const char* container_name,
    int* normalized_start,
    int* normalized_end
);

#endif // INDEXING_H_
