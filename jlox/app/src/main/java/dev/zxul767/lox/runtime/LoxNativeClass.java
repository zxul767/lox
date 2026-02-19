package dev.zxul767.lox.runtime;

import java.util.ArrayList;
import java.util.Collections;
import java.util.LinkedHashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

class LoxNativeClass extends LoxClass {
  private static final String loxTypes = "(int|number|bool|list|str|any|nil)";

  private static final String signatureRegex =
      String.join(
          "",
          "(?<name>[A-Za-z_]+) \\s*", // name
          "\\( (?<params> [^)]*) \\) \\s*", // parameters
          "(->\\s*(?<return>" + loxTypes + ") \\s*)?" // optional return type
          );

  private static final Pattern signaturePattern = Pattern.compile(signatureRegex, Pattern.COMMENTS);

  private static final String paramRegex =
      String.join(
          "",
          "\\s*(?<name>[A-Za-z_]+)\\s*", // parameter name
          "(:(?<type>" + loxTypes + "))?\\s*" // optional type
          );

  private static final Pattern paramPattern = Pattern.compile(paramRegex, Pattern.COMMENTS);

  LoxNativeClass(String name, Map<String, LoxCallable> methods) {
    super(name, /*superclass:*/ null, methods);
  }

  static final class NativeCallableSpec {
    final CallableSignature signature;
    final String docstring;
    final NativeCallable nativeCallable;

    private NativeCallableSpec(
        CallableSignature signature, String docstring, NativeCallable nativeCallable) {
      this.signature = signature;
      this.docstring = docstring;
      this.nativeCallable = nativeCallable;
    }

    static NativeCallableSpec create(
        CallableSignature signature, String docstring, NativeCallable nativeMethod) {
      return new NativeCallableSpec(signature, docstring, nativeMethod);
    }
  }

  static NativeCallableSpec nativeMethod(
      String signatureText, String docstring, NativeCallable nativeMethod) {
    CallableSignature signature = parseSignature(signatureText);
    String resolvedDocstring = docstring;
    if (resolvedDocstring == null && signature.name.equals(LoxClass.INIT)) {
      resolvedDocstring = String.format("Creates an instance of class %s.", signature.returnType);
    }
    return NativeCallableSpec.create(signature, resolvedDocstring, nativeMethod);
  }

  static NativeCallableSpec nativeFunction(
      String signatureText, String docstring, NativeCallable nativeFunction) {
    return NativeCallableSpec.create(parseSignature(signatureText), docstring, nativeFunction);
  }

  static Map<String, LoxCallable> bindMethods(List<NativeCallableSpec> specs) {
    var methods = new LinkedHashMap<String, LoxCallable>();
    for (NativeCallableSpec spec : specs) {
      methods.put(
          spec.signature.name,
          new NativeBoundMethod(spec.signature, spec.nativeCallable, spec.docstring));
    }
    return methods;
  }

  static Map<String, LoxCallable> bindFunctions(List<NativeCallableSpec> specs) {
    var functions = new LinkedHashMap<String, LoxCallable>();
    for (NativeCallableSpec spec : specs) {
      functions.put(
          spec.signature.name,
          new NativeFunction(spec.signature, spec.nativeCallable, spec.docstring));
    }
    return functions;
  }

  static void throwRuntimeError(String token, String message) {
    throw new RuntimeError(token, message);
  }

  private static CallableSignature parseSignature(String text) {
    Matcher matcher = signaturePattern.matcher(text);
    boolean matchedSignature = matcher.matches();
    assert matchedSignature : String.format("invalid signature: %s", text);

    String name = matcher.group("name");
    assert name != null : String.format("name is mandatory in callable signature: %s", text);

    String params = matcher.group("params");
    String returnType = matcher.group("return");

    if (returnType == null) {
      returnType = "nil";
    }
    return new CallableSignature(name, parseParameters(params), returnType);
  }

  private static List<CallableParameter> parseParameters(String text) {
    if (text == null) {
      return Collections.emptyList();
    }
    var result = new ArrayList<CallableParameter>();
    for (String param : text.split(",")) {
      if (param.length() == 0) continue;

      Matcher matcher = paramPattern.matcher(param);
      boolean matchedParam = matcher.matches();
      assert matchedParam : String.format("invalid parameter: %s", param);

      result.add(new CallableParameter(matcher.group("name"), matcher.group("type")));
    }
    return result;
  }

  private static <T> T assertType(LoxInstance instance, Class<T> type) {
    assert (type.isInstance(instance)) : "unexpected instance type";
    return type.cast(instance);
  }

  static LoxString assertString(LoxInstance instance) {
    return assertType(instance, LoxString.class);
  }

  static LoxList assertList(LoxInstance instance) {
    return assertType(instance, LoxList.class);
  }

  private static <T> T requireType(
      Object value, Class<T> type, String functionName, String loxTypeName) {
    if (!type.isInstance(value)) {
      throwRuntimeError(functionName, String.format("argument must be of type '%s'", loxTypeName));
    }
    return type.cast(value);
  }

  static LoxString requireString(Object value, String functionName) {
    return requireType(value, LoxString.class, functionName, "string");
  }

  static double requireDouble(Object value, String functionName) {
    return requireType(value, Double.class, functionName, "number");
  }

  static int requireInt(Object value, String functionName) {
    double number = requireType(value, Double.class, functionName, "int");
    if (number != Math.floor(number)) {
      throwRuntimeError(functionName, "argument must be an integer");
    }
    return (int) number;
  }

  @Override
  public String toString() {
    return "<built-in class>";
  }
}
