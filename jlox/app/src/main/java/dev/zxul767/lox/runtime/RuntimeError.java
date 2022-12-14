package dev.zxul767.lox.runtime;

import dev.zxul767.lox.parsing.Token;
import dev.zxul767.lox.parsing.TokenType;

public class RuntimeError extends RuntimeException {
  public final Token token;

  // we use this constuctor for cases where we don't have the actual token
  // available, but we do know enough about the context to provide the
  // corresponding lexeme (e.g., during argument checking in function calls)
  public RuntimeError(String lexeme, String message) {
    this(new Token(TokenType.IDENTIFIER, lexeme), message);
  }

  public RuntimeError(Token token, String message) {
    super(message);
    this.token = token;
  }
}
