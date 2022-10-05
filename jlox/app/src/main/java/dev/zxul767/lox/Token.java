package dev.zxul767.lox;

class Token {
  final TokenType type;
  final String lexeme;
  final Object value;
  final int line;

  Token(TokenType type, String lexeme, Object value, int line) {
    this.type = type;
    this.lexeme = lexeme;
    this.value = value;
    this.line = line;
  }

  Token(TokenType type, String lexeme) {
    this(type, lexeme, /*value:*/ null, /*line:*/ 1);
  }

  public String toString() { return type + " " + lexeme + " " + value; }
}
