#include <assert.h>
#include <stdio.h>
#include <stdlib.h>

#include "common.h"
#include "compiler.h"
#include "scanner.h"

#ifdef DEBUG_PRINT_CODE
#include "debug.h"
#endif

static const char *string__remove_prefix(const char *prefix,
                                         const char *string) {
  while (*string && *prefix && *string == *prefix) {
    prefix++;
    string++;
  }
  return string;
}

// There is no separate parser.c file because this is a single-pass compiler,
// so we don't build explicit intermediate code representations (i.e., ASTs).
//
// Therefore, the parsing and code emitting phases are fused into a single
// module.
//
typedef struct {
  Token current;
  Token previous;

  bool had_error;
  bool panic_mode;

  Scanner *scanner;

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
  Parser *parser;
  Bytecode *current_bytecode;

} Compiler;

typedef void (*ParseFn)(Compiler *);
typedef struct {
  ParseFn prefix;
  ParseFn infix;
  Precedence precedence;

} ParseRule;

static void error_at(Token *token, const char *message, Parser *parser) {
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

static void error(const char *message, Parser *parser) {
  error_at(&parser->previous, message, parser);
}

static void error_at_current(const char *message, Parser *parser) {
  error_at(&parser->current, message, parser);
}

static void parser__advance(Parser *parser) {
  parser->previous = parser->current;
  for (;;) {
    parser->current = scanner__next_token(parser->scanner);
    /* fprintf(stderr, "next: %s\n", TOKEN_TO_STRING[parser->current.type]); */
    if (parser->current.type != TOKEN_ERROR)
      break;
    error_at_current(parser->current.start, parser);
  }
}

static void parser__init(Parser *parser, Scanner *scanner) {
  parser->had_error = false;
  parser->panic_mode = false;
  parser->scanner = scanner;
}

static void init(Compiler *compiler, Parser *parser, Bytecode *bytecode) {
  compiler->parser = parser;
  compiler->current_bytecode = bytecode;
}

static void parser__consume(TokenType type, const char *message,
                            Parser *parser) {
  if (parser->current.type == type) {
    parser__advance(parser);
    return;
  }
  error_at_current(message, parser);
}

static void emit_byte(uint8_t byte, Compiler *compiler) {
  bytecode__append(compiler->current_bytecode, byte,
                   compiler->parser->previous.line);
}

static void emit_bytes(uint8_t byte1, uint8_t byte2, Compiler *compiler) {
  emit_byte(byte1, compiler);
  emit_byte(byte2, compiler);
}

static void emit_return(Compiler *compiler) { emit_byte(OP_RETURN, compiler); }

static uint8_t make_constant(Value value, Compiler *compiler) {
  int index = bytecode__store_constant(compiler->current_bytecode, value);
  if (index > UINT8_MAX) {
    error("Too many constants in one chunk", compiler->parser);
    return 0;
  }
  return (uint8_t)index;
}

static void emit_constant(Value value, Compiler *compiler) {
  emit_bytes(OP_CONSTANT, make_constant(value, compiler), compiler);
}

static void finish(Compiler *compiler) {
  emit_return(compiler);
#ifdef DEBUG_PRINT_CODE
  if (!compiler->parser->had_error) {
    debug__disassemble(compiler->current_bytecode, "COMPILED CODE");
    putchar('\n');
  }
#endif
}

// Forward declarations to break cyclic references between the set
// of rules (which reference parsing functions) and `get_parsing_rule`
// which is invoked in one or more of those parsing functions.
static void expression(Compiler *);
static ParseRule *get_parsing_rule(TokenType);
static void parse_precedence(Precedence, Compiler *);

static void binary(Compiler *compiler) {
  TokenType operator_type = compiler->parser->previous.type;
  ParseRule *rule = get_parsing_rule(operator_type);
  parse_precedence((Precedence)(rule->precedence + 1), compiler);

  switch (operator_type) {
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

static void parse_precedence(Precedence precedence, Compiler *compiler) {
  Parser *parser = compiler->parser;

  parser__advance(parser);
  ParseFn prefix_rule = get_parsing_rule(parser->previous.type)->prefix;
  if (prefix_rule == NULL) {
    error("Expected expression", parser);
    return;
  }
  prefix_rule(compiler);

  while (precedence <= get_parsing_rule(parser->current.type)->precedence) {
    parser__advance(parser);
    ParseFn infix_rule = get_parsing_rule(parser->previous.type)->infix;
    if (infix_rule == NULL) {
      error("Expected operator", parser);
      return;
    }
    infix_rule(compiler);
  }
}

static void expression(Compiler *compiler) {
  parse_precedence(PREC_ASSIGNMENT, compiler);
}

static void grouping(Compiler *compiler) {
  expression(compiler);
  parser__consume(TOKEN_RIGHT_PAREN, "Expected ')' after expression.",
                  compiler->parser);
}

static void number(Compiler *compiler) {
  // if `residue: char**` is non-null, `strtod` sets it to point to the rest
  // of the string which couldn't be parsed as a double
  double value = strtod(compiler->parser->previous.start, /*residue:*/ NULL);
  emit_constant(value, compiler);
}

static void unary(Compiler *compiler) {
  TokenType operator_type = compiler->parser->previous.type;
  // compile the operand
  parse_precedence(PREC_UNARY, compiler);
  // emit the operator instruction
  switch (operator_type) {
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
  [TOKEN_BANG]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_BANG_EQUAL]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EQUAL_EQUAL]   = {NULL,     NULL,   PREC_NONE},
  [TOKEN_GREATER]       = {NULL,     NULL,   PREC_NONE},
  [TOKEN_GREATER_EQUAL] = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LESS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_LESS_EQUAL]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IDENTIFIER]    = {NULL,     NULL,   PREC_NONE},
  [TOKEN_STRING]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NUMBER]        = {number,   NULL,   PREC_NONE},
  [TOKEN_AND]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_CLASS]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ELSE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FALSE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FOR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_FUN]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_IF]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_NIL]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_OR]            = {NULL,     NULL,   PREC_NONE},
  [TOKEN_PRINT]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_RETURN]        = {NULL,     NULL,   PREC_NONE},
  [TOKEN_SUPER]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_THIS]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_TRUE]          = {NULL,     NULL,   PREC_NONE},
  [TOKEN_VAR]           = {NULL,     NULL,   PREC_NONE},
  [TOKEN_WHILE]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_ERROR]         = {NULL,     NULL,   PREC_NONE},
  [TOKEN_EOF]           = {NULL,     NULL,   PREC_NONE},
};
/* clang-format on */

static ParseRule *get_parsing_rule(TokenType type) { return &rules[type]; }

bool compiler__compile(const char *source, Bytecode *bytecode) {
  Scanner scanner;
  scanner__init(&scanner, source);

  Parser parser;
  parser__init(&parser, &scanner);

  Compiler compiler;
  init(&compiler, &parser, bytecode);

  // kickstart parsing & compilation
  parser__advance(&parser);

  expression(&compiler);
  parser__consume(TOKEN_EOF, "Expected end of expression", &parser);

  finish(&compiler);
  return !parser.had_error;
}

static void scan_tokens(const char *source) {
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
           string__remove_prefix("TOKEN_", TOKEN_TO_STRING[token.type]),
           token.length, token.start);

    if (token.type == TOKEN_EOF)
      break;
  }
}
