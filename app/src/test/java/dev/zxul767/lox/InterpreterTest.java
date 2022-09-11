package dev.zxul767.lox;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.*;
import static org.junit.jupiter.api.Assertions.*;

import java.util.List;
import org.junit.jupiter.api.Test;

class InterpreterTest {
  private static Object interpret(String singleExpression) {
    Scanner scanner = new Scanner(singleExpression);
    List<Token> tokens = scanner.scanTokens();
    Parser parser = new Parser(tokens);
    List<Stmt> statements = parser.parse();

    // stop if there was a syntax error
    if (statements.size() == 0)
      return null;

    Interpreter interpreter = new Interpreter();
    Stmt.Expression expr = (Stmt.Expression)statements.get(0);
    return interpreter.evaluate(expr.expression);
  }

  @Test
  void canInterpretArithmeticExpressions() {
    Object result = interpret("(10 + 20) / (2 * 5);");
    assertEquals(3.0, (double)result);
  }

  @Test
  void canInterpretBooleanExpressions() {
    Object result = interpret("(1 + 1) == 2 == true;");
    assertEquals(true, (boolean)result);
  }
}
