#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
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

  bool had_error;
  bool panic_mode;

  Scanner* scanner;

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

typedef struct {
  Parser* parser;
  Bytecode* output_bytecode;

  // `vm->objects` tracks references to all heap-allocated
  // objects to be disposed when the VM shuts-down. In the
  // compiler, such objects are string literals.
  VM* vm;

} Compiler;

typedef void (*ParseFn)(Compiler*);
typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;

} ParseRule;

static void error_at(Token* token, const char* message, Parser* parser) {
  // Don't report errors which are likely to be cascade errors (panic mode
  // means we failed to parse a rule and we're lost until we perform a
  // synchronization, so intermediate errors are likely to be spurious)
  if (parser->panic_mode)
    return;
  parser->panic_mode = true;

  fprintf(stderr, "[line %d] Error", token->line);
  if (token->type == TOKEN_EOF) {
    fprintf(stderr, " at end");
  } else if (token->type == TOKEN_ERROR) {
    // Nothing
  } else {
    fprintf(stderr, " at '%.*s'", token->length, token->start);
  }
  fprintf(stderr, ": %s\n", message);
  parser->had_error = true;
}

static void error(const char* message, Parser* parser) {
  error_at(&parser->previous_token, message, parser);
}

static void error_at_current(const char* message, Parser* parser) {
  error_at(&parser->current_token, message, parser);
}

static void parser__advance(Parser* parser) {
  parser->previous_token = parser->current_token;
  for (;;) {
    parser->current_token = scanner__next_token(parser->scanner);
    if (parser->current_token.type != TOKEN_ERROR)
      break;
    error_at_current(parser->current_token.start, parser);
  }
}

static void parser__init(Parser* parser, Scanner* scanner) {
  parser->previous_token = BOF_TOKEN;
  parser->current_token = BOF_TOKEN;
  parser->had_error = false;
  parser->panic_mode = false;
  parser->scanner = scanner;
}

static void init(Compiler* compiler, Parser* parser, Bytecode* bytecode,
                 VM* vm) {
  compiler->parser = parser;
  compiler->output_bytecode = bytecode;
  compiler->vm = vm;
}

static void parser__consume(TokenType type, const char* message,
                            Parser* parser) {
  if (parser->current_token.type == type) {
    parser__advance(parser);
    return;
  }
  error_at_current(message, parser);
}

static bool parser__check(TokenType type, Parser* parser) {
  return parser->current_token.type == type;
}

static bool parser__match(TokenType type, Parser* parser) {
  if (!parser__check(type, parser))
    return false;
  parser__advance(parser);
  return true;
}

static void emit_byte(uint8_t byte, Compiler* compiler) {
  bytecode__append(compiler->output_bytecode, byte,
                   compiler->parser->previous_token.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2, Compiler* compiler) {
  emit_byte(byte1, compiler);
  emit_byte(byte2, compiler);
}

static void emit_return(Compiler* compiler) { emit_byte(OP_RETURN, compiler); }

static uint8_t make_constant(Value value, Compiler* compiler) {
  int index = bytecode__store_constant(compiler->output_bytecode, value);
  if (index > UINT8_MAX) {
    error("Too many constants in one chunk", compiler->parser);
    return 0;
  }
  return (uint8_t)index;
}

static void emit_constant(Value value, Compiler* compiler) {
  emit_bytes(OP_CONSTANT, make_constant(value, compiler), compiler);
}

static void finish(Compiler* compiler) {
  emit_return(compiler);
#ifdef DEBUG_PRINT_CODE
  if (!compiler->parser->had_error) {
    debug__disassemble(compiler->output_bytecode, "COMPILED CODE");
    putchar('\n');
  }
#endif
}

// Forward declarations to break cyclic references between the set
// of rules (which reference parsing functions) and `get_parsing_rule`
// which is invoked in one or more of those parsing functions.
static ParseRule* get_parsing_rule(TokenType);
static void parse_with(Precedence min_precedence, Compiler*);
// blocks can contain declarations, and control flow statements can contain
// other statements; that means these two functions will eventually be recursive
static void statement();
static void declaration();

static uint8_t identifier_constant(Token* identifier, Compiler* compiler) {
  return make_constant(
      OBJECT_VAL(
          string__copy(identifier->start, identifier->length, compiler->vm)),
      compiler);
}

static uint8_t parse_variable(const char* error_message, Compiler* compiler) {
  parser__consume(TOKEN_IDENTIFIER, error_message, compiler->parser);
  return identifier_constant(&compiler->parser->previous_token, compiler);
}

static void define_global(uint8_t index, Compiler* compiler) {
  emit_bytes(OP_DEFINE_GLOBAL, index, compiler);
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
static void binary(Compiler* compiler) {
  TokenType operator_type = compiler->parser->previous_token.type;
  ParseRule* rule = get_parsing_rule(operator_type);
  // by passing `rule->precedence + 1` we ensure that parsing done by this
  // function is left-associative.
  //
  // this happens because by passing a higher-precedence, we prevent a
  // recursive call to `binary` via `parse_with`, and instead only `(1 + 2)`
  // is parsed here, followed by `(... + 3)` in the caller of this function
  // (i.e., in `parse_with` via `parse_infix` inside the parsing loop)
  //
  // notice that if we used `rule->precedence` instead, we would parse
  // the same kind of expression recursively (binary -> parse_with -> binary),
  // so `1 + 2 + 3` would parse as right-associative (i.e., as `(1 + (2 + 3))`)
  //
  // see https://craftinginterpreters.com/compiling-expressions.html for more
  // details
  parse_with(/*min_precedence:*/ (Precedence)(rule->precedence + 1), compiler);

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

static void literal(Compiler* compiler) {
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

// parses (and compiles) either a unary expression; or a binary one, as long as
// the binary operation's precedence is higher than `min_precedence`
//
// pre-condition: compiler->scanner is looking at the first token of a new
// expression to be parsed
//
static void parse_with(Precedence min_precedence, Compiler* compiler) {
  Parser* parser = compiler->parser;

  // consume the next token to decide which parse function we need next
  parser__advance(parser);
  ParseFn parse_prefix = get_parsing_rule(parser->previous_token.type)->prefix;
  if (parse_prefix == NULL) {
    error("Unexpected token in primary expression", parser);
    return;
  }
  // we compile what could be a single unary expression, or the first operand of
  // larger binary expression...
  parse_prefix(compiler);

  // at this point, we've either:
  // 1) compiled a full expression and we won't enter the loop (since EOF has
  //    the lowest precedence of all tokens);
  // 2) compiled the first operand of a binary expression and we may or may not
  // enter the loop, depending on whether the next token/operator has higher
  // or equal precedence than required by `min_precedence`
  while (get_parsing_rule(parser->current_token.type)->precedence >=
         min_precedence) {
    ParseFn parse_infix = get_parsing_rule(parser->current_token.type)->infix;
    // we consume the operator and parse the rest of the expression
    parser__advance(parser);
    if (parse_infix == NULL) {
      error("Expected valid operator after expression", parser);
      return;
    }
    parse_infix(compiler);
  }
}

static void expression(Compiler* compiler) {
  // assignments have the lowest precedence level of all expressions
  // so this parses any expression
  parse_with(/*min_precedence:*/ PREC_ASSIGNMENT, compiler);
}

static void var_declaration(Compiler* compiler) {
  uint8_t global = parse_variable("Expected variable name.", compiler);

  if (parser__match(TOKEN_EQUAL, compiler->parser)) {
    expression(compiler);
  } else {
    // implicit nil initialization
    emit_byte(OP_NIL, compiler);
  }
  parser__consume(TOKEN_SEMICOLON, "Expected '; after variable declaration'",
                  compiler->parser);

  define_global(global, compiler);
}

static void expression_statement(Compiler* compiler) {
  expression(compiler);
  parser__consume(TOKEN_SEMICOLON, "Expected ';' after expression",
                  compiler->parser);
  emit_byte(OP_POP, compiler);
}

static void print_statement(Compiler* compiler) {
  expression(compiler);
  parser__consume(TOKEN_SEMICOLON, "Expected ';' after expression",
                  compiler->parser);
  emit_byte(OP_PRINT, compiler);
}

static void synchronize(Parser* parser) {
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
    case TOKEN_PRINT:
    case TOKEN_RETURN:
      return;
    default:; // do nothing
    }
    parser__advance(parser);
  }
}

static void declaration(Compiler* compiler) {
  if (parser__match(TOKEN_VAR, compiler->parser)) {
    var_declaration(compiler);
  } else {
    statement(compiler);
  }
  if (compiler->parser->panic_mode) {
    synchronize(compiler->parser);
  }
}

static void statement(Compiler* compiler) {
  if (parser__match(TOKEN_PRINT, compiler->parser)) {
    print_statement(compiler);
  } else {
    expression_statement(compiler);
  }
}

static void grouping(Compiler* compiler) {
  expression(compiler);
  parser__consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.",
                  compiler->parser);
}

static void number(Compiler* compiler) {
  // if `residue: char**` is non-null, `strtod` sets it to point to the rest
  // of the string which couldn't be parsed as a double.
  double value =
      strtod(compiler->parser->previous_token.start, /*residue:*/ NULL);
  emit_constant(NUMBER_VAL(value), compiler);
}

static void string(Compiler* compiler) {
  Parser* parser = compiler->parser;

  ObjectString* _string =
      string__copy(parser->previous_token.start + 1,
                   parser->previous_token.length - 2, compiler->vm);

  emit_constant(OBJECT_VAL(_string), compiler);
}

static void named_variable(Token name, Compiler* compiler) {
  uint8_t index = identifier_constant(&name, compiler);
  emit_bytes(OP_GET_GLOBAL, index, compiler);
}

static void variable(Compiler* compiler) {
  named_variable(compiler->parser->previous_token, compiler);
}

// pre-conditions: the operator for this expression has just been consumed (it
// is what triggered the invocation to this function via the rules table)
//
// post-conditions: the expression to which the operator applies (with the right
// precedence taken into account) has been consumed, and the corresponding
// bytecode has been emitted.
//
static void unary(Compiler* compiler) {
  TokenType operator_type = compiler->parser->previous_token.type;
  // compile the operand "recursively" (`unary` is indirectly called from
  // `parse_with`) so that expressions such as `---1` are parsed as `-(-(-1))`,
  // i.e., with right-associativity.
  parse_with(/*min_precedence:*/ PREC_UNARY, compiler);

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

/* clang-format off */
ParseRule rules[] = {
  // TOKEN                PREFIX     INFIX   PRECEDENCE
  [TOKEN_LEFT_PAREN]    = {grouping, NULL,   PREC_NONE},
  [TOKEN_RIGHT_PAREN]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LEFT_BRACE]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RIGHT_BRACE]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_COMMA]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_DOT]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_MINUS]         = {unary,    binary, PREC_TERM},
  [TOKEN_PLUS]          = {NULL,     binary, PREC_TERM},
  [TOKEN_SEMICOLON]     = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SLASH]         = {NULL,     binary, PREC_FACTOR},
  [TOKEN_STAR]          = {NULL,     binary, PREC_FACTOR},
  [TOKEN_BANG]          = {unary,    NULL,   PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL_EQUAL]   = {NULL,     binary, PREC_EQUALITY},
  [TOKEN_GREATER]       = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_GREATER_EQUAL] = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS]          = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_LESS_EQUAL]    = {NULL,     binary, PREC_COMPARISON},
  [TOKEN_IDENTIFIER]    = {variable, NULL,   PREC_NONE},
  [TOKEN_STRING]        = {string,   NULL,   PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {literal,  NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]           = {literal,  NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_TRUE]          = {literal,  NULL,   PREC_NONE},
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};
/* clang-format on */

static ParseRule* get_parsing_rule(TokenType type) { return &rules[type]; }

bool compiler__compile(const char* source, Bytecode* bytecode, VM* vm) {
  Scanner scanner;
  scanner__init(&scanner, source);

  Parser parser;
  parser__init(&parser, &scanner);

  Compiler compiler;
  init(&compiler, &parser, bytecode, vm);

  // kickstart parsing & compilation
  parser__advance(&parser);

  while (!parser__match(TOKEN_EOF, &parser)) {
    declaration(&compiler);
  }

  finish(&compiler);

  return !parser.had_error;
}
