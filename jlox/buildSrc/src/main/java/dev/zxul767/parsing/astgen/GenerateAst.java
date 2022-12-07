package dev.zxul767.parsing.astgen;

import java.io.IOException;
import java.io.PrintWriter;
import java.util.Arrays;
import java.util.List;

public class GenerateAst {
  IndentingWriter writer;

  GenerateAst(String path) throws IOException {
    this.writer = new IndentingWriter(new PrintWriter(path, "UTF-8"));
  }

  public static void main(String[] args) throws IOException {
    if (args.length != 1) {
      System.err.println("Usage: generate_ast <output directory>");
      System.exit(64);
    }
    String outputDir = args[0];
    /* clang-format off */
    generateASTFile(
        outputDir, "Expr",
        Arrays.asList(
            "Assign   : Token name, Expr value",
            "Binary   : Expr left, Token operator, Expr right",
            "Call     : Expr callee, Token paren, List<Expr> arguments",
            "Get      : Expr object, Token name",
            "Grouping : Expr expression",
            "Literal  : Object value",
            "Logical  : Expr left, Token operator, Expr right",
            "Set      : Expr object, Token name, Expr value",
            "Super    : Token keyword, Token method",
            "This     : Token keyword",
            "Unary    : Token operator, Expr right",
            "Variable : Token name"
        )
    );
    generateASTFile(
        outputDir, "Stmt",
        Arrays.asList(
            "Block       : List<Stmt> statements",
            "Class       : Token name, Expr.Variable superclass, List<Stmt.Function> methods",
            "Expression  : Expr expression",
            "Function    : Token name, List<Token> params, List<Stmt> body",
            "If          : Expr condition, Stmt thenBranch, Stmt elseBranch",
            "Print       : Expr expression, boolean includeNewline, boolean unquote",
            "Return      : Token keyword, Expr value",
            "Var         : Token name, Expr initializer",
            "While       : Expr condition, Stmt body"
        )
    );
    /* clang-format on */
  }

  private static void
  generateASTFile(String outputDir, String baseName, List<String> types)
      throws IOException {

    String path = String.format("%s/%s.java", outputDir, baseName);
    GenerateAst generator = new GenerateAst(path);
    generator.generateAST(baseName, types);
  }

  void generateAST(String baseName, List<String> types) {
    writeFileHeader();
    writeBaseClass(baseName, types);
    this.writer.close();
  }

  private void writeFileHeader() {
    this.writer.println("package dev.zxul767.lox.parsing;");
    this.writer.newline();
    this.writer.println("import java.util.List;");
    this.writer.newline();
  }

  private void writeBaseClass(String baseName, List<String> types) {
    // All AST classes (`Expr` and `Stmt` classes) are simple data structures
    // with no real behavior so it's okay for them (and their fields) to be
    // public.
    this.writer.println("public abstract class %s ", baseName);
    this.writer.openBlock();

    writeVisitorInterface(baseName, types);

    this.writer.println("public abstract <R> R accept(Visitor<R> visitor);");
    this.writer.newline();

    writeASTs(baseName, types);

    this.writer.closeBlock();
  }

  private void writeASTs(String baseName, List<String> types) {
    for (String type : types) {
      String[] parts = type.split(":");
      String className = parts[0].trim();
      String fieldList = parts[1].trim();

      this.writer.newline();
      writeAST(baseName, className, fieldList);
    }
  }

  private void writeAST(String baseName, String className, String fieldList) {
    this.writer.println(
        "public static class %s extends %s ", className, baseName
    );
    this.writer.openBlock();

    writeASTConstructor(className, fieldList);

    this.writer.newline();
    writeASTVisitorDispatch(className, baseName);

    this.writer.newline();
    writeASTFieldDeclarations(fieldList);

    this.writer.closeBlock();
  }

  private void writeASTConstructor(String className, String fieldList) {
    this.writer.println("public %s(%s) ", className, fieldList);
    this.writer.openBlock();

    String[] fields = fieldList.split(", ");
    for (String field : fields) {
      String name = field.split(" ")[1];
      this.writer.println("this.%s = %s;", name, name);
    }

    this.writer.closeBlock();
  }

  private void writeASTFieldDeclarations(String fieldList) {
    String[] fields = fieldList.split(", ");
    for (String field : fields) {
      this.writer.println("public final %s;", field);
    }
  }

  private void writeASTVisitorDispatch(String className, String baseName) {
    this.writer.println("@Override");
    this.writer.println("public <R> R accept(Visitor<R> visitor) ");
    this.writer.openBlock();

    this.writer.println("return visitor.visit%s(this);", className + baseName);

    this.writer.closeBlock();
  }

  private void writeVisitorInterface(String baseName, List<String> types) {
    this.writer.println("public interface Visitor<R> ");
    this.writer.openBlock();

    for (String type : types) {
      String typeName = type.split(":")[0].trim();
      this.writer.println(
          "public R visit%s(%s %s);", typeName + baseName, typeName,
          baseName.toLowerCase()
      );
    }

    this.writer.closeBlock();
  }
}

class IndentingWriter {
  private final PrintWriter writer;
  private final int INDENT_SIZE = 2;

  int indentation;

  IndentingWriter(PrintWriter writer) {
    this.writer = writer;
    this.indentation = 0;
  }

  void openBlock() {
    println("{");
    indent();
  }

  void closeBlock() {
    dedent();
    println("}");
  }

  void newline() { this.writer.println(); }

  void println(String s) {
    printIndentation();
    this.writer.println(s);
  }

  void println(String format, Object... args) {
    printIndentation();
    this.writer.printf(format, args);
    newline();
  }

  void close() { this.writer.close(); }

  private void indent() { this.indentation += INDENT_SIZE; }

  private void dedent() { this.indentation -= INDENT_SIZE; }

  private void printIndentation() {
    this.writer.print(" ".repeat(this.indentation));
  }
}
