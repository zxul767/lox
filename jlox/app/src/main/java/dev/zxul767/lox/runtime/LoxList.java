package dev.zxul767.lox.runtime;

import java.util.ArrayList;
import java.util.List;

class LoxListClass extends LoxNativeClass {
  LoxListClass() {
    super("list", /*methods:*/ bindMethods(createNativeMethods()));
  }

  private static List<NativeCallableSpec> createNativeMethods() {
    return List.of(
        nativeMethod(
            "__init__    ()                 -> list", null, (interpreter, self, args) -> self),
        nativeMethod(
            "length      ()                 -> int",
            "Returns the number of elements in the list.",
            (interpreter, self, args) -> length(self)),
        nativeMethod(
            "append      (value)            -> nil",
            "Appends a value to the end of the list.",
            (interpreter, self, args) -> add(self, args.get(0))),
        nativeMethod(
            "at          (index:int)        -> any",
            "Returns the element at index (negative indexes are supported).",
            (interpreter, self, args) -> at(self, args.get(0))),
        nativeMethod(
            "set         (index:int, value) -> any",
            "Sets the element at index and returns the old value.",
            (interpreter, self, args) -> set(self, args.get(0), args.get(1))),
        nativeMethod(
            "clear       ()                 -> nil",
            "Removes all elements from the list.",
            (interpreter, self, args) -> clear(self)),
        nativeMethod(
            "pop         ()                 -> any",
            "Removes and returns the last element.",
            (interpreter, self, args) -> pop(self)),
        nativeMethod(
            "__getitem__ (index:int)        -> any",
            "Alias of at(index).",
            (interpreter, self, args) -> at(self, args.get(0))),
        nativeMethod(
            "__setitem__ (index:int, value) -> any",
            "Chainable alias of set(index, value).",
            (interpreter, self, args) -> chainable_set(self, args.get(0), args.get(1))));
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
  ArrayList<Object> list = new ArrayList<>();

  LoxList() {
    super(/* _class: */ StandardLibrary.list);
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
    return equalsByClassAndKey(other, LoxList.class, l -> l.list);
  }
}
