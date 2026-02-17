package dev.zxul767.lox.runtime;

import static dev.zxul767.lox.parsing.TokenType.*;

import java.util.Arrays;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

@FunctionalInterface
interface NativeMethod<T, R> {
  R invoke(LoxInstance self, T[] args);
}

abstract class NativeCallable implements LoxCallable {
  private final CallableSignature signature;
  private final String docstring;

  NativeCallable(CallableSignature signature) {
    this(signature, /* docstring: */ null);
  }

  NativeCallable(CallableSignature signature, String docstring) {
    this.signature = signature;
    this.docstring = docstring;
  }

  @Override
  public CallableSignature signature() {
    return this.signature;
  }

  @Override
  public String docstring() {
    return this.docstring;
  }

  @Override
  public LoxCallable bind(LoxInstance instance) {
    // by default, native functions don't bind to anything
    return this;
  }

  @Override
  public String toString() {
    return String.format("<built-in function>");
  }
}

class NativeBoundMethod implements LoxCallable {
  private final NativeMethod<Object, Object> nativeFunction;
  private final CallableSignature signature;
  private final String docstring;
  private LoxInstance instance;

  NativeBoundMethod(CallableSignature signature, NativeMethod<Object, Object> nativeFunction) {
    this(signature, nativeFunction, /* instance: */ null, /* docstring: */ null);
  }

  NativeBoundMethod(
      CallableSignature signature, NativeMethod<Object, Object> nativeFunction, String docstring) {
    this(signature, nativeFunction, /* instance: */ null, /* docstring: */ docstring);
  }

  private NativeBoundMethod(
      CallableSignature signature,
      NativeMethod<Object, Object> nativeFunction,
      LoxInstance instance,
      String docstring) {
    this.signature = signature;
    this.nativeFunction = nativeFunction;
    this.instance = instance;
    this.docstring = docstring;
  }

  @Override
  public CallableSignature signature() {
    return this.signature;
  }

  @Override
  public String docstring() {
    return this.docstring;
  }

  @Override
  public Object call(Interpreter interpreter, List<Object> args) {
    if (this.instance == null) {
      throw new RuntimeError(this.signature.name, "Cannot invoke unbound method.");
    }
    return this.nativeFunction.invoke(this.instance, args.toArray(new Object[0]));
  }

  @Override
  public LoxCallable bind(LoxInstance instance) {
    return new NativeBoundMethod(this.signature, this.nativeFunction, instance, this.docstring);
  }

  @Override
  public String toString() {
    return String.format("<built-in method>");
  }
}

abstract class NoArgsCallable extends NativeCallable {
  NoArgsCallable(String name) {
    super(new CallableSignature(name));
  }

  NoArgsCallable(String name, String returnType) {
    super(new CallableSignature(name, returnType));
  }

  NoArgsCallable(String name, String returnType, String docstring) {
    super(new CallableSignature(name, returnType), docstring);
  }
}

abstract class OneArgCallable extends NativeCallable {
  OneArgCallable(String name, CallableParameter parameter) {
    this(name, parameter, "any");
  }

  OneArgCallable(String name, CallableParameter parameter, String returnType) {
    super(new CallableSignature(name, parameter, returnType));
  }

  OneArgCallable(String name, CallableParameter parameter, String returnType, String docstring) {
    super(new CallableSignature(name, parameter, returnType), docstring);
  }
}

public final class StandardLibrary {
  private StandardLibrary() {}

  static final LoxClass list = new LoxListClass();
  static final LoxClass string = new LoxStringClass();

  // TODO: use the same machinery to build signatures that is being used in
  // `LoxNativeClass`
  //
  static final LoxCallable clock =
      new NoArgsCallable("clock", "number", "Returns elapsed process time in seconds.") {
        @Override
        public Object call(Interpreter interpreter, List<Object> args) {
          return (double) System.currentTimeMillis() / 1000.0;
        }
      };

  static final LoxCallable sin =
      new OneArgCallable("sin", new CallableParameter("n", "number"), "number") {
        @Override
        public Object call(Interpreter interpreter, List<Object> args) {
          double value = LoxNativeClass.requireDouble(args.get(0), "sin");
          return Math.sin(value);
        }
      };

  static final LoxCallable print =
      new OneArgCallable(
          "print",
          new CallableParameter("value"),
          "nil",
          "Prints a value without a trailing newline.") {
        @Override
        public Object call(Interpreter interpreter, List<Object> args) {
          System.out.print(Interpreter.stringify(args.get(0)));
          return null;
        }
      };

  static final LoxCallable println =
      new OneArgCallable(
          "println",
          new CallableParameter("value"),
          "nil",
          "Prints a value followed by a newline.") {
        @Override
        public Object call(Interpreter interpreter, List<Object> args) {
          System.out.println(Interpreter.stringify(args.get(0)));
          return null;
        }
      };

  static final LoxCallable help =
      new OneArgCallable(
          "help",
          new CallableParameter("object"),
          "nil",
          "Shows details for values, and signature/docs for callables when available.") {
        @Override
        public Object call(Interpreter interpreter, List<Object> args) {
          Object arg = args.get(0);
          printHelp(arg);
          return null;
        }
      };

  private static String typeDescription(Object value) {
    if (value == null) return "nil";
    if (value instanceof Double) return "number";
    if (value instanceof Boolean) return "boolean";
    if (value instanceof LoxString) return "string";
    if (value instanceof LoxClass) return "class";
    if (value instanceof NativeBoundMethod) return "bound method";
    if (value instanceof NativeCallable) return "native function";
    if (value instanceof LoxFunction) return "function";
    if (value instanceof LoxInstance) return ((LoxInstance) value)._class.name;
    if (value instanceof LoxCallable) return "callable";
    return "any";
  }

  private static void printDocstring(String docstring) {
    if (docstring != null && !docstring.isBlank()) {
      System.out.print(" | " + docstring);
    }
  }

  private static void printCallableDetails(LoxCallable callable) {
    System.out.print("[" + typeDescription(callable) + "] ");
    System.out.print(callable.signature());
    printDocstring(callable.docstring());
    System.out.println();
  }

  private static int countNonInitializerMethods(LoxClass _class) {
    int count = 0;
    for (String name : _class.methods.keySet()) {
      if (!name.equals(LoxClass.INIT)) {
        count++;
      }
    }
    return count;
  }

  private static void printMethodEntry(LoxCallable method) {
    System.out.print("  - " + method.signature());
    printDocstring(method.docstring());
    System.out.println();
  }

  private static void printClassHelp(LoxClass _class) {
    System.out.println("[class] <class " + _class.name + ">");

    LoxCallable initializer = _class.methods.getOrDefault(LoxClass.INIT, null);
    CallableSignature ctor =
        initializer != null
            ? initializer.signature().withName(_class.name)
            : new CallableSignature(_class.name, /* parameters: */ List.of(), _class.name);
    System.out.print("constructor: " + ctor);
    printDocstring(initializer != null ? initializer.docstring() : null);
    System.out.println();

    System.out.println("methods: " + countNonInitializerMethods(_class));
    for (var entry : _class.methods.entrySet()) {
      String name = entry.getKey();
      if (name.equals(LoxClass.INIT)) {
        continue;
      }
      printMethodEntry(entry.getValue());
    }
  }

  private static void printInstanceHelp(LoxInstance instance) {
    String className = instance._class.name;
    System.out.println("[" + className + "] <" + className + " instance>");
    System.out.println("Use help(" + className + ") to inspect constructor and methods.");
  }

  private static void printValueHelp(Object value) {
    System.out.println(Interpreter.repr(value) + " [" + typeDescription(value) + "]");
  }

  private static void printHelp(Object value) {
    if (value instanceof LoxClass) {
      printClassHelp((LoxClass) value);
      return;
    }
    if (value instanceof LoxInstance) {
      printInstanceHelp((LoxInstance) value);
      return;
    }
    if (value instanceof LoxCallable) {
      printCallableDetails((LoxCallable) value);
      return;
    }
    printValueHelp(value);
  }

  // TODO: this is public because it needs to be accessed from the REPL (to
  // provide auto-completion)
  public static final Map<String, LoxCallable> members;

  static {
    List<LoxCallable> callables = Arrays.asList(list, string, clock, sin, print, println, help);

    members = new HashMap<>();
    for (LoxCallable callable : callables) {
      members.put(callable.signature().name, callable);
    }
  }
}
