#include "scanner.h"
#include "common.h"

#include <assert.h>
#include <stdarg.h>
#include <stdio.h>
#include <string.h>

const char* TOKEN_TO_STRING[] = {FOREACH_TOKEN(GENERATE_STRING)};

// TODO: is it possible to elegantly generate the strings from TOKEN_TO_STRING?
const char* KEYWORDS[] = {"and",  "class", "else", "false", "for",    "fun",
                          "if",   "nil",   "or",   "print", "return", "super",
                          "this", "true",  "var",  "while", NULL};

// sentinel tokens
const Token BOF_TOKEN = {
    .type = TOKEN_BOF, .start = "", .length = 0, .line = 0};

// we don't need positional information for newlines (for the time being)
const Token NEWLINE_TOKEN = {
    .type = TOKEN_NEWLINE, .start = "", .length = 0, .line = 0};

// we don't need positional information for ignorables (for the time being)
const Token IGNORABLE_TOKEN = {
    .type = TOKEN_IGNORABLE, .start = "", .length = 0, .line = 0};

void scanner__init(Scanner* scanner, const char* source_code)
{
  scanner->start = source_code;
  scanner->current = source_code;
  scanner->current_line = 1;
}

static bool is_digit(char c) { return c >= '0' && c <= '9'; }

static bool is_alpha(char c)
{
  return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
}

static bool is_at_end(const Scanner* scanner)
{
  return *(scanner->current) == '\0';
}

static Token error_token(const Scanner* scanner, const char* format, ...)
{
  static char error_message[1024];

  va_list args;
  va_start(args, format);
  sprintf(error_message, "Runtime Error: ");
  vsprintf(error_message, format, args);
  va_end(args);

  Token token;
  token.type = TOKEN_ERROR;
  token.start = error_message;
  token.length = (int)strlen(error_message);
  token.line = scanner->current_line;
  return token;
}

static Token make_token(TokenType type, const Scanner* scanner)
{
  Token token;
  token.type = type;
  token.start = scanner->start;
  token.length = (int)(scanner->current - scanner->start);
  token.line = scanner->current_line;
  return token;
}

static char advance(Scanner* scanner)
{
  scanner->current++;
  return scanner->current[-1];
}

static char peek(Scanner* scanner) { return *(scanner->current); }

static char peek_next(Scanner* scanner)
{
  if (is_at_end(scanner))
    return '\0';
  return scanner->current[1];
}

static bool match(char expected, Scanner* scanner)
{
  if (is_at_end(scanner))
    return false;
  if (*scanner->current != expected)
    return false;
  scanner->current++;
  return true;
}

// pre-condition: the characters // have just been consumed
static void skip_single_line_comment(Scanner* scanner)
{
  // a single line comment goes until the end of the line
  while (peek(scanner) != '\n' && !is_at_end(scanner))
    advance(scanner);
}

// pre-condition: the characters /* have just been consumed
static Token skip_multiline_comment(Scanner* scanner)
{
  int open_comments = 1;
  int newlines = 0;

  while (!is_at_end(scanner)) {
    if (peek(scanner) == '/' && peek_next(scanner) == '*') {
      open_comments++;
      advance(scanner);
      advance(scanner);

    } else if (peek(scanner) == '*' && peek_next(scanner) == '/') {
      open_comments--;
      advance(scanner);
      advance(scanner);

    } else {
      if (peek(scanner) == '\n')
        newlines++;
      advance(scanner);
    }
    if (open_comments == 0)
      break;
  }
  // `open_comments` should be zero if the first opening delimiter was closed
  if (is_at_end(scanner) && open_comments != 0) {
    return error_token(scanner, "Unterminated multi-line comment.");
  }
  if (newlines > 0) {
    // to implement the "optional semicolon" feature we need to detect both
    // explicit and implicit newlines (i.e., multiline comments have newlines)
    return make_token(TOKEN_MULTILINE_COMMENT, scanner);
  }
  // actually a single-line comment
  return IGNORABLE_TOKEN;
}

static Token collapse_whitespace(Scanner* scanner)
{
  int newlines = 0;

  bool done = false;
  while (!done) {
    char c = peek(scanner);
    switch (c) {
    case ' ':
    case '\r':
    case '\t':
      advance(scanner);
      break;
    case '\n':
      scanner->current_line++;
      newlines++;
      advance(scanner);
      break;
    default:
      done = true;
    }
  }
  if (newlines > 0) {
    return NEWLINE_TOKEN;
  }
  return IGNORABLE_TOKEN;
}

// returns true if [this_start, this_end) === that; false otherwise
// pre-conditions:
// + `this_start` and `this_end` are pointers to sections of a null-terminated
//    c-string;
// + the substring to be compared to `that` begins at `this_start` and ends
//   at `this_end-1` (i.e., `this_end` is a non-inclusive "index")
// + `that` is a null-terminated c-string;
static bool
cstring__equals(const char* this_start, const char* this_end, const char* that)
{
  while (*that && this_start < this_end) {
    if (*this_start != *that)
      break;
    that++;
    this_start++;
  }
  // equality only happens if both strings were fully "consumed"
  return *that == '\0' && this_start == this_end;
}

static TokenType check_keyword(
    const char* keyword, TokenType type, const Scanner* scanner, int skip)
{
  // `skip` is the size of the prefix we've already verified for equality
  if (cstring__equals(
          scanner->start + skip, scanner->current, keyword + skip)) {
    return type;
  }
  return TOKEN_IDENTIFIER;
}

static TokenType identifier_type(Scanner* scanner)
{
  // This is a hard-coded trie of keywords for very quick identification.
  // See https://en.wikipedia.org/wiki/Trie for details.
  switch (scanner->start[0]) {
  case 'a':
    return check_keyword("and", TOKEN_AND, scanner, /*skip:*/ 1);
  case 'c':
    return check_keyword("class", TOKEN_CLASS, scanner, /*skip:*/ 1);
  case 'e':
    return check_keyword("else", TOKEN_ELSE, scanner, /*skip:*/ 1);
  case 'f':
    if (scanner->current - scanner->start > 1) {
      switch (scanner->start[1]) {
      case 'a':
        return check_keyword("false", TOKEN_FALSE, scanner, /*skip:*/ 2);
      case 'o':
        return check_keyword("for", TOKEN_FOR, scanner, /*skip:*/ 2);
      case 'u':
        return check_keyword("fun", TOKEN_FUN, scanner, /*skip:*/ 2);
      }
    }
    break;
  case 'i':
    return check_keyword("if", TOKEN_IF, scanner, /*skip:*/ 1);
  case 'n':
    return check_keyword("nil", TOKEN_NIL, scanner, /*skip:*/ 1);
  case 'o':
    return check_keyword("or", TOKEN_OR, scanner, /*skip:*/ 1);
  case 'p':
    return check_keyword("print", TOKEN_PRINT, scanner, /*skip:*/ 1);
  case 'r':
    return check_keyword("return", TOKEN_RETURN, scanner, /*skip:*/ 1);
  case 's':
    return check_keyword("super", TOKEN_SUPER, scanner, /*skip:*/ 1);
  case 't':
    if (scanner->current - scanner->start > 1) {
      switch (scanner->start[1]) {
      case 'h':
        return check_keyword("this", TOKEN_THIS, scanner, /*skip:*/ 2);
      case 'r':
        return check_keyword("true", TOKEN_TRUE, scanner, /*skip:*/ 2);
      }
    }
    break;
  case 'v':
    return check_keyword("var", TOKEN_VAR, scanner, /*skip:*/ 1);
  case 'w':
    return check_keyword("while", TOKEN_WHILE, scanner, /*skip:*/ 1);
  }
  return TOKEN_IDENTIFIER;
}

static Token identifier(Scanner* scanner)
{
  while (is_alpha(peek(scanner)) || is_digit(peek(scanner)))
    advance(scanner);
  return make_token(identifier_type(scanner), scanner);
}

static Token number(Scanner* scanner)
{
  // consume the integral part...
  while (is_digit(peek(scanner)))
    advance(scanner);

  // look for a fractional part...
  if (peek(scanner) == '.' && is_digit(peek_next(scanner))) {
    // consume the "."
    advance(scanner);
    // consume the fractional part...
    while (is_digit(peek(scanner)))
      advance(scanner);
  }
  return make_token(TOKEN_NUMBER, scanner);
}

static Token string(Scanner* scanner)
{
  while (peek(scanner) != '"' && !is_at_end(scanner)) {
    if (peek(scanner) == '\n')
      scanner->current_line++;
    advance(scanner);
  }
  if (is_at_end(scanner))
    return error_token(scanner, "Unterminated string.");

  // the closing quote
  advance(scanner);
  return make_token(TOKEN_STRING, scanner);
}

Token scanner__next_token(Scanner* scanner)
{
  Token token = collapse_whitespace(scanner);
  // needed for the "optional semicolon" feature
  if (token.type == TOKEN_NEWLINE) {
    return token;
  }
  assert(token.type == TOKEN_IGNORABLE);

  scanner->start = scanner->current;
  if (is_at_end(scanner))
    return make_token(TOKEN_EOF, scanner);

  char c = advance(scanner);
  if (is_alpha(c))
    return identifier(scanner);
  if (is_digit(c))
    return number(scanner);

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
  case '/': {
    if (match('/', scanner)) {
      skip_single_line_comment(scanner);
      return IGNORABLE_TOKEN;

    } else if (match('*', scanner)) {
      return skip_multiline_comment(scanner);
    }
    return make_token(TOKEN_SLASH, scanner);
  }
  case '*':
    return make_token(TOKEN_STAR, scanner);
  case '!':
    return make_token(
        match('=', scanner) ? TOKEN_BANG_EQUAL : TOKEN_BANG, scanner);
  case '=':
    return make_token(
        match('=', scanner) ? TOKEN_EQUAL_EQUAL : TOKEN_EQUAL, scanner);
  case '<':
    return make_token(
        match('=', scanner) ? TOKEN_LESS_EQUAL : TOKEN_LESS, scanner);
  case '>':
    return make_token(
        match('=', scanner) ? TOKEN_GREATER_EQUAL : TOKEN_GREATER, scanner);
  case '"':
    return string(scanner);
  }
  // TODO: include the unexpected token in the error message
  return error_token(scanner, "Unexpected character: %c", c);
}
