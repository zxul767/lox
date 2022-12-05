package dev.zxul767.lox.runtime;

import dev.zxul767.lox.Errors;
import dev.zxul767.lox.parsing.*;
import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

public class Interpreter implements Expr.Visitor<Object>, Stmt.Visitor<Void> {
  final Environment globals = new Environment();
  private Environment environment = globals;
  // maps every variable expression to the number of environment hops
  // needed to get to its declaring scope
  private final Map<Expr, Integer> locals = new HashMap<>();

  public Interpreter() {
    for (var entry : StandardLibrary.members.entrySet()) {
      globals.define(entry.getKey(), entry.getValue());
    }
  }

  public void interpret(List<Stmt> statements) {
    try {
      for (Stmt statement : statements) {
        execute(statement);
      }
    } catch (RuntimeError error) {
      Errors.runtimeError(error);
    }
  }
  public Object evaluate(Expr expression) { return expression.accept(this); }

  void execute(Stmt stmt) { stmt.accept(this); }

  void executeBlock(List<Stmt> statements, Environment environment) {
    Environment previous = this.environment;
    try {
      this.environment = environment;
      for (Stmt statement : statements) {
        execute(statement);
      }
    } finally {
      this.environment = previous;
    }
  }

  // Track how many environment hops (from the active environment) are
  // necessary to make to reach the environment were the variable contained
  // in `expr` was declared.
  void resolve(Expr expr, int hops) { locals.put(expr, hops); }

  @Override
  public Void visitBlockStmt(Stmt.Block stmt) {
    executeBlock(stmt.statements, new Environment(environment));
    return null;
  }

  @Override
  public Void visitClassStmt(Stmt.Class stmt) {
    Object superclass = null;
    if (stmt.superclass != null) {
      superclass = evaluate(stmt.superclass);
      if (!(superclass instanceof LoxClass)) {
        throw new RuntimeError(
            stmt.superclass.name, "Superclass must be a class."
        );
      }
    }
    environment.define(stmt.name.lexeme, null);

    if (stmt.superclass != null) {
      environment = new Environment(environment);
      environment.define("super", superclass);
    }

    Map<String, LoxCallable> methods = new HashMap<>();
    for (Stmt.Function method : stmt.methods) {
      LoxFunction function = new LoxFunction(
          method, environment,
          /* isInitializer: */ method.name.lexeme.equals(LoxClass.INIT)
      );
      methods.put(method.name.lexeme, function);
    }
    LoxClass _class =
        new LoxClass(stmt.name.lexeme, (LoxClass)superclass, methods);

    // The enviroment defined for "super" was transient and only needed
    // while defining the methods in the class (so their closures can
    // bind "super" to the superclass)
    if (superclass != null) {
      environment = environment.enclosing;
    }
    environment.assign(stmt.name, _class);

    return null;
  }

  @Override
  public Void visitExpressionStmt(Stmt.Expression stmt) {
    evaluate(stmt.expression);
    return null;
  }

  @Override
  public Void visitFunctionStmt(Stmt.Function stmt) {
    LoxFunction function =
        new LoxFunction(stmt, environment, /* isInitializer: */ false);
    environment.define(stmt.name.lexeme, function);
    return null;
  }

  @Override
  public Void visitIfStmt(Stmt.If stmt) {
    if (isTruthy(evaluate(stmt.condition))) {
      execute(stmt.thenBranch);
    } else if (stmt.elseBranch != null) {
      execute(stmt.elseBranch);
    }
    return null;
  }

  @Override
  public Void visitPrintStmt(Stmt.Print stmt) {
    Object value = evaluate(stmt.expression);
    System.out.print(stringify(value));
    if (stmt.includeNewline)
      System.out.println();

    return null;
  }

  @Override
  public Void visitReturnStmt(Stmt.Return stmt) {
    Object value = null;
    if (stmt.value != null)
      value = evaluate(stmt.value);
    throw new Return(value);
  }

  @Override
  public Void visitVarStmt(Stmt.Var stmt) {
    Object value = null;
    if (stmt.initializer != null) {
      value = evaluate(stmt.initializer);
    }
    environment.define(stmt.name.lexeme, value);
    return null;
  }

  @Override
  public Void visitWhileStmt(Stmt.While stmt) {
    while (isTruthy(evaluate(stmt.condition))) {
      execute(stmt.body);
    }
    return null;
  }

  @Override
  public Object visitAssignExpr(Expr.Assign expr) {
    Object value = evaluate(expr.value);

    Integer distance = locals.get(expr);
    if (distance != null) {
      environment.assignAt(distance, expr.name, value);
    } else {
      globals.assign(expr.name, value);
    }
    return value;
  }

  @Override
  public Object visitLiteralExpr(Expr.Literal expr) {
    if (expr.value instanceof String) {
      return new LoxString((String)expr.value);
    }
    return expr.value;
  }

  @Override
  public Object visitLogicalExpr(Expr.Logical expr) {
    Object left = evaluate(expr.left);
    if (expr.operator.type == TokenType.OR) {
      if (isTruthy(left))
        return left;
    } else /* TokenType.AND */ {
      if (!isTruthy(left))
        return left;
    }
    return evaluate(expr.right);
  }

  @Override
  public Object visitSetExpr(Expr.Set expr) {
    Object object = evaluate(expr.object);
    if (!(object instanceof LoxInstance)) {
      throw new RuntimeError(expr.name, "Only instances have fields.");
    }

    Object value = evaluate(expr.value);
    ((LoxInstance)object).set(expr.name, value);
    return value;
  }

  @Override
  public Object visitSuperExpr(Expr.Super expr) {
    int distance = locals.get(expr);
    LoxClass superclass = (LoxClass)environment.getAt(distance, "super");
    LoxInstance object = (LoxInstance)environment.getAt(distance - 1, "this");
    LoxCallable method = superclass.findMethod(expr.method.lexeme);

    if (method == null) {
      throw new RuntimeError(
          expr.method,
          String.format("Undefined property '%s'.", expr.method.lexeme)
      );
    }
    return method.bind(object);
  }

  @Override
  public Object visitThisExpr(Expr.This expr) {
    return lookupVariable(expr.keyword, expr);
  }

  @Override
  public Object visitUnaryExpr(Expr.Unary expr) {
    Object right = evaluate(expr.right);
    switch (expr.operator.type) {
    case BANG:
      return !isTruthy(right);
    case MINUS:
      checkNumberOperand(expr.operator, right);
      return -(double)right;
    }
    // unreachable
    return null;
  }

  @Override
  public Object visitVariableExpr(Expr.Variable expr) {
    return lookupVariable(expr.name, expr);
  }

  private Object lookupVariable(Token name, Expr expr) {
    Integer distance = locals.get(expr);
    if (distance != null) {
      return environment.getAt(distance, name.lexeme);
    } else {
      return globals.get(name);
    }
  }

  @Override
  public Object visitGroupingExpr(Expr.Grouping expr) {
    return evaluate(expr.expression);
  }

  @Override
  public Object visitBinaryExpr(Expr.Binary expr) {
    Object left = evaluate(expr.left);
    Object right = evaluate(expr.right);

    switch (expr.operator.type) {
    case GREATER:
      checkNumberOperands(expr.operator, left, right);
      return (double)left > (double)right;
    case GREATER_EQUAL:
      checkNumberOperands(expr.operator, left, right);
      return (double)left >= (double)right;
    case LESS:
      checkNumberOperands(expr.operator, left, right);
      return (double)left < (double)right;
    case LESS_EQUAL:
      checkNumberOperands(expr.operator, left, right);
      return (double)left <= (double)right;
    case MINUS:
      checkNumberOperands(expr.operator, left, right);
      return (double)left - (double)right;
    case PLUS:
      if (left instanceof Double && right instanceof Double) {
        return (double)left + (double)right;
      }
      if (left instanceof LoxString && right instanceof
                                                   LoxString) {
        return ((LoxString)left).concatenate((LoxString)right);
      }
      throw new RuntimeError(
          expr.operator, "Operands must be two numbers or two strings"
      );
    case SLASH:
      checkNumberOperands(expr.operator, left, right);
      return (double)left / (double)right;
    case STAR:
      checkNumberOperands(expr.operator, left, right);
      return (double)left * (double)right;
    case BANG_EQUAL:
      return !isEqual(left, right);
    case EQUAL_EQUAL:
      return isEqual(left, right);
    }
    // unreachable
    return null;
  }

  @Override
  public Object visitCallExpr(Expr.Call expr) {
    Object callee = evaluate(expr.callee);
    List<Object> args = new ArrayList<>();
    for (Expr arg : expr.arguments) {
      args.add(evaluate(arg));
    }
    if (!(callee instanceof LoxCallable)) {
      throw new RuntimeError(
          expr.paren, "Can only call functions and classes."
      );
    }
    LoxCallable function = (LoxCallable)callee;
    if (args.size() != function.signature().arity()) {
      throw new RuntimeError(
          expr.paren, String.format(
                          "Expected %d arguments but got %d.",
                          function.signature().arity(), args.size()
                      )
      );
    }
    return function.call(this, args);
  }

  @Override
  public Object visitGetExpr(Expr.Get expr) {
    Object object = evaluate(expr.object);
    if (object instanceof LoxInstance) {
      return ((LoxInstance)object).get(expr.name);
    }
    throw new RuntimeError(expr.name, "Only instances have properties.");
  }

  private void checkNumberOperand(Token operator, Object operand) {
    if (operand instanceof Double)
      return;
    throw new RuntimeError(operator, "Operand must be a number.");
  }

  private void checkNumberOperands(Token operator, Object left, Object right) {
    if (left instanceof Double && right instanceof Double)
      return;
    throw new RuntimeError(operator, "Operands must be numbers");
  }

  private boolean isTruthy(Object object) {
    if (object == null)
      return false;
    if (object instanceof Boolean)
      return (boolean)object;
    return true;
  }

  private boolean isEqual(Object a, Object b) {
    if (a == null && b == null)
      return true;
    if (a == null)
      return false;
    return a.equals(b);
  }

  public static String stringify(Object object) {
    if (object == null)
      return "nil";

    if (object instanceof Double) {
      String text = object.toString();
      if (text.endsWith(".0")) {
        text = text.substring(0, text.length() - 2);
      }
      return text;
    }
    if (object instanceof LoxString) {
      return String.format("'%s'", object.toString());
    }
    return object.toString();
  }

  public static String repr(Object object) {
    if (object instanceof LoxString) {
      return String.format("\"%s\"", object.toString());
    }
    return stringify(object);
  }
}
