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
If you want to test single-line commands in `clox`, you can run its [REPL](https://en.wikipedia.org/wiki/Read%E2%80%93eval%E2%80%93print_loop) as follows:

`make repl`

When you're done with it, you can type `quit` to exit the REPL.

# Running Individual Scripts
Note that the REPL only supports single-line strings, so if you want to test something larger, your best bet is to put it in a script and run it as follows:

`make run NAME=../samples/fib.lox` 

