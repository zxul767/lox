package dev.zxul767.lox;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.*;
import static org.junit.jupiter.api.Assertions.*;

import java.util.List;
import org.junit.jupiter.api.Test;

class InterpreterTest {
  // returns the value of the last statement (expression) in `program`.
  //
  // pre-condition: the last statement in `program` is an expression.
  // post-condition: all statements in `program` have been executed.
  private static Object interpret(String program) {
    Scanner scanner = new Scanner(program);
    List<Token> tokens = scanner.scanTokens();
    Parser parser = new Parser(tokens);
    List<Stmt> statements = parser.parse();

    // stop if there was a syntax error
    if (statements.size() == 0)
      return null;

    Interpreter interpreter = new Interpreter();
    interpreter.interpret(statements);
    int lastIndex = statements.size() - 1;
    Stmt.Expression expr = (Stmt.Expression)statements.get(lastIndex);
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

  @Test
  void canUseGlobalVariables() {
    Object result = interpret("var a = 1; var b = 2; (a + b) == 3 == true;");
    assertEquals(true, (boolean)result);
  }

  @Test
  void localVariablesShouldShadowGlobalOnes() {
    Object result =
        interpret("var a = 1; var result; { var a = 2; result = a; } result;");
    assertEquals(2.0, (double)result);
  }

  @Test
  void localVariablesShouldOnesInOuterScope() {
    Object result = interpret(
        "var a = 1; var result; { var a = 2; { var a = 3; result = a; } } result;");
    assertEquals(3.0, (double)result);
  }
}
