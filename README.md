Implementations of the Lox interpreter and compiler described in the wonderful book: [https://craftinginterpreters.com](https://craftinginterpreters.com) (freely available online!)

![crafting-interpreters](https://user-images.githubusercontent.com/442314/194959983-582ace28-9181-4ed6-925f-e35666cd8868.jpeg)

The original code comes straight from the book, but I made quite a few refactorings as I was studying each chapter.

# Book Challenges
At the end of every chapter in the book, there are a few challenges to deepen the reader's understanding of the material, or to add additional feature to the interpreter or compiler. You can find my answers in the [`challenges.md`](./challenges/challenges.md) file.

# `jlox`
[![jlox](https://github.com/zxul767/lox/actions/workflows/jlox.yml/badge.svg)](https://github.com/zxul767/lox/actions/workflows/jlox.yml)

[`jlox`](./jlox) is a simple interpreter for Lox written in Java. It parses the code and turns into an AST which is then directly traversed for interpretation (i.e., no bytecode generation). It is easy to understand but it is also relatively slow. 

Its REPL includes a `help` function for quickly inspecting values, functions, methods, and classes.

# `clox`
[![clox](https://github.com/zxul767/lox/actions/workflows/clox.yml/badge.svg)](https://github.com/zxul767/lox/actions/workflows/clox.yml)

[`clox`](./clox) is a compiler/interpreter for Lox written in C. It is a single-pass compiler (unlike `jlox` which does several passes) which generates bytecode for a Lox virtual machine directly as it is parsing (i.e., it doesn't build an AST). It is designed to be much faster than `jlox` but it is harder to understand due to the low-level constructs needed (e.g., bytecode generation, hash tables, garbage collection). 

Its REPL includes a `help` function for quickly inspecting values, functions, methods, and classes.
