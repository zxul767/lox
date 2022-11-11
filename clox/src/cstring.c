#include "cstring.h"
#include <ctype.h>

// FNV-1a hash function. For details, see:
// https://en.wikipedia.org/wiki/Fowler%E2%80%93Noll%E2%80%93Vo_hash_function
uint32_t cstr__hash(const char* key, int length)
{
  uint32_t hash = 2166136261u;
  for (int i = 0; i < length; i++) {
    hash ^= (uint8_t)key[i];
    hash *= 16777619;
  }
  return hash;
}

char* cstr__trim_leading_whitespace(char* string)
{
  while (*string && isspace(*string))
    string++;
  return string;
}

char* cstr__trim_trailing_whitespace(char* string)
{
  if (*string == '\0')
    return string;

  char* end = string;
  while (*end)
    end++;

  end--;
  while (end >= string && isspace(*end))
    end--;

  *(end + 1) = '\0';
  return string;
}

char* cstr__trim_prefix(const char* prefix, char* string)
{
  while (*prefix && *string && *prefix == *string) {
    prefix++;
    string++;
  }
  return string;
}

bool cstr__startswith(const char* prefix, const char* string)
{
  while (*prefix && *string && *prefix == *string) {
    prefix++;
    string++;
  }
  return *prefix == '\0';
}

bool cstr__is_empty(const char* string) { return *string == '\0'; }

// returns true if [this_start, this_end) === that; false otherwise
// pre-conditions:
// + `this_start` and `this_end` are pointers to sections of a null-terminated
//    c-string;
// + the substring to be compared to `that` begins at `this_start` and ends
//   at `this_end-1` (i.e., `this_end` is a non-inclusive "index")
// + `that` is a null-terminated c-string;
bool cstr__equals(
    const char* this_start, const char* this_end, const char* that)
{
  while (*that && this_start < this_end) {
    if (*this_start != *that)
      break;
    that++;
    this_start++;
  }
  // equality only happens if both strings were fully "consumed"
  return *that == '\0' && this_start == this_end;
}
