package dev.zxul767.lox.parsing;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.*;
import static org.junit.jupiter.api.Assertions.*;

import org.junit.jupiter.api.Test;

// TODO: merge AstPrinterTest & ParserTest
class AstPrinterTest {
  @Test
  void canPrintAST() {
    Expr expression = new Expr.Binary(
        new Expr.Unary(new Token(TokenType.MINUS, "-"), new Expr.Literal(123)),
        new Token(TokenType.STAR, "*"),
        new Expr.Grouping(new Expr.Literal(45.67)));

    AstPrinter printer = new AstPrinter();
    String ast = printer.print(expression);

    assertEquals("(* (- 123) (group 45.67))", ast);
  }
}
