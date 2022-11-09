# Quick Start
`make` -- Builds the project

`make run NAME=../samples/fib.lox` -- Builds the project and runs a script.

# Dependencies
+ A C compiler ([`gcc`]() or [`clang`]() should work fine)
   + Most OS provide easy ways to install a C compiler (e.g., in Debian-based distributions, `apt install build-essentials` installs `gcc`)
+ [GNU make](https://www.gnu.org/software/make/) to build the project. 
   + It's important to ensure that it's GNU make and not some other implementation since we're using specific features from that version. 

# Building and Running Tests
We don't have tests for `clox` yet :-(

# Running the REPL
![clox-repl](https://user-images.githubusercontent.com/442314/197453260-86f8d97d-0556-4f02-af69-d6103790916c.png)

If you want to test single-line commands in `clox`, you can run its [REPL](https://en.wikipedia.org/wiki/Read%E2%80%93eval%E2%80%93print_loop) as follows:

`make repl`

When you're done with it, you can type `quit` to exit the REPL.

## Debugging
The REPL also supports a few directives to ease debugging of code:

+ `:load <file-path>` loads a Lox file into the current session (absolute and relative paths are supported)
   + e.g., `:load ../test.lox` should work.
+ `:toggle-tracing` turns on/off execution tracing of any statement/expression you evaluate
+ `:toggle-bytecode` turns on/off the automatic display of bytecode generated for the last statement/expression you typed.

Also, if you want to activate any of these settings when you fire up the REPL (they're both turned off by default), you can write in `.loxrc` with the following flags in the same directory that you launch `clox` from:

```
# this will make the REPL show bytecode by default but not execution tracing
:show-bytecode
#:enable-tracing
```

# Running Individual Scripts
Note that the REPL only supports single-line strings, so if you want to test something larger, your best bet is to put it in a script and run it as follows:

`make run NAME=../samples/fib.lox` 

