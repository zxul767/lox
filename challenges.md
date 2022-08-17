# Chapter 1
**1. The lexical grammars of Python and Haskell are not *regular*. What does that mean, and why aren't they?**

**2. Aside from separating tokens--distinguishing `print foo` from `printfoo`--spaces aren't used for much in most languages. However, in a couple of dark corners, a space does affect how code is parsed in CoffeeScript, Ruby, and the C preprocessor. Where and what effect does it have in each of those languages?**

**3. Our scanner here, like most, discards comments and whitespace since those aren't needed by the parser. Why might you want to write a scanner that does *not* discard those? What would it be useful for?**

**4. Add support to Lox's scanner for C-style `/* ... */` block comments. Make sure to handle newlines in them. Consider allowing them to nest. Is adding support for nesting more work than you expected? Why?**

See [this commit](https://github.com/zxul767/lox/commit/f1cec178a56710d040573b41eea07566c1a787f4) for the implementation.

Adding support for nested comments turned out to be more a design than an implementation challenge, since a decision needs to be made regarding what to do with the following edge cases:

```
/* nested /* ... */
```

On its own, it might be reasonble to argue that the above should be considered a single comment. But consider what happens in this very similar case:

```
/* nested /* ... */ var a = 1;
```

Now there's more ambiguity as to whether we should consider this to be a single comment that was never closed, or a comment followed by regular code.

In the end, I decided to allow for nested comments, and throw errors if any of them (at any level of nesting) didn't properly close.
