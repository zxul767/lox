package dev.zxul767.lox.runtime;

import dev.zxul767.lox.parsing.Token;

public class RuntimeError extends RuntimeException {
  public final Token token;

  public RuntimeError(Token token, String message) {
    super(message);
    this.token = token;
  }
}
