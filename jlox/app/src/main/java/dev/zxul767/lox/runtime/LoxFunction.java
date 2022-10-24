package dev.zxul767.lox.runtime;

import dev.zxul767.lox.parsing.Stmt;
import java.util.List;

class LoxFunction implements LoxCallable {
  private final Stmt.Function declaration;
  private final Environment closure;
  private final boolean isInitializer;

  LoxFunction(
      Stmt.Function declaration, Environment closure, boolean isInitializer
  ) {
    this.isInitializer = isInitializer;
    this.declaration = declaration;
    this.closure = closure;
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
  public int arity() {
    return declaration.params.size();
  }

  @Override
  public String toString() {
    return String.format("<fn %s>", declaration.name.lexeme);
  }
}
