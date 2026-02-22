package dev.zxul767.lox.parsing;

public class Token {
  public final TokenType type;
  public final String lexeme;
  public final Object value;
  public final int line;

  public Token(TokenType type, String lexeme) {
    this(type, lexeme, /*value:*/ null, /*line:*/ 1);
  }

  public Token(TokenType type, String lexeme, Object value, int line) {
    this.type = type;
    this.line = line;
    this.lexeme = lexeme;
    this.value = value; // some tokens (like numbers or string literals) have an associated value
  }

  public String toString() {
    return type + " " + lexeme + " " + value;
  }
}
