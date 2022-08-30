# Chapter 1
**1. There are least six domain-specific languages used in the [little system I cobbled together](https://github.com/munificent/craftinginterpreters) to write and publish this book. What are they?**

CSS (for redering graphics), SCSS (a superset of CSS), HTML (for structuring web content), Markdown (for writing formatted text), YAML (for projects and tools configurations), Mustache (for rendering strings with interpolated values), and the language used to specify Makefiles (to build projects according to specified dependencies). I may have missed some other domain-specific languages.

**2. Get a "Hello, world!" program written and running in Java. Set up whatever makefiles or IDE projects you need to get it working. If yu have a debugger, get comfortable with it and step through your program as it runs.**

I guess this project setup covers the above and beyond?

**3. Do the same thing for C. To get some practice with pointers, define a doubly linked list of heap-allocated strings. Write functions to insert, find, and delete items from it. Test them.**

# Chapter 4
1. The lexical grammars of Python and Haskell are not *regular*. What does that mean, and why aren't they?**

It means that one or more tokens in those languages cannot be described by a [regular expression](https://en.wikipedia.org/wiki/Regular_expression). Consider, for example, the `INDENT`/`DEDENT` tokens--both languages use indentation to create code blocks--which require indentation context (i.e., information about previous tokens) to be correctly determined.

Notice that this is a pragmatic decision, rather than something absolutely necessary. Indeed, it is possible to implement Python/Haskell without using `INDENT`, `DEDENT` tokens (just `WHITESPACE` tokens), but then the burden of establishing where a block begins and where it ends is completely delegated to the parser.

You can find a discussion of this in [this Reddit thread](https://www.reddit.com/r/compsci/comments/kkzn3r/the_lexical_grammars_of_python_and_haskell_are/)

**2. Aside from separating tokens--distinguishing `print foo` from `printfoo`--spaces aren't used for much in most languages. However, in a couple of dark corners, a space does affect how code is parsed in CoffeeScript, Ruby, and the C preprocessor. Where and what effect does it have in each of those languages?**

In CoffeeScript this happens in expressions such as `[1..10].map (i) -> i*2` (which no longer works if you remove the whitespace between `map` and `(`). This is a consequence of allowing the parentheses for function calls to be optional. You can read more details about this [here](https://stackoverflow.com/questions/9014970/why-does-coffeescript-require-whitespace-after-map).

In Ruby, an analogous issue happens due to the unary splat operator, which makes the expressions `*10` and `* 10` semantically different. You can read all the details in [this post](https://stackoverflow.com/questions/50543569/is-ruby-whitespace-sensitive-in-certain-cases).

Finally, in the C preprocessor, the issue manifests in the definition of macros: the expression `#define A (n) (n)++` is not the same as `#define A(n) (n)++`. In the former case, we're simply defining a literal substitution for the character `A` for the literal characters `(n) (n)++`, whereas in the latter any occurence of `A(x)` will be replaced by `(x)++` (where `x` could be a number, a string, etc.)

**3. Our scanner here, like most, discards comments and whitespace since those aren't needed by the parser. Why might you want to write a scanner that does *not* discard those? What would it be useful for?**

One reason could be so that we can reconstruct the original source code from the AST representation (although that would also require keeping comments around.) This may be useful when writing refactoring tools which want to operate on specific parts of the source code, while leaving everything else intact. 

**4. Add support to Lox's scanner for C-style `/* ... */` block comments. Make sure to handle newlines in them. Consider allowing them to nest. Is adding support for nesting more work than you expected? Why?**

See [this commit](https://github.com/zxul767/lox/commit/f1cec178a56710d040573b41eea07566c1a787f4) for the implementation.

Adding support for nested comments turned out to be more a design than an implementation challenge, since a decision needs to be made regarding what to do with the following edge cases:

```
/* nested /* ... */
```

On its own, it might be reasonable to argue that the above should be considered a single comment. But consider what happens in this very similar case:

```
/* nested /* ... */ var a = 1;
```

Now there's more ambiguity as to whether we should consider this to be a single comment that was never closed, or a comment followed by regular code.

In the end, I decided to allow for nested comments, and throw errors if any of them (at any level of nesting) didn't properly close.

# Chapter 5

**1. Earlier, I said that the `|`, `*` and `+` forms we added to our grammar meta-syntax were just syntactic sugar. Take this grammar:**

```
expr -> expr ( "(" (expr ( "," expr )* )? ")" | "." IDENTIFIER )+
     | IDENTIFIER
     | NUMBER
```

**Produce a game that matches the same language but doesn't use any of that notational sugar.**

**Bonus: What kind of expression does this bit of grammar encode?**

**2. The Visitor pattern lets you emulate the functional style in an object-oriented language. Devise a complementary pattern for a functional language. It should let you bundle all of the operations on one type together and let you define new types easily.**

**(SML or Haskell would be ideal for this exercise, but Scheme or another Lisp works as well.)**

**3. In [reverse Polish notation](https://en.wikipedia.org/wiki/Reverse_Polish_notation) (RPN), the operands to an arithmetic operator are both placed before the operator, so `1 + 2` becomes `1 2 +`. Evaluation proceeds from left to right. Numbers are pushed onto an implicit stack. An arithmetic operator pops the top two numbers, performs the operation, and pushes the result. Thus, this:**

```
(1 + 2) * (4 - 3)
```

**in RPN becomes:**

```
1 2 + 4 3 - *
```

**Define a visitor class for our syntax tree that takes an expression, converts it to RPN, and returns the resulting string.**

The implementation for this visitor can be seen in [this commit](https://github.com/zxul767/lox/commit/08a9e9755b8271bd12dcff81172f7662e3e77810).
