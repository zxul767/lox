package dev.zxul767.lox.parsing;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.*;
import static org.junit.jupiter.api.Assertions.*;

import java.util.List;
import org.junit.jupiter.api.Test;

class RPNPrinterTest {
  static Expr parse(String source) {
    Scanner scanner = new Scanner(source);
    List<Token> tokens = scanner.scanTokens();
    Parser parser = new Parser(tokens);
    List<Stmt> statements = parser.parse();

    assertEquals(1, statements.size());
    assertTrue(statements.get(0) instanceof Stmt.Expression);

    Stmt.Expression expr = (Stmt.Expression)statements.get(0);
    return expr.expression;
  }

  @Test
  void canPrintInRPN() {
    Expr expression = parse("(1 + 2) * (4 - 3);");
    RPNPrinter printer = new RPNPrinter();
    String rpn = printer.print(expression);

    assertEquals("1.0 2.0 +  4.0 3.0 -  *", rpn);
  }
}
