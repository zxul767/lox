# Features Design
This document contains concise descriptions of how certain features are implemented.

## Optional Semicolon
Statements are often terminated by a semicolon (`;`) in many languages, but it is often quite tedious to write it when it can in fact be omitted. This is the case when a single statement is written on a single line (i.e., when the newline serves as an implicit statement delimiter.) 

To implement this behavior, the scanner must emit "newline" tokens[^newline-token], "multiline comment" tokens[^multiline-token], and "ignorable" tokens[^ignorable-token]. Later on, the compiler must check that expression statements end either with a semicolon, a newline, or a multiline comment (the latter is needed because when it actually spans multiple lines, it can be effectively treated as a newline.)

The simplest case is this:
```
print "hello"
```

Multiline comments `/**/` are expected to mark the end of a statement when they actually span multiple lines:
```
print "hello" /* there is an implicit
newline here */
```

But they should not be treated as a statement delimiter in situations such as this:
```
print "hello" /* there is an implicit
newline here */ + " world!"
```

We should be careful to not blindly treat newlines as implicit semicolons. The following expression, for example, should be treated as `print 12 + 12`:
```
print 12
    + 12
```

But multiple statements on a single line without a semicolon as separator should raise an error:
```
var item = "item" var name = "mike"
```

To properly implement this feature, it's important to realize that while the presence of those special tokens is needed, they must be tracked independently and not be mixed with regular tokens so as not to disrupt the token-driven parsing done in the `parse_only` function in `compiler.c`. Hence the field `immediately_prior_newline` in the `Parser` structure.


[^newline-token]: Collapsing all contiguous whitespace that contains at least one newline into a single newline token is fine for the purposes of this feature.

[^multiline-token]: Not every `/*...*/` comment is emitted as a "multiline comment" token. Only those that actually span two or more lines are emitted as such; the rest are emitted as "ignorable" tokens.

[^ignorable-token]: "Ignorable" tokens are tokens which are not used to drive parsing/compilation, but which may be needed for features such as "optional semicolon". 
