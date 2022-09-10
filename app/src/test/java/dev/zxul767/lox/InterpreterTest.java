package dev.zxul767.lox;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.*;
import static org.junit.jupiter.api.Assertions.*;

import java.util.List;
import org.junit.jupiter.api.Test;

class InterpreterTest {
  private static Object interpret(String source) {
    Scanner scanner = new Scanner(source);
    List<Token> tokens = scanner.scanTokens();
    Parser parser = new Parser(tokens);
    Expr expression = parser.parse();

    // stop if there was a syntax error
    if (expression == null)
      return null;

    Interpreter interpreter = new Interpreter();
    return interpreter.evaluate(expression);
  }

  @Test
  void canInterpretArithmeticExpressions() {
    Object result = interpret("(10 + 20) / (2 * 5)");
    assertEquals(3.0, (double)result);
  }

  @Test
  void canInterpretBooleanExpressions() {
    Object result = interpret("(1 + 1) == 2 == true");
    assertEquals(true, (boolean)result);
  }
}
