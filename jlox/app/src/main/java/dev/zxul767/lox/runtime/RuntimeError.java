package dev.zxul767.lox.runtime;

import dev.zxul767.lox.parsing.Token;
import dev.zxul767.lox.parsing.TokenType;

public class RuntimeError extends RuntimeException {
  public final Token token;

  public RuntimeError(String lexeme, String message) {
    this(new Token(TokenType.IDENTIFIER, lexeme), message);
  }

  public RuntimeError(Token token, String message) {
    super(message);
    this.token = token;
  }
}
