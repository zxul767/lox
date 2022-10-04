# Chapter 1
**1. There are least six domain-specific languages used in the [little system I cobbled together](https://github.com/munificent/craftinginterpreters) to write and publish this book. What are they?**

CSS (for redering graphics), SCSS (a superset of CSS), HTML (for structuring web content), Markdown (for writing formatted text), YAML (for projects and tools configurations), Mustache (for rendering strings with interpolated values), and the language used to specify Makefiles (to build projects according to specified dependencies). I may have missed some other domain-specific languages.

**2. Get a "Hello, world!" program written and running in Java. Set up whatever makefiles or IDE projects you need to get it working. If yu have a debugger, get comfortable with it and step through your program as it runs.**

I guess this project setup covers the above and beyond?

**3. Do the same thing for C. To get some practice with pointers, define a doubly linked list of heap-allocated strings. Write functions to insert, find, and delete items from it. Test them.**

# Chapter 2
**1. Pick an open source implementation of a language you like. Download the source code and poke around in it. Try to find the code that implements the scanner and parser. Are they handwritten, or generated using tools like Lex and Yacc? (`.l` or `.y` files usually imply the latter.)**

**2. Just-in-time compilation tends to be the fastest way to implement dynamically typed languages, but not all of them use it. What reasons are there to *not* JIT?**

**3. Most Lisp implementations that compile to C also contain an interpreter that lets them execute Lisp code on the fly as well. Why?**

# Chapter 3
**1. Write some sample Lox programs and run them (you can use the implementations of Lox in [my repository]()). Try to come up with edge case behavior I didn't specify here. Does it do what you expect? Why or why not?**

**2. This informal introduction leaves a *lot* unspecified. List several open questions you have about the language's syntax and semantics. What do you think the answers should be?**

**3. Lox is a pretty tiny language. What features do you think it is missing that would make it annoying to use for real programs? (Aside from the standard library, of course.)**

# Chapter 4
**1. The lexical grammars of Python and Haskell are not *regular*. What does that mean, and why aren't they?**

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

# Chapter 6
**1. In C, a block is a statement form that allows you to pack a series of statements where a single one is expected. The [comma operator](https://en.wikipedia.org/wiki/Comma_operator) is an analogous syntax for expressions. A comma-separated series of expressions can be given a single expression is expected (except inside a function call's argument list). At runtime, the comma operator evaluates the left operand and discards the result. Then it evaluates and returns the right operand.**

**Add support for comma expressions. Give them the same precedence and associativity as in C. Write the grammar, and then implement the necessary parsing code.**

**2. Likewise, add support for the C-style conditional or "ternary" operator `?:`. What precedence level is allowed between the `?` and `:`? Is the whole operator left-associative or right-associative?**

**3. Add error productioons to handle each binary operator appearing without a left-hand operand. In other words, detect a binary operator appearing at the beginning of an expression. Report that as an error, but also parse and discard a right-hand operand with the appropriate precedence.**

# Chapter 7
**1. Allowing comparisons on types other than numbers could be useful. The operators might have a reasonable interpretation for strings. Even comparisons among mixed types, like `3 < "pancake"` could be handy to enable things like ordered collections of heterogenous types. Or it could simply lead to bugs and confusions.**

**Would you extend Lox to support comparing other types? If so, which pairs of types do you allow and how do you define their ordering? Justify your choicess and compare them to other languages.**

**2. Many languages define `+` such that if *either* operand is a string, the other is converted to a string and the results are then concatenated. For example, `"scone" + 4` would yield `scone4`. Extend the code in `visitBinaryExpr()` to support that.**

**3. What happens right now if you divide a number by zero? What do you think should happen? Justify your choice. How do other languages you know handle division by zero, and why do they make the choices they do?**

**Change the implementation in `visitBinaryExpr()` to detect and report a runtime error for this case.**

# Chapter 8
**1. The REPL no longer supports entering a single expression and automatically printing its result value. That's a drag. Add support to the REPL to let users type in both statements and expressions. If they enter a statement, execute it. If they enter an expression, evaluate it and display the result value.**

**2. Maybe you want Lox to be a little more explicit about variable initialization. Instead of implicitly initializing variables to `nil`, make it a runtime error to access a variable that has not been initialized or assigned to, as in:**

```
// No initializers.
var a;
var b;

a = "assigned";
print a; // OK, was assigned first.

print b; // Error!
```

**3.  What does the following program do?**

```
var a = 1;
{
   var a = a + 2;
   print a;
}
```

**What did you expect it to do? Is it what you think it should do? What does analogous code in other languages you are familiar with do? What do you think users will expect this to do?**

# Chapter 9
**1. A few chapters from now, when Lox supports first-class functions and dynamic dispatch, we technically won't *need* branching statements built into the language. Show how conditional execution can be implemented in terms of those. Name a language that uses this technique for its control flow.**

**2. Likewise, looping can be implemented using those same tools, provided our interpreter supports an important optimization. What is it, and why is it necessary? Name a language that uses this technique for iteration.**

**3. Unlike Lox, most other C-style languages also support `break` and `continue` statements inside loops. Add support for `break` statements.**

**The syntax is a `break` keyword followed by a semicolon. It should be a syntax error to have a `break` statement appear outside of any enclosing loop. At runtime, a `break` statement causes execution to jump to the end of the nearest enclosing loop and proceeds from there. Note that the `break` may be nested inside other blocks and `if` statements that also need to be exited.**

# Chapter 10
**1. Our interpreter carefully checks that the number of arguments passed to a function matches the number of parameters it expects. Since this check is done at runtime on every call, it has a performance cost. Smalltalk implementations don't have that problem. Why not?**

**2. Lox's function declaration syntax performs two independent operations. It creates a function and also binds it to a name. This improves usability for the common case where you do want to associate a name with the function. But in functional-styled code, you often want to create a function to immediately pass it to some other function or return it. In that case, it doesn't need a name.**

**Languages that encourage a functional style usually support *anonymous functions* or *lambdas* -- an expression syntax that creates a function without binding it to a name. Add anonymous function syntax to Lox so that this works:**

```
fun thrice(fn) {
    for (var i = 1; i <= 3; i = i + 1) {
       fn(i);
    }
}

thrice(fun (a) {
    print a;
});

// "1".
// "2".
// "3".
```

**How do you handle the tricky  case of an anonymous function expression occurring in an expression statement:**

```
fun () {};
```

**3. Is this program valid?**

```
fun scope(a) {
    var a = "local"; 
}
```

**In other words, are a function's parameters in the same scope as its local variables, or in an outer scope? What does Lox do? What about other languages you are familiar with? What do you think a language *should* do?**

# Chapter 11

**1. Why is it safe to eagerly define the variable bound to a function’s name when other variables must wait until after they are initialized before they can be used?**

**2. How do other languages you know handle local variables that refer to the same name in their initializer, like:**
```
var a = "outer";
{
  var a = a;
}
```
**Is it a runtime error? Compile error? Allowed? Do they treat global variables differently? Do you agree with their choices? Justify your answer.**

**3. Extend the resolver to report an error if a local variable is never used.**

**4. Our resolver calculates which environment the variable is found in, but it’s still looked up by name in that map. A more efficient environment representation would store local variables in an array and look them up by index.**

**Extend the resolver to associate a unique index for each local variable declared in a scope. When resolving a variable access, look up both the scope the variable is in and its index and store that. In the interpreter, use that to quickly access a variable by its index instead of using a map.**

# Chapter 12
**1. We have methods on instances, but there is no way to define "static" methods that can be called directly on the class object itself. Add support for them. Use a `class` keyword preceding the method to indicate a static method that hangs off the class object.**

```
class Math {
  class square(n) {
    return n * n;
  }
}

print Math.square(3); // Prints "9".
```

**You can solve this however you like, but the ["metaclasses"](https://en.wikipedia.org/wiki/Metaclass) used by Smalltalk and Ruby are a particularly elegant approach. *Hint: Make LoxClass extend LoxInstance and go from there*.**

**2. Most modern languages support “getters” and “setters”—members on a class that look like field reads and writes but that actually execute user-defined code. Extend Lox to support getter methods. These are declared without a parameter list. The body of the getter is executed when a property with that name is accessed.**

```
class Circle {
  init(radius) {
    this.radius = radius;
  }

  area {
    return 3.141592653 * this.radius * this.radius;
  }
}

var circle = Circle(4);
print circle.area; // Prints roughly "50.2655".
```

**3. Python and JavaScript allow you to freely access an object’s fields from outside of its own methods. Ruby and Smalltalk encapsulate instance state. Only methods on the class can access the raw fields, and it is up to the class to decide which state is exposed. Most statically typed languages offer modifiers like `private` and `public` to control which parts of a class are externally accessible on a per-member basis.**

**What are the trade-offs between these approaches and why might a language prefer one or the other?**
