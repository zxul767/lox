package dev.zxul767.lox;

import dev.zxul767.lox.parsing.Expr;
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
import java.util.ArrayList;
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
      /* clang-format off */
      System.out.println(""
          + "██╗      ██████╗ ██╗  ██╗    ██████╗ ███████╗██████╗ ██╗" + "\n"
          + "██║     ██╔═══██╗╚██╗██╔╝    ██╔══██╗██╔════╝██╔══██╗██║" + "\n"
          + "██║     ██║   ██║ ╚███╔╝     ██████╔╝█████╗  ██████╔╝██║" + "\n"
          + "██║     ██║   ██║ ██╔██╗     ██╔══██╗██╔══╝  ██╔═══╝ ██║" + "\n"
          + "███████╗╚██████╔╝██╔╝ ██╗    ██║  ██║███████╗██║     ███████╗" + "\n"
          + "╚══════╝ ╚═════╝ ╚═╝  ╚═╝    ╚═╝  ╚═╝╚══════╝╚═╝     ╚══════╝" + "\n"
      );
      // source: https://manytools.org/hacker-tools/ascii-banner/
      /* clang-format on */

      System.out.println("Welcome to the Lox REPL. Ready to hack?");
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

    // when running in REPL mode, sometimes there's a race condition
    // between the output and error streams, leading to misleading
    // output that confuses the user as it seems like the input prompt
    // has vanished:
    //
    // > var l = list();
    // > l.at(0);
    // Runtime Error: cannot access elements in empty list
    // > [line 1]
    //
    // The correct output should be:
    //
    // > var l = list();
    // > l.at(0);
    // Runtime Error: cannot access elements in empty list
    // [line 1]
    // >
    //
    // There's a reason why errors are logged to System.err, but
    // for the REPL, which is interactive, we can forego this:
    System.setErr(System.out);

    for (;;) {
      System.out.print(">>> ");
      System.out.flush();

      String line = reader.readLine();
      if (line == null || line.equals("quit"))
        break;

      // FIXME: implement the "optional semicolon" feature properly;
      // this is a brittle kludge to make working with the REPL a little
      // less annoying in the meantime...
      if (!line.endsWith(";") && !line.endsWith("}")) {
        line += ";";
      }
      run(line);

      // if the user makes a mistake, we don't kill the session
      Errors.reset();
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

    statements = ensureLastExpressionIsPrinted(statements);
    interpreter.interpret(statements);
  }

  static List<Stmt> ensureLastExpressionIsPrinted(List<Stmt> statements) {
    int n = statements.size() - 1;
    Stmt last = statements.get(n);

    if (last instanceof Stmt.Expression) {
      ArrayList<Stmt> patched = new ArrayList<>(statements);
      Expr expression = ((Stmt.Expression)last).expression;
      patched.set(n, new Stmt.Print(expression, /*includeNewline:*/ true));

      return patched;
    }
    return statements;
  }
}
