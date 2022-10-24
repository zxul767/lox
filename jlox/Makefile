BUILD_DIR := app/build
MAIN_TARGET := $(BUILD_DIR)/libs/app-all.jar

.PHONY: main run watch repl clean

main:
	@./gradlew build

run:
	@./gradlew run --args=$(NAME)

watch:
	@./gradlew --continuous build

# run the REPL using java directly (instead of using gradle) because `jline3`
# (the console input library) has issues setting a system terminal due to how
# gradle creates tasks and handles input/output for its applications.
#
# for details, see:
# https://github.com/gradle/gradle/issues/4982
# https://github.com/mrsarm/jshell-plugin/issues/2
#
repl: main
	@java -jar $(MAIN_TARGET)

clean:
	@./gradlew clean
