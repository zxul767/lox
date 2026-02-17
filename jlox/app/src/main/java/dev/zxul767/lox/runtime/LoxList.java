package dev.zxul767.lox.runtime;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

class LoxListClass extends LoxNativeClass {
  static final Map<String, CallableSignature> signatures;
  static final Map<String, String> docstrings;

  static {
    signatures =
        createSignatures(
            "__init__    ()                 -> list",
            "length      ()                 -> int",
            "append      (value)            -> nil",
            "at          (index:int)        -> any",
            "set         (index:int, value) -> any",
            "__getitem__ (index:int)        -> any",
            "__setitem__ (index:int, value) -> any",
            "clear       ()                 -> nil",
            "pop         ()                 -> any");

    docstrings = new HashMap<>();
    docstrings.put("length", "Returns the number of elements in the list.");
    docstrings.put("append", "Appends a value to the end of the list.");
    docstrings.put("at", "Returns the element at index (negative indexes are supported).");
    docstrings.put("set", "Sets the element at index and returns the old value.");
    docstrings.put("__getitem__", "Alias of at(index).");
    docstrings.put("__setitem__", "Chainable alias of set(index, value).");
    docstrings.put("clear", "Removes all elements from the list.");
    docstrings.put("pop", "Removes and returns the last element.");
  }

  LoxListClass() {
    super("list", /*methods:*/ createDunderMethods(signatures));

    define("length", (self, args) -> length(self), signatures, docstrings);
    define("append", (self, args) -> add(self, args[0]), signatures, docstrings);
    define("at", (self, args) -> at(self, args[0]), signatures, docstrings);
    define("set", (self, args) -> set(self, args[0], args[1]), signatures, docstrings);
    define("clear", (self, args) -> clear(self), signatures, docstrings);
    define("pop", (self, args) -> pop(self), signatures, docstrings);
    define("__getitem__", (self, args) -> at(self, args[0]), signatures, docstrings);
    define(
        "__setitem__",
        (self, args) -> chainable_set(self, args[0], args[1]),
        signatures,
        docstrings);
  }

  // constructor call; this is equivalent to `__new__` in Python, in that it
  // only performs allocation, leaving initialization to `__init__`
  @Override
  public Object call(Interpreter interpreter, List<Object> args) {
    return new LoxList();
  }

  static Object length(LoxInstance instance) {
    LoxList self = assertList(instance);
    return (double) self.list.size();
  }

  static Object add(LoxInstance instance, Object value) {
    LoxList self = assertList(instance);
    self.list.add(value);
    return null;
  }

  static Object clear(LoxInstance instance) {
    LoxList self = assertList(instance);
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
  static Object chainable_set(LoxInstance instance, Object index, Object expression) {
    LoxList self = assertList(instance);

    int normedIndex = normalizeIndex(index, self, "set");
    self.list.set(normedIndex, expression);

    return expression;
  }

  static Object set(LoxInstance instance, Object index, Object expression) {
    LoxList self = assertList(instance);

    int normedIndex = normalizeIndex(index, self, "set");
    return self.list.set(normedIndex, expression);
  }

  static Object at(LoxInstance instance, Object index) {
    LoxList self = assertList(instance);

    int normedIndex = normalizeIndex(index, self, "at");
    return self.list.get(normedIndex);
  }

  static Object pop(LoxInstance instance) {
    LoxList self = assertList(instance);

    if (self.list.isEmpty()) {
      throwEmptyListError("pop");
    }
    return self.list.remove(self.list.size() - 1);
  }

  static int normalizeIndex(Object index, LoxList self, String methodName) {
    int originalIndex = requireInt(index, methodName);
    int normedIndex = originalIndex;
    if (originalIndex < 0) {
      normedIndex = self.list.size() + originalIndex;
    }
    if (normedIndex < 0 || normedIndex >= self.list.size()) {
      // we report on the original index to avoid confusing users
      throwIndexError(self, methodName, originalIndex);
    }
    return normedIndex;
  }

  static void throwEmptyListError(String lexeme) {
    String message = "cannot perform operation on empty list";
    throwRuntimeError(lexeme, message);
  }

  static void throwIndexError(LoxList self, String token, int index) {
    String message =
        String.format(
            "tried to access index %d, but valid range is [0..%d] or [-%d..-1]",
            index, self.list.size() - 1, self.list.size());
    if (self.list.size() == 0) {
      message = "cannot access elements in empty list";
    }
    throwRuntimeError(token, message);
  }
}

class LoxList extends LoxInstance {
  public ArrayList<Object> list = new ArrayList<>();

  LoxList() {
    super(StandardLibrary.list);
  }

  @Override
  public String toString() {
    var strings = new ArrayList<String>();
    for (Object object : this.list) {
      // if we don't stop the recursion here, we risk getting an infinite cycle
      if (object == this) {
        strings.add("@");

      } else if (object instanceof LoxList) {
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
    if (!(other instanceof LoxList)) {
      return false;
    }
    var instance = (LoxList) other;
    return this.list.equals(instance.list);
  }
}
