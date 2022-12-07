package dev.zxul767.lox.runtime;

import static dev.zxul767.lox.parsing.TokenType.*;

import dev.zxul767.lox.parsing.Token;
import java.lang.Math;
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

  NativeCallable(CallableSignature signature) { this.signature = signature; }

  @Override
  public CallableSignature signature() {
    return this.signature;
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
  private LoxInstance instance;

  NativeBoundMethod(
      CallableSignature signature, NativeMethod<Object, Object> nativeFunction
  ) {
    this(signature, nativeFunction, /* instance: */ null);
  }

  NativeBoundMethod(
      CallableSignature signature, NativeMethod<Object, Object> nativeFunction,
      LoxInstance instance
  ) {
    this.signature = signature;
    this.nativeFunction = nativeFunction;
    this.instance = instance;
  }

  @Override
  public CallableSignature signature() {
    return this.signature;
  }

  @Override
  public Object call(Interpreter interpreter, List<Object> args) {
    if (this.instance == null) {
      throw new RuntimeError(
          this.signature.name, "Cannot invoke unbound method."
      );
    }
    return this.nativeFunction.invoke(
        this.instance, args.toArray(new Object[0])
    );
  }

  @Override
  public LoxCallable bind(LoxInstance instance) {
    return new NativeBoundMethod(this.signature, this.nativeFunction, instance);
  }

  @Override
  public String toString() {
    return String.format("<built-in method>");
  }
}

abstract class NoArgsCallable extends NativeCallable {
  NoArgsCallable(String name) { super(new CallableSignature(name)); }
  NoArgsCallable(String name, String returnType) {
    super(new CallableSignature(name, returnType));
  }
}

abstract class OneArgCallable extends NativeCallable {
  OneArgCallable(String name, Parameter parameter) {
    this(name, parameter, "any");
  }
  OneArgCallable(String name, Parameter parameter, String returnType) {
    super(new CallableSignature(name, parameter, returnType));
  }
}

public final class StandardLibrary {
  private StandardLibrary() {}

  static final LoxClass list = new LoxListClass();
  static final LoxClass string = new LoxStringClass();

  // TODO: use the same machinery to build signatures that is being used in
  // `LoxNativeClass`
  //
  static final LoxCallable clock = new NoArgsCallable("clock", "number") {
    @Override
    public Object call(Interpreter interpreter, List<Object> args) {
      return (double)System.currentTimeMillis() / 1000.0;
    }
  };

  static final LoxCallable sin =
      new OneArgCallable("sin", new Parameter("n", "number"), "number") {
        @Override
        public Object call(Interpreter interpreter, List<Object> args) {
          double value = LoxNativeClass.requireDouble(args.get(0), "sin");
          return Math.sin(value);
        }
      };

  static final LoxCallable print =
      new OneArgCallable("print", new Parameter("value"), "nil") {
        @Override
        public Object call(Interpreter interpreter, List<Object> args) {
          System.out.print(Interpreter.stringify(args.get(0)));
          return null;
        }
      };

  static final LoxCallable println =
      new OneArgCallable("println", new Parameter("value"), "nil") {
        @Override
        public Object call(Interpreter interpreter, List<Object> args) {
          System.out.println(Interpreter.stringify(args.get(0)));
          return null;
        }
      };

  static final LoxCallable help =
      new OneArgCallable("help", new Parameter("object")) {
        @Override
        public Object call(Interpreter interpreter, List<Object> args) {
          Object arg = args.get(0);

          String valueRepr = arg == null ? "nil" : arg.toString();
          String valueType = arg == null ? "nil" : "any";

          if (arg instanceof LoxCallable) {
            // TODO: show members for native classes
            valueRepr = ((LoxCallable)arg).signature().toString();
            valueType = arg.toString();

          } else if (arg instanceof Double) {
            valueType = "number";

          } else if (arg instanceof Boolean) {
            valueType = "boolean";

          } else if (arg instanceof LoxString) {
            valueRepr = String.format("'%s'", valueRepr);
            valueType = ((LoxInstance)arg)._class.name;

          } else if (arg instanceof LoxInstance) {
            valueType = ((LoxInstance)arg)._class.name;
          }

          System.out.println(String.format("%s : %s", valueRepr, valueType));

          return null;
        }
      };

  // TODO: this is public because it needs to be accessed from the REPL (to
  // provide auto-completion)
  public static final Map<String, LoxCallable> members;
  static {
    List<LoxCallable> callables =
        Arrays.asList(list, string, clock, sin, print, println, help);

    members = new HashMap<>();
    for (LoxCallable callable : callables) {
      members.put(callable.signature().name, callable);
    }
  }
}
