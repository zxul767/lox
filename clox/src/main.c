#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bytecode.h"
#include "common.h"
#include "debug.h"
#include "vm.h"

static void repl(VM* vm) {

  /* clang-format off */
  const char* banner =
      "██╗      ██████╗ ██╗  ██╗    ██████╗ ███████╗██████╗ ██╗\n"
      "██║     ██╔═══██╗╚██╗██╔╝    ██╔══██╗██╔════╝██╔══██╗██║\n"
      "██║     ██║   ██║ ╚███╔╝     ██████╔╝█████╗  ██████╔╝██║\n"
      "██║     ██║   ██║ ██╔██╗     ██╔══██╗██╔══╝  ██╔═══╝ ██║\n"
      "███████╗╚██████╔╝██╔╝ ██╗    ██║  ██║███████╗██║     ███████╗\n"
      "╚══════╝ ╚═════╝ ╚═╝  ╚═╝    ╚═╝  ╚═╝╚══════╝╚═╝     ╚══════╝\n";
  /* clang-format on */

  printf("%s", banner);
  printf("Welcome to the Lox REPL. Ready to hack?\n");

  char line[1024];
  for (;;) {
    printf(">>> ");
    if (!fgets(line, sizeof(line), stdin)) {
      printf("\n");
      break;
    }
    if (!strcmp("quit\n", line))
      break;
    vm__interpret(line, vm);
  }
}

// TODO: replace `74` with symbolic constant
static char* read_file(const char* path) {
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(74);
  }
  fseek(file, 0L, SEEK_END);
  size_t file_size = ftell(file);
  rewind(file);

  char* buffer = (char*)malloc(file_size + 1);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to read \"%s\".\n", path);
    exit(74);
  }
  size_t bytes_read = fread(buffer, sizeof(char), file_size, file);
  if (bytes_read < file_size) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(74);
  }
  buffer[bytes_read] = '\0';

  fclose(file);
  return buffer;
}

static void run_file(const char* path, VM* vm) {
  char* source = read_file(path);
  InterpretResult result = vm__interpret(source, vm);
  free(source);

  // TODO: replace magic numbers with symbolic constants
  if (result == INTERPRET_COMPILE_ERROR)
    exit(65);
  if (result == INTERPRET_RUNTIME_ERROR)
    exit(70);
}

int main(int args_count, const char* args[]) {
  VM vm;
  vm__init(&vm);

  if (args_count == 1) {
    repl(&vm);
  } else if (args_count == 2) {
    // args[0] is always the name of the program
    run_file(args[1], &vm);
  } else {
    fprintf(stderr, "Usage: clox [path]\n");
    // TODO: replace `64` with symbolic constant
    exit(64);
  }
  vm__dispose(&vm);

  return 0;
}
