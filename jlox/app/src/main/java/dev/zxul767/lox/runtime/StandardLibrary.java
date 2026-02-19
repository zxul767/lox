package dev.zxul767.lox.runtime;

import static dev.zxul767.lox.parsing.TokenType.*;

import java.util.HashMap;
import java.util.List;
import java.util.Map;

@FunctionalInterface
interface NativeCallable {
  // this interface is used for both functions and methods; functions simply pass `self` as `null`
  Object invoke(Interpreter interpreter, LoxInstance self, List<Object> args);
}

abstract class AbstractNativeCallable implements LoxCallable {
  protected final CallableSignature signature;
  protected final String docstring;
  protected final NativeCallable nativeCallable;

  AbstractNativeCallable(
      CallableSignature signature, String docstring, NativeCallable nativeCallable) {
    this.signature = signature;
    this.docstring = docstring;
    this.nativeCallable = nativeCallable;
  }

  @Override
  public CallableSignature signature() {
    return this.signature;
  }

  @Override
  public String docstring() {
    return this.docstring;
  }
}

class NativeFunction extends AbstractNativeCallable {
  NativeFunction(CallableSignature signature, NativeCallable nativeFunction, String docstring) {
    super(signature, docstring, nativeFunction);
  }

  @Override
  public Object call(Interpreter interpreter, List<Object> args) {
    return this.nativeCallable.invoke(interpreter, /* self: */ null, args);
  }

  @Override
  public LoxCallable bind(LoxInstance instance) {
    throw new RuntimeError(this.signature.name, "Cannot bind native function");
  }

  @Override
  public String toString() {
    return String.format("<built-in function>");
  }
}

class NativeBoundMethod extends AbstractNativeCallable {
  private LoxInstance instance;

  NativeBoundMethod(CallableSignature signature, NativeCallable nativeMethod) {
    this(signature, nativeMethod, /* instance: */ null, /* docstring: */ null);
  }

  NativeBoundMethod(CallableSignature signature, NativeCallable nativeMethod, String docstring) {
    this(signature, nativeMethod, /* instance: */ null, /* docstring: */ docstring);
  }

  private NativeBoundMethod(
      CallableSignature signature,
      NativeCallable nativeMethod,
      LoxInstance instance,
      String docstring) {
    super(signature, docstring, nativeMethod);
    this.instance = instance;
  }

  @Override
  public Object call(Interpreter interpreter, List<Object> args) {
    if (this.instance == null) {
      throw new RuntimeError(this.signature.name, "Cannot invoke unbound method.");
    }
    return this.nativeCallable.invoke(interpreter, this.instance, args);
  }

  @Override
  public LoxCallable bind(LoxInstance instance) {
    return new NativeBoundMethod(this.signature, this.nativeCallable, instance, this.docstring);
  }

  @Override
  public String toString() {
    return String.format("<built-in method>");
  }
}

public final class StandardLibrary {
  private StandardLibrary() {}

  static final LoxClass list = new LoxListClass();
  static final LoxClass string = new LoxStringClass();
  static final List<LoxNativeClass.NativeCallableSpec> nativeFunctions = createNativeFunctions();

  private static List<LoxNativeClass.NativeCallableSpec> createNativeFunctions() {
    return List.of(
        LoxNativeClass.nativeFunction(
            "clock   ()            -> number",
            "Returns a Unix timestamp in seconds.",
            (interpreter, self, args) -> (double) System.currentTimeMillis() / 1000.0),
        LoxNativeClass.nativeFunction(
            "sin     (x:number)    -> number",
            "Returns the value of the sine function for x.",
            (interpreter, self, args) -> {
              double value = LoxNativeClass.requireDouble(args.get(0), "sin");
              return Math.sin(value);
            }),
        LoxNativeClass.nativeFunction(
            "print   (value:any)   -> nil",
            "Prints a value without a trailing newline.",
            (interpreter, self, args) -> {
              System.out.print(Interpreter.stringify(args.get(0)));
              return null;
            }),
        LoxNativeClass.nativeFunction(
            "println (value:any)   -> nil",
            "Prints a value followed by a newline.",
            (interpreter, self, args) -> {
              System.out.println(Interpreter.stringify(args.get(0)));
              return null;
            }),
        LoxNativeClass.nativeFunction(
            "help    (object:any)  -> nil",
            "Shows details for values, and signature/docs for callables when available.",
            (interpreter, self, args) -> {
              LoxHelp.print(args.get(0));
              return null;
            }));
  }

  // NOTE: this is public because it needs to be accessed from the REPL
  // (to provide auto-completion)
  public static final Map<String, LoxCallable> members;

  static {
    members = new HashMap<>();
    members.put(list.signature().name, list);
    members.put(string.signature().name, string);
    members.putAll(LoxNativeClass.bindFunctions(nativeFunctions));
  }
}
