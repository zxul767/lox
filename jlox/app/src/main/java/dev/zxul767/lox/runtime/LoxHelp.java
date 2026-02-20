package dev.zxul767.lox.runtime;

import java.util.List;

final class LoxHelp {
  private LoxHelp() {}

  static void print(Object value) {
    if (value instanceof LoxClass) {
      printClassHelp((LoxClass) value);
      return;
    }
    if (value instanceof LoxString) {
      printStringHelp((LoxString) value);
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

  private static String typeDescription(Object value) {
    if (value == null) return "nil";
    if (value instanceof Double) return "number";
    if (value instanceof Boolean) return "boolean";
    if (value instanceof LoxString) return "string";
    if (value instanceof LoxClass) return "class";
    if (value instanceof NativeBoundMethod) return "bound method";
    if (value instanceof NativeFunction) return "native function";
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

  private static void printMethodsOnly(LoxClass _class) {
    System.out.println("methods: " + countNonInitializerMethods(_class));
    var names = _class.methods.keySet().stream().sorted().toList();
    for (String name : names) {
      if (name.equals(LoxClass.INIT)) {
        continue;
      }
      printMethodEntry(_class.methods.get(name));
    }
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

    printMethodsOnly(_class);
  }

  private static void printStringHelp(LoxString value) {
    System.out.println("[string] <string instance>");
    System.out.println("length: " + value.string.length());
    printMethodsOnly(StandardLibrary.string);
  }

  private static void printInstanceHelp(LoxInstance instance) {
    String className = instance._class.name;
    System.out.println("[" + className + "] <" + className + " instance>");
    System.out.println("Use help(" + className + ") to inspect constructor and methods.");
  }

  private static void printValueHelp(Object value) {
    System.out.println(Interpreter.repr(value) + " [" + typeDescription(value) + "]");
  }
}
