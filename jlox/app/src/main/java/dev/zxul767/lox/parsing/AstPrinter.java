package dev.zxul767.lox.parsing;

import java.util.ArrayList;

class AstPrinter implements Expr.Visitor<String> {
  String print(Expr expr) { return expr.accept(this); }

  @Override
  public String visitBinaryExpr(Expr.Binary expr) {
    return parenthesize(expr.operator.lexeme, expr.left, expr.right);
  }

  @Override
  public String visitGroupingExpr(Expr.Grouping expr) {
    return parenthesize("group", expr.expression);
  }

  @Override
  public String visitLiteralExpr(Expr.Literal expr) {
    if (expr.value == null)
      return "nil";
    return expr.value.toString();
  }

  @Override
  public String visitLogicalExpr(Expr.Logical expr) {
    return parenthesize(expr.operator.lexeme, expr.left, expr.right);
  }

  @Override
  public String visitUnaryExpr(Expr.Unary expr) {
    return parenthesize(expr.operator.lexeme, expr.right);
  }

  @Override
  public String visitVariableExpr(Expr.Variable expr) {
    return expr.name.lexeme;
  }

  @Override
  public String visitAssignExpr(Expr.Assign expr) {
    return parenthesize("assign", new Expr.Variable(expr.name), expr.value);
  }

  @Override
  public String visitCallExpr(Expr.Call expr) {
    ArrayList<Expr> args = new ArrayList<>(expr.arguments);
    args.add(0, expr.callee);
    return parenthesize("funcall", args.toArray(new Expr[0]));
  }

  @Override
  public String visitGetExpr(Expr.Get expr) {
    return parenthesize("get", expr.object, new Expr.Variable(expr.name));
  }

  @Override
  public String visitSetExpr(Expr.Set expr) {
    return parenthesize("set", expr.object, new Expr.Variable(expr.name),
                        expr.value);
  }

  @Override
  public String visitThisExpr(Expr.This expr) {
    return parenthesize("get", new Expr.Variable(expr.keyword));
  }

  @Override
  public String visitSuperExpr(Expr.Super expr) {
    return parenthesize("super", new Expr.Variable(expr.method));
  }

  private String parenthesize(String name, Expr... exprs) {
    StringBuilder builder = new StringBuilder();

    builder.append("(").append(name);
    for (Expr expr : exprs) {
      builder.append(" ");
      builder.append(expr.accept(this));
    }
    builder.append(")");

    return builder.toString();
  }
}
