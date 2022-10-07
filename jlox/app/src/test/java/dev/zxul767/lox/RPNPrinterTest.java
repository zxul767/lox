package dev.zxul767.lox.parsing;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.*;
import static org.junit.jupiter.api.Assertions.*;

import org.junit.jupiter.api.Test;

class RPNPrinterTest {
  @Test
  void canPrintInRPN() {
    Expr expression = new Expr.Binary(
        new Expr.Binary(new Expr.Literal(1), new Token(TokenType.PLUS, "+"),
                        new Expr.Literal(2)),
        new Token(TokenType.STAR, "*"),
        new Expr.Binary(new Expr.Literal(4), new Token(TokenType.MINUS, "-"),
                        new Expr.Literal(3)));

    RPNPrinter printer = new RPNPrinter();
    String rpn = printer.print(expression);

    assertEquals("1 2 + 4 3 - *", rpn);
  }
}
