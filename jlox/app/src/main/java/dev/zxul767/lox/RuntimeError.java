package dev.zxul767.lox;

import dev.zxul767.lox.parsing.Token;

class RuntimeError extends RuntimeException {
  final Token token;

  RuntimeError(Token token, String message) {
    super(message);
    this.token = token;
  }
}
