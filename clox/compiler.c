#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "vm.h"

static const char *remove_common_prefix(const char *prefix,
                                        const char *string) {
  while (*string && *prefix && *string == *prefix) {
    prefix++;
    string++;
  }
  return string;
}

void compiler__compile(const char *source, VM *vm) {
  Scanner scanner;
  scanner__init(&scanner, source);

  int line = -1;
  for (;;) {
    Token token = scanner__next_token(&scanner);
    if (token.line != line) {
      printf("%4d ", token.line);
      line = token.line;
    } else {
      printf("%4s ", "|");
    }
    printf("%15s '%.*s'\n",
           remove_common_prefix("TOKEN_", TOKEN_TO_STRING[token.type]),
           token.length, token.start);

    if (token.type == TOKEN_EOF)
      break;
  }
}
