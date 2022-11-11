#ifndef SCANNER_H_
#define SCANNER_H_

// For details on this peculiar technique to be convert enum symbols to strings
// without redundancy, see:
//
// https://stackoverflow.com/questions/9907160/how-to-convert-enum-names-to-string-in-c
#define FOREACH_TOKEN(TOKEN)                                                   \
  TOKEN(TOKEN_BOF)                                                             \
  TOKEN(TOKEN_LEFT_PAREN)                                                      \
  TOKEN(TOKEN_RIGHT_PAREN)                                                     \
  TOKEN(TOKEN_LEFT_BRACE)                                                      \
  TOKEN(TOKEN_RIGHT_BRACE)                                                     \
  TOKEN(TOKEN_COMMA)                                                           \
  TOKEN(TOKEN_DOT)                                                             \
  TOKEN(TOKEN_MINUS)                                                           \
  TOKEN(TOKEN_PLUS)                                                            \
  TOKEN(TOKEN_SEMICOLON)                                                       \
  TOKEN(TOKEN_SLASH)                                                           \
  TOKEN(TOKEN_STAR)                                                            \
  TOKEN(TOKEN_BANG)                                                            \
  TOKEN(TOKEN_BANG_EQUAL)                                                      \
  TOKEN(TOKEN_EQUAL)                                                           \
  TOKEN(TOKEN_EQUAL_EQUAL)                                                     \
  TOKEN(TOKEN_GREATER)                                                         \
  TOKEN(TOKEN_GREATER_EQUAL)                                                   \
  TOKEN(TOKEN_LESS)                                                            \
  TOKEN(TOKEN_LESS_EQUAL)                                                      \
  TOKEN(TOKEN_IDENTIFIER)                                                      \
  TOKEN(TOKEN_STRING)                                                          \
  TOKEN(TOKEN_NUMBER)                                                          \
  TOKEN(TOKEN_AND)                                                             \
  TOKEN(TOKEN_CLASS)                                                           \
  TOKEN(TOKEN_ELSE)                                                            \
  TOKEN(TOKEN_FALSE)                                                           \
  TOKEN(TOKEN_FOR)                                                             \
  TOKEN(TOKEN_FUN)                                                             \
  TOKEN(TOKEN_IF)                                                              \
  TOKEN(TOKEN_NIL)                                                             \
  TOKEN(TOKEN_OR)                                                              \
  TOKEN(TOKEN_RETURN)                                                          \
  TOKEN(TOKEN_SUPER)                                                           \
  TOKEN(TOKEN_THIS)                                                            \
  TOKEN(TOKEN_TRUE)                                                            \
  TOKEN(TOKEN_VAR)                                                             \
  TOKEN(TOKEN_WHILE)                                                           \
  TOKEN(TOKEN_ERROR)                                                           \
  TOKEN(TOKEN_EOF)                                                             \
  TOKEN(TOKEN_MULTILINE_COMMENT)                                               \
  TOKEN(TOKEN_NEWLINE)                                                         \
  TOKEN(TOKEN_IGNORABLE)

#define GENERATE_ENUM(ENUM) ENUM,
#define GENERATE_STRING(STRING) #STRING,

typedef enum { FOREACH_TOKEN(GENERATE_ENUM) } TokenType;

// We make it `extern` so it can be used from compiler.c;
// see initialization in scanner.c
extern const char* TOKEN_TO_STRING[];
extern const char* KEYWORDS[];

typedef struct {
  TokenType type;
  const char* start;
  int length;
  int line;
} Token;

extern const Token BOF_TOKEN;
extern const Token NEWLINE_TOKEN;
extern const Token IGNORABLE_TOKEN;

typedef struct {
  const char* start;
  const char* current;
  int current_line;
} Scanner;

void scanner__init(Scanner* scanner, const char* source);
Token scanner__next_token(Scanner* scanner);

#endif // SCANNER_H_
