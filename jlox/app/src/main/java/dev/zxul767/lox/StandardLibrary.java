package dev.zxul767.lox;

import java.lang.Math;
import java.util.List;

abstract class NoArgsCallable implements LoxCallable {
  @Override
  public int arity() {
    return 0;
  }
  @Override
  public String toString() {
    return String.format("<native fn> [arity:%d]", arity());
  }
}

abstract class OneArgCallable implements LoxCallable {
  @Override
  public int arity() {
    return 1;
  }
  @Override
  public String toString() {
    return String.format("<native fn> [arity:%d]", arity());
  }
}

final class StandardLibrary {
  private StandardLibrary() {}

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
