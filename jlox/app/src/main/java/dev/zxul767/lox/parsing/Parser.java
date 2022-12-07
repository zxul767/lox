package dev.zxul767.lox.parsing;

import static dev.zxul767.lox.parsing.TokenType.*;

import dev.zxul767.lox.Errors;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.function.Supplier;

public class Parser {
  private static class ParseError extends RuntimeException {}

  private final List<Token> tokens;
  // indexes the token currently being looked at
  private int current = 0;

  public Parser(List<Token> tokens) { this.tokens = tokens; }

  public List<Stmt> parse() {
    List<Stmt> statements = new ArrayList<>();
    while (!isAtEnd()) {
      Stmt statement = declaration();
      if (statement != null) {
        statements.add(statement);
      }
    }
    return statements;
  }

  // expression -> assignment
  private Expr expression() { return assignment(); }

  // declaration -> varDeclaration
  //              | functionDeclaration
  //              | statement
  private Stmt declaration() {
    try {
      if (match(CLASS))
        return classDeclaration();
      if (match(FUN))
        return functionDeclaration("function");
      if (match(VAR))
        return varDeclaration();
      return statement();
    } catch (ParseError error) {
      synchronize();
      return null;
    }
  }

  // classDeclaration -> "class" IDENTIFIER ( "<" IDENTIFIER )?
  //                     "{" function* "}"
  private Stmt classDeclaration() {
    Token name = consume(IDENTIFIER, "Expected class name.");

    Expr.Variable superclass = null;
    if (match(LESS)) {
      consume(IDENTIFIER, "Expected superclass name.");
      superclass = new Expr.Variable(previous());
    }

    consume(LEFT_BRACE, "Expected '{' before class body");
    List<Stmt.Function> methods = new ArrayList<>();
    while (!check(RIGHT_BRACE) && !isAtEnd()) {
      methods.add(functionDeclaration("method"));
    }
    consume(RIGHT_BRACE, "Expected '}' after class body");

    return new Stmt.Class(name, superclass, methods);
  }

  // statement -> expressionStatement
  //            | ifStatement
  //            | block
  private Stmt statement() {
    if (match(FOR))
      return forStatement();

    if (match(IF))
      return ifStatement();

    if (match(RETURN))
      return returnStatement();

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

  // returnStatement -> "return" expression? ";"
  private Stmt returnStatement() {
    Token keyword = previous();
    Expr value = null;
    if (!check(SEMICOLON)) {
      value = expression();
    }
    consume(SEMICOLON, "Expected ';' after return value.");

    return new Stmt.Return(keyword, value);
  }

  // varDeclaration -> "var" IDENTIFIER ("=" expression)? ";"
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

  // functionDeclaration -> "fun" IDENTIFIER "(" parameters? ")" block
  private Stmt.Function functionDeclaration(String kind) {
    Token name = consume(IDENTIFIER, String.format("Expected %s name.", kind));
    consume(LEFT_PAREN, String.format("Expected '(' after %s name.", kind));
    List<Token> parameters = new ArrayList<>();
    if (!check(RIGHT_PAREN)) {
      do {
        if (parameters.size() >= 255) {
          error(peek(), "Can't have more than 255 parameters.");
        }
        parameters.add(consume(IDENTIFIER, "Expected parameter name."));
      } while (match(COMMA));
    }
    consume(RIGHT_PAREN, "Expected ')' after parameters");

    consume(LEFT_BRACE, String.format("Expected '{' before %s body.", kind));
    List<Stmt> body = block();

    return new Stmt.Function(name, parameters, body);
  }

  // In Lox, assignments are expressions, so one can do things like:
  //    if (a = true) print "a is true";
  // Is this wise? Probably not, as evidenced by its prohibition in
  // modern languages (e.g., Python and Go)
  //
  // assignment -> ( call "." )? IDENTIFIER "=" assignment
  //             | logic_or
  private Expr assignment() {
    Expr expr = or();

    if (match(EQUAL)) {
      Token equals = previous();
      Expr value = assignment();

      if (expr instanceof Expr.Variable) {
        Token name = ((Expr.Variable)expr).name;
        return new Expr.Assign(name, value);

      } else if (expr instanceof Expr.Get) {
        Expr.Get get = (Expr.Get)expr;
        return new Expr.Set(get.object, get.name, value);
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
  //        | call
  private Expr unary() {
    if (match(BANG, MINUS)) {
      Token operator = previous();
      Expr right = unary();
      return new Expr.Unary(operator, right);
    }
    return callOrIndex();
  }

  // TODO: since we're handling "get" expressions here too, the name "call"
  // no longer seems appropriate and may actually be confusing. Suggestions?
  //
  // call -> primary ( "(" arguments? ")" | "." IDENTIFIER )*
  // arguments -> expression ( "," expression )*
  private Expr callOrIndex() {
    Expr callee = primary();

    // this loop is necessary because we might have an expression like this:
    //    callee(a, b)(c, d, e)(f, g)
    //
    while (true) {
      if (match(LEFT_BRACKET)) {
        callee = index(callee);

      } else if (match(LEFT_PAREN)) {
        callee = finishCall(callee);

      } else if (match(DOT)) {
        Token name = consume(IDENTIFIER, "Expected property name after '.'.");
        callee = new Expr.Get(callee, name);

      } else {
        break;
      }
    }
    return callee;
  }

  private Expr index(Expr indexable) {
    // we can desugar the expression `indexable[index]` down to
    // `indexable.__getitem__(index)`
    List<Expr> args = new ArrayList<>();
    Expr index = expression();
    args.add(index);
    Token right_bracket =
        consume(RIGHT_BRACKET, "Expected closing ']' on index access.");

    // we have an assignment
    // we need to desugar to `indexable[index] = expr` down to
    // `indexable.__setitem__(index, value)`
    if (match(EQUAL)) {
      indexable = new Expr.Get(indexable, new Token(IDENTIFIER, "__setitem__"));
      Expr value = expression();
      args.add(value);

    } else {
      indexable = new Expr.Get(indexable, new Token(IDENTIFIER, "__getitem__"));
    }

    // we need `right_bracket` to show location information on errors
    return new Expr.Call(indexable, right_bracket, args);
  }

  // Gather all arguments and create a single call expression
  // to be used by `call`
  //
  // pre-condition: a LEFT_PAREN has just been consumed
  // post-condition: a RIGHT_PAREN is the last consumed token
  private Expr finishCall(Expr callee) {
    List<Expr> arguments = new ArrayList<>();
    if (!check(RIGHT_PAREN)) {
      do {
        // TODO: hoist 255 into a symbolic constant (maybe in a Constraints
        // static class?)
        if (arguments.size() >= 255) {
          error(peek(), "Can't have more than 255 arguments.");
        }
        arguments.add(expression());
      } while (match(COMMA));
    }
    Token paren = consume(RIGHT_PAREN, "Expected ')' after arguments.");
    return new Expr.Call(callee, paren, arguments);
  }

  // primary -> NUMBER | STRING | "true" | "false" | "nil"
  //          | "(" expression ")"
  //          | "super" "." IDENTIFIER
  private Expr primary() {
    if (match(FALSE)) {
      return new Expr.Literal(false);
    }
    if (match(TRUE)) {
      return new Expr.Literal(true);
    }
    if (match(NIL)) {
      return new Expr.Literal(null);
    }
    if (match(NUMBER, STRING)) {
      return new Expr.Literal(previous().value);
    }
    if (match(SUPER)) {
      Token keyword = previous();
      consume(DOT, "Expected '.' after 'super'.");
      Token method = consume(IDENTIFIER, "Expected superclass method name.");
      return new Expr.Super(keyword, method);
    }
    if (match(THIS)) {
      return new Expr.This(previous());
    }
    if (match(IDENTIFIER)) {
      return new Expr.Variable(previous());
    }
    if (match(LEFT_PAREN)) {
      Expr expr = expression();
      // TODO: we should present the original source code for easier
      // debugging; that will require keep source maps in the tokens, though.
      consume(
          RIGHT_PAREN,
          String.format(
              "Expected ')' after expression: %s", new AstPrinter().print(expr)
          )
      );
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
    Errors.error(token, message);
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
      case RETURN:
        return;
      }
      advance();
    }
  }
}
