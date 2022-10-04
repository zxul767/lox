package dev.zxul767.lox;

import java.util.List;
import java.util.Map;

class LoxClass implements LoxCallable {
  final String name;
  private final Map<String, LoxFunction> methods;

  LoxClass(String name, Map<String, LoxFunction> methods) {
    this.name = name;
    this.methods = methods;
  }

  LoxFunction findMethod(String name) {
    if (methods.containsKey(name)) {
      return methods.get(name);
    }
    return null;
  }

  // this is the constructor call
  @Override
  public Object call(Interpreter interpreter, List<Object> args) {
    LoxInstance instance = new LoxInstance(this);
    return instance;
  }

  @Override
  public int arity() {
    // we don't support arguments in the constructor yet
    return 0;
  }

  @Override
  public String toString() {
    return name;
  }
}
