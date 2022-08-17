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
    // writer.println("abstract class " + baseName + " {");

    writer.println(String.format("abstract class %s {", baseName));

    // the AST classes
    for (String type : types) {
      String className = type.split(":")[0].trim();
      String fields = type.split(":")[1].trim();
      defineType(writer, baseName, className, fields);
      writer.println();
    }
    writer.println("}");
    writer.close();
  }

  private static void defineType(PrintWriter writer, String baseName,
                                 String className, String fieldList) {
    indent(writer, 2);
    writer.println(
        String.format("static class %s extends %s {", className, baseName));

    // constructor
    indent(writer, 4);
    writer.println(String.format("%s (%s) {", className, fieldList));

    // store parameters in fields
    String[] fields = fieldList.split(", ");
    for (String field : fields) {
      String name = field.split(" ")[1];
      indent(writer, 6);
      writer.println(String.format("this.%s = %s;", name, name));
    }
    indent(writer, 4);
    writer.println("}");

    // fields
    for (String field : fields) {
      indent(writer, 4);
      writer.println(String.format("final %s;", field));
    }
    indent(writer, 2);
    writer.println("}");
  }

  private static void indent(PrintWriter writer, int spaces) {
    writer.print(" ".repeat(spaces));
  }
}
