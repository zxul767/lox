package dev.zxul767.lox.runtime;

import dev.zxul767.lox.parsing.Token;
import dev.zxul767.lox.parsing.TokenType;
import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

class LoxString extends LoxNativeClass {
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

  LoxString() {
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
    if (arg instanceof LoxStringInstance) {
      return new LoxStringInstance(((LoxStringInstance)arg).string);
    }
    return new LoxStringInstance(Interpreter.stringify(args.get(0)));
  }

  static Object length(LoxInstance instance) {
    var self = (LoxStringInstance)instance;
    return (double)self.string.length();
  }

  static Object starts_with(LoxInstance instance, Object value) {
    var self = (LoxStringInstance)instance;
    if (!(value instanceof LoxStringInstance)) {
      // TODO: define token information when creating a `LoxStringInstance` in
      // the interpreter so we can report good errors
      throwRuntimeError("starts_with", "argument must be a string");
    }
    LoxStringInstance arg = (LoxStringInstance)value;
    return self.string.startsWith(arg.string);
  }

  static Object ends_with(LoxInstance instance, Object value) {
    var self = (LoxStringInstance)instance;
    if (!(value instanceof LoxStringInstance)) {
      throwRuntimeError("starts_with", "argument must be a string");
    }
    LoxStringInstance arg = (LoxStringInstance)value;
    return self.string.endsWith(arg.string);
  }

  static Object index_of(LoxInstance instance, Object target) {
    var self = (LoxStringInstance)instance;
    var arg = (LoxStringInstance)target;
    return (double)self.string.indexOf(arg.string);
  }

  static Object
  slice(LoxInstance instance, Object startIndex, Object endIndex) {
    var self = (LoxStringInstance)instance;
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
    return new LoxStringInstance(self.string.substring(start, end));
  }

  static void
  throwIndexError(LoxStringInstance self, String lexeme, int index) {
    var message = String.format(
        "tried to access index %d, but valid range is [0..%d] or [-%d..-1]",
        index, self.string.length() - 1, self.string.length()
    );
    if (self.string.length() == 0)
      message = "cannot access elements in empty list";

    throwRuntimeError(lexeme, message);
  }
}

class LoxStringInstance extends LoxInstance {
  public String string;

  LoxStringInstance(String string) {
    super(StandardLibrary.string);
    this.string = string;
  }

  public LoxStringInstance concatenate(LoxStringInstance other) {
    return new LoxStringInstance(this.string + other.string);
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
    if (!(other instanceof LoxStringInstance)) {
      return false;
    }
    var instance = (LoxStringInstance)other;
    return this.string.equals(instance.string);
  }
}
