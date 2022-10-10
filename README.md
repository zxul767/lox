Implementations of the Lox interpreter and compiler described in the wonderful book: [https://craftinginterpreters.com](https://craftinginterpreters.com) (freely available online!)

![crafting-interpreters](https://user-images.githubusercontent.com/442314/194959983-582ace28-9181-4ed6-925f-e35666cd8868.jpeg)

The original code comes straight from the book, but I made quite a few refactorings as I was studying each chapter. Refactoring and implementing the features in the [challenges](./challenges.md) section of each chapter is a really good way to really understand the topics and the code in the book.

# `jlox`
[`jlox`](./jlox) is a simple interpreter for Lox written in Java. It parses the code and turns into an AST which is then directly traversed for interpretation (i.e., no bytecode generation). It is easy to understand but it is also relatively slow.

# `clox`
[`clox`](./clox) is a compiler/interpreter for Lox written in C. It is a single-pass compiler (unlike `jlox` which does several passes) which generates bytecode for a Lox virtual machine directly as it is parsing (i.e., it doesn't build an AST). It is designed to be much faster than `jlox` but it is harder to understand due to the low-level constructs needed (e.g., bytecode generation, hash tables, garbage collection).
