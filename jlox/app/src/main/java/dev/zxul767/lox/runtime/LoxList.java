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
  R invoke(LoxListInstance self, T[] args);
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
        /*methods:*/ new HashMap<String, LoxCallable>()
    );

    registerMethod("length", 0, (self, args) -> (double)self.list.size());
    registerMethod("append", 1, (self, args) -> self.list.add(args[0]));
    registerMethod("at", 1, (self, args) -> at(self, args[0]));
    registerMethod("__getitem__", 1, (self, args) -> at(self, args[0]));
    registerMethod("set", 2, (self, args) -> set(self, args[0], args[1]));
    registerMethod(
        "__setitem__", 2, (self, args) -> chainable_set(self, args[0], args[1])
    );
    registerMethod("clear", 0, (self, args) -> clear(self));
    registerMethod("pop", 0, (self, args) -> pop(self));
  }

  void registerMethod(String name, int arity, Invoker<Object, Object> invoker) {
    this.methods.put(name, new LoxListMethod(name, arity, invoker));
  }

  static Object clear(LoxListInstance instance) {
    instance.list.clear();
    return null;
  }

  static int normalizeIndex(Object index, LoxListInstance instance) {
    int originalIndex = (int)(double)index;
    int normedIndex = originalIndex;
    if (originalIndex < 0) {
      normedIndex = instance.list.size() + originalIndex;
    }
    if (normedIndex < 0 || normedIndex >= instance.list.size()) {
      // we report on the original index to avoid confusing users
      throwIndexError(instance, "at", originalIndex);
    }
    return normedIndex;
  }

  // we need a special method `chainable_set` because `set` returns the previous
  // value at `instance[index]` and that would produce counterintuitive
  // (apparently non-deterministic) behavior in programs like:
  //
  // >>> var l = list()
  // >>> l.append(list())
  // >>> (l[0] = 1).length()
  // 0
  // >>> (l[0] = 1).length()
  // RuntimeError: Only instances have properties
  // [line 1]
  //
  // `list[index] = 1` needs then to desugar to `list.chainable_set(index, 1)`
  //
  static Object
  chainable_set(LoxListInstance instance, Object index, Object expression) {
    int normedIndex = normalizeIndex(index, instance);
    instance.list.set(normedIndex, expression);
    return expression;
  }

  static Object set(LoxListInstance instance, Object index, Object expression) {
    int normedIndex = normalizeIndex(index, instance);
    return instance.list.set(normedIndex, expression);
  }

  static Object at(LoxListInstance instance, Object index) {
    int normedIndex = normalizeIndex(index, instance);
    return instance.list.get(normedIndex);
  }

  static Object pop(LoxListInstance instance) {
    if (instance.list.isEmpty()) {
      throwEmptyListError("pop");
    }
    return instance.list.remove(instance.list.size() - 1);
  }

  static void throwEmptyListError(String lexeme) {
    String message = "cannot perform operation on empty list";
    throw new RuntimeError(new Token(TokenType.IDENTIFIER, lexeme), message);
  }

  static void
  throwIndexError(LoxListInstance instance, String lexeme, int index) {
    String message = String.format(
        "tried to access index %d, but valid range is [0..%d] or [-%d..-1]",
        index, instance.list.size() - 1, instance.list.size()
    );
    if (instance.list.size() == 0)
      message = "cannot access elements in empty list";

    throw new RuntimeError(new Token(TokenType.IDENTIFIER, lexeme), message);
  }
}

class LoxListInstance extends LoxInstance {
  // TODO: check if we can just make it a nested class in LoxClass
  public ArrayList<Object> list = new ArrayList<>();

  LoxListInstance(LoxClass _class) { super(_class); }

  Object get(Token name) {
    // Unlike regular user-defined clases where fields shadow methods
    // Lox lists can have user-defined attributes, but if names collide
    // predefined methods take precedence.
    LoxCallable method = this._class.findMethod(name.lexeme);
    if (method != null) {
      return method.bind(this);
    }
    if (this.fields.containsKey(name.lexeme)) {
      return fields.get(name.lexeme);
    }

    throw new RuntimeError(
        name, String.format("Undefined property '%s'.", name.lexeme)
    );
  }

  @Override
  public String toString() {
    ArrayList<String> strings = new ArrayList<>();
    for (Object object : this.list) {
      strings.add(Interpreter.repr(object));
    }
    return "[" + String.join(", ", strings) + "]";
  }
}

class LoxListMethod implements LoxCallable {
  private String name;
  private int arity;
  private Invoker<Object, Object> nativeCall;
  private LoxListInstance instance = null;

  LoxListMethod(String name) { this(name, /*arity:*/ 0); }

  LoxListMethod(String name, int arity) {
    this(name, arity, /*instance:*/ null);
  }

  LoxListMethod(String name, int arity, Invoker<Object, Object> nativeCall) {
    this.name = name;
    this.arity = arity;
    this.nativeCall = nativeCall;
  }

  @Override
  public LoxCallable bind(LoxInstance instance) {
    LoxListMethod method =
        new LoxListMethod(this.name, this.arity, this.nativeCall);
    method.instance = (LoxListInstance)instance;

    return method;
  }

  @Override
  public Object call(Interpreter interpreter, List<Object> args) {
    if (this.instance == null) {
      throw new RuntimeError(
          new Token(TokenType.IDENTIFIER, this.name),
          "Cannot invoke unbound method."
      );
    }
    return this.nativeCall.invoke(this.instance, args.toArray(new Object[0]));
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
