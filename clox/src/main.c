#include <ctype.h>
#include <isocline.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "bytecode.h"
#include "common.h"
#include "debug.h"
#include "memory.h"
#include "scanner.h"
#include "vm.h"

static void
word_completer(ic_completion_env_t* input_until_cursor, const char* word)
{
  static const char* commands[] = {"quit", NULL};

  ic_add_completions(input_until_cursor, word, commands);
  ic_add_completions(input_until_cursor, word, KEYWORDS);
}

// We use `ic_complete_word` to only consider the final token on the input.
// (almost all user defined completers should use this)
static void
completer(ic_completion_env_t* input_until_cursor, const char* input)
{
  ic_complete_word(
      input_until_cursor, input, &word_completer,
      NULL /* from default word boundary; whitespace or separator */
  );
}

static void setup_line_reader()
{
  setlocale(LC_ALL, "C.UTF-8"); // we use utf-8 in this example

  ic_style_def("kbd", "gray underline");    // you can define your own styles
  ic_style_def("ic-prompt", "ansi-maroon"); // or re-define system styles

  ic_enable_hint(false);

  ic_printf(
      "- Type 'quit' to exit. (or use [kbd]ctrl-d[/]).\n"
      "- Use [kbd]shift-tab[/] for multiline input. (or [kbd]ctrl-enter[/], or "
      "[kbd]ctrl-j[/])\n"
      "- Use [kbd]tab[/]  for word completion.\n"
      "- Use [kbd]ctrl-r[/] to search the history.\n\n");

  // enable completion with a default completion function
  ic_set_default_completer(&completer, NULL);

  // try to auto complete after a completion as long as the completion is unique
  ic_enable_auto_tab(true);

  // enable history; use a NULL filename to not persist history to disk
  ic_set_history(".clox_history", -1 /* default entries (= 200) */);
}

static char* cstring__trim_leading_whitespace(char* string)
{
  while (*string && isspace(*string))
    string++;
  return string;
}

static char* cstring__trim_trailing_whitespace(char* string)
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

static char* cstring__trim_prefix(const char* prefix, char* string)
{
  while (*prefix && *string && *prefix == *string) {
    prefix++;
    string++;
  }
  return string;
}

static bool cstring__startswith(const char* prefix, char* string)
{
  while (*prefix && *string && *prefix == *string) {
    prefix++;
    string++;
  }
  return *prefix == '\0';
}

static void read_configuration(const char* file_path, VM* vm)
{
  FILE* file = fopen(file_path, "r");
  if (file == NULL) {
    // TODO: properly validate why the failure occurred by checking `errno`
    // https://man7.org/linux/man-pages/man3/errno.3.html
    //
    // for now we assume the configuration file simply did not exist
    return;
  }

  ic_printf("configuration read from %s\n", file_path);
  const size_t max_line_length = 256;
  char line[max_line_length];
  while (fgets(line, max_line_length, file)) {
    cstring__trim_trailing_whitespace(line);

    if (!strcmp(":enable-tracing", line)) {
      vm->trace_execution = true;
      ic_printf(
          "-> execution tracing: %s\n", vm->trace_execution ? "on" : "off");

    } else if (!strcmp(":show-bytecode", line)) {
      vm->show_bytecode = true;
      ic_printf("-> bytecode display: %s\n", vm->show_bytecode ? "on" : "off");
    }
  }
  fclose(file);
}

static FILE* open_or_die(const char* path, int die_status)
{
  FILE* file = fopen(path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", path);
    exit(die_status);
  }
  return file;
}

static void* allocate_or_die(size_t size, int die_status)
{
  void* buffer = malloc(size);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to continue!\n");
    exit(die_status);
  }
  return buffer;
}

static size_t file_size(FILE* file)
{
  fseek(file, 0L, SEEK_END);
  size_t size = ftell(file);
  rewind(file);

  return size;
}

static char* read_file_or_die(FILE* file, const char* path, int die_status)
{
  size_t size = file_size(file);
  char* buffer = (char*)allocate_or_die(size + 1, die_status);

  size_t bytes_read = fread(buffer, sizeof(char), size, file);
  if (bytes_read < size) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    exit(die_status);
  }
  buffer[bytes_read] = '\0';

  return buffer;
}

static char* read_file(const char* path)
{
  // EX_IOERR == an error occurred while doing I/O on some file
  FILE* file = open_or_die(path, /*die_status:*/ EX_IOERR);
  char* buffer = read_file_or_die(file, path, /*die_status:*/ EX_IOERR);
  fclose(file);

  return buffer;
}

static InterpretResult load_file(const char* path, VM* vm)
{
  char* source_code = read_file(path);

  vm->execution_mode = VM_SCRIPT_MODE;
  InterpretResult result = vm__interpret(source_code, vm);
  free(source_code);

  return result;
}

static void run_file(const char* path, VM* vm)
{
  InterpretResult result = load_file(path, vm);

  if (result == INTERPRET_COMPILE_ERROR)
    exit(EX_DATAERR);

  if (result == INTERPRET_RUNTIME_ERROR)
    exit(EX_SOFTWARE);
}

static void repl(VM* vm)
{
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
  read_configuration(".loxrc", vm);

  char* line;
  while ((line = ic_readline(">>")) != NULL) {
    line = cstring__trim_trailing_whitespace(line);

    if (!strcmp("quit", line))
      break;

    if (cstring__startswith(":load", line)) {
      load_file(
          cstring__trim_leading_whitespace(cstring__trim_prefix(":load", line)),
          vm);
      continue;
    }

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

int main(int args_count, const char* args[])
{
  VM vm;
  vm__init(&vm);

  if (args_count == 1) {
    repl(&vm);
  } else if (args_count == 2) {
    // args[0] is always the name of the program
    run_file(args[1], &vm);
  } else {
    fprintf(stderr, "Usage: clox [path]\n");
    exit(EX_USAGE);
  }
  vm__dispose(&vm);

  return 0;
}
