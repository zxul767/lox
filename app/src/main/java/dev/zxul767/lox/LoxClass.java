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
    LoxFunction initializer = findMethod("__init__");
    if (initializer != null) {
      initializer.bind(instance).call(interpreter, args);
    }
    return instance;
  }

  @Override
  public int arity() {
    LoxFunction initializer = findMethod("__init__");
    if (initializer == null)
      return 0;
    return initializer.arity();
  }

  @Override
  public String toString() {
    return name;
  }
}
