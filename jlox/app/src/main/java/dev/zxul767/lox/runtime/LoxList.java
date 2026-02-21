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
            "__init__    ()                 -> list", "", (interpreter, self, args) -> self),
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
            "Sets the element at index and returns the assigned value.",
            (interpreter, self, args) -> set(self, args.get(0), args.get(1))),
        nativeMethod(
            "slice       (start:int, end:int=nil) -> list",
            "Returns sublist in [start, end).",
            (interpreter, self, args) ->
                slice(self, args.get(0), args.size() >= 2 ? args.get(1) : null)),
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
            (interpreter, self, args) -> getItem(self, args.get(0))),
        nativeMethod(
            "__setitem__ (index:int, value) -> any",
            "Chainable alias of index assignment.",
            (interpreter, self, args) -> setItem(self, args.get(0), args.get(1))));
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

  static Object set(LoxInstance instance, Object index, Object expression) {
    setItemAt(instance, index, expression, "set");
    return expression;
  }

  static Object slice(LoxInstance instance, Object startIndex, Object endIndex) {
    LoxList self = assertList(instance);
    requireNonEmptySliceTarget(self.list.size(), "list", "slice");
    int start =
        normalizeSliceIndex(
            requireInt(startIndex, "slice"),
            self.list.size(),
            "start",
            /*allowEndpoint:*/ false,
            "slice");
    int end = self.list.size();
    if (endIndex != null) {
      end =
          normalizeSliceIndex(
              requireInt(endIndex, "slice"),
              self.list.size(),
              "end",
              /*allowEndpoint:*/ true,
              "slice");
    }
    if (start > end) {
      throwRuntimeError("slice", "start cannot be greater than end");
    }

    var result = new LoxList();
    result.list.addAll(self.list.subList(start, end));
    return result;
  }

  static Object setItem(LoxInstance instance, Object index, Object expression) {
    setItemAt(instance, index, expression, "[");
    return expression;
  }

  static Object at(LoxInstance instance, Object index) {
    return getAt(instance, index, "at");
  }

  static Object setItemAt(LoxInstance instance, Object index, Object expression, String accessor) {
    LoxList self = assertList(instance);

    int normedIndex = normalizeIndex(index, self, accessor);
    return self.list.set(normedIndex, expression);
  }

  static Object getItem(LoxInstance instance, Object index) {
    return getAt(instance, index, "[");
  }

  static Object getAt(LoxInstance instance, Object index, String accessor) {
    LoxList self = assertList(instance);

    int normedIndex = normalizeIndex(index, self, accessor);
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
