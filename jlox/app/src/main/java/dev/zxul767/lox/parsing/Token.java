package dev.zxul767.lox.parsing;

public class Token {
  // `Token` is just a data structure with no behavior so it's okay for
  // its fields to be public
  public final TokenType type;
  public final String lexeme;
  public final Object value;
  public final int line;

  public Token(TokenType type, String lexeme, Object value, int line) {
    this.type = type;
    this.lexeme = lexeme;
    this.value = value;
    this.line = line;
  }

  public Token(TokenType type, String lexeme) {
    this(type, lexeme, /*value:*/ null, /*line:*/ 1);
  }

  public String toString() { return type + " " + lexeme + " " + value; }
}
