#include <isocline.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "bytecode.h"
#include "common.h"
#include "debug.h"
#include "memory.h"
#include "scanner.h"
#include "vm.h"

static void
word_completer(ic_completion_env_t* input_until_cursor, const char* word) {
  static const char* commands[] = {"quit", NULL};

  ic_add_completions(input_until_cursor, word, commands);
  ic_add_completions(input_until_cursor, word, KEYWORDS);
}

// We use `ic_complete_word` to only consider the final token on the input.
// (almost all user defined completers should use this)
static void
completer(ic_completion_env_t* input_until_cursor, const char* input) {
  ic_complete_word(
      input_until_cursor, input, &word_completer,
      NULL /* from default word boundary; whitespace or separator */
  );
}

static void setup_line_reader() {
  setlocale(LC_ALL, "C.UTF-8"); // we use utf-8 in this example

  ic_style_def("kbd", "gray underline");    // you can define your own styles
  ic_style_def("ic-prompt", "ansi-maroon"); // or re-define system styles

  ic_enable_hint(false);

  ic_printf(
      "- Type 'quit' to exit. (or use [kbd]ctrl-d[/]).\n"
      "- Use [kbd]shift-tab[/] for multiline input. (or [kbd]ctrl-enter[/], or "
      "[kbd]ctrl-j[/])\n"
      "- Use [kbd]tab[/]  for word completion.\n"
      "- Use [kbd]ctrl-r[/] to search the history.\n\n"
  );

  // enable completion with a default completion function
  ic_set_default_completer(&completer, NULL);

  // try to auto complete after a completion as long as the completion is unique
  ic_enable_auto_tab(true);

  // enable history; use a NULL filename to not persist history to disk
  ic_set_history(".lox_repl", -1 /* default entries (= 200) */);
}

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

  ic_printf("[LightSalmon]%s[/]", banner);
  ic_printf("[LightSalmon]Welcome to the Lox REPL. Ready to hack?\n[/]");

  setup_line_reader();
  vm->execution_mode = VM_REPL_MODE;

  char* line;
  while ((line = ic_readline(">>")) != NULL) {
    if (!strcmp("quit", line))
      break;

    if (!strcmp(":toggle-bytecode", line)) {
      vm->show_bytecode = !vm->show_bytecode;
      ic_printf("bytecode display: %s\n", vm->show_bytecode ? "on" : "off");
      continue;
    }

    if (!strcmp(":toggle-tracing", line)) {
      vm->trace_execution = !vm->trace_execution;
      ic_printf("execution tracing: %s\n", vm->trace_execution ? "on" : "off");
      continue;
    }

    vm__interpret(line, vm);

    free(line);
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
  char* source_code = read_file(path);

  vm->execution_mode = VM_SCRIPT_MODE;
  InterpretResult result = vm__interpret(source_code, vm);
  free(source_code);

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
