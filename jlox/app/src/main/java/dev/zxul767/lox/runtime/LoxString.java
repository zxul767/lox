package dev.zxul767.lox.runtime;

import dev.zxul767.lox.parsing.Token;
import dev.zxul767.lox.parsing.TokenType;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

class LoxStringClass extends LoxNativeClass {
  static final Map<String, CallableSignature> signatures;
  static {
    signatures = createSignatures(
        /* clang-format off */
        "__init__    (value:any)          -> str",
        "length      ()                   -> int",
        "starts_with (prefix:str)         -> bool",
        "ends_with   (suffix:str)         -> bool",
        "index_of    (target:str)         -> int",
        "slice       (start:int, end:int) -> str"
        /* clang-format on */
    );
  }

  LoxStringClass() {
    super(
        "str",
        /* methods: */ createDunderMethods(signatures)
    );

    define("length", (self, args) -> length(self), signatures);
    define(
        "starts_with", (self, args) -> starts_with(self, args[0]), signatures
    );
    define("ends_with", (self, args) -> ends_with(self, args[0]), signatures);
    define("index_of", (self, args) -> index_of(self, args[0]), signatures);
    define("slice", (self, args) -> slice(self, args[0], args[1]), signatures);
  }

  // constructor call
  @Override
  public Object call(Interpreter interpreter, List<Object> args) {
    Object arg = args.get(0);
    if (arg instanceof LoxString) {
      return new LoxString(((LoxString)arg).string);
    }
    return new LoxString(Interpreter.stringify(args.get(0)));
  }

  static Object length(LoxInstance instance) {
    var self = (LoxString)instance;
    return (double)self.string.length();
  }

  static Object starts_with(LoxInstance instance, Object value) {
    var self = (LoxString)instance;
    if (!(value instanceof LoxString)) {
      // TODO: define token information when creating a `LoxString` in
      // the interpreter so we can report good errors
      throwRuntimeError("starts_with", "argument must be a string");
    }
    var arg = (LoxString)value;
    return self.string.startsWith(arg.string);
  }

  static Object ends_with(LoxInstance instance, Object value) {
    var self = (LoxString)instance;
    if (!(value instanceof LoxString)) {
      throwRuntimeError("starts_with", "argument must be a string");
    }
    var arg = (LoxString)value;
    return self.string.endsWith(arg.string);
  }

  static Object index_of(LoxInstance instance, Object target) {
    var self = (LoxString)instance;
    var arg = (LoxString)target;
    return (double)self.string.indexOf(arg.string);
  }

  static Object
  slice(LoxInstance instance, Object startIndex, Object endIndex) {
    var self = (LoxString)instance;
    int start = ((Double)startIndex).intValue();
    int end = ((Double)endIndex).intValue();

    // TODO: research why python simply returns empy strings instead of these
    // errors. should we do the same?
    if (start < 0 || start >= self.string.length()) {
      throwIndexError(self, "slice(start, ...)", start);
    }
    if (end < 0 || end >= self.string.length()) {
      throwIndexError(self, "slice(..., end)", start);
    }
    if (start > end) {
      throwRuntimeError("slice", "start cannot be greater than end");
    }
    return new LoxString(self.string.substring(start, end));
  }

  static void throwIndexError(LoxString self, String lexeme, int index) {
    var message = String.format(
        "tried to access index %d, but valid range is [0..%d] or [-%d..-1]",
        index, self.string.length() - 1, self.string.length()
    );
    if (self.string.length() == 0)
      message = "cannot access elements in empty list";

    throwRuntimeError(lexeme, message);
  }
}

class LoxString extends LoxInstance {
  public String string;

  LoxString(String string) {
    super(StandardLibrary.string);
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
    if (other == this) {
      return true;
    }
    if (!(other instanceof LoxString)) {
      return false;
    }
    var instance = (LoxString)other;
    return this.string.equals(instance.string);
  }
}
