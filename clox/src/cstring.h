#ifndef CSTRING_H_
#define CSTRING_H_

#include "common.h"

uint32_t cstr__hash(const char* key, int length);
char* cstr__trim_leading_whitespace(char* string);
char* cstr__trim_trailing_whitespace(char* string);
char* cstr__trim_prefix(const char* prefix, char* string);

bool cstr__startswith(const char* prefix, const char* string);
bool cstr__is_empty(const char* string);
bool cstr__equals(
    const char* this_start, const char* this_end, const char* that);

#endif // CSTRING_H_
