package dev.zxul767.lox.runtime;

import dev.zxul767.lox.parsing.Token;
import dev.zxul767.lox.parsing.TokenType;
import java.util.Arrays;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

class LoxString extends LoxClass {
  public static final Map<String, CallableSignature> signatures;
  static {
    signatures = new HashMap<>();
    // TODO: parse signatures from lines like these:
    //
    // __init__ : (value:str)
    // length() -> number
    // starts_with(prefix:str) -> bool
    // ends_with(suffix:str) -> bool
    //
    List<CallableSignature> members = Arrays.asList(
        new CallableSignature("__init__", new Parameter("value"), "str"),
        new CallableSignature("length", "number"),
        new CallableSignature(
            "starts_with", new Parameter("prefix", "str"), "bool"
        ),
        new CallableSignature(
            "ends_with", new Parameter("suffix", "str"), "bool"
        ),
        new CallableSignature(
            "index_of", new Parameter("target", "str"), "number"
        ),
        new CallableSignature(
            "slice", Parameter.list("start", "int").add("end", "int").build(),
            "str"
        )
    );

    for (CallableSignature signature : members) {
      signatures.put(signature.name, signature);
    }
  }

  static Map<String, LoxCallable> buildConstructorMethod() {
    Map<String, LoxCallable> methods = new HashMap<String, LoxCallable>();
    methods.put(
        "__init__",
        new NativeBoundMethod(
            signatures.get("__init__"), (self, args) -> { return null; }
        )
    );
    return methods;
  }

  LoxString() {
    super(
        "str", /*superclass:*/ null,
        /*methods:*/ buildConstructorMethod()
    );

    registerMethod("length", (self, args) -> length(self));
    registerMethod("starts_with", (self, args) -> starts_with(self, args[0]));
    registerMethod("ends_with", (self, args) -> ends_with(self, args[0]));
    registerMethod("index_of", (self, args) -> index_of(self, args[0]));
    registerMethod("slice", (self, args) -> slice(self, args[0], args[1]));
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

  void
  registerMethod(String name, NativeMethod<Object, Object> nativeFunction) {
    CallableSignature signature = signatures.get(name);
    this.methods.put(
        signature.name, new NativeBoundMethod(signature, nativeFunction)
    );
  }

  static Object length(LoxInstance instance) {
    LoxStringInstance self = (LoxStringInstance)instance;
    return (double)self.string.length();
  }

  static Object starts_with(LoxInstance instance, Object value) {
    LoxStringInstance self = (LoxStringInstance)instance;
    if (!(value instanceof LoxStringInstance)) {
      // TODO: add token information when creating a `LoxStringInstance` in the
      // interpreter so we can report good errors
      throwInvalidArgument("starts_with", "argument must be a string");
    }
    LoxStringInstance arg = (LoxStringInstance)value;
    return self.string.startsWith(arg.string);
  }

  static Object ends_with(LoxInstance instance, Object value) {
    LoxStringInstance self = (LoxStringInstance)instance;
    if (!(value instanceof LoxStringInstance)) {
      throwInvalidArgument("starts_with", "argument must be a string");
    }
    LoxStringInstance arg = (LoxStringInstance)value;
    return self.string.endsWith(arg.string);
  }

  static Object index_of(LoxInstance instance, Object target) {
    LoxStringInstance self = (LoxStringInstance)instance;
    LoxStringInstance arg = (LoxStringInstance)target;
    return (double)self.string.indexOf(arg.string);
  }

  static Object
  slice(LoxInstance instance, Object startIndex, Object endIndex) {
    LoxStringInstance self = (LoxStringInstance)instance;
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
      throwInvalidArgument("slice", "start cannot be greater than end");
    }
    return new LoxStringInstance(self.string.substring(start, end));
  }

  static void
  throwIndexError(LoxStringInstance self, String lexeme, int index) {
    String message = String.format(
        "tried to access index %d, but valid range is [0..%d] or [-%d..-1]",
        index, self.string.length() - 1, self.string.length()
    );
    if (self.string.length() == 0)
      message = "cannot access elements in empty list";

    throw new RuntimeError(new Token(TokenType.IDENTIFIER, lexeme), message);
  }

  static void throwInvalidArgument(String lexeme, String message) {
    throw new RuntimeError(new Token(TokenType.IDENTIFIER, lexeme), message);
  }

  static void throwInvalidOperation(String token, String message) {
    throw new RuntimeError(new Token(TokenType.IDENTIFIER, token), message);
  }

  @Override
  public String toString() {
    return "<built-in class>";
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

    LoxStringInstance instance = (LoxStringInstance)other;

    return this.string.equals(instance.string);
  }
}
