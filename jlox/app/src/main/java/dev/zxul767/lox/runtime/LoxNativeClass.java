package dev.zxul767.lox.runtime;

import java.util.ArrayList;
import java.util.Collections;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.regex.Matcher;
import java.util.regex.Pattern;

class LoxNativeClass extends LoxClass {
  private final static String loxTypes = "(int|bool|list|str|any|nil)";

  private final static String signatureRegex = String.join(
      "",
      "(?<name>[A-Za-z_]+) \\s*",       // name
      "\\( (?<params> [^)]*) \\) \\s*", // parameters with optional type
      "(->\\s*(?<return>" + loxTypes + ") \\s*)?" // optional return type
  );

  private final static Pattern signaturePattern =
      Pattern.compile(signatureRegex, Pattern.COMMENTS);

  private final static Pattern paramPattern = Pattern.compile(
      "\\s*(?<name>[A-Za-z_]+)\\s*(:(?<type>" + loxTypes + "))?\\s*",
      Pattern.COMMENTS
  );

  LoxNativeClass(String name, Map<String, LoxCallable> methods) {
    super(name, /*superclass:*/ null, methods);
  }

  static Map<String, LoxCallable>
  createDunderMethods(Map<String, CallableSignature> signatures) {
    // `Map.of` provides nice "literal" syntax but always returns immutable
    // maps; hence why we copy it into a mutable one.
    return new HashMap<>(Map.of(
        "__init__",
        new NativeBoundMethod(
            signatures.get("__init__"), (self, args) -> { return self; }
        )
    ));
  }

  static Map<String, CallableSignature> createSignatures(String... args) {
    List<CallableSignature> members = parseSignatures(args);

    var signatures = new HashMap<String, CallableSignature>();
    for (CallableSignature signature : members) {
      signatures.put(signature.name, signature);
    }
    return signatures;
  }

  static List<CallableSignature> parseSignatures(String... texts) {
    var signatures = new ArrayList<CallableSignature>();
    for (String text : texts) {
      signatures.add(parseSignature(text));
    }
    return signatures;
  }

  static CallableSignature parseSignature(String text) {
    Matcher matcher = signaturePattern.matcher(text);
    boolean matchedSignature = matcher.matches();
    assert matchedSignature : String.format("invalid signature: %s", text);

    String name = matcher.group("name");
    assert name != null
        : String.format("name is mandatory in callable signature: %s", text);

    String params = matcher.group("params");
    String returnType = matcher.group("return");

    if (returnType == null) {
      returnType = "nil";
    }
    return new CallableSignature(name, parseParameters(params), returnType);
  }

  static List<Parameter> parseParameters(String text) {
    if (text == null)
      return Collections.emptyList();

    var result = new ArrayList<Parameter>();
    for (String param : text.split(",")) {
      if (param.length() == 0)
        continue;

      Matcher matcher = paramPattern.matcher(param);
      boolean matchedParam = matcher.matches();
      assert matchedParam : String.format("invalid parameter: %s", param);

      String name = matcher.group("name");
      String type = matcher.group("type");
      result.add(new Parameter(name, type));
    }
    return result;
  }

  static void throwRuntimeError(String lexeme, String message) {
    throw new RuntimeError(lexeme, message);
  }

  void define(
      String name, NativeMethod<Object, Object> nativeMethod,
      Map<String, CallableSignature> signatures
  ) {
    CallableSignature signature = signatures.getOrDefault(name, null);
    if (signature == null) {
      System.err.println(String.format(
          "ERROR: failed to register method: %s::%s", this.name, name
      ));
    } else {
      this.methods.put(
          signature.name, new NativeBoundMethod(signature, nativeMethod)
      );
    }
  }

  @Override
  public String toString() {
    return "<built-in class>";
  }
}
