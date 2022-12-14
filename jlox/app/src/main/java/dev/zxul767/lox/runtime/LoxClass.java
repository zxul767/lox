package dev.zxul767.lox.runtime;

import java.util.Collections;
import java.util.List;
import java.util.Map;

class LoxClass implements LoxCallable {
  public static final String INIT = "__init__";

  final String name;
  final LoxClass superclass;
  final CallableSignature signature;
  protected final Map<String, LoxCallable> methods;

  LoxClass(String name, LoxClass superclass, Map<String, LoxCallable> methods) {
    this.superclass = superclass;
    this.name = name;
    this.methods = methods;

    if (methods.containsKey(INIT)) {
      LoxCallable initializer = methods.get(INIT);
      this.signature = initializer.signature().withName(name);

    } else {
      this.signature = new CallableSignature(name, /*returnType:*/ name);
    }
  }

  LoxCallable findMethod(String name) {
    if (methods.containsKey(name)) {
      return methods.get(name);
    }
    if (superclass != null) {
      return superclass.findMethod(name);
    }
    return null;
  }

  @Override
  public CallableSignature signature() {
    return this.signature;
  }

  @Override
  public LoxCallable bind(LoxInstance instance) {
    // as a callable, a class is just a constructor, so it can't be bound.
    return this;
  }

  // this is the constructor call
  @Override
  public Object call(Interpreter interpreter, List<Object> args) {
    LoxInstance instance = new LoxInstance(this);
    LoxCallable initializer = findMethod(INIT);
    if (initializer != null) {
      initializer.bind(instance).call(interpreter, args);
    }
    return instance;
  }

  @Override
  public String toString() {
    return "<class:" + name + ">";
  }
}
