package dev.zxul767.lox;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.*;
import static org.junit.jupiter.api.Assertions.*;

import dev.zxul767.lox.parsing.*;
import java.util.List;
import org.junit.jupiter.api.Test;

class InterpreterTest {
  // returns the value of the last statement (expression) in `program`.
  //
  // pre-condition: the last statement in `program` is an expression.
  // post-condition: all statements in `program` have been executed.
  private static Object interpret(String program) {
    // TODO: we should have a high-level API for the interpreter to avoid
    // duplicating logic with Lox.java
    Scanner scanner = new Scanner(program);
    List<Token> tokens = scanner.scanTokens();
    Parser parser = new Parser(tokens);
    List<Stmt> statements = parser.parse();

    // stop if there was a syntax error
    if (statements.size() == 0)
      return null;

    Interpreter interpreter = new Interpreter();
    // if we don't resolve the program, variable references will fail
    Resolver resolver = new Resolver(interpreter);
    resolver.resolve(statements);

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

  @Test
  void canUseConditionalsWithBareStatements() {
    Object result = interpret("var result; if (1 == 1) result = true; result;");
    assertEquals(true, (boolean)result);
  }

  @Test
  void canUseConditionalsWithBlock() {
    Object result =
        interpret("var result; if (1 == 1) { result = true; } result;");
    assertEquals(true, (boolean)result);
  }

  @Test
  void canUseConditionalsWithElse() {
    Object result = interpret(
        "var result; if (1 == 2) { result = true; } else { result = false; } result;");
    assertEquals(false, (boolean)result);
  }

  @Test
  void whileStatementShouldProvideIteration() {
    Object result = interpret(
        "var sum = 0; var i = 1; while (i <= 10) { sum = sum + i; i = i + 1; } sum;");
    assertEquals(55.0, (double)result);
  }

  @Test
  void forStatementShouldProvideIteration() {
    Object result = interpret(
        "var sum = 0; for (var i = 0; i <= 10; i = i + 1) { sum = sum + i; } sum;");
    assertEquals(55.0, (double)result);
  }

  @Test
  void canDefineSimpleProcedures() {
    Object result = interpret(
        "var total = 10; fun addTax() { total = total * 1.1; } addTax(); total;");
    assertEquals(11.0, (double)result);
  }

  @Test
  void canDefineRecursiveFunctions() {
    Object result = interpret(
        "fun fib(n) { if (n <= 1) return n; return fib(n-1) + fib(n-2); } fib(10);");
    assertEquals(55.0, (double)result);
  }

  @Test
  void canReturnClosures() {
    Object result = interpret(
        "fun counter() { var i = -1; fun next() { i = i + 1; return i; } return next; } var c = counter(); c(); c();");
    assertEquals(2.0, (double)result);
  }

  @Test
  void lexicalScopeShouldBeHonored() {
    // if F() didn't honor lexical scoping, `result` would end up being 2.0
    Object result = interpret(
        "var result; var a = 1; { fun F() { return a; } F(); var a = 2; result = F(); } result;");
    assertEquals(1.0, result);
  }

  @Test
  void canUseStandardLibraryFunctions() {
    Object result = interpret("sin(3.14159265359);");
    assertEquals(0.0, (double)result, /*delta:*/ 1e-5);
  }
}
