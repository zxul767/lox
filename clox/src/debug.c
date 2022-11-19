#include "debug.h"
#include "object.h"
#include <stdio.h>
#include <string.h>

// TODO: parameterize justification values for all helper functions that print
// code in a table
//
static void print_char(char c, int times)
{
  for (int i = 0; i < times; i++)
    putchar(c);
}

void debug__print_section_divider()
{
  print_char('=', 80);
  putchar('\n');
}

void debug__print_callframe_divider()
{
  print_char('-', 80);
  putchar('\n');
}

void debug__disassemble(const Bytecode* code, const char* name)
{
  fprintf(
      stderr, "BYTECODE for '%s' %s\n", name != NULL ? name : "<script>",
      name != NULL ? "function" : "");
  debug__print_section_divider();
  for (int offset = 0; offset < code->count;) {
    offset = debug__disassemble_instruction(code, offset);
  }
  debug__print_section_divider();
}

static int simple_instruction(const char* name, int offset)
{
  // simple instructions are encoded as:
  // [OP_CODE]...
  //     ^
  //   offset
  fprintf(stderr, "%s\n", name);

  return offset + 1;
}

static int byte_instruction(
    const char* name, const Bytecode* code, int offset, const char* value_name)
{
  // byte instructions are encoded as:
  // [OP_CODE][value]...
  //     ^
  //   offset
  uint8_t value = code->instructions[offset + 1];
  fprintf(stderr, "%-20s %s:%-4d\n", name, value_name, value);

  return offset + 2;
}

static int jump_instruction(
    const char* name, int direction, const Bytecode* code, int offset)
{
  // jump instructions are encoded as:
  // [JUMP_OP_CODE][jump_length_high_bits][jump_length_low_bits]...
  //       ^
  //     offset
  uint16_t jump_length = (uint16_t)(code->instructions[offset + 1] << 8);
  jump_length |= code->instructions[offset + 2];
  fprintf(
      stderr, "%-20s -> offset:%04d\n", name,
      offset + 3 + direction * jump_length);

  return offset + 3;
}

static int
constant_instruction(const char* name, const Bytecode* code, int offset)
{
  // constant instructions are encoded as:
  // [OP_CODE][index]...
  //     ^
  //  offset
  uint8_t constant_location = code->instructions[offset + 1];
  if (!strcmp(name, "OP_DEFINE_GLOBAL") || !strcmp(name, "OP_GET_GLOBAL") ||
      !strcmp(name, "OP_SET_GLOBAL")) {
    fprintf(stderr, "%-20s name:", name);
    // variables should be printed without quotes for easier identification
    value__print(code->constants.values[constant_location]);

  } else {
    fprintf(stderr, "%-20s index:%d (=", name, constant_location);
    Value value = code->constants.values[constant_location];
    if (!strcmp(name, "OP_SET_PROPERTY") || !strcmp(name, "OP_GET_PROPERTY")) {
      // variables should be printed without quotes for easier identification
      value__print(value);

    } else {
      // string constants should be printed with quotes for easier
      // identification
      value__print_repr(value);
    }
    fprintf(stderr, ")");
  }
  fprintf(stderr, "\n");

  return offset + 2;
}

int closure_instruction(const Bytecode* code, int offset)
{
  // TODO: why do we skip over one byte here?
  offset++;
  uint8_t location = code->instructions[offset++];
  fprintf(stderr, "%-20s index:%d (=", "OP_NEW_CLOSURE", location);
  value__print_repr(code->constants.values[location]);
  fprintf(stderr, ")\n");

  ObjectFunction* function = AS_FUNCTION(code->constants.values[location]);
  for (int i = 0; i < function->upvalues_count; i++) {
    int is_local = code->instructions[offset++];
    int index = code->instructions[offset++];
    fprintf(
        stderr, "%04d    |    %-20supvalue:(index:%d,%s)\n", offset - 2, "",
        index, is_local ? "parent" : "ancestor");
  }
  return offset;
}

int debug__disassemble_instruction(const Bytecode* code, int offset)
{
  fprintf(stderr, "%04d ", offset);

  if (offset > 0 &&
      code->to_source_line[offset] == code->to_source_line[offset - 1]) {
    // reduce visual clutter by not printing repeated source line numbers
    // but still add an indicator to let the user know source line number is the
    // same as the previous instruction
    fprintf(stderr, "   |   ");
  } else {
    fprintf(stderr, "%4d   ", code->to_source_line[offset]);
  }

  uint8_t instruction = code->instructions[offset];
  switch (instruction) {
  case OP_LOAD_CONSTANT:
    return constant_instruction("OP_LOAD_CONSTANT", code, offset);
  case OP_NIL:
    return simple_instruction("OP_NIL", offset);
  case OP_TRUE:
    return simple_instruction("OP_TRUE", offset);
  case OP_FALSE:
    return simple_instruction("OP_FALSE", offset);
  case OP_POP:
    return simple_instruction("OP_POP", offset);
  case OP_DEFINE_GLOBAL:
    return constant_instruction("OP_DEFINE_GLOBAL", code, offset);
  case OP_GET_GLOBAL:
    return constant_instruction("OP_GET_GLOBAL", code, offset);
  case OP_SET_GLOBAL:
    return constant_instruction("OP_SET_GLOBAL", code, offset);
  case OP_GET_UPVALUE:
    return byte_instruction("OP_GET_UPVALUE", code, offset, "index");
  case OP_SET_UPVALUE:
    return byte_instruction("OP_SET_UPVALUE", code, offset, "index");
  case OP_GET_LOCAL:
    return byte_instruction("OP_GET_LOCAL", code, offset, "index");
  case OP_SET_LOCAL:
    return byte_instruction("OP_SET_LOCAL", code, offset, "index");
  case OP_EQUAL:
    return simple_instruction("OP_EQUAL", offset);
  case OP_GREATER:
    return simple_instruction("OP_GREATER", offset);
  case OP_LESS:
    return simple_instruction("OP_LESS", offset);
  case OP_ADD:
    return simple_instruction("OP_ADD", offset);
  case OP_SUBTRACT:
    return simple_instruction("OP_SUBTRACT", offset);
  case OP_MULTIPLY:
    return simple_instruction("OP_MULTIPLY", offset);
  case OP_DIVIDE:
    return simple_instruction("OP_DIVIDE", offset);
  case OP_NOT:
    return simple_instruction("OP_NOT", offset);
  case OP_NEGATE:
    return simple_instruction("OP_NEGATE", offset);
  case OP_PRINT:
    return simple_instruction("OP_PRINT", offset);
  case OP_PRINTLN:
    return simple_instruction("OP_PRINT", offset);
  case OP_LOOP:
    return jump_instruction("OP_JUMP", /*direction:*/ -1, code, offset);
  case OP_JUMP:
    return jump_instruction("OP_JUMP", /*direction:*/ +1, code, offset);
  case OP_JUMP_IF_FALSE:
    return jump_instruction(
        "OP_JUMP_IF_FALSE", /*direction:*/ +1, code, offset);
  case OP_CALL:
    return byte_instruction("OP_CALL", code, offset, "#args");
  case OP_NEW_CLASS:
    return constant_instruction("OP_NEW_CLASS", code, offset);
  case OP_NEW_METHOD:
    return constant_instruction("OP_NEW_METHOD", code, offset);
  case OP_GET_PROPERTY:
    return constant_instruction("OP_GET_PROPERTY", code, offset);
  case OP_SET_PROPERTY:
    return constant_instruction("OP_SET_PROPERTY", code, offset);
  case OP_NEW_CLOSURE:
    return closure_instruction(code, offset);
  case OP_RETURN:
    return simple_instruction("OP_RETURN", offset);
  case OP_CLOSE_UPVALUE:
    return simple_instruction("OP_CLOSE_UPVALUE", offset);
  default:
    fprintf(stderr, "Unknown opcode %d\n", instruction);
    return offset + 1;
  }
}

// we're not always interested in printing the whole stack but only the values
// from a given frame (typically the current frame); `from` allows us to
// parameterize that.
//
void debug__dump_value_stack(const VM* vm, const Value* from)
{
  fprintf(stderr, "            stack: ");
  for (const Value* value = from ? from : vm->value_stack;
       value < vm->stack_free_slot; value++) {
    fprintf(stderr, "[");
    // `value__print` would print the string `"true"` in the same way as the
    // literal `true` but during debugging we want to distinguish between the
    // two, hence the need for `value__print_repr`
    value__print_repr(*value);
    fprintf(stderr, "]");
  }
  fprintf(stderr, "\n");
}

static int
get_current_source_line_in_frame(const CallFrame* frame, const VM* vm)
{
  const Bytecode* bytecode = &(frame->closure->function->bytecode);
  int offset = callframe__current_offset(frame);

  return bytecode->to_source_line[offset];
}

void debug__dump_stacktrace(const VM* vm)
{
  for (int i = vm->frames_count - 1; i >= 0; i--) {
    const CallFrame* frame = &vm->frames[i];
    ObjectCallable* function = CALLABLE_CAST(frame->closure->function);

    int line = get_current_source_line_in_frame(frame, vm);
    fprintf(stderr, "[line %d] in ", line);

    if (function->name == NULL) {
      fprintf(stderr, "script\n");
    } else {
      fprintf(stderr, "%s()\n", function->name->chars);
    }
  }
}

void debug__show_callframe_names(const VM* vm)
{
  fprintf(stderr, "CALL STACK: ");
  for (int i = 0; i < vm->frames_count; i++) {
    const CallFrame* frame = &vm->frames[i];
    ObjectString* name = CALLABLE_CAST(frame->closure->function)->name;
    if (i > 0) {
      fprintf(stderr, " > ");
    }
    fprintf(stderr, "%s", name ? name->chars : "*top*");
  }
  fprintf(stderr, "\n");
  debug__print_callframe_divider();
}

void debug__show_entries(const Table* table)
{
  for (int i = 0; i < table->capacity; ++i) {
    if (table->entries[i].key != NULL) {
      fprintf(stderr, "%s -> ", table->entries[i].key->chars);
      value__print(table->entries[i].value);
      fprintf(stderr, "\n");
    }
  }
  fprintf(stderr, "\n");
}
