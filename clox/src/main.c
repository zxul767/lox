#include <ctype.h>
#include <isocline.h>
#include <limits.h>
#include <locale.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sysexits.h>

#include "bytecode.h"
#include "common.h"
#include "cstring.h"
#include "debug.h"
#include "memory.h"
#include "scanner.h"
#include "vm.h"

// there seems to be an issue with autocompletion of commands starting with `:`
// this issue is tracking that: https://github.com/daanx/isocline/issues/17
static char* const TOGGLE_BYTECODE = ":toggle-bytecode";
static char* const TOGGLE_TRACING = ":toggle-tracing";
static char* const LOAD_FILE = ":load";
static char* const GC_RUN = ":gc";
static char* const GC_STATS = ":gc-stats";
static char* const QUIT = "quit";
static char* const EXIT = "exit";

static const char* COMMANDS[] = {
    TOGGLE_BYTECODE, TOGGLE_TRACING, LOAD_FILE, GC_RUN,
    GC_STATS,        QUIT,           EXIT,      NULL};

static void
word_completer(ic_completion_env_t* input_until_cursor, const char* word)
{
  ic_add_completions(input_until_cursor, word, COMMANDS);
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
    cstr__trim_trailing_whitespace(line);

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

static FILE*
try_open_file(const char* path, int die_status, bool die_on_failure)
{
  char resolved_path[PATH_MAX];
  realpath(path, resolved_path);

  FILE* file = fopen(resolved_path, "rb");
  if (file == NULL) {
    fprintf(stderr, "Could not open file \"%s\".\n", resolved_path);
    if (die_on_failure)
      exit(die_status);
  }
  return file;
}

static void*
try_allocate_or_die(size_t size, int die_status, bool die_on_failure)
{
  void* buffer = malloc(size);
  if (buffer == NULL) {
    fprintf(stderr, "Not enough memory to continue!\n");
    if (die_on_failure)
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

static char* try_read_stream(
    FILE* file, const char* path, int die_status, bool die_on_failure)
{
  size_t size = file_size(file);
  char* buffer = (char*)try_allocate_or_die(
      size + 1, die_status, /*die_on_failure:*/ true);

  size_t bytes_read = fread(buffer, sizeof(char), size, file);
  if (bytes_read < size) {
    fprintf(stderr, "Could not read file \"%s\".\n", path);
    if (die_on_failure)
      exit(die_status);
    return NULL;
  }
  buffer[bytes_read] = '\0';

  return buffer;
}

static char* try_read_file(const char* path, bool die_on_failure)
{
  // EX_IOERR == an error occurred while doing I/O on some file
  FILE* file = try_open_file(path, /*die_status:*/ EX_IOERR, die_on_failure);
  char* buffer = NULL;
  if (file) {
    buffer =
        try_read_stream(file, path, /*die_status:*/ EX_IOERR, die_on_failure);
    fclose(file);
  }
  return buffer;
}

static InterpretResult load_file(const char* path, VM* vm, bool die_on_failure)
{
  char* source_code = try_read_file(path, die_on_failure);
  if (source_code) {
    InterpretResult result = vm__interpret(source_code, vm);
    free(source_code);
    return result;
  }
  // FIXME: we should be returning a more general error, since this is strictly
  // incorrect
  return INTERPRET_COMPILE_ERROR;
}

static void run_file(const char* path, VM* vm)
{
  InterpretResult result = load_file(path, vm, /*die_on_failure:*/ true);

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
    line = cstr__trim_trailing_whitespace(line);
    if (cstr__is_empty(line))
      goto next;

    if (!strcmp(QUIT, line) || !strcmp(EXIT, line))
      break;

    if (cstr__startswith(LOAD_FILE, line)) {
      load_file(
          cstr__trim_leading_whitespace(cstr__trim_prefix(LOAD_FILE, line)), vm,
          /*die_on_failure:*/ false);

      goto next;
    }

    if (!strcmp(TOGGLE_BYTECODE, line)) {
      vm->show_bytecode = !vm->show_bytecode;
      ic_printf("bytecode display: %s\n", vm->show_bytecode ? "on" : "off");
      goto next;
    }

    if (!strcmp(TOGGLE_TRACING, line)) {
      vm->trace_execution = !vm->trace_execution;
      ic_printf("execution tracing: %s\n", vm->trace_execution ? "on" : "off");
      goto next;
    }

    if (!strcmp(GC_RUN, line)) {
      memory__run_gc();
      goto next;
    }

    if (!strcmp(GC_STATS, line)) {
      memory__print_gc_stats();
      goto next;
    }

    vm__interpret(line, vm);

  next:
    free(line);
  }
  free(line);
}

int main(int args_count, const char* args[])
{
  VM vm;
  // NOTE: the order of these initialization steps is important: the garbage
  // collector just needs to grab a pointer to the VM, but `vm__init` allocates
  // memory and might trigger a GC run.
  memory__init_gc(&vm);
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
  memory__shutdown_gc();

  return 0;
}
