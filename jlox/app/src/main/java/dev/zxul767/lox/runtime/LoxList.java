package dev.zxul767.lox.runtime;

import dev.zxul767.lox.parsing.Token;
import dev.zxul767.lox.parsing.TokenType;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.function.Function;

class LoxList extends LoxNativeClass {
  static final Map<String, CallableSignature> signatures;
  static {
    signatures = createSignatures(
        /* clang-format off */
        "__init__    ()                 -> list",
        "length      ()                 -> int",
        "append      (value)            -> bool",
        "at          (index:int)        -> any",
        "set         (index:int, value) -> any",
        "__getitem__ (index:int)        -> any",
        "__setitem__ (index:int, value) -> any",
        "clear       ()                 -> nil",
        "pop         ()                 -> any"
        /* clang-format on */
    );
  }

  LoxList() {
    super(
        "list",
        /*methods:*/ createDunderMethods(signatures)
    );

    define("length", (self, args) -> length(self), signatures);
    define("append", (self, args) -> add(self, args[0]), signatures);
    define("at", (self, args) -> at(self, args[0]), signatures);
    define("set", (self, args) -> set(self, args[0], args[1]), signatures);
    define("clear", (self, args) -> clear(self), signatures);
    define("pop", (self, args) -> pop(self), signatures);
    define("__getitem__", (self, args) -> at(self, args[0]), signatures);
    define(
        "__setitem__",
        (self, args) -> chainable_set(self, args[0], args[1]), signatures
    );
  }

  // constructor call
  @Override
  public Object call(Interpreter interpreter, List<Object> args) {
    return new LoxListInstance();
  }

  static Object length(LoxInstance instance) {
    var self = (LoxListInstance)instance;
    return (double)self.list.size();
  }

  static Object add(LoxInstance instance, Object value) {
    var self = (LoxListInstance)instance;
    return self.list.add(value);
  }

  static Object clear(LoxInstance instance) {
    var self = (LoxListInstance)instance;
    self.list.clear();
    return null;
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
  chainable_set(LoxInstance instance, Object index, Object expression) {
    LoxListInstance self = (LoxListInstance)instance;
    int normedIndex = normalizeIndex(index, self);
    self.list.set(normedIndex, expression);
    return expression;
  }

  static Object set(LoxInstance instance, Object index, Object expression) {
    LoxListInstance self = (LoxListInstance)instance;
    int normedIndex = normalizeIndex(index, self);
    return self.list.set(normedIndex, expression);
  }

  static Object at(LoxInstance instance, Object index) {
    LoxListInstance self = (LoxListInstance)instance;
    int normedIndex = normalizeIndex(index, self);
    return self.list.get(normedIndex);
  }

  static Object pop(LoxInstance instance) {
    LoxListInstance self = (LoxListInstance)instance;
    if (self.list.isEmpty()) {
      throwEmptyListError("pop");
    }
    return self.list.remove(self.list.size() - 1);
  }

  static int normalizeIndex(Object index, LoxListInstance self) {
    int originalIndex = (int)(double)index;
    int normedIndex = originalIndex;
    if (originalIndex < 0) {
      normedIndex = self.list.size() + originalIndex;
    }
    if (normedIndex < 0 || normedIndex >= self.list.size()) {
      // we report on the original index to avoid confusing users
      throwIndexError(self, "at", originalIndex);
    }
    return normedIndex;
  }

  static void throwEmptyListError(String lexeme) {
    String message = "cannot perform operation on empty list";
    throwRuntimeError(lexeme, message);
  }

  static void throwIndexError(LoxListInstance self, String lexeme, int index) {
    String message = String.format(
        "tried to access index %d, but valid range is [0..%d] or [-%d..-1]",
        index, self.list.size() - 1, self.list.size()
    );
    if (self.list.size() == 0)
      message = "cannot access elements in empty list";

    throwRuntimeError(lexeme, message);
  }
}

class LoxListInstance extends LoxInstance {
  public ArrayList<Object> list = new ArrayList<>();

  LoxListInstance() { super(StandardLibrary.list); }

  @Override
  public String toString() {
    var strings = new ArrayList<String>();
    for (Object object : this.list) {
      // if we don't stop the recursion here, we risk getting an infinite cycle
      if (object == this) {
        strings.add("@");

      } else if (object instanceof LoxListInstance) {
        strings.add("[...]");

      } else {
        strings.add(Interpreter.repr(object));
      }
    }
    return "[" + String.join(", ", strings) + "]";
  }

  @Override
  public boolean equals(Object other) {
    if (other == this) {
      return true;
    }
    if (!(other instanceof LoxListInstance)) {
      return false;
    }
    var instance = (LoxListInstance)other;
    return this.list.equals(instance.list);
  }
}
