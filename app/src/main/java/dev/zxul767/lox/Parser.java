package dev.zxul767.lox;

import static dev.zxul767.lox.TokenType.*;

import java.util.ArrayList;
import java.util.Collections;
import java.util.List;
import java.util.function.Supplier;

class Parser {
  private static class ParseError extends RuntimeException {}

  private final List<Token> tokens;
  // indexes the token currently being looked at
  private int current = 0;

  Parser(List<Token> tokens) { this.tokens = tokens; }

  List<Stmt> parse() {
    List<Stmt> statements = new ArrayList<>();
    try {
      while (!isAtEnd()) {
        statements.add(statement());
      }
      return statements;
    } catch (ParseError error) {
      return Collections.emptyList();
    }
  }

  // expression -> equality ;
  private Expr expression() { return equality(); }

  private Stmt statement() {
    if (match(PRINT))
      return printStatement();
    return expressionStatement();
  }

  private Stmt printStatement() {
    Expr value = expression();
    consume(SEMICOLON, "Expected ';' after value.");
    return new Stmt.Print(value);
  }

  private Stmt expressionStatement() {
    Expr value = expression();
    consume(SEMICOLON, "Expected ';' after expression.");
    return new Stmt.Expression(value);
  }

  // equality -> equality ( "-" | "+" ) comparison
  //           | comparison ;
  private Expr equality() {
    return binary(() -> comparison(), BANG_EQUAL, EQUAL_EQUAL);
  }

  // comparison -> comparison ( ">" | ">=" | "<" | "<=" ) term
  //             | term ;
  private Expr comparison() {
    return binary(() -> term(), GREATER, GREATER_EQUAL, LESS, LESS_EQUAL);
  }

  // term -> term ( "-" | "+" ) factor
  //       | factor ;
  private Expr term() { return binary(() -> factor(), MINUS, PLUS); }

  // factor -> factor ( "/" | "*" ) unary
  //         | unary ;
  private Expr factor() { return binary(() -> unary(), SLASH, STAR); }

  // unary -> ( "!" | "-" ) unary
  //        | primary ;
  private Expr unary() {
    if (match(BANG, MINUS)) {
      Token operator = previous();
      Expr right = unary();
      return new Expr.Unary(operator, right);
    }
    return primary();
  }

  // primary -> NUMBER | STRING | "true" | "false" | "nil"
  //          | "(" expression ")" ;
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
      // TODO: we should present the original source code for easier
      // debugging; that will require keep source maps in the tokens, though.
      consume(RIGHT_PAREN, String.format("Expected ')' after expression: %s",
                                         new AstPrinter().print(expr)));
      return new Expr.Grouping(expr);
    }

    throw error(peek(), "Unexpected token in primary expression!");
  }

  // binary -> binary ONE_OF<tokenTypes> subunit
  //         | subunit ;
  private Expr binary(Supplier<Expr> subunitParser, TokenType... tokenTypes) {
    Expr expr = subunitParser.get();
    while (match(tokenTypes)) {
      Token operator = previous();
      Expr right = subunitParser.get();
      expr = new Expr.Binary(expr, operator, right);
    }
    return expr;
  }

  // returns true if it was able to consume the next token
  // consumes the next token if it matches one of `expectedTypes`
  private boolean match(TokenType... expectedTypes) {
    for (TokenType type : expectedTypes) {
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

  private boolean check(TokenType expectedType) {
    if (isAtEnd())
      return false;
    return peek().type == expectedType;
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

  // skips as many tokens as necessary to get out of the current
  // state of confusion (when we've failed to parse a rule but
  // we want to tolerate errors)
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
