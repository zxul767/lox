package dev.zxul767.astgen;

import java.io.IOException;
import java.io.PrintWriter;
import java.util.Arrays;
import java.util.List;

public class GenerateAst {
  public static void main(String[] args) throws IOException {
    if (args.length != 1) {
      System.err.println("Usage: generate_ast <output directory>");
      System.exit(64);
    }
    String outputDir = args[0];
    defineAst(outputDir, "Expr",
              Arrays.asList("Binary   : Expr left, Token operator, Expr right",
                            "Grouping : Expr expression",
                            "Literal  : Object value",
                            "Unary    : Token operator, Expr right"));
  }

  private static void defineAst(String outputDir, String baseName,
                                List<String> types) throws IOException {
    String path = outputDir + "/" + baseName + ".java";
    PrintWriter writer = new PrintWriter(path, "UTF-8");

    writer.println("package dev.zxul767.lox;");
    writer.println();
    writer.println("import java.util.List;");
    writer.println();

    writer.println(String.format("abstract class %s {", baseName));

    defineVisitor(writer, baseName, types);

    // The base accept() method
    iprintln(writer, /*indentation:*/ 2,
             "abstract <R> R accept(Visitor<R> visitor);");

    // the AST classes
    writer.println();
    for (String type : types) {
      String[] parts = type.split(":");
      String className = parts[0].trim();
      String fields = parts[1].trim();
      defineType(writer, baseName, className, fields);
      writer.println();
    }

    writer.println("}");
    writer.close();
  }

  private static void defineVisitor(PrintWriter writer, String baseName,
                                    List<String> types) {
    iprintln(writer, /*indentation:*/ 2, "interface Visitor<R> {");
    for (String type : types) {
      String typeName = type.split(":")[0].trim();
      iprintln(writer, /*indentation:*/ 4,
               String.format("R visit%s(%s);", typeName + baseName,
                             typeName + " " + baseName.toLowerCase()));
    }
    iprintln(writer, /*indentation:*/ 2, "}");
  }

  private static void defineType(PrintWriter writer, String baseName,
                                 String className, String fieldList) {
    iprintln(
        writer, /*indentation:*/ 2,
        String.format("static class %s extends %s {", className, baseName));

    // constructor
    iprintln(writer, /*indentation:*/ 4,
             String.format("%s (%s) {", className, fieldList));

    // store parameters in fields
    String[] fields = fieldList.split(", ");
    for (String field : fields) {
      String name = field.split(" ")[1];
      iprintln(writer, /*indentation:*/ 6,
               String.format("this.%s = %s;", name, name));
    }
    iprintln(writer, /*indentation:*/ 4, "}");

    // visitor pattern
    writer.println();
    iprintln(writer, /*indentation:*/ 4, "@Override");
    iprintln(writer, /*indentation:*/ 4, "<R> R accept(Visitor<R> visitor) {");
    iprintln(
        writer, /*indentation:*/ 6,
        String.format("return visitor.visit%s(this);", className + baseName));
    iprintln(writer, /*indentation:*/ 4, "}");

    // fields
    for (String field : fields) {
      iprintln(writer, /*indentation:*/ 4, String.format("final %s;", field));
    }
    iprintln(writer, /*indentation:*/ 2, "}");
  }

  private static void iprintln(PrintWriter writer, int indentation,
                               String message) {
    indent(writer, indentation);
    writer.println(message);
  }

  private static void indent(PrintWriter writer, int spaces) {
    writer.print(" ".repeat(spaces));
  }
}
