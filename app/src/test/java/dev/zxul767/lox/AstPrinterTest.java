package dev.zxul767.lox;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.*;
import static org.junit.jupiter.api.Assertions.*;

import org.junit.jupiter.api.Test;

class ScannerTest {
  @Test
  void canPrintAST() {
    Expr expression =
        new Expr.Binary(new Expr.Unary(new Token(TokenType.MINUS, "-", null, 1),
                                       new Expr.Literal(123)),
                        new Token(TokenType.STAR, "*", null, 1),
                        new Expr.Grouping(new Expr.Literal(45.67)));

    AstPrinter printer = new AstPrinter();
    String ast = printer.print(expression);

    assertEquals("(* (- 123) (group 45.67))", ast);
  }
}
