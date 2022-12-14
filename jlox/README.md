# Quick Start
`make` -- Builds the project and runs unit tests.

`make run NAME="$PWD/../samples/fib.lox"` -- Builds the project and runs a script.

# Dependencies
+ A recent version of the [JDK](https://www.oracle.com/java/technologies/downloads/) (I have only tested it with `OpenJDK 14.0` on Mac OS).
+ The [`gradle`](https://gradle.org/install/) build system. 
   + Strictly speaking, you might be able to get the project running without using `gradle`, but then you'll have to manually manage the third-party dependencies with whatever other build system you decide to use.
+ (Optional) [make](https://www.gnu.org/software/make/) to be able to run the same commands as in [`clox`](../clox). Our `Makefile` is really a thin wrapper that calls `gradle`. In the unlikely case that you don't have `make` installed (and you don't care to install it), check out the `Makefile` to see the commands you need to run for each task.

# Building and Running Tests
`make`

If you're doing incremental development (e.g., [`TDD`](https://en.wikipedia.org/wiki/Test-driven_development)), you will eventually find it tedious to run this command after every change in the code. To make the command run automatically after a file in the project changes, you can run:
`make watch`

Also, if you're curious about all the tasks that are being run, you can run `./gradlew` directly using the `--console=verbose` flag as follows:
`./gradlew --console=verbose build`

# Running the REPL
![jlox-repl](https://user-images.githubusercontent.com/442314/197428785-ea1d982e-7ef4-4be2-a258-df9db3d4b59e.png)

If you want to test single-line commands in Lox, you can run its [REPL](https://en.wikipedia.org/wiki/Read%E2%80%93eval%E2%80%93print_loop) as follows:

`make repl`

When you're done with it, you can press `Ctrl-D` or type `quit` to exit.

# Running Individual Scripts
Note that the REPL only supports single-line strings, so if you want to test something larger, your best bet is to put it in a script and run it as follows:

`make run NAME="$PWD/../samples/fib.lox"`

# Setting up I/O
By default, the `system.in` of the `gradle` build is not wired up with the `system.in` of the `run` (`JavaExec`) task. To fix this, `app/build.gradle` contains this:

``` groovy
run {
    standardInput = System.in
}
```

See more details [here](https://stackoverflow.com/questions/13172137/console-application-with-java-and-gradle)

# FAQ
### Binary Files in the Repository?
As you might have noticed, `gradle` stores `gradle/wrapper/gradle-wrapper.jar`. This is indeed something that seems fishy, and there is an ongoing discussion about this [here](https://discuss.gradle.org/t/adding-gradle-wrapper-files-to-gitignore/27428).
