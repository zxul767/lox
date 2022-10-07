package dev.zxul767.lox;

import dev.zxul767.lox.parsing.Token;
import java.util.HashMap;
import java.util.Map;

class LoxInstance {
  private LoxClass _class;
  private final Map<String, Object> fields = new HashMap<>();

  LoxInstance(LoxClass _class) { this._class = _class; }

  Object get(Token name) {
    if (fields.containsKey(name.lexeme)) {
      return fields.get(name.lexeme);
    }
    LoxFunction method = _class.findMethod(name.lexeme);
    if (method != null) {
      return method.bind(this);
    }

    throw new RuntimeError(
        name, String.format("Undefined property '%s'.", name.lexeme));
  }

  void set(Token name, Object value) { fields.put(name.lexeme, value); }

  @Override
  public String toString() {
    return _class.name + " instance";
  }
}
