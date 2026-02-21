package dev.zxul767.lox.runtime;

import java.util.List;

class LoxStringClass extends LoxNativeClass {
  LoxStringClass() {
    super("str", /* methods: */ bindMethods(createNativeMethods()));
  }

  private static List<NativeCallableSpec> createNativeMethods() {
    return List.of(
        nativeMethod(
            "__init__    (value:any)          -> str", null, (interpreter, self, args) -> self),
        nativeMethod(
            "length      ()                   -> int",
            "Returns the string length.",
            (interpreter, self, args) -> length(self)),
        nativeMethod(
            "starts_with (prefix:str)         -> bool",
            "Returns true if string starts with prefix.",
            (interpreter, self, args) -> starts_with(self, args.get(0))),
        nativeMethod(
            "ends_with   (suffix:str)         -> bool",
            "Returns true if string ends with suffix.",
            (interpreter, self, args) -> ends_with(self, args.get(0))),
        nativeMethod(
            "index_of    (target:str)         -> int",
            "Returns first index of target, or -1 if not found.",
            (interpreter, self, args) -> index_of(self, args.get(0))),
        nativeMethod(
            "slice       (start:int, end:int=nil) -> str",
            "Returns substring in [start, end).",
            (interpreter, self, args) ->
                slice(self, args.get(0), args.size() >= 2 ? args.get(1) : null)));
  }

  // constructor call
  @Override
  public Object call(Interpreter interpreter, List<Object> args) {
    Object arg = args.get(0);
    if (arg instanceof LoxString) {
      return new LoxString(((LoxString) arg).string);
    }
    return new LoxString(Interpreter.stringify(arg));
  }

  static Object length(LoxInstance instance) {
    LoxString self = assertString(instance);
    return (double) self.string.length();
  }

  static Object starts_with(LoxInstance instance, Object value) {
    LoxString self = assertString(instance);
    LoxString prefix = requireString(value, "starts_with");

    return self.string.startsWith(prefix.string);
  }

  static Object ends_with(LoxInstance instance, Object value) {
    LoxString self = assertString(instance);
    LoxString suffix = requireString(value, "ends_with");

    return self.string.endsWith(suffix.string);
  }

  static Object index_of(LoxInstance instance, Object value) {
    LoxString self = assertString(instance);
    LoxString target = requireString(value, "index_of");

    return (double) self.string.indexOf(target.string);
  }

  static Object slice(LoxInstance instance, Object startIndex, Object endIndex) {
    LoxString self = assertString(instance);
    requireNonEmptySliceTarget(self.string.length(), "string", "slice");
    int start =
        normalizeSliceIndex(
            requireInt(startIndex, "slice"),
            self.string.length(),
            "start",
            /*allowEndpoint:*/ false,
            "slice");
    int end = self.string.length();
    if (endIndex != null) {
      end =
          normalizeSliceIndex(
              requireInt(endIndex, "slice"),
              self.string.length(),
              "end",
              /*allowEndpoint:*/ true,
              "slice");
    }
    if (start > end) {
      throwRuntimeError("slice", "start cannot be greater than end");
    }
    return new LoxString(self.string.substring(start, end));
  }
}

class LoxString extends LoxInstance {
  String string;

  LoxString(String string) {
    super(/* _class: */ StandardLibrary.string);
    this.string = string;
  }

  public LoxString concatenate(LoxString other) {
    return new LoxString(this.string + other.string);
  }

  @Override
  public String toString() {
    return this.string;
  }

  @Override
  public boolean equals(Object other) {
    return equalsByClassAndKey(other, LoxString.class, s -> s.string);
  }
}
