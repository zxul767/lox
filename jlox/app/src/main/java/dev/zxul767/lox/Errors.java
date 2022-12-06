package dev.zxul767.lox;

import dev.zxul767.lox.parsing.Token;
import dev.zxul767.lox.parsing.TokenType;
import dev.zxul767.lox.runtime.RuntimeError;

// TODO: turn into an injectable error reporting interface to be used
// in various modules of the interpreter
public class Errors {
  static boolean hadError = false;
  static boolean hadRuntimeError = false;

  public static void error(int line, String message) {
    report(line, /* where: */ "", message);
  }

  public static void error(Token token, String message) {
    if (token.type == TokenType.EOF) {
      report(token.line, " at end", message);
    } else {
      report(token.line, String.format(" at '%s'", token.lexeme), message);
    }
  }

  public static void reset() {
    hadRuntimeError = false;
    hadError = false;
  }

  public static void runtimeError(RuntimeError error) {
    System.err.println(String.format(
        "Runtime Error: %s\n[line %d, token: '%s']", error.getMessage(),
        error.token.line, error.token.lexeme
    ));
    System.err.flush();
    hadRuntimeError = true;
    hadError = true;
  }

  private static void report(int line, String where, String message) {
    System.err.println(
        "Parsing Error: [line " + line + "] Error" + where + ": " + message
    );
    System.err.flush();
    hadError = true;
  }
}
