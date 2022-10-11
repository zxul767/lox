#include <stdio.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"
#include "vm.h"

void compiler__compile(const char *source, VM *vm) {
  Scanner scanner;
  /* printf("compiling: [%s]", source); */
  scanner__init(&scanner, source);

  int line = -1;
  for (;;) {
    Token token = scanner__next_token(&scanner);
    if (token.line != line) {
      printf("%4d ", token.line);
      line = token.line;
    } else {
      printf("   | ");
    }
    printf("%2d '%.*s'\n", token.type, token.length, token.start);

    if (token.type == TOKEN_EOF)
      break;
  }
}
