package dev.zxul767.lox;

import java.util.ArrayList;

// RPN stands for Reverse Polish Notation
// See https://en.wikipedia.org/wiki/Reverse_Polish_notation
//
class RPNPrinter implements Expr.Visitor<String> {
  String print(Expr expr) { return expr.accept(this); }

  @Override
  public String visitBinaryExpr(Expr.Binary expr) {
    return toRPN(expr.operator.lexeme, expr.left, expr.right);
  }

  @Override
  public String visitGroupingExpr(Expr.Grouping expr) {
    return toRPN("", expr.expression);
  }

  @Override
  public String visitLiteralExpr(Expr.Literal expr) {
    if (expr.value == null)
      return "nil";
    return expr.value.toString();
  }

  @Override
  public String visitLogicalExpr(Expr.Logical expr) {
    return toRPN(expr.operator.lexeme, expr.left, expr.right);
  }

  @Override
  public String visitUnaryExpr(Expr.Unary expr) {
    return toRPN(expr.operator.lexeme, expr.right);
  }

  @Override
  public String visitVariableExpr(Expr.Variable expr) {
    return expr.name.lexeme;
  }

  @Override
  public String visitAssignExpr(Expr.Assign expr) {
    return toRPN("assign", new Expr.Variable(expr.name), expr.value);
  }

  @Override
  public String visitCallExpr(Expr.Call expr) {
    ArrayList<Expr> args = new ArrayList<>(expr.arguments);
    args.add(0, expr.callee);
    return toRPN("funcall", args.toArray(new Expr[0]));
  }

  @Override
  public String visitGetExpr(Expr.Get expr) {
    return toRPN("get", expr.object, new Expr.Variable(expr.name));
  }

  @Override
  public String visitSetExpr(Expr.Set expr) {
    return toRPN("set", expr.object, new Expr.Variable(expr.name), expr.value);
  }

  @Override
  public String visitThisExpr(Expr.This expr) {
    return toRPN("get", new Expr.Variable(expr.keyword), expr);
  }

  private String toRPN(String operator, Expr... operands) {
    StringBuilder builder = new StringBuilder();
    for (Expr operand : operands) {
      builder.append(operand.accept(this));
      builder.append(" ");
    }
    builder.append(operator);
    return builder.toString();
  }
}
