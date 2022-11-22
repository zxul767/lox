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

class LoxList extends LoxClass {
  public static final Map<String, CallableSignature> signatures;
  static {
    List<CallableSignature> members = Arrays.asList(
        new CallableSignature("__init__", "list"),
        new CallableSignature("length", "number"),
        new CallableSignature("append", new Parameter("value"), "bool"),
        new CallableSignature("at", new Parameter("index", "int")),
        new CallableSignature("__getitem__", new Parameter("index", "int")),
        new CallableSignature(
            "set", Parameter.list("index", "int").add("value").build()
        ),
        new CallableSignature(
            "__setitem__", Parameter.list("index", "int").add("value").build()
        ),
        new CallableSignature("clear", "nil"), new CallableSignature("pop")
    );
    signatures = new HashMap<>();
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

  LoxList() {
    super(
        "list", /*superclass:*/ null,
        /*methods:*/ buildConstructorMethod()
    );

    registerMethod("length", (self, args) -> length(self));
    registerMethod("append", (self, args) -> add(self, args[0]));
    registerMethod("at", (self, args) -> at(self, args[0]));
    registerMethod("__getitem__", (self, args) -> at(self, args[0]));
    registerMethod("set", (self, args) -> set(self, args[0], args[1]));
    registerMethod(
        "__setitem__", (self, args) -> chainable_set(self, args[0], args[1])
    );
    registerMethod("clear", (self, args) -> clear(self));
    registerMethod("pop", (self, args) -> pop(self));
  }

  // TODO: Create a NativeClass subclass so we can hoist common methods and code
  // between LoxString and LoxList
  void
  registerMethod(String name, NativeMethod<Object, Object> nativeFunction) {
    CallableSignature signature = signatures.getOrDefault(name, null);
    if (signature == null) {
      System.err.println(
          String.format("ERROR: failed to register method: %s", name)
      );
    } else {
      this.methods.put(
          signature.name, new NativeBoundMethod(signature, nativeFunction)
      );
    }
  }

  // constructor call
  @Override
  public Object call(Interpreter interpreter, List<Object> args) {
    return new LoxListInstance();
  }

  static Object length(LoxInstance instance) {
    LoxListInstance self = (LoxListInstance)instance;
    return (double)self.list.size();
  }

  static Object add(LoxInstance instance, Object value) {
    LoxListInstance self = (LoxListInstance)instance;
    return self.list.add(value);
  }

  static Object clear(LoxInstance instance) {
    LoxListInstance self = (LoxListInstance)instance;
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
    throw new RuntimeError(new Token(TokenType.IDENTIFIER, lexeme), message);
  }

  static void throwIndexError(LoxListInstance self, String lexeme, int index) {
    String message = String.format(
        "tried to access index %d, but valid range is [0..%d] or [-%d..-1]",
        index, self.list.size() - 1, self.list.size()
    );
    if (self.list.size() == 0)
      message = "cannot access elements in empty list";

    throw new RuntimeError(new Token(TokenType.IDENTIFIER, lexeme), message);
  }

  @Override
  public String toString() {
    return "<built-in class>";
  }
}

class LoxListInstance extends LoxInstance {
  public ArrayList<Object> list = new ArrayList<>();

  LoxListInstance() { super(StandardLibrary.list); }

  @Override
  public String toString() {
    ArrayList<String> strings = new ArrayList<>();
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

    LoxListInstance instance = (LoxListInstance)other;

    return this.list.equals(instance.list);
  }
}
