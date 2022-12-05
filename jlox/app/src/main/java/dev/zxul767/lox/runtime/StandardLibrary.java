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
    // by default, methods don't bind to anything
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
  private LoxInstance instance = null;

  NativeBoundMethod(
      CallableSignature signature, NativeMethod<Object, Object> nativeFunction
  ) {
    this.signature = signature;
    this.nativeFunction = nativeFunction;
  }

  @Override
  public CallableSignature signature() {
    return this.signature;
  }

  @Override
  public Object call(Interpreter interpreter, List<Object> args) {
    if (this.instance == null) {
      throw new RuntimeError(
          new Token(IDENTIFIER, this.signature.name),
          "Cannot invoke unbound method."
      );
    }
    return this.nativeFunction.invoke(
        this.instance, args.toArray(new Object[0])
    );
  }

  @Override
  public LoxCallable bind(LoxInstance instance) {
    NativeBoundMethod method =
        new NativeBoundMethod(this.signature, this.nativeFunction);
    method.instance = instance;

    return method;
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

  // the constructor for the native list type in Lox
  static final LoxClass list = new LoxList();
  static final LoxClass string = new LoxString();

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
          if (!(args.get(0) instanceof Double)) {
            throw new RuntimeError(
                new Token(IDENTIFIER, "sin(..)"), "argument must be a number"
            );
          }
          return Math.sin((double)args.get(0));
        }
      };

  static final LoxCallable help =
      new OneArgCallable("help", new Parameter("object")) {
        @Override
        public Object call(Interpreter interpreter, List<Object> args) {
          Object arg = args.get(0);
          if (arg instanceof LoxCallable) {
            System.out.println(String.format(
                "{ %s } : %s", ((LoxCallable)arg).signature().toString(),
                arg.toString()
            ));

          } else if (arg instanceof Double) {
            System.out.println(arg.toString() + " : number");

          } else if (arg instanceof Boolean) {
            System.out.println(arg.toString() + " : boolean");

          } else if (arg instanceof LoxStringInstance) {
            System.out.println(String.format(
                "'%s' : %s", arg.toString(), ((LoxInstance)arg)._class.name
            ));

          } else if (arg instanceof LoxInstance) {
            System.out.println(String.format(
                "%s : %s", arg.toString(), ((LoxInstance)arg)._class.name
            ));
          }
          return null;
        }
      };

  // TODO: this is public because it needs to be accessed from the REPL (to
  // provide auto-completion)
  public static final Map<String, LoxCallable> members;
  static {
    members = new HashMap<>();
    // native classes
    members.put(list.signature().name, list);
    members.put(string.signature().name, string);
    // native functions
    members.put(clock.signature().name, clock);
    members.put(sin.signature().name, sin);
    members.put(help.signature().name, help);
  }
}
