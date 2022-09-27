package dev.zxul767.lox;

import static dev.zxul767.lox.TokenType.*;

import java.util.ArrayList;
import java.util.Arrays;
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
        statements.add(declaration());
      }
    } catch (ParseError error) {
      // TODO: call error on future error reporter
    }
    return statements;
  }

  // expression -> assignment
  private Expr expression() { return assignment(); }

  // declaration -> "var" varDeclaration
  //              | statement
  private Stmt declaration() {
    try {
      if (match(VAR))
        return varDeclaration();
      return statement();
    } catch (ParseError error) {
      synchronize();
      return null;
    }
  }

  // statement -> expressionStatement
  //            | ifStatement
  //            | printStatement
  //            | block
  private Stmt statement() {
    if (match(FOR))
      return forStatement();

    if (match(IF))
      return ifStatement();

    if (match(PRINT))
      return printStatement();

    if (match(WHILE))
      return whileStatement();

    if (match(LEFT_BRACE))
      return new Stmt.Block(block());

    return expressionStatement();
  }

  // forStatement ->  "for" "(" (varDeclaration | expressionStatement | ";")
  //                            expression? ";"
  //                            expression?
  //                         ")" statement
  private Stmt forStatement() {
    consume(LEFT_PAREN, "Expected '(' after 'for'.");
    Stmt initializer;
    if (match(SEMICOLON)) {
      initializer = null;
    } else if (match(VAR)) {
      initializer = varDeclaration();
    } else {
      initializer = expressionStatement();
    }
    Expr condition = null;
    if (!check(SEMICOLON)) {
      condition = expression();
    }
    consume(SEMICOLON, "Expected ';' after loop condition.");

    Expr increment = null;
    if (!check(RIGHT_PAREN)) {
      increment = expression();
    }
    consume(RIGHT_PAREN, "Expected ')' after for clauses.");

    //
    // NOTE: for statements are "desugared" into while statements
    //
    Stmt body = statement();
    if (increment != null) {
      body =
          new Stmt.Block(Arrays.asList(body, new Stmt.Expression(increment)));
    }
    if (condition == null) {
      condition = new Expr.Literal(true);
    }
    body = new Stmt.While(condition, body);

    if (initializer != null) {
      body = new Stmt.Block(Arrays.asList(initializer, body));
    }

    return body;
  }

  // ifStatement -> "if" "(" expression ")" statement
  //                ("else" statement)?
  private Stmt ifStatement() {
    consume(LEFT_PAREN, "Expected '(' after 'if'.");
    Expr condition = expression();
    consume(RIGHT_PAREN, "Expected ')' after 'if' condition.");

    Stmt thenBranch = statement();
    Stmt elseBranch = null;
    if (match(ELSE)) {
      elseBranch = statement();
    }
    return new Stmt.If(condition, thenBranch, elseBranch);
  }

  // printStatement -> "print" expression ";"
  private Stmt printStatement() {
    Expr value = expression();
    consume(SEMICOLON, "Expected ';' after value.");
    return new Stmt.Print(value);
  }

  // varDeclaration -> IDENTIFIER ("=" expression)? ";"
  private Stmt varDeclaration() {
    Token name = consume(IDENTIFIER, "expect variable name.");
    Expr initializer = null;
    if (match(EQUAL)) {
      initializer = expression();
    }
    consume(SEMICOLON, "Expected ';' after variable declaration.");
    return new Stmt.Var(name, initializer);
  }

  // whileStatement -> "while" "(" expression ")" statement
  private Stmt whileStatement() {
    consume(LEFT_PAREN, "Expected '(' after 'while'.");
    Expr condition = expression();
    consume(RIGHT_PAREN, "Expected ')' after 'while'.");
    Stmt body = statement();

    return new Stmt.While(condition, body);
  }

  // expressionStatement -> expression ";"
  private Stmt expressionStatement() {
    Expr value = expression();
    consume(SEMICOLON, "Expected ';' after expression.");
    return new Stmt.Expression(value);
  }

  // block -> "{" declaration* "}"
  private List<Stmt> block() {
    List<Stmt> statements = new ArrayList<>();
    while (!check(RIGHT_BRACE) && !isAtEnd()) {
      statements.add(declaration());
    }
    consume(RIGHT_BRACE, "Expected '}' after block.");
    return statements;
  }

  // In Lox, assignments are expressions, so one can do things like:
  //    if (a = true) print "a is true";
  // Is this wise? Probably not, as evidenced by its prohibition in
  // modern languages (e.g., Python and Go)
  //
  // assignment -> IDENTIFIER "=" assignment
  //             | logic_or
  private Expr assignment() {
    Expr expr = or();

    if (match(EQUAL)) {
      Token equals = previous();
      Expr value = assignment();

      if (expr instanceof Expr.Variable) {
        Token name = ((Expr.Variable)expr).name;
        return new Expr.Assign(name, value);
      }
      error(equals, "Invalid assignment target.");
    }
    return expr;
  }

  // logic_or -> logic_nad ( "or" logic_and )*
  private Expr or() {
    Expr expr = and();

    while (match(OR)) {
      Token operator = previous();
      Expr right = and();
      expr = new Expr.Logical(expr, operator, right);
    }
    return expr;
  }

  // logic_and -> equality ( "and" equality )*
  private Expr and() {
    Expr expr = equality();

    while (match(AND)) {
      Token operator = previous();
      Expr right = equality();
      expr = new Expr.Logical(expr, operator, right);
    }
    return expr;
  }

  // equality -> equality ( "-" | "+" ) comparison
  //           | comparison
  private Expr equality() {
    return binary(() -> comparison(), BANG_EQUAL, EQUAL_EQUAL);
  }

  // comparison -> comparison ( ">" | ">=" | "<" | "<=" ) term
  //             | term
  private Expr comparison() {
    return binary(() -> term(), GREATER, GREATER_EQUAL, LESS, LESS_EQUAL);
  }

  // term -> term ( "-" | "+" ) factor
  //       | factor
  private Expr term() { return binary(() -> factor(), MINUS, PLUS); }

  // factor -> factor ( "/" | "*" ) unary
  //         | unary
  private Expr factor() { return binary(() -> unary(), SLASH, STAR); }

  // unary -> ( "!" | "-" ) unary
  //        | primary
  private Expr unary() {
    if (match(BANG, MINUS)) {
      Token operator = previous();
      Expr right = unary();
      return new Expr.Unary(operator, right);
    }
    return primary();
  }

  // primary -> NUMBER | STRING | "true" | "false" | "nil"
  //          | "(" expression ")"
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
    if (match(IDENTIFIER)) {
      return new Expr.Variable(previous());
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
  //         | subunit
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
  // state of confusion (i.e., we've failed to parse a rule but
  // we want to tolerate errors, so we try to move past to the
  // begining of the next statement.)
  //
  // pre-condition: the current token "breaks" the current grammar
  // rule being parsed.
  //
  // post-condition: all tokens in the previous statement have been
  // discarded, and the current token is now at the beginning of the
  // next statement or at EOF
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
