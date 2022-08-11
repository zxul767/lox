package dev.zxul767.lox;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.*;
import static org.junit.jupiter.api.Assertions.*;

import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;
import org.junit.jupiter.api.Test;

class LoxTest {
  @Test
  void canTokenizeSimpleProgram() {
    List<String> expectedLexemes = Arrays.asList(
        "while", "(", "true", ")", "{", "var", "line", "=", "read", "(", ")",
        ";", "print", "(", "line", ")", ";", "}");

    Scanner scanner =
        new Scanner("while (true) { var line = read(); print(line); }");
    List<Token> tokens = scanner.scanTokens();

    assertThatLexemesMatch(tokens, expectedLexemes);
  }

  @Test
  void shouldIgnoreSinglelineComments() {
    List<String> expectedLexemes = Arrays.asList("var", "line", "=", "10", ";");

    Scanner scanner = new Scanner(
        "var line = 10; // this is a comment that will be ignored!");
    List<Token> tokens = scanner.scanTokens();
  }

  @Test
  void shouldIgnoreMultilineComments() {
    List<String> expectedLexemes = Arrays.asList("var", "line", "=", "10", ";");

    Scanner scanner = new Scanner(
        "var line = 10; /* while (true) { var line = read(); print(line); } */");
    List<Token> tokens = scanner.scanTokens();

    assertThatLexemesMatch(tokens, expectedLexemes);
  }

  // TODO: add support for nested multiline comments
  @Test
  void cannotProcessNestedMultilineComments() {
    List<String> expectedLexemes = Arrays.asList("*", "/");

    Scanner scanner = new Scanner("/* nested /* comment */ */");
    List<Token> tokens = scanner.scanTokens();

    // with support for nested multiline comments, there should be no tokens
    assertThatLexemesMatch(tokens, expectedLexemes);
  }

  private static void assertThatLexemesMatch(List<Token> tokens,
                                             List<String> expectedLexemes) {
    // the last element is the EOF sentinel token
    tokens.remove(tokens.size() - 1);

    List<String> lexemes =
        tokens.stream().map(token -> token.lexeme).collect(Collectors.toList());

    assertThat(lexemes, is(expectedLexemes));
  }
}
