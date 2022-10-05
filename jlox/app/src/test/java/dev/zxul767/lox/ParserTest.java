package dev.zxul767.lox;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.*;
import static org.junit.jupiter.api.Assertions.*;

import java.util.List;
import org.junit.jupiter.api.Disabled;
import org.junit.jupiter.api.Test;

class ParserTest {
  static Expr parse(String source) {
    Scanner scanner = new Scanner(source);
    List<Token> tokens = scanner.scanTokens();
    Parser parser = new Parser(tokens);
    List<Stmt> statements = parser.parse();

    assert (statements.size() == 1);
    assert (statements.get(0) instanceof Stmt.Expression);

    Stmt.Expression expr = (Stmt.Expression)statements.get(0);
    return expr.expression;
  }

  @Test
  void canParseParenthesizedExpression() {
    Expr expression = parse("((1 + 1) <= 3) == false;");
    String ast = (new AstPrinter()).print(expression);
    assertEquals("(== (group (<= (group (+ 1.0 1.0)) 3.0)) false)", ast);
  }

  @Test
  void canParseWithCorrectOperatorPrecedence() {
    Expr expression = parse("1 + 2.5 * 3 / 3 == 1.0;");
    String ast = (new AstPrinter()).print(expression);
    assertEquals("(== (+ 1.0 (/ (* 2.5 3.0) 3.0)) 1.0)", ast);
  }
}
