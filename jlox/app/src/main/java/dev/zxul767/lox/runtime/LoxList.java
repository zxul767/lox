package dev.zxul767.lox.runtime;

import dev.zxul767.lox.parsing.Token;
import dev.zxul767.lox.parsing.TokenType;
import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Function;

@FunctionalInterface
interface Invoker<T, R> {
  R invoke(T[] args);
}

class LoxList extends LoxClass {
  // constructor call
  @Override
  public Object call(Interpreter interpreter, List<Object> args) {
    return new LoxListInstance(this);
  }

  LoxList() {
    super(
        "list", /*superclass:*/ null,
        /*methods:*/ Collections.<String, LoxFunction>emptyMap()
    );
  }
}

class LoxListInstance extends LoxInstance {
  ArrayList<Object> list = new ArrayList<>();
  final Map<String, LoxListMethod> methods = new HashMap<>();

  LoxListInstance(LoxClass _class) {
    super(_class);

    // TODO: does the extra level of indirection is too bad for performance?
    addMethod("length", 0, (args) -> (double)this.list.size());
    addMethod("append", 1, (args) -> this.list.add(args[0]));
    addMethod("at", 1, (args) -> _at(args));
    addMethod("clear", 0, (args) -> _clear(args));
    addMethod("pop", 0, (args) -> _pop(args));
  }

  Object get(Token name) {
    // Unlike regular user-defined clases where fields shadow methods
    // Lox lists can have user-defined attributes, but if names collide
    // predefined methods take precedence.
    LoxListMethod method = this.methods.getOrDefault(name.lexeme, null);
    if (method != null) {
      return method;
    }

    if (this.fields.containsKey(name.lexeme)) {
      return fields.get(name.lexeme);
    }

    throw new RuntimeError(
        name, String.format("Undefined property '%s'.", name.lexeme)
    );
  }

  void addMethod(String name, int arity, Invoker<Object, Object> invoker) {
    this.methods.put(name, new LoxListMethod(name, arity, invoker));
  }

  Object _clear(Object... args) {
    this.list.clear();
    return null;
  }

  Object _at(Object... args) {
    int index = (int)(double)args[0];
    if (index < 0 || index >= this.list.size()) {
      throwIndexError("at", index);
    }
    return this.list.get(index);
  }

  Object _pop(Object... args) {
    if (this.list.isEmpty()) {
      throwEmptyListError("pop");
    }
    return this.list.remove(this.list.size() - 1);
  }

  void throwEmptyListError(String lexeme) {
    String message = "cannot perform operation on empty list";
    throw new RuntimeError(new Token(TokenType.IDENTIFIER, lexeme), message);
  }

  void throwIndexError(String lexeme, int index) {
    String message = String.format(
        "tried to access index %d, but valid range is [0, %d]", index,
        this.list.size() - 1
    );
    if (this.list.size() == 0)
      message = "cannot access elements in empty list";

    throw new RuntimeError(new Token(TokenType.IDENTIFIER, lexeme), message);
  }

  @Override
  public String toString() {
    ArrayList<String> strings = new ArrayList<>();
    for (Object object : this.list) {
      strings.add(Interpreter.stringify(object));
    }
    return "[" + String.join(", ", strings) + "]";
  }
}

class LoxListMethod implements LoxCallable {
  private String name;
  private int arity;
  // this closure captures a LoxListInstance
  private Invoker<Object, Object> closure;

  LoxListMethod(String name) { this(name, /*arity:*/ 0); }

  LoxListMethod(String name, int arity) {
    this(name, arity, /*instance:*/ null);
  }

  LoxListMethod(String name, int arity, Invoker<Object, Object> closure) {
    this.name = name;
    this.arity = arity;
    this.closure = closure;
  }

  @Override
  public Object call(Interpreter interpreter, List<Object> args) {
    return this.closure.invoke(args.toArray(new Object[0]));
  }

  @Override
  public int arity() {
    return this.arity;
  }

  @Override
  public String toString() {
    return String.format("<fn %s>", this.name);
  }
}
