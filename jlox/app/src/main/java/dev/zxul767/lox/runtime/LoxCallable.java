package dev.zxul767.lox.runtime;

import java.util.ArrayList;
import java.util.Arrays;
import java.util.Collections;
import java.util.List;
import java.util.stream.Collectors;

interface LoxCallable {
  CallableSignature signature();
  Object call(Interpreter interpreter, List<Object> args);
  LoxCallable bind(LoxInstance instance);
}

class Parameter {
  public final String name;
  public final String type;

  public Parameter(String name) { this(name, "any"); }

  public Parameter(String name, String type) {
    assert name != null : "name is mandatory in Parameter";
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
  public final List<Parameter> parameters;
  public final String returnType;

  CallableSignature(String name) { this(name, Collections.emptyList()); }

  CallableSignature(String name, String returnType) {
    this(name, Collections.emptyList(), returnType);
  }

  CallableSignature(String name, Parameter parameter) {
    this(name, Arrays.asList(parameter));
  }

  CallableSignature(String name, List<Parameter> parameters) {
    this(name, parameters, "any");
  }

  CallableSignature(String name, Parameter parameter, String returnType) {
    this(name, Arrays.asList(parameter), returnType);
  }

  CallableSignature(
      String name, List<Parameter> parameters, String returnType
  ) {
    this.name = name;
    this.parameters = parameters;
    this.returnType = returnType;
  }

  public int arity() { return this.parameters.size(); }

  CallableSignature withName(String name) {
    return new CallableSignature(name, this.parameters, this.returnType);
  }

  @Override
  public String toString() {
    String params = String.join(
        ",", this.parameters.stream()
                 .map(p -> p.toString())
                 .collect(Collectors.toList())
    );
    return String.format("%s(%s) -> %s", this.name, params, this.returnType);
  }
}
