package dev.zxul767.lox;

import static org.hamcrest.CoreMatchers.*;
import static org.hamcrest.MatcherAssert.assertThat;
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
    // the last element is the EOF sentinel token
    tokens.remove(tokens.size() - 1);

    assertEquals(tokens.size(), expectedLexemes.size());

    List<String> lexemes =
        tokens.stream().map(token -> token.lexeme).collect(Collectors.toList());
    assertThat(lexemes, is(expectedLexemes));
  }
}
