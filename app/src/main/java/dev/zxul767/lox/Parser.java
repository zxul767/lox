package dev.zxul767.lox;

import static dev.zxul767.lox.TokenType.*;

import java.util.List;
import java.util.function.Supplier;

class Parser {
  private static class ParseError extends RuntimeException {}

  private final List<Token> tokens;
  private int current = 0;

  Parser(List<Token> tokens) { this.tokens = tokens; }

  Expr parse() {
    try {
      return expression();
    } catch (ParseError error) {
      return null;
    }
  }

  private Expr expression() { return equality(); }

  private Expr equality() {
    return binary(() -> comparison(), BANG_EQUAL, EQUAL_EQUAL);
  }

  private Expr comparison() {
    return binary(() -> term(), GREATER, GREATER_EQUAL, LESS, LESS_EQUAL);
  }

  private Expr term() { return binary(() -> factor(), MINUS, PLUS); }

  private Expr factor() { return binary(() -> unary(), SLASH, STAR); }

  private Expr unary() {
    if (match(BANG, MINUS)) {
      Token operator = previous();
      Expr right = unary();
      return new Expr.Unary(operator, right);
    }
    return primary();
  }

  private Expr binary(Supplier<Expr> subunitParser, TokenType... types) {
    Expr expr = subunitParser.get();
    while (match(types)) {
      Token operator = previous();
      Expr right = subunitParser.get();
      expr = new Expr.Binary(expr, operator, right);
    }
    return expr;
  }

  private Expr primary() {
    if (match(FALSE))
      return new Expr.Literal(false);
    if (match(TRUE))
      return new Expr.Literal(true);
    if (match(NIL))
      return new Expr.Literal(null);

    if (match(NUMBER, STRING)) {
      return new Expr.Literal(previous().value);
    }
    if (match(LEFT_PAREN)) {
      Expr expr = expression();
      // TODO: we should present the original source code for easier debugging;
      // that will require keep source maps in the tokens, though.
      consume(RIGHT_PAREN, String.format("Expected ')' after expression: %s",
                                         new AstPrinter().print(expr)));
      return new Expr.Grouping(expr);
    }

    throw error(peek(), "Unexpected token in primary expression!");
  }

  private boolean match(TokenType... types) {
    for (TokenType type : types) {
      if (check(type)) {
        advance();
        return true;
      }
    }
    return false;
  }

  private Token consume(TokenType type, String message) {
    if (check(type))
      return advance();
    throw error(peek(), message);
  }

  private boolean check(TokenType type) {
    if (isAtEnd())
      return false;
    return peek().type == type;
  }

  private Token advance() {
    if (!isAtEnd())
      current++;
    return previous();
  }

  private boolean isAtEnd() { return peek().type == EOF; }

  private Token peek() { return tokens.get(current); }

  private Token previous() { return tokens.get(current - 1); }

  private ParseError error(Token token, String message) {
    Lox.error(token, message);
    return new ParseError();
  }

  private void synchronize() {
    advance();

    while (!isAtEnd()) {
      if (previous().type == SEMICOLON)
        return;
      switch (peek().type) {
      case CLASS:
      case FUN:
      case VAR:
      case FOR:
      case IF:
      case WHILE:
      case PRINT:
      case RETURN:
        return;
      }
      advance();
    }
  }
}
