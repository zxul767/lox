package dev.zxul767.lox.runtime;

import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.stream.Collectors;

interface LoxCallable {
  CallableSignature signature();

  Object call(Interpreter interpreter, List<Object> args);

  LoxCallable bind(LoxInstance instance);

  default String docstring() {
    return null;
  }
}

class CallableParameter {
  public final String name;
  public final String type;

  public CallableParameter(String name) {
    this(name, "any");
  }

  public CallableParameter(String name, String type) {
    assert name != null : "name is mandatory in CallableParameter";
    this.name = name;
    this.type = type != null ? type : "any";
  }

  @Override
  public String toString() {
    return String.format("%s:%s", this.name, this.type);
  }
}

class CallableSignature {
  public final String name;
  public final List<CallableParameter> parameters;
  public final String returnType;

  CallableSignature(String name) {
    this(name, Collections.emptyList());
  }

  CallableSignature(String name, String returnType) {
    this(name, Collections.emptyList(), returnType);
  }

  CallableSignature(String name, CallableParameter parameter) {
    this(name, Arrays.asList(parameter));
  }

  CallableSignature(String name, List<CallableParameter> parameters) {
    this(name, parameters, "any");
  }

  CallableSignature(String name, CallableParameter parameter, String returnType) {
    this(name, Arrays.asList(parameter), returnType);
  }

  CallableSignature(String name, List<CallableParameter> parameters, String returnType) {
    this.name = name;
    this.parameters = parameters;
    this.returnType = returnType;
  }

  public int arity() {
    return this.parameters.size();
  }

  CallableSignature withName(String name) {
    return new CallableSignature(name, this.parameters, this.returnType);
  }

  @Override
  public String toString() {
    String params =
        String.join(
            ", ", this.parameters.stream().map(p -> p.toString()).collect(Collectors.toList()));
    return String.format("%s(%s) -> %s", this.name, params, this.returnType);
  }
}
