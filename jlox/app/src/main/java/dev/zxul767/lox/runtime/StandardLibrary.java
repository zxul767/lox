package dev.zxul767.lox.runtime;

import java.lang.Math;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

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

public final class StandardLibrary {
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

  public static final Map<String, LoxCallable> members;
  static {
    members = new HashMap<>();
    members.put("list", list);
    members.put("clock", clock);
    members.put("sin", clock);
  }
}
