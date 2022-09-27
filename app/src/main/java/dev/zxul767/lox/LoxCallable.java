package dev.zxul767.lox;

import java.util.List;

interface LoxCallable {
  int arity();
  Object call(Interpreter interpreter, List<Object> args);
}
