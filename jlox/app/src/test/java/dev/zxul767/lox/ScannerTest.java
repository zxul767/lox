package dev.zxul767.lox.parsing;

import static org.hamcrest.MatcherAssert.assertThat;
import static org.hamcrest.Matchers.*;
import static org.junit.jupiter.api.Assertions.*;

import java.util.Arrays;
import java.util.List;
import java.util.stream.Collectors;
import org.junit.jupiter.api.Test;

class ScannerTest {
  private static List<Token> scan(String source) {
    Scanner scanner = new Scanner(source);
    return scanner.scanTokens(/*includeEOF:*/ false);
  }

  @Test
  void canTokenizeSimpleProgram() {
    List<String> expectedLexemes = Arrays.asList(
        "while", "(", "true", ")", "{", "var", "line", "=", "read", "(", ")",
        ";", "print", "(", "line", ")", ";", "}");

    List<Token> tokens =
        scan("while (true) { var line = read(); print(line); }");

    assertThatLexemesMatch(tokens, expectedLexemes);
  }

  @Test
  void shouldIgnoreSinglelineComments() {
    List<String> expectedLexemes = Arrays.asList("var", "line", "=", "10", ";");

    List<Token> tokens =
        scan("var line = 10; // this is a comment that will be ignored!");

    assertThatLexemesMatch(tokens, expectedLexemes);
  }

  @Test
  void shouldIgnoreMultilineComments() {
    List<String> expectedLexemes = Arrays.asList("var", "line", "=", "10", ";");

    List<Token> tokens = scan(
        "var line = 10; /* while (true) {\n var line = read();\n print(line);\n} */");

    assertThatLexemesMatch(tokens, expectedLexemes);
  }

  @Test
  void canProcessNestedMultilineComments() {
    List<String> expectedLexemes = Arrays.asList("var", "line", "=", "10", ";");

    List<Token> tokens = scan("/* nested /* comment */ */ var line = 10;");

    assertThatLexemesMatch(tokens, expectedLexemes);
  }

  @Test
  void canProcessDeeplyNestedMultilineComments() {
    List<String> expectedLexemes = Arrays.asList("var", "line", "=", "10", ";");

    List<Token> tokens =
        scan("/* nested /* 1 */ /* 2 /* 2.1 */ */ */ var line = 10;");

    assertThatLexemesMatch(tokens, expectedLexemes);
  }

  @Test
  void canProcessTokensBetweenComments() {
    List<String> expectedLexemes = Arrays.asList("var", "line", "=", "10", ";");

    List<Token> tokens =
        scan("/* nested /* comment */ */var line = 10;/* the // end */");

    assertThatLexemesMatch(tokens, expectedLexemes);
  }

  @Test
  void shouldErrorOnIncompleteNestedMultilineComments() {
    // TODO: assert on errors reported once we have an error interface
    List<Token> tokens = scan("/* /* */\nvar a = 10;");
    assertEquals(0, tokens.size());
  }

  @Test
  void canProcessNestedHybridComments() {
    List<Token> tokens = scan("/* \n nested /* // comment \n */ */");
    assertEquals(0, tokens.size());
  }

  @Test
  void tokensAfterMultilineCommentsHaveCorrectLineNumber() {
    List<Token> tokens = scan("/* comment\nstill in comment */ var a = 1;");
    assertEquals(2, tokens.get(0).line);
  }

  @Test
  void shouldIncludeEOFTokenByDefault() {
    Scanner scanner = new Scanner("");
    List<Token> tokens = scanner.scanTokens();

    assertEquals(1, tokens.size());
    assertEquals(TokenType.EOF, tokens.get(0).type);
  }

  private static void assertThatLexemesMatch(List<Token> tokens,
                                             List<String> expectedLexemes) {
    List<String> lexemes =
        tokens.stream().map(token -> token.lexeme).collect(Collectors.toList());

    assertThat(lexemes, is(expectedLexemes));
  }
}
