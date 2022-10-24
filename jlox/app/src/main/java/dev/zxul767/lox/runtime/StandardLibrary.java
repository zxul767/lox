package dev.zxul767.lox.runtime;

import java.lang.Math;
import java.util.List;

abstract class NativeCallable implements LoxCallable {
  @Override
  public String toString() {
    return String.format("<native fn> [arity:%d]", arity());
  }
  @Override
  public LoxCallable bind(LoxInstance instance) {
    return null;
  }
}

abstract class NoArgsCallable extends NativeCallable {
  @Override
  public int arity() {
    return 0;
  }
}

abstract class OneArgCallable extends NativeCallable {
  @Override
  public int arity() {
    return 1;
  }
}

final class StandardLibrary {
  private StandardLibrary() {}

  // the constructor for the native list type in Lox
  static final LoxCallable list = new LoxList();

  static final LoxCallable clock = new NoArgsCallable() {
    @Override
    public Object call(Interpreter interpreter, List<Object> args) {
      return (double)System.currentTimeMillis() / 1000.0;
    }
  };

  static final LoxCallable sin = new OneArgCallable() {
    @Override
    public Object call(Interpreter interpreter, List<Object> args) {
      return Math.sin((double)args.get(0));
    }
  };
}
