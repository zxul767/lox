package dev.zxul767.lox.runtime;

import dev.zxul767.lox.parsing.Stmt;
import java.util.List;
import java.util.stream.Collectors;

class LoxFunction implements LoxCallable {
  private final Stmt.Function declaration;
  private final Environment closure;
  private final boolean isInitializer;
  private final CallableSignature signature;

  LoxFunction(
      Stmt.Function declaration, Environment closure, boolean isInitializer
  ) {
    this.isInitializer = isInitializer;
    this.declaration = declaration;
    this.closure = closure;

    List<Parameter> parameters = declaration.params.stream()
                                     .map(token -> new Parameter(token.lexeme))
                                     .collect(Collectors.toList());
    this.signature =
        new CallableSignature(this.declaration.name.lexeme, parameters);
  }

  @Override
  public LoxCallable bind(LoxInstance instance) {
    Environment environment = new Environment(closure);
    environment.define("this", instance);

    return new LoxFunction(declaration, environment, isInitializer);
  }

  @Override
  public Object call(Interpreter interpreter, List<Object> arguments) {
    Environment environment = new Environment(closure);
    for (int i = 0; i < declaration.params.size(); i++) {
      environment.define(declaration.params.get(i).lexeme, arguments.get(i));
    }
    try {
      interpreter.executeBlock(declaration.body, environment);
    } catch (Return returnValue) {
      return returnValue.value;
    }
    if (isInitializer) {
      return closure.getAt(0, "this");
    }
    return null;
  }

  @Override
  public CallableSignature signature() {
    return this.signature;
  }

  @Override
  public String toString() {
    return String.format("<user-defined function>");
  }
}
