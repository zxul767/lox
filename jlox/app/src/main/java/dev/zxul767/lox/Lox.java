package dev.zxul767.lox;

import dev.zxul767.lox.parsing.Parser;
import dev.zxul767.lox.parsing.Scanner;
import dev.zxul767.lox.parsing.Stmt;
import dev.zxul767.lox.parsing.Token;
import dev.zxul767.lox.runtime.Interpreter;
import dev.zxul767.lox.runtime.Resolver;
import java.io.BufferedReader;
import java.io.IOException;
import java.io.InputStreamReader;
import java.nio.charset.Charset;
import java.nio.file.Files;
import java.nio.file.Paths;
import java.util.List;

public class Lox {
  private static final Interpreter interpreter = new Interpreter();

  public static void main(String[] args) throws IOException {
    if (args.length > 1) {
      System.out.println("Usage jlox [script]");
      System.exit(64);
    } else if (args.length == 1) {
      runFile(args[0]);
    } else {
      System.out.println("Running lox repl...");
      runPrompt();
    }
  }

  private static void runFile(String path) throws IOException {
    byte[] bytes = Files.readAllBytes(Paths.get(path));
    run(new String(bytes, Charset.defaultCharset()));
    if (Errors.hadError)
      System.exit(65);
    if (Errors.hadRuntimeError)
      System.exit(70);
  }

  private static void runPrompt() throws IOException {
    InputStreamReader input = new InputStreamReader(System.in);
    BufferedReader reader = new BufferedReader(input);

    for (;;) {
      System.out.print("> ");
      String line = reader.readLine();
      if (line == null)
        break;
      run(line);
      // if the user makes a mistake, we shouldn't kill the entire
      // session
      Errors.hadError = false;
    }
  }

  private static void run(String source) {
    Scanner scanner = new Scanner(source);
    List<Token> tokens = scanner.scanTokens();
    Parser parser = new Parser(tokens);
    List<Stmt> statements = parser.parse();

    // stop if there was a syntax error
    if (Errors.hadError)
      return;

    Resolver resolver = new Resolver(interpreter);
    resolver.resolve(statements);

    // stop if there was a resolution error
    if (Errors.hadError)
      return;

    interpreter.interpret(statements);
  }
}
