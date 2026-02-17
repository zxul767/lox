package dev.zxul767.lox.runtime;

import dev.zxul767.lox.parsing.Expr;
import dev.zxul767.lox.parsing.Stmt;
import java.util.List;
import java.util.stream.Collectors;

class LoxFunction implements LoxCallable {
  private final Stmt.Function declaration;
  private final Environment parentEnvironment;
  private final boolean isInitializer;
  private final CallableSignature signature;
  private final String docstring;

  LoxFunction(Stmt.Function declaration, Environment parentEnvironment, boolean isInitializer) {
    this.isInitializer = isInitializer;
    this.declaration = declaration;
    this.parentEnvironment = parentEnvironment;

    List<CallableParameter> parameters =
        this.declaration.params.stream()
            .map(token -> new CallableParameter(token.lexeme))
            .collect(Collectors.toList());
    this.signature = new CallableSignature(this.declaration.name.lexeme, parameters);
    this.docstring = findDocstring(this.declaration);
  }

  private static String findDocstring(Stmt.Function declaration) {
    if (declaration.body.isEmpty()) {
      return null;
    }
    // a docstring must be an expression...
    Stmt first = declaration.body.get(0);
    if (!(first instanceof Stmt.Expression)) {
      return null;
    }
    // ... of type "literal" ...
    Expr expression = ((Stmt.Expression) first).expression;
    if (!(expression instanceof Expr.Literal)) {
      return null;
    }
    // ... which happens to be a double-quoted string
    Object value = ((Expr.Literal) expression).value;
    return value instanceof String ? (String) value : null;
  }

  @Override
  public LoxCallable bind(LoxInstance instance) {
    Environment environment = new Environment(parentEnvironment);
    environment.define("this", instance);

    return new LoxFunction(this.declaration, environment, isInitializer);
  }

  @Override
  public Object call(Interpreter interpreter, List<Object> arguments) {
    // place all formal parameters and their values in a new environment
    Environment environment = new Environment(this.parentEnvironment);
    for (int i = 0; i < declaration.params.size(); i++) {
      environment.define(declaration.params.get(i).lexeme, arguments.get(i));
    }
    // evaluate the function's body in that newly created environment
    try {
      interpreter.executeBlock(declaration.body, environment);
    } catch (Return returnValue) {
      // the return value is modeled as an exception because evaluation happens
      // by interpreting (possibly recursively nested) ASTs, so it's the easiest
      // way to unwind the call stack.
      return returnValue.value;
    }
    // constructors always return the newly created instance
    if (isInitializer) {
      return parentEnvironment.getAt(0, "this");
    }
    return null;
  }

  @Override
  public CallableSignature signature() {
    return this.signature;
  }

  @Override
  public String docstring() {
    return this.docstring;
  }

  @Override
  public String toString() {
    return String.format("<user-defined function>");
  }
}
