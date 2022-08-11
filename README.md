# Quick Start
`./gradlew build` -- Builds the project and runs unit tests.
`./gradlew run --args="$HOME/src/projects/lox/app/test/resources/infinite-loop.lox"` -- Builds the project and runs the `infinite-loop.lox` file.

# Dependencies
+ A recent version of the Java Runtime and Compiler (I've personally only tested it with `OpenJDK 14.0` on Mac OS). As the procedure to install it may change over time, your best bet is to do an online search for something like "install java on <your-favorite-os>"

# Building and Running Tests
`./gradlew run`

If you're doing incremental development (e.g., [`TDD`](https://en.wikipedia.org/wiki/Test-driven_development)), you will eventually find it tedious to run this command after every change in the code. To make the command run automatically after a file in the project changes, you can use the `--continous` flag:

`./gradlew --continous run`

Also, if you're curious about all the tasks that are being run, you can use the `--console=verbose` flag as follows:
`./gradlew --console=verbose build`

# Running the REPL
If you want to test single-line commands in Lox, you can run its [REPL](https://en.wikipedia.org/wiki/Read%E2%80%93eval%E2%80%93print_loop) as follows:

`./gradlew run -q --console=plain`

When you're done with it, you can press `Ctrl-D` to exit the REPL.

# Running Individual Scripts
Note that the REPL only supports single-line strings, so if you want to test something larger, your best bet is to put it in a script and run it as follows:

`./gradlew run --args="$HOME/src/projects/lox/app/test/resources/infinite-loop.lox"`

Notice that we need to pass an absolute path for this to work.

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
As you might have noticed, `gradle`--our build system--stores `gradle/wrapper/gradle-wrapper.jar`. This is indeed something that seems fishy, and there is an ongoing discussion about this [here](https://discuss.gradle.org/t/adding-gradle-wrapper-files-to-gitignore/27428).
