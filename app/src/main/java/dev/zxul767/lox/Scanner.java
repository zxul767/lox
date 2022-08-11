package dev.zxul767.lox;

import static dev.zxul767.lox.TokenType.*;

import java.util.ArrayList;
import java.util.HashMap;
import java.util.List;
import java.util.Map;

class Scanner {
  private static final Map<String, TokenType> keywords;
  static {
    keywords = new HashMap<>();
    keywords.put("and", AND);
    keywords.put("class", CLASS);
    keywords.put("else", ELSE);
    keywords.put("false", FALSE);
    keywords.put("for", FOR);
    keywords.put("fun", FUN);
    keywords.put("if", IF);
    keywords.put("nil", NIL);
    keywords.put("or", OR);
    keywords.put("print", PRINT);
    keywords.put("return", RETURN);
    keywords.put("super", SUPER);
    keywords.put("this", THIS);
    keywords.put("true", TRUE);
    keywords.put("var", VAR);
    keywords.put("while", WHILE);
  }

  private final String sourceCode;
  private final List<Token> tokens = new ArrayList<>();
  // `start` & `current` are meant to index `sourceCode`
  private int start = 0;
  private int current = 0;
  // `line` starts at 1 (and not 0) to be user friendly
  private int line = 1;

  Scanner(String sourceCode) { this.sourceCode = sourceCode; }

  List<Token> scanTokens() {
    while (!isAtEnd()) {
      start = current;
      scanToken();
    }
    tokens.add(new Token(EOF, /* lexeme: */ "", /* value: */ null, line));
    return tokens;
  }

  private void scanToken() {
    char c = advance();
    switch (c) {
    case '(':
      addToken(LEFT_PAREN);
      break;
    case ')':
      addToken(RIGHT_PAREN);
      break;
    case '{':
      addToken(LEFT_BRACE);
      break;
    case '}':
      addToken(RIGHT_BRACE);
      break;
    case ',':
      addToken(COMMA);
      break;
    case '.':
      addToken(DOT);
      break;
    case '-':
      addToken(MINUS);
      break;
    case '+':
      addToken(PLUS);
      break;
    case ';':
      addToken(SEMICOLON);
      break;
    case '*':
      addToken(STAR);
      break;

    case '!':
      addToken(match('=') ? BANG_EQUAL : BANG);
      break;
    case '=':
      addToken(match('=') ? EQUAL_EQUAL : EQUAL);
      break;
    case '<':
      addToken(match('=') ? LESS_EQUAL : LESS);
      break;
    case '>':
      addToken(match('=') ? GREATER_EQUAL : GREATER);
      break;
    case '/':
      if (match('/')) {
        singleLineComment();
      } else {
        addToken(SLASH);
      }
      break;

    case ' ':
    case '\r':
    case '\t':
      // ignore whitespace;
      break;

    case '\n':
      line++;
      break;

    case '"':
      string();
      break;

    default:
      if (isDigit(c)) {
        number();
      } else if (isAlpha(c)) {
        identifier();
      } else {
        Lox.error(line, "Unexpected character.");
      }
    }
  }

  // ignores all chars until the end of the line (excluding the newline char)
  // pre-condition: the opening delimiter (//) has just been consumed
  private void singleLineComment() {
    while (peek() != '\n' && !isAtEnd())
      advance();
  }

  private void identifier() {
    while (isAlphaNumeric(peek()))
      advance();

    String text = sourceCode.substring(start, current);
    TokenType type = keywords.get(text);
    if (type == null)
      type = IDENTIFIER;

    addToken(type);
  }

  private void string() {
    while (peek() != '"' && !isAtEnd()) {
      // strings in Lox are multi-line by default
      if (peek() == '\n')
        line++;
      advance();
    }
    if (isAtEnd()) {
      Lox.error(line, "Unterminated string.");
      return;
    }
    // the closing "
    advance();

    // trim the surrounding quotes
    String value = sourceCode.substring(start + 1, current - 1);
    addToken(STRING, value);
  }

  private void number() {
    // both integers and decimals start with digits...
    while (isDigit(peek()))
      advance();

    // we're looking at a decimal number...
    if (peek() == '.' && isDigit(peekNext())) {
      // consume the "."
      advance();
      while (isDigit(peek()))
        advance();
    }
    // however, Lox uses floating point (i.e., double) to represent both:
    addToken(NUMBER, Double.parseDouble(sourceCode.substring(start, current)));
  }

  private boolean match(char expected) {
    if (isAtEnd())
      return false;
    if (sourceCode.charAt(current) != expected)
      return false;
    current++;
    return true;
  }

  private char peek() {
    if (isAtEnd())
      return '\0';
    return sourceCode.charAt(current);
  }

  private char peekNext() {
    if (current + 1 >= sourceCode.length())
      return '\0';
    return sourceCode.charAt(current + 1);
  }

  private boolean isAlpha(char c) {
    return (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_';
  }

  private boolean isAlphaNumeric(char c) { return isAlpha(c) || isDigit(c); }

  private boolean isDigit(char c) { return c >= '0' && c <= '9'; }

  private boolean isAtEnd() { return current >= sourceCode.length(); }

  private char advance() { return sourceCode.charAt(current++); }

  private void addToken(TokenType type) { addToken(type, /* value: */ null); }

  private void addToken(TokenType type, Object value) {
    String text = sourceCode.substring(start, current);
    tokens.add(new Token(type, text, value, line));
  }
}
