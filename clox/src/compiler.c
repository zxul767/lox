#include <assert.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "common.h"
#include "compiler.h"
#include "memory.h"
#include "object.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

// There is no separate parser.c file because this is a single-pass compiler,
// so we don't build explicit intermediate code representations (i.e., ASTs).
//
// Therefore, the parsing and code emitting phases are fused into a single
// module. As a result, functions that appear to be doing parsing (or have
// `parse` in their names) are actually doing both parsing and compilation
// in a single place.
//
typedef struct {
  Token current_token;
  Token previous_token;
  // we need to keep this as a separate token because we don't want parsing to
  // be driven by ignorable tokens but we do need to keep the last one around to
  // properly implement the "optional semicolon" feature
  //
  // see "optional semicolon" in `features_design.md` for more details
  Token immediately_prior_newline;

  bool had_error;
  bool panic_mode;

  Scanner scanner;

} Parser;

typedef enum {
  PREC_NONE,
  PREC_ASSIGNMENT, // =
  PREC_OR,         // or
  PREC_AND,        // and
  PREC_EQUALITY,   // == !=
  PREC_COMPARISON, // < > <= >=
  PREC_TERM,       // + -
  PREC_FACTOR,     // * /
  PREC_UNARY,      // ! -
  PREC_CALL,       // . ()
  PREC_PRIMARY

} Precedence;

typedef struct Local {
  Token name;
  int scope_depth;
  bool is_captured;

} Local;

typedef enum { TYPE_FUNCTION, TYPE_SCRIPT } FunctionType;

// see https://craftinginterpreters.com/closures.html for a full
// description of the role of upvalues in implementing closures
typedef struct Upvalue {
  uint8_t index;
  bool is_local;

} Upvalue;

typedef struct FunctionCompiler {
  struct FunctionCompiler* enclosing;
  ObjectFunction* function;
  FunctionType function_type;

  Upvalue upvalues[UINT8_COUNT];
  Local locals[UINT8_COUNT];
  int locals_count;
  int scope_depth;

  Parser* parser;

} FunctionCompiler;

typedef struct {
  Parser* parser;
  FunctionCompiler* function_compiler;

  // `vm->objects` tracks references to all heap-allocated objects to be
  // disposed when the VM shuts-down. In the compiler, such objects are string
  // literals.
  VM* vm;

  // We need this to make sure that the `named_variable` function only consumes
  // the `=` operator if it's in the context of a low-precedence expression, so
  // that an expression such as `a+b=2` is not incorrectly parsed as `a+(b=2)`.
  //
  // The fundamental root of this problem is that we are parsing and compiling
  // simultaneously, i.e., this is a single-pass compiler, so we need upstream
  // context information in downstream parser functions to compile correctly.
  //
  // This wouldn't be necessary if we had an intermediate AST, because an AST
  // gathers all relevant information in local nodes so it's easy to rule out
  // semantic violations with just local information in a subsequent pass.
  //
  // For example, for the `a+b=2` expression, the AST would be `(= (+ a b) 2)`
  // (since `=` has lower precedence than `+`), and then the compiler could
  // easily detect (by just looking at the `=` node) that `(+ a b)` is an
  // invalid target.
  bool can_assign;

} Compiler;

typedef void (*ParseFn)(Compiler*);
typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;

} ParseRule;

// ----------------------------------------------------------------------------
// Forward Declarations (Break Circular References)
// ----------------------------------------------------------------------------
// (parse_only -> get_parse_rule -> binary -> parse_only)
// (parse_only -> get_parse_rule -> unary -> parse_only)
// ...
static ParseRule* get_parse_rule(TokenType);
// (statement -> block -> declaration -> statement)
static void statement();
// (block -> declaration -> fun_declaration -> function -> block)
static void declaration(Compiler* compiler);

static void error_at(const Token* token, const char* message, Parser* parser)
{
  // Don't report errors which are likely to be cascade errors (panic mode
  // means we failed to parse a rule and we're lost until we perform a
  // synchronization, so intermediate errors are likely to be spurious)
  if (parser->panic_mode)
    return;
  parser->panic_mode = true;
  parser->had_error = true;

  fprintf(stderr, "[line %d] Error", token->line);
  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");

  } else if (token->type == TOKEN_ERROR) {
    if (parser->previous_token.type != TOKEN_BOF)
      fprintf(
          stderr, " after '%.*s'", parser->previous_token.length,
          parser->previous_token.start);

  } else {
    fprintf(
        stderr, " at '%.*s' [%s]", token->length, token->start,
        TOKEN_TO_STRING[token->type]);
  }
  fprintf(stderr, ": %s\n", message);
}

static void error(const char* message, Parser* parser)
{
  error_at(&parser->previous_token, message, parser);
}

static void error_at_current(const char* message, Parser* parser)
{
  error_at(&parser->current_token, message, parser);
}

static inline Bytecode* current_bytecode(Compiler* compiler)
{
  return &(compiler->function_compiler->function->bytecode);
}

static uint8_t store_constant(Value value, Compiler* compiler)
{
  int location =
      bytecode__store_constant(current_bytecode(compiler), value, compiler->vm);
  if (location > UINT8_MAX) {
    error("Too many constants in one chunk", compiler->parser);
    return 0;
  }
  return (uint8_t)location;
}

static uint8_t store_identifier_constant(Token* identifier, Compiler* compiler)
{
  return store_constant(
      OBJECT_VAL(
          string__copy(identifier->start, identifier->length, compiler->vm)),
      compiler);
}

static void emit_byte(uint8_t byte, Compiler* compiler)
{
  bytecode__append(
      current_bytecode(compiler), byte, compiler->parser->previous_token.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2, Compiler* compiler)
{
  emit_byte(byte1, compiler);
  emit_byte(byte2, compiler);
}

static void emit_loop(int loop_start_offset, Compiler* compiler)
{
  // OP_LOOP and OP_JUMP are fundamentally the same, except for the direction
  // of the jump (negative for OP_LOOP and positive for OP_JUMP).
  emit_byte(OP_LOOP, compiler);

  //                  <--- jump_length --->
  // [ ] ... [OP_LOOP][high-bits][low-bits][*] ...
  //  ^          ^                          ^
  //  |         count                     start (jump starts here)
  //  |
  // loop_start_offset (jump lands here)
  //
  // start - jump_length = loop_start_offset
  int jump_length = current_bytecode(compiler)->count - loop_start_offset + 2;
  if (jump_length > UINT16_MAX) {
    error("Loop body too large.", compiler->parser);
  }

  emit_byte((jump_length >> 8) & 0xff, compiler);
  emit_byte(jump_length & 0xff, compiler);
}

static int emit_placeholder_jump(uint8_t instruction, Compiler* compiler)
{
  emit_byte(instruction, compiler);
  // 0xff is the marker for a "placeholder" jump distance to be patched later
  // A 16-bit offset lets us jump over 65,535 bytes of code, more than enough!
  emit_byte(0xff, compiler);
  emit_byte(0xff, compiler);
  // this points at the first placeholder instruction
  return current_bytecode(compiler)->count - 2;
}

static void patch_jump(int offset, Compiler* compiler)
{
  //          <- jump_length ->
  // [OP_JUMP][ 0xff  ][ 0xff ][ ] ... [*] ...
  //              ^             ^       ^
  //            offset          |     count (jump lands here)
  //                            |
  //                          start (jump starts here)
  //
  // start + jump_length = count
  Bytecode* code = current_bytecode(compiler);
  int jump_length = code->count - offset - 2;
  if (jump_length > UINT16_MAX) {
    error("Too much code to jump over.", compiler->parser);
  }
  // `jump_length` is encoded [high-bits][low-bits]
  //                               ^         ^
  //                             offset  offset + 1
  code->instructions[offset] = (jump_length >> 8) & 0xff;
  code->instructions[offset + 1] = jump_length & 0xff;
}

static void emit_constant(Value value, Compiler* compiler)
{
  emit_bytes(OP_LOAD_CONSTANT, store_constant(value, compiler), compiler);
}

static void emit_default_return(Compiler* compiler)
{
  // all functions return `nil` by default
  emit_byte(OP_NIL, compiler);
  emit_byte(OP_RETURN, compiler);
}

static bool identifier__equals(const Token* a, const Token* b)
{
  if (a->length != b->length)
    return false;
  return !memcmp(a->start, b->start, a->length);
}

static void advance(Parser* parser)
{
  parser->previous_token = parser->current_token;
  parser->immediately_prior_newline = IGNORABLE_TOKEN;

  for (;;) {
    Token token = parser->current_token = scanner__next_token(&parser->scanner);

    if (token.type == TOKEN_IGNORABLE || token.type == TOKEN_BOF)
      continue;

    if (token.type == TOKEN_ERROR) {
      error_at_current(token.start, parser);
      continue;
    }

    // see "optional semicolon" in `features_design.md` for more details
    if (token.type == TOKEN_NEWLINE || token.type == TOKEN_MULTILINE_COMMENT) {
      parser->immediately_prior_newline = token;
      continue;
    }

    break;
  }
}

static bool check(TokenType type, Parser* parser)
{
  return parser->current_token.type == type;
}

static bool match(TokenType type, Parser* parser)
{
  if (!check(type, parser))
    return false;
  advance(parser);
  return true;
}

static void consume(TokenType type, const char* message, Parser* parser)
{
  if (parser->current_token.type == type) {
    advance(parser);
    return;
  }
  error_at_current(message, parser);
}

static inline Precedence current_token_precedence(const Parser* parser)
{
  return get_parse_rule(parser->current_token.type)->precedence;
}

// parses (and compiles) either a unary expression; or a binary one, as long as
// the binary operation's precedence is higher or equal than `min_precedence`
//
// pre-condition: compiler->scanner is looking at the first token of a new
// expression to be parsed
//
static void parse_only(Precedence min_precedence, Compiler* compiler)
{
  Parser* parser = compiler->parser;

  // consume the next token to decide which parse function we need next
  advance(parser);

  ParseFn parse_prefix = get_parse_rule(parser->previous_token.type)->prefix;
  if (parse_prefix == NULL) {
    error("Unexpected token in primary expression", parser);
    return;
  }

  // We could pass the `can_assign_upstream` value to all downstream functions,
  // but it adds cognitive clutter since most functions don't need it. The idea
  // is to prevent expression such as `a+b=1` from being parsed as `a+(b=1)`
  // (see the comment in the definition of `Compiler` for more details.)
  bool can_assign_upstream = compiler->can_assign;
  compiler->can_assign = min_precedence <= PREC_ASSIGNMENT;

  // We compile what could be a single unary expression, or the first operand of
  // larger binary expression...
  parse_prefix(compiler);

  // At this point, we've either:
  // 1) compiled a full expression and we won't enter the loop (since EOF has
  //    the lowest precedence of all tokens);
  // 2) compiled the first operand of a binary expression and we may or may not
  // enter the loop, depending on whether the next token/operator has higher
  // or equal precedence than required by `min_precedence`.
  while (current_token_precedence(parser) >= min_precedence) {
    ParseFn parse_infix = get_parse_rule(parser->current_token.type)->infix;
    // We consume the operator and parse the rest of the expression.
    advance(parser);
    if (parse_infix == NULL) {
      error("Expected valid operator after expression", parser);
      return;
    }
    parse_infix(compiler);
  }
  if (compiler->can_assign && match(TOKEN_EQUAL, parser)) {
    error("Invalid assignment target.", parser);
  }
  // Keep in mind this function is recursive, so we don't want to clobber values
  // "upstream" (i.e., in function calls higher up in the call stack)
  compiler->can_assign = can_assign_upstream;
}

static void expression(Compiler* compiler)
{
  // assignments have the lowest precedence level of all expressions
  // so this parses any expression
  parse_only(/*min_precedence:*/ PREC_ASSIGNMENT, compiler);
}

static void
pop_all_accessible_locals_in_scope(Compiler* compiler, int scope_depth)
{
  FunctionCompiler* current = compiler->function_compiler;
  while (current->locals_count > 0) {
    if (current->locals[current->locals_count - 1].scope_depth < scope_depth)
      break;

    if (current->locals[current->locals_count - 1].is_captured) {
      emit_byte(OP_CLOSE_UPVALUE, compiler);

    } else {
      emit_byte(OP_POP, compiler);
    }
    current->locals_count--;
  }
}

// returns the index of the local variable identified by `name` or -1 if it
// doesn't exist
static int resolve_local(Token* name, FunctionCompiler* current)
{
  for (int i = current->locals_count - 1; i >= 0; i--) {
    Local* local = &current->locals[i];
    if (identifier__equals(name, &local->name)) {
      // -1 means "declared but not initialized yet"
      if (local->scope_depth == -1) {
        error(
            "Can't read local variable in its own initializer.",
            current->parser);
      }
      return i;
    }
  }
  return -1;
}

static int
add_or_get_upvalue(FunctionCompiler* current, uint8_t index, bool is_local)
{
  int upvalues_count = current->function->upvalues_count;
  for (int i = 0; i < upvalues_count; i++) {
    Upvalue* upvalue = &current->upvalues[i];
    if (upvalue->index == index && upvalue->is_local == is_local) {
      return i;
    }
  }
  if (upvalues_count == UINT8_COUNT) {
    error(
        "Too many captured (closure) variables in function.", current->parser);
  }
  current->upvalues[upvalues_count].is_local = is_local;
  current->upvalues[upvalues_count].index = index;

  return current->function->upvalues_count++;
}

// consider the following program:
//
// fun out() {
//    var x = "out";
//    fun mid() {
//       fun in() {
//            print x;
//       }
//       in();
//   }
//   mid();
// }
// out();
//
// 1. at compilation time:
//
// when we're processing `in` and we find a reference to `x`, we find it
// impossible to resolve it locally, so we know it is potentially a captured
// (closure) variable. during this stage we can record the indexing information
// required to successfully resolve the reference at runtime.
//
// for each function capturing non-local variables (i.e., a closure), we keep an
// "upvalues" array that tells us how to get hold of them by looking at "parent"
// or "ancestor" call frames at runtime:
//
// "mid" (closure) upvalues: [x=(index:1,parent)]
// "in" (closure) upvalues: [x=(index:0,ancestor)]
//
// NOTE: for "parent" upvalues, `index` indexes into the locals array of the
// parent function (i.e., a portion of the value stack); for "ancestor"
// upvalues, `index` indexes into the `upvalues` array of the closure.
//
// this information is written in the resulting bytecode to be used by the VM to
// properly instantiate closures and resolve their non-local variable
// references.
//
// 2. at runtime:
//
// when processing an OP_CLOSURE instruction, the compile-time upvalues
// information helps us build the final upvalues arrays, turning this:
//
// top-level frame
//  |
//  |        `out` frame            `mid` frame       `in` frame
//  |  ________________________   ________________   ______________
//  v /    0       1       2   \ /   0         1  \ /   0      1   |
// [*][<fn out>]["out"][<fn mid>][<fn mid>][<fn in>][<fn in>]["out"] (stack)
// -----------------------------------------------------------------
//                ^
//                |
//               [x=(index:1, kind:parent)] (`mid` compile-time upvalues)
//                ^
//                |
//               [x=(index:0, kind:ancestor)] (`in` compile-time upvalues)
//
// into this (the effective final data structure used at runtime):
//
// top-level frame
//  |
//  |        `out` frame            `mid` frame       `in` frame
//  |  ________________________   ________________   ______________
//  v /    0       1       2   \ /   0         1  \ /   0      1   |
// [*][<fn out>]["out"][<fn mid>][<fn mid>][<fn in>][<fn in>]["out"] (stack)
// -----------------------------------------------------------------
//  0  1         2  ^   3         4         5        6        7
//                  |
//            +-----+------------+
//            |                  |
//            |                 [x = &stack[2]]
//            |                 (`mid` runtime upvalues)
//            |
//            |------------------------------------[x = &stack[2]]
//                                                 (`in` runtime upvalues)
//
// with this in place, OP_GET_UPVALUE instructions are very quick to execute
// since they simply resolve non-local variables in a single array lookup.
//
static int resolve_upvalue(Token* name, FunctionCompiler* current)
{
  if (current->enclosing == NULL)
    return -1;

  int index;
  index = resolve_local(name, current->enclosing);
  if (index != -1) {
    current->enclosing->locals[index].is_captured = true;
    return add_or_get_upvalue(current, (uint8_t)index, /*is_local:*/ true);
  }

  index = resolve_upvalue(name, current->enclosing);
  if (index != -1) {
    return add_or_get_upvalue(current, (uint8_t)index, /*is_local:*/ false);
  }
  return -1;
}

static void named_variable_or_assignment(Token name, Compiler* compiler)
{
  int location = resolve_local(&name, compiler->function_compiler);

  uint8_t get_op, set_op;
  if (location != -1) {
    get_op = OP_GET_LOCAL;
    set_op = OP_SET_LOCAL;

  } else if (
      (location = resolve_upvalue(&name, compiler->function_compiler)) != -1) {
    get_op = OP_GET_UPVALUE;
    set_op = OP_SET_UPVALUE;

  } else {
    location = store_identifier_constant(&name, compiler);
    get_op = OP_GET_GLOBAL;
    set_op = OP_SET_GLOBAL;
  }

  if (compiler->can_assign && match(TOKEN_EQUAL, compiler->parser)) {
    expression(compiler);
    emit_bytes(set_op, location, compiler);

  } else {
    emit_bytes(get_op, location, compiler);
  }
}

static void variable(Compiler* compiler)
{
  named_variable_or_assignment(compiler->parser->previous_token, compiler);
}

static void mark_latest_local_initialized(FunctionCompiler* current)
{
  if (current->scope_depth == 0)
    return;

  current->locals[current->locals_count - 1].scope_depth = current->scope_depth;
}

static void define_variable(uint8_t location, Compiler* compiler)
{
  // local variable
  if (compiler->function_compiler->scope_depth > 0) {
    mark_latest_local_initialized(compiler->function_compiler);
    return;
  }
  // global variable
  emit_bytes(OP_DEFINE_GLOBAL, location, compiler);
}

// returns the number of arguments compiled
static uint8_t argument_list(Compiler* compiler)
{
  uint8_t args_count = 0;
  Parser* parser = compiler->parser;

  if (!check(TOKEN_RIGHT_PAREN, parser)) {
    do {
      expression(compiler);
      if (args_count == 255) {
        error("Can't have more than 255 arguments", parser);
      }
      args_count++;
    } while (match(TOKEN_COMMA, parser));
  }
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after arguments", parser);

  return args_count;
}

// parses (and compiles) the right operand of a boolean AND expression, making
// sure to implement the correct short-circuiting semantics (i.e., if the left
// operand is false, then the right operand won't be evaluated.)
//
// pre-condition: the left operand has already been compiled and its result is
// on the top of the stack
//
static void and_(Compiler* compiler)
{
  // expected bytecode generation:
  //
  //      [ Left Operand Expression  ]   <- previously compiled
  //      ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // +--- OP_JUMP_IF_FALSE (short-circuit when LOE is false)
  // |    OP_POP (pop left operand; result is value(ROE))
  // |
  // |    [ Right Operand Expression ]
  // |    ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  // |
  // +--> continues...
  //
  int end_jump_ofsset = emit_placeholder_jump(OP_JUMP_IF_FALSE, compiler);
  // discard left operand, let the result be the value of the right operand
  emit_byte(OP_POP, compiler);

  // passing `min_precedence=PREC_AND` causes right-associativity (i.e., `false
  // and true and true` is parsed as `(false and (true and true))`)
  parse_only(/* min_precedence: */ PREC_AND, compiler);

  patch_jump(end_jump_ofsset, compiler);
}

// parses (and compiles) the right operand of a boolean OR expression, making
// sure to implement the correct short-circuiting semantics (i.e., if the left
// operand is true, then the right operand won't be evaluated.)
//
// pre-condition: the left operand has already been compiled and its result is
// on the top of the stack
//
static void or_(Compiler* compiler)
{
  // expected bytecode generation:
  //
  //         [ Left Operand Expression  ]   <- previously compiled
  //         ~~~~~~~~~~~~~~~~~~~~~~~~~~~
  //    +--- OP_JUMP_IF_FALSE
  //  +-|--- OP_JUMP (short-circuit when LOE is true)
  //  | |
  //  | +--> OP_POP (pop left operand; result is value(ROE))
  //  |      [ Right Operand Expression ]
  //  |      ~~~~~~~~~~~~~~~~~~~~~~~~~~~~
  //  |
  //  +----> continues...

  int roe_jump_ofsset = emit_placeholder_jump(OP_JUMP_IF_FALSE, compiler);
  int end_jump_ofsset = emit_placeholder_jump(OP_JUMP, compiler);

  patch_jump(roe_jump_ofsset, compiler);
  // discard left operand, let the result be the value of the right operand
  emit_byte(OP_POP, compiler);

  // passing `min_precedence=PREC_OR` causes right-associativity (i.e., `false
  // or true or true` is parsed as `(false or (true or true))`)
  parse_only(/* min_precedence: */ PREC_OR, compiler);

  patch_jump(end_jump_ofsset, compiler);
}

static void
check_duplicate_declaration(const Token* name, const FunctionCompiler* current)
{
  for (int i = current->locals_count - 1; i >= 0; i--) {
    const Local* local = &current->locals[i];
    // local->scope_depth == -1 means the local has been declared but not
    // yet initialized
    if (local->scope_depth != -1 && local->scope_depth < current->scope_depth) {
      // we only care about duplicate declarations in the same scope
      break;
    }
    if (identifier__equals(name, &local->name)) {
      error(
          "Already a variable with this name in this scope.", current->parser);
    }
  }
}

static void add_local_variable(Token name, Compiler* compiler)
{
  FunctionCompiler* current = compiler->function_compiler;
  if (current->locals_count == UINT8_COUNT) {
    error("Too many local variables in function.", compiler->parser);
    return;
  }
  Local* local = &current->locals[current->locals_count++];
  local->name = name;
  local->scope_depth = -1; // mark declared but uninitialized
  local->is_captured = false;
}

static void declare_local_variable(Compiler* compiler)
{
  assert(compiler->function_compiler->scope_depth > 0);

  Token* name = &compiler->parser->previous_token;
  check_duplicate_declaration(name, compiler->function_compiler);

  add_local_variable(*name, compiler);
}

static uint8_t declare_variable(const char* error_message, Compiler* compiler)
{
  consume(TOKEN_IDENTIFIER, error_message, compiler->parser);

  // local variables
  if (compiler->function_compiler->scope_depth > 0) {
    declare_local_variable(compiler);
    // locals don't need to store their names in constants as globals do
    return 0;
  }
  // global variables
  return store_identifier_constant(&compiler->parser->previous_token, compiler);
}

static bool has_implicit_statement_terminator(Parser* parser)
{
  bool result =
      parser->immediately_prior_newline.type == TOKEN_NEWLINE ||
      parser->immediately_prior_newline.type == TOKEN_MULTILINE_COMMENT ||
      // handles statements like `{ return expression }`
      parser->current_token.type == TOKEN_RIGHT_BRACE;

  return result;
}

// see "optional semicolon" in `features_design.md`
static bool optional_semicolon(Parser* parser)
{
  if (match(TOKEN_SEMICOLON, parser)) {
    return true;

    // at first glance it may seem like we could use
    // `match(TOKEN_NEWLINE)` but that's not the case because we don't
    // track newlines and comments as regular tokens (since otherwise the
    // regular parsing process would break!)
  } else if (has_implicit_statement_terminator(parser)) {
    return true;

  } else if (parser->current_token.type == TOKEN_EOF) {
    return true;
  }
  return false;
}

static void var_declaration(Compiler* compiler)
{
  uint8_t location = declare_variable("Expected variable's name.", compiler);

  if (match(TOKEN_EQUAL, compiler->parser)) {
    // optional initialization
    expression(compiler);
  } else {
    // implicit nil initialization
    emit_byte(OP_NIL, compiler);
  }
  if (!optional_semicolon(compiler->parser)) {
    error_at_current(
        "Expected ';' after variable's declaration", compiler->parser);
  }
  define_variable(location, compiler);
}

static void reserve_local_for_callee(FunctionCompiler* compiler)
{
  Local* local = &compiler->locals[compiler->locals_count++];
  local->scope_depth = 0;
  local->name.start = "";
  local->name.length = 0;
  local->is_captured = false;
}

static void function_compiler__init(
    FunctionCompiler* compiler, FunctionType function_type,
    FunctionCompiler* enclosing, Parser* parser, VM* vm)
{
  compiler->enclosing = enclosing;
  compiler->function_type = function_type;
  compiler->function = function__new(vm);
  compiler->locals_count = 0;
  compiler->scope_depth = 0;
  compiler->parser = parser;
  // we need this back reference so we can do garbage collection
  // on objects allocated on all nested function compilers
  vm->current_compiler = compiler;

  reserve_local_for_callee(compiler);
}

static void begin_scope(Compiler* compiler)
{
  compiler->function_compiler->scope_depth++;
}

static void end_scope(Compiler* compiler)
{
  pop_all_accessible_locals_in_scope(
      compiler, compiler->function_compiler->scope_depth);
  compiler->function_compiler->scope_depth--;
}

static void block(Compiler* compiler)
{
  while (!check(TOKEN_RIGHT_BRACE, compiler->parser) &
         !check(TOKEN_EOF, compiler->parser)) {
    declaration(compiler);
  }
  consume(TOKEN_RIGHT_BRACE, "Expect '}' after block", compiler->parser);
}

static ObjectFunction* finish_function_compilation(Compiler* compiler)
{
  emit_default_return(compiler);
  ObjectFunction* function = compiler->function_compiler->function;

#ifdef DEBUG_PRINT_CODE
  if (!compiler->parser->had_error && compiler->vm->show_bytecode) {
    debug__disassemble(
        current_bytecode(compiler),
        function->name != NULL ? function->name->chars : NULL);
    putchar('\n');
  }
#endif

  compiler->function_compiler = compiler->function_compiler->enclosing;
  return function;
}

static void function_parameters(Compiler* compiler)
{
  do {
    compiler->function_compiler->function->arity++;
    if (compiler->function_compiler->function->arity > 255) {
      error_at_current("Can't have more than 255 parameters", compiler->parser);
    }
    uint8_t location = declare_variable("Expected parameters name.", compiler);
    define_variable(location, compiler);

  } while (match(TOKEN_COMMA, compiler->parser));
}

static void start_function_compilation(
    Compiler* compiler, FunctionCompiler* function_compiler,
    FunctionType function_type)
{
  function_compiler__init(
      function_compiler, function_type,
      /* enclosing_compiler: */ compiler->function_compiler, compiler->parser,
      compiler->vm);
  compiler->function_compiler = function_compiler;

  if (function_type != TYPE_SCRIPT) {
    function_compiler->function->name = string__copy(
        compiler->parser->previous_token.start,
        compiler->parser->previous_token.length, compiler->vm);
  }
}

static void function(FunctionType type, Compiler* compiler)
{
  FunctionCompiler function_compiler;
  start_function_compilation(compiler, &function_compiler, type);

  // This `begin_scope` doesn’t have a corresponding `end_scope` call because we
  // end `function_compiler` completely by the end of this function.
  begin_scope(compiler);

  // function parameters
  consume(
      TOKEN_LEFT_PAREN, "Expected '(' after function name.", compiler->parser);
  if (!check(TOKEN_RIGHT_PAREN, compiler->parser)) {
    function_parameters(compiler);
  }
  consume(
      TOKEN_RIGHT_PAREN, "Expected ')' after parameters.", compiler->parser);

  // function body
  consume(
      TOKEN_LEFT_BRACE, "Expected '{' before function body.", compiler->parser);
  block(compiler);

  ObjectFunction* function = finish_function_compilation(compiler);
  emit_bytes(
      OP_CLOSURE, store_constant(OBJECT_VAL(function), compiler), compiler);

  for (int i = 0; i < function->upvalues_count; i++) {
    emit_byte(function_compiler.upvalues[i].is_local ? 1 : 0, compiler);
    emit_byte(function_compiler.upvalues[i].index, compiler);
  }
}

static void class_declaration(Compiler* compiler)
{
  uint8_t location = declare_variable("Expected class name.", compiler);

  emit_bytes(OP_CLASS, location, compiler);
  define_variable(location, compiler);

  consume(
      TOKEN_LEFT_BRACE, "Expected '{' before class body.", compiler->parser);
  consume(
      TOKEN_RIGHT_BRACE, "Expected '}' after class body.", compiler->parser);
}

static void fun_declaration(Compiler* compiler)
{
  uint8_t location = declare_variable("Expected function name.", compiler);
  // this allows recursive functions to work properly (there is no danger since
  // the definition of a function is not executed during the implicit
  // assignment)
  mark_latest_local_initialized(compiler->function_compiler);

  function(TYPE_FUNCTION, compiler);

  define_variable(location, compiler);
}

// pre-conditions: the operator for this expression has just been consumed (it
// is what triggered the invocation to this function via the rules table)
//
// post-conditions: the expression to which the operator applies (with the right
// precedence taken into account) has been consumed, and the corresponding
// bytecode has been emitted.
//
static void unary(Compiler* compiler)
{
  TokenType operator_type = compiler->parser->previous_token.type;
  // compile the operand "recursively" (`unary` is indirectly called from
  // `parse_only`) so that expressions such as `---1` are parsed as
  // `-(-(-1))`, i.e., with right-associativity.
  parse_only(/*min_precedence:*/ PREC_UNARY, compiler);

  switch (operator_type) {
  case TOKEN_BANG:
    emit_byte(OP_NOT, compiler);
    break;
  case TOKEN_MINUS:
    emit_byte(OP_NEGATE, compiler);
    break;
  default:
    return; // unreachable
  }
}

static void return_statement(Compiler* compiler)
{
  if (compiler->function_compiler->function_type == TYPE_SCRIPT) {
    error("Can't return from top-level code", compiler->parser);
  }

  if (match(TOKEN_SEMICOLON, compiler->parser)) {
    emit_default_return(compiler);

  } else {
    expression(compiler);
    if (optional_semicolon(compiler->parser)) {
      emit_byte(OP_RETURN, compiler);

    } else {
      error_at_current("Expected ';' after return value.", compiler->parser);
    }
  }
}

static void while_statement(Compiler* compiler)
{
  // expected bytecode generation:
  //
  //         [ condition expression ] <---+
  //         ~~~~~~~~~~~~~~~~~~~~~~~~     |
  //    +--- OP_JUMP_IF_FALSE             |
  //    |    OP_POP (pop condition)       |
  //    |                                 |
  //    |    [ body statement       ]     |
  //    |    ~~~~~~~~~~~~~~~~~~~~~~~~     |
  //    |    OP_LOOP ---------------------+
  //    |
  //    +--> OP_POP (pop condition)
  //         continues...
  //
  int loop_start_offset = current_bytecode(compiler)->count;
  consume(TOKEN_LEFT_PAREN, "Expected '(' after 'while'", compiler->parser);
  expression(compiler);
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after condition", compiler->parser);

  int exit_jump_offset = emit_placeholder_jump(OP_JUMP_IF_FALSE, compiler);
  emit_byte(OP_POP, compiler); // discard condition

  // compile body statement
  statement(compiler);
  emit_loop(loop_start_offset, compiler);

  patch_jump(exit_jump_offset, compiler);
  emit_byte(OP_POP, compiler); // discard condition
}

static void expression_statement(Compiler* compiler)
{
  expression(compiler);
  Parser* parser = compiler->parser;

  if (!optional_semicolon(parser)) {
    error_at_current("Expected ';' instead", compiler->parser);
  }

  // for the benefit of the REPL, we will automatically print the value of the
  // last expression (which would be otherwise lost)
  if (check(TOKEN_EOF, parser) &&
      compiler->vm->execution_mode == VM_REPL_MODE) {
    emit_byte(OP_PRINTLN, compiler);

  } else {
    emit_byte(OP_POP, compiler);
  }
}

static void for_statement(Compiler* compiler)
{
  // expected bytecode generation:
  //
  //         [   initializer clause  ]
  //         ~~~~~~~~~~~~~~~~~~~~~~~~~
  //         [  condition expression ] <--+
  //         ~~~~~~~~~~~~~~~~~~~~~~~~~    |
  //    +--- OP_JUMP_IF_FALSE             |
  //    |    OP_POP (pop condition)       |
  //  +-|--- OP_JUMP                      |
  //  | |                                 |
  //  | |    [  increment expression ] <--|---+
  //  | |    ~~~~~~~~~~~~~~~~~~~~~~~~~    |   |
  //  | |    OP_POP (pop expression)      |   |
  //  | |    OP_LOOP ---------------------+   |
  //  | |                                     |
  //  +-|--> [     body statement    ]        |
  //    |    ~~~~~~~~~~~~~~~~~~~~~~~~~        |
  //    |    OP_LOOP -------------------------+
  //    +--> OP_POP (pop condition)
  //         continues...
  //
  consume(TOKEN_LEFT_PAREN, "Expected '(' after 'for'", compiler->parser);
  // variables in the initializer should be local
  begin_scope(compiler);
  // initialization clause
  if (match(TOKEN_SEMICOLON, compiler->parser)) {
    // no initializer
  } else if (match(TOKEN_VAR, compiler->parser)) {
    var_declaration(compiler);
  } else {
    expression_statement(compiler);
  }

  // condition clause
  int loop_start_offset = current_bytecode(compiler)->count;
  int exit_jump_offset = -1;
  if (!match(TOKEN_SEMICOLON, compiler->parser)) {
    expression(compiler);
    consume(
        TOKEN_SEMICOLON, "Expected ';' after loop condition.",
        compiler->parser);
    // jump out of the loop if the condition is false
    exit_jump_offset = emit_placeholder_jump(OP_JUMP_IF_FALSE, compiler);
    emit_byte(OP_POP, compiler); // pop condition before executing body
  }

  // increment clause
  //
  // the convoluted jumping done here is due to the fact that syntactically the
  // increment appears before the body, but semantically the body should be
  // executed before the increment (remember we're doing single-pass
  // compilation)
  //
  if (!match(TOKEN_RIGHT_PAREN, compiler->parser)) {
    // skip over the increment directly into the body
    int body_jump_offset = emit_placeholder_jump(OP_JUMP, compiler);
    int increment_start_offset = current_bytecode(compiler)->count;
    // compile increment expression
    expression(compiler);
    emit_byte(OP_POP, compiler); // discard increment expression
    consume(
        TOKEN_RIGHT_PAREN, "Expected ')' after 'for' clauses",
        compiler->parser);

    // go back to the start of the loop (the condition clause)
    emit_loop(loop_start_offset, compiler);

    // ensure we loop back to the increment block after the body block
    loop_start_offset = increment_start_offset;

    patch_jump(body_jump_offset, compiler);
  }
  // body block
  statement(compiler);
  emit_loop(loop_start_offset, compiler);

  if (exit_jump_offset != -1) {
    patch_jump(exit_jump_offset, compiler);
    emit_byte(OP_POP, compiler); // pop condition after final exit jump
  }
  end_scope(compiler);
}

static void if_statement(Compiler* compiler)
{
  // expected bytecode generation:
  //
  //         [ condition expression ]
  //         ~~~~~~~~~~~~~~~~~~~~~~~~
  //    +--- OP_JUMP_IF_FALSE
  //    |    OP_POP (pop condition)
  //    |
  //    |    [ then branch statement ]
  //    |    ~~~~~~~~~~~~~~~~~~~~~~~~~
  //  +-|--- OP_JUMP
  //  | |
  //  | +--> OP_POP (pop condition)
  //  |      [ else branch statement ]
  //  |      ~~~~~~~~~~~~~~~~~~~~~~~~~
  //  |
  //  +----> continues...
  //
  consume(TOKEN_LEFT_PAREN, "Expected '(' after 'if'.", compiler->parser);
  // [ condition expression ]
  expression(compiler);
  consume(TOKEN_RIGHT_PAREN, "Expected ')' after condition.", compiler->parser);
  // conditionally jump to just before "else" branch
  int then_jump_offset = emit_placeholder_jump(OP_JUMP_IF_FALSE, compiler);
  emit_byte(OP_POP, compiler);

  // [ then branch statement ]
  statement(compiler);
  // jump over "else" branch since we just executed "then" branch
  int else_jump_offset = emit_placeholder_jump(OP_JUMP, compiler);

  patch_jump(then_jump_offset, compiler);
  emit_byte(OP_POP, compiler);
  // [ else branch statement ]
  if (match(TOKEN_ELSE, compiler->parser)) {
    statement(compiler);
  }
  // continues...
  patch_jump(else_jump_offset, compiler);
}

static void synchronize(Parser* parser)
{
  parser->panic_mode = false;
  while (parser->current_token.type != TOKEN_EOF) {
    if (parser->previous_token.type == TOKEN_SEMICOLON)
      return;
    switch (parser->current_token.type) {
    case TOKEN_CLASS:
    case TOKEN_FUN:
    case TOKEN_VAR:
    case TOKEN_FOR:
    case TOKEN_IF:
    case TOKEN_WHILE:
    case TOKEN_RETURN:
      return;
    default:; // do nothing
    }
    advance(parser);
  }
}

static void declaration(Compiler* compiler)
{
  if (match(TOKEN_CLASS, compiler->parser)) {
    class_declaration(compiler);

  } else if (match(TOKEN_FUN, compiler->parser)) {
    fun_declaration(compiler);

  } else if (match(TOKEN_VAR, compiler->parser)) {
    var_declaration(compiler);

  } else {
    statement(compiler);
  }
  if (compiler->parser->panic_mode) {
    synchronize(compiler->parser);
  }
}

static void statement(Compiler* compiler)
{
  if (match(TOKEN_IF, compiler->parser)) {
    if_statement(compiler);

  } else if (match(TOKEN_RETURN, compiler->parser)) {
    return_statement(compiler);

  } else if (match(TOKEN_WHILE, compiler->parser)) {
    while_statement(compiler);

  } else if (match(TOKEN_FOR, compiler->parser)) {
    for_statement(compiler);

  } else if (match(TOKEN_LEFT_BRACE, compiler->parser)) {
    begin_scope(compiler);
    block(compiler);
    end_scope(compiler);

  } else {
    expression_statement(compiler);
  }
}

// parses (and compiles) the operator and right operand of a binary
// expression in a left-associative manner (i.e., `1+2+3` is parsed
// as `((1+2)+3)`)
//
// pre-conditions:
// + the left operand for this operation has already been compiled
// + the operator for this expression has just been consumed (it is in fact
//   what triggered the invocation to this function via the rules table)
//
// post-condition: the smallest expression of higher precedence than
// that associated with this binary operation has been compiled
static void binary(Compiler* compiler)
{
  TokenType operator_type = compiler->parser->previous_token.type;
  ParseRule* rule = get_parse_rule(operator_type);
  // by passing `rule->precedence + 1` we ensure that parsing done by this
  // function is left-associative.
  //
  // this happens because by passing a higher-precedence, we prevent a
  // recursive call to `binary` via `parse_only`, so that if the original
  // expression is `1+2+3` only `(1+2)` is parsed here, followed by `(...+3)` in
  // the caller of this function (i.e., in `parse_only` via `parse_infix` inside
  // the parsing loop)
  //
  // notice that if we used `rule->precedence` instead, we would parse
  // the same kind of expression recursively (binary -> parse_only ->
  // binary), so `1+2+3` would parse as right-associative (i.e., as `(1+(2+3))`)
  //
  // see https://craftinginterpreters.com/compiling-expressions.html for more
  // details
  parse_only(
      /*min_precedence:*/ (Precedence)(rule->precedence + 1), compiler);

  switch (operator_type) {
  case TOKEN_BANG_EQUAL:
    emit_bytes(OP_EQUAL, OP_NOT, compiler);
    break;
  case TOKEN_EQUAL_EQUAL:
    emit_byte(OP_EQUAL, compiler);
    break;
  case TOKEN_GREATER:
    emit_byte(OP_GREATER, compiler);
    break;
  case TOKEN_GREATER_EQUAL:
    emit_bytes(OP_LESS, OP_NOT, compiler);
    break;
  case TOKEN_LESS:
    emit_byte(OP_LESS, compiler);
    break;
  case TOKEN_LESS_EQUAL:
    emit_bytes(OP_GREATER, OP_NOT, compiler);
    break;
  case TOKEN_PLUS:
    emit_byte(OP_ADD, compiler);
    break;
  case TOKEN_MINUS:
    emit_byte(OP_SUBTRACT, compiler);
    break;
  case TOKEN_STAR:
    emit_byte(OP_MULTIPLY, compiler);
    break;
  case TOKEN_SLASH:
    emit_byte(OP_DIVIDE, compiler);
    break;
  default:
    return; // unreachable
  }
}

static void call(Compiler* compiler)
{
  uint8_t args_count = argument_list(compiler);
  emit_bytes(OP_CALL, args_count, compiler);
}

static void dot(Compiler* compiler)
{
  uint8_t location =
      declare_variable("Expected property name after '.'", compiler);

  if (compiler->can_assign && match(TOKEN_EQUAL, compiler->parser)) {
    expression(compiler);
    emit_bytes(OP_SET_PROPERTY, location, compiler);

  } else {
    emit_bytes(OP_GET_PROPERTY, location, compiler);
  }
}

static void grouping(Compiler* compiler)
{
  expression(compiler);
  consume(
      TOKEN_RIGHT_PAREN, "Expected ')' after expression.", compiler->parser);
}

static void number(Compiler* compiler)
{
  // if `residue: char**` is non-null, `strtod` sets it to point to the rest
  // of the string which couldn't be parsed as a double.
  double value =
      strtod(compiler->parser->previous_token.start, /*residue:*/ NULL);
  emit_constant(NUMBER_VAL(value), compiler);
}

static void string(Compiler* compiler)
{
  Parser* parser = compiler->parser;

  ObjectString* _string = string__copy(
      parser->previous_token.start + 1, parser->previous_token.length - 2,
      compiler->vm);

  emit_constant(OBJECT_VAL(_string), compiler);
}

static void literal(Compiler* compiler)
{
  switch (compiler->parser->previous_token.type) {
  case TOKEN_FALSE:
    emit_byte(OP_FALSE, compiler);
    break;
  case TOKEN_NIL:
    emit_byte(OP_NIL, compiler);
    break;
  case TOKEN_TRUE:
    emit_byte(OP_TRUE, compiler);
    break;
  default:
    return; // unreachable;
  }
}

static void parser__init(Parser* parser, const char* source_code)
{
  parser->previous_token = BOF_TOKEN;
  parser->current_token = BOF_TOKEN;
  parser->had_error = false;
  parser->panic_mode = false;
  scanner__init(&parser->scanner, source_code);
}

static void compiler__init(
    Compiler* compiler, Parser* parser, FunctionCompiler* function_compiler,
    VM* vm)
{
  compiler->vm = vm;
  compiler->parser = parser;
  compiler->function_compiler = function_compiler;
  compiler->can_assign = false;
}

ObjectFunction* compiler__compile(const char* source_code, VM* vm)
{
  Parser parser;
  parser__init(&parser, source_code);

  FunctionCompiler function_compiler;
  function_compiler__init(
      &function_compiler, TYPE_SCRIPT, /*enclosing_compiler:*/ NULL, &parser,
      vm);

  Compiler compiler;
  compiler__init(&compiler, &parser, &function_compiler, vm);

  // kickstart parsing & compilation
  advance(&parser);
  while (!match(TOKEN_EOF, &parser)) {
    declaration(&compiler);
  }

  ObjectFunction* function = finish_function_compilation(&compiler);
  return parser.had_error ? NULL : function;
}

/* clang-format off */
ParseRule rules[] = {
  // TOKEN                    PREFIX     INFIX   PRECEDENCE
  [TOKEN_LEFT_PAREN]        = {grouping, call,   PREC_CALL},
  [TOKEN_RIGHT_PAREN]       = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RIGHT_BRACE]       = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]             = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]               = {NULL,     dot,    PREC_CALL},
  [TOKEN_MINUS]             = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]              = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]             = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]              = {NULL,     binary, PREC_FACTOR},
  [TOKEN_BANG]              = {unary,    NULL,   PREC_NONE},
  [TOKEN_BANG_EQUAL]        = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_EQUAL]             = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL_EQUAL]       = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_GREATER]           = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_GREATER_EQUAL]     = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS]              = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS_EQUAL]        = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_IDENTIFIER]        = {variable, NULL,   PREC_NONE},
  [TOKEN_STRING]            = {string,   NULL,   PREC_NONE},
  [TOKEN_NUMBER]            = {number,   NULL,   PREC_NONE},
  [TOKEN_AND]               = {NULL,     and_,   PREC_AND},
  [TOKEN_CLASS]             = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]              = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]             = {literal,  NULL,   PREC_NONE},
  [TOKEN_FOR]               = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]               = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]                = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]               = {literal,  NULL,   PREC_NONE},
  [TOKEN_OR]                = {NULL,     or_,    PREC_OR},
  [TOKEN_RETURN]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]             = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]              = {NULL,     NULL,   PREC_NONE},
  [TOKEN_TRUE]              = {literal,  NULL,   PREC_NONE},
  [TOKEN_VAR]               = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]             = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]             = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]               = {NULL,     NULL,   PREC_NONE},
  [TOKEN_MULTILINE_COMMENT] = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NEWLINE]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IGNORABLE]         = {NULL,     NULL,   PREC_NONE},
};
/* clang-format on */

static ParseRule* get_parse_rule(TokenType type) { return &rules[type]; }

void compiler__mark_roots(FunctionCompiler* current)
{
  while (current != NULL) {
    memory__mark_object_as_alive((Object*)(current->function));
    current = current->enclosing;
  }
}
