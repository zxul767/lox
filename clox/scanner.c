#include "scanner.h"
#include "common.h"

#include <stdio.h>
#include <string.h>

void scanner__init(Scanner *scanner, const char *source) {
  scanner->start = source;
  scanner->current = source;
  scanner->line = 1;
}

static bool is_at_end(const Scanner *scanner) {
  return *(scanner->current) == '\0';
}

static Token make_token(TokenType type, const Scanner *scanner) {
  Token token;
  token.type = type;
  token.start = scanner->start;
  token.length = (int)(scanner->current - scanner->start);
  token.line = scanner->line;
  return token;
}

static Token error_token(const char *message, const Scanner *scanner) {
  Token token;
  token.type = TOKEN_ERROR;
  token.start = message;
  token.length = (int)strlen(message);
  token.line = scanner->line;
  return token;
}

static char advance(Scanner *scanner) {
  scanner->current++;
  return scanner->current[-1];
}

static char peek(Scanner *scanner) { return *(scanner->current); }
static char peek_next(Scanner *scanner) {
  if (is_at_end(scanner))
    return '\0';
  return scanner->current[1];
}

static bool match(char expected, Scanner *scanner) {
  if (is_at_end(scanner))
    return false;
  if (*scanner->current != expected)
    return false;
  scanner->current++;
  return true;
}

static void skip_whitespace(Scanner *scanner) {
  for (;;) {
    char c = peek(scanner);
    switch (c) {
    case ' ':
    case '\r':
    case '\t':
      advance(scanner);
      break;
    case '\n':
      scanner->line++;
      advance(scanner);
      break;
    case '/':
      if (peek_next(scanner) == '/') {
        // a comment goes until the end of the line
        while (peek(scanner) != '\n' && !is_at_end(scanner))
          advance(scanner);
      } else {
        return;
      }
      break;
    default:
      return;
    }
  }
}

Token scanner__next_token(Scanner *scanner) {
  skip_whitespace(scanner);

  scanner->start = scanner->current;
  if (is_at_end(scanner))
    return make_token(TOKEN_EOF, scanner);

  char c = advance(scanner);
  switch (c) {
  case '(':
    return make_token(TOKEN_LEFT_PAREN, scanner);
  case ')':
    return make_token(TOKEN_RIGHT_PAREN, scanner);
  case '{':
    return make_token(TOKEN_LEFT_BRACE, scanner);
  case '}':
    return make_token(TOKEN_RIGHT_BRACE, scanner);
  case ';':
    return make_token(TOKEN_SEMICOLON, scanner);
  case ',':
    return make_token(TOKEN_COMMA, scanner);
  case '.':
    return make_token(TOKEN_DOT, scanner);
  case '-':
    return make_token(TOKEN_MINUS, scanner);
  case '+':
    return make_token(TOKEN_PLUS, scanner);
  case '/':
    return make_token(TOKEN_SLASH, scanner);
  case '*':
    return make_token(TOKEN_STAR, scanner);
  case '!':
    return make_token(match('=', scanner) ? TOKEN_BANG_EQUAL : TOKEN_BANG,
                      scanner);
  case '=':
    return make_token(match('=', scanner) ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL,
                      scanner);
  case '<':
    return make_token(match('=', scanner) ? TOKEN_LESS_EQUAL : TOKEN_LESS,
                      scanner);
  case '>':
    return make_token(match('=', scanner) ? TOKEN_GREATER_EQUAL : TOKEN_GREATER,
                      scanner);
  }
  return error_token("Unexpected character.", scanner);
}
