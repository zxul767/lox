package dev.zxul767.lox.runtime;

import static dev.zxul767.lox.parsing.TokenType.*;

import dev.zxul767.lox.Errors;
import dev.zxul767.lox.parsing.Expr;
import dev.zxul767.lox.parsing.Stmt;
import dev.zxul767.lox.parsing.Token;
import java.util.HashMap;
import java.util.List;
import java.util.Map;
import java.util.Stack;

// `Resolver`'s main task is to ensure the interpreter can resolve
// each variable's reference to its correct lexical scope declaration.
// This is done by keeping track of the number of scope (environment)
// hops needed to go from a variable's reference to its declaring scope.
//
// We also piggyback error reporting for simple situations such as illegal
// variable shadowing, redeclaration of local variables, or `return`
// statements outside function bodies. This is not the best in terms
// of separation of concerns, but traversing the AST is costly so we
// try to avoid multiple passes.
//
// That said, if we ever add something more complicated (or if this
// class simply becomes unwieldy due to all the minor secondary tasks),
// it will make sense to partition it into another class that performs
// an additional traversal.
//
public class Resolver implements Expr.Visitor<Void>, Stmt.Visitor<Void> {
  private enum VarResolution { DECLARED, DEFINED }
  private enum FunctionType { NONE, FUNCTION, INITIALIZER, METHOD }
  private enum ClassType { NONE, CLASS, SUBCLASS }

  // We need the interpreter to communicate to it information about the
  // declaring scopes for each variable reference in the AST.
  private final Interpreter interpreter;
  // NOTE: `scopes` mirrors the environments that will be created at runtime
  // (see `Interpreter`), so it's important to keep these two classes in sync
  // or else you'll get "amusing" errors that may be hard to track down.
  private final Stack<Map<String, VarResolution>> scopes = new Stack<>();

  // This is needed to detect illegal `return` statements (i.e., those
  // outside of a function body). In theory, one might disallow them in
  // the grammar (i.e., in the `Parser` class), but there's really no
  // way to keep a context-free grammar if we do that because `for`
  // statements (and others) can appear both in top-level and in function
  // body contexts.
  private FunctionType currentFunction = FunctionType.NONE;
  // Something similar to `return` happens with `this`, so we use this
  // resolver to detect invalid uses as well.
  private ClassType currentClass = ClassType.NONE;

  public Resolver(Interpreter interpreter) { this.interpreter = interpreter; }

  public void resolve(List<Stmt> statements) {
    for (Stmt statement : statements) {
      resolve(statement);
    }
  }
  private void resolve(Stmt stmt) { stmt.accept(this); }
  private void resolve(Expr expr) { expr.accept(this); }

  // This is the only function that actually "resolves" things
  // all other "resolve" functions simply traverse the AST or
  // add declaration scopes to mirror the runtime environments
  private void resolveLocal(Expr expr, Token name) {
    for (int i = scopes.size() - 1; i >= 0; i--) {
      if (scopes.get(i).containsKey(name.lexeme)) {
        // Write down the number of scope (environment) hops needed
        // to find the declaring scope of the variable in `expr`
        interpreter.resolve(expr, scopes.size() - 1 - i);
        return;
      }
    }
  }

  private void resolveFunction(Stmt.Function function,
                               FunctionType functionType) {
    FunctionType enclosingFunction = currentFunction;
    currentFunction = functionType;

    beginScope();
    for (Token param : function.params) {
      declareAndDefine(param);
    }
    resolve(function.body);
    endScope();

    currentFunction = enclosingFunction;
  }

  private void beginScope() {
    scopes.push(new HashMap<String, VarResolution>());
  }

  private void endScope() { scopes.pop(); }

  private void declare(Token name) {
    if (scopes.isEmpty())
      return;
    Map<String, VarResolution> scope = scopes.peek();
    if (scope.containsKey(name.lexeme)) {
      Errors.error(name, "Already a variable with this name in this scope.");
    }
    scope.put(name.lexeme, VarResolution.DECLARED);
  }

  private void define(Token name) {
    if (scopes.isEmpty())
      return;
    scopes.peek().put(name.lexeme, VarResolution.DEFINED);
  }

  private void declareAndDefine(Token name) {
    declare(name);
    define(name);
  }

  @Override
  public Void visitBlockStmt(Stmt.Block stmt) {
    beginScope();
    resolve(stmt.statements);
    endScope();
    return null;
  }

  @Override
  public Void visitClassStmt(Stmt.Class stmt) {
    if (stmt.superclass != null &&
        stmt.name.lexeme.equals(stmt.superclass.name.lexeme)) {
      Errors.error(stmt.superclass.name, "A class can't inherit from itself.");
    }

    ClassType enclosingClass = currentClass;

    // It is valid to refer to the class name inside its definition,
    // so we'll pretend that it is defined at this point.
    declareAndDefine(stmt.name);

    if (stmt.superclass != null) {
      // This doesn't usually do anything useful (since classes are usually
      // declared at the top level). However, classes can be declared even
      // inside blocks, so it's possible the superclass name refers to a local
      // variable.
      // TODO: do we really want/need to allow classes to be defined locally?
      currentClass = ClassType.SUBCLASS;
      resolve(stmt.superclass);

      // This scope ensures that "super.method(...)" references are resolved
      // by starting the lookup on the superclass containing the "super"
      // expression (and not on the superclass of the "this" instance)
      beginScope();
      define(new Token(SUPER, "super"));
      // Consider the following program to see why this is necessary:
      //
      // class A {
      //   method() {
      //     print "A method";
      //   }
      // }
      // class B < A {
      //   method() {
      //     print "B method";
      //   }
      //   test() {
      //     super.method();
      //   }
      // }
      // class C < B {}
      // C().test();
      //
      // Without the additional environment, resolving "super" could only be
      // done with information available to C's instance, which would point
      // to B. However, our expectation is that "super" would resolve to A
      // instead (because "super" has lexical-scope resolution semantics.)
      //
      // See `Interpreter.visitSuperExpr` for more details.
    }
    currentClass = ClassType.CLASS;
    // "this" (the instance) only needs to be declared (but not defined,
    // since its definition is implicit in the construction of the instance)
    beginScope();
    declare(new Token(THIS, "this"));
    for (Stmt.Function method : stmt.methods) {
      FunctionType declaration = FunctionType.METHOD;
      if (method.name.lexeme.equals(LoxClass.INIT)) {
        declaration = FunctionType.INITIALIZER;
      }
      resolveFunction(method, declaration);
    }
    endScope();

    if (stmt.superclass != null) {
      // ends the scope associated with the "super" keyword
      endScope();
    }

    currentClass = enclosingClass;
    return null;
  }

  @Override
  public Void visitExpressionStmt(Stmt.Expression stmt) {
    resolve(stmt.expression);
    return null;
  }

  @Override
  public Void visitFunctionStmt(Stmt.Function stmt) {
    declareAndDefine(stmt.name);
    resolveFunction(stmt, FunctionType.FUNCTION);
    return null;
  }

  @Override
  public Void visitIfStmt(Stmt.If stmt) {
    resolve(stmt.condition);
    resolve(stmt.thenBranch);
    if (stmt.elseBranch != null) {
      resolve(stmt.elseBranch);
    }
    return null;
  }

  @Override
  public Void visitPrintStmt(Stmt.Print stmt) {
    resolve(stmt.expression);
    return null;
  }

  @Override
  public Void visitReturnStmt(Stmt.Return stmt) {
    if (currentFunction == FunctionType.NONE) {
      Errors.error(stmt.keyword, "Can't return from top-level code.");
    }
    if (stmt.value != null) {
      if (currentFunction == FunctionType.INITIALIZER) {
        Errors.error(stmt.keyword, "Can't return a value from an initializer.");
      }
      resolve(stmt.value);
    }
    return null;
  }

  @Override
  public Void visitVarStmt(Stmt.Var stmt) {
    // We declare the variable's existence (but not its definition yet)
    // so we can detect illegal shadowing (i.e., var a = 1; { var a = a; })
    // (see: `visitVariableExpr` in this file)
    declare(stmt.name);
    if (stmt.initializer != null) {
      resolve(stmt.initializer);
    }
    define(stmt.name);
    return null;
  }

  @Override
  public Void visitWhileStmt(Stmt.While stmt) {
    resolve(stmt.condition);
    resolve(stmt.body);
    return null;
  }

  @Override
  public Void visitAssignExpr(Expr.Assign expr) {
    resolve(expr.value);
    resolveLocal(expr, expr.name);
    return null;
  }

  @Override
  public Void visitBinaryExpr(Expr.Binary expr) {
    resolve(expr.left);
    resolve(expr.right);
    return null;
  }

  @Override
  public Void visitCallExpr(Expr.Call expr) {
    resolve(expr.callee);
    for (Expr argument : expr.arguments) {
      resolve(argument);
    }
    return null;
  }

  @Override
  public Void visitGetExpr(Expr.Get expr) {
    resolve(expr.object);
    return null;
  }

  @Override
  public Void visitGroupingExpr(Expr.Grouping expr) {
    resolve(expr.expression);
    return null;
  }

  @Override
  public Void visitLiteralExpr(Expr.Literal expr) {
    return null;
  }

  @Override
  public Void visitLogicalExpr(Expr.Logical expr) {
    resolve(expr.left);
    resolve(expr.right);
    return null;
  }

  @Override
  public Void visitSetExpr(Expr.Set expr) {
    resolve(expr.value);
    resolve(expr.object);
    return null;
  }

  @Override
  public Void visitSuperExpr(Expr.Super expr) {
    if (currentClass == ClassType.NONE) {
      Errors.error(expr.keyword, "Can't use 'super' outside of class.");
    } else if (currentClass != ClassType.SUBCLASS) {
      Errors.error(
          expr.keyword, "Can't use 'super' in a class with no superclass."
      );
    }
    resolveLocal(expr, expr.keyword);
    return null;
  }

  @Override
  public Void visitThisExpr(Expr.This expr) {
    if (currentClass == ClassType.NONE) {
      Errors.error(expr.keyword, "Can't use 'this' outside of a class.");
    } else {
      resolveLocal(expr, expr.keyword);
    }
    return null;
  }

  @Override
  public Void visitUnaryExpr(Expr.Unary expr) {
    resolve(expr.right);
    return null;
  }

  @Override
  public Void visitVariableExpr(Expr.Variable expr) {
    if (isDeclaredButUninitialized(expr)) {
      Errors.error(
          expr.name, "Can't read local variable in its own initializer."
      );
    }
    resolveLocal(expr, expr.name);
    return null;
  }

  private boolean isDeclaredButUninitialized(Expr.Variable expr) {
    return !scopes.isEmpty() &&
        scopes.peek().get(expr.name.lexeme) == VarResolution.DECLARED;
  }
}
