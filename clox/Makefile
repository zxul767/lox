###############################################################################
# COMPILER
###############################################################################

# we only support gcc for the time being
CC = gcc

###############################################################################
# PROJECT LAYOUT
#
# src/*.{c,h}
# build/
# |---obj/*.o
# |   |---.deps/*.d
# |---bin/clox
###############################################################################
TARGET_NAME := clox

BUILD_DIR := build
SRC_DIR := src
OBJ_DIR := $(BUILD_DIR)/obj
DEPS_DIR := $(OBJ_DIR)/.deps
BIN_DIR := $(BUILD_DIR)/bin
MAIN_TARGET := $(BIN_DIR)/$(TARGET_NAME)

###############################################################################
# COMPILATION FLAGS
###############################################################################

# include debugging symbols and all warnings
CFLAGS = -g -Wall -I$(SRC_DIR)

# keep symbols in the final executable so we can debug
LDFLAGS = -rdynamic

# generate dependencies information while compiling
DEPS_FLAGS = -MT $@ -MMD -MF $(DEPS_DIR)/$*.Td

###############################################################################
# FILES
###############################################################################

SOURCE_FILES := $(wildcard $(SRC_DIR)/*.c)
OBJ_FILES := $(SOURCE_FILES:$(SRC_DIR)/%.c=$(OBJ_DIR)/%.o)
DEPS_FILES := $(SOURCE_FILES:$(SRC_DIR)/%.c=$(DEPS_DIR)/%.d)

###############################################################################
# BUILD RULES
# for details on pattern rules, see:
# https://www.gnu.org/software/make/manual/html_node/Pattern-Rules.html
###############################################################################

POST_COMPILE = mv -f $(DEPS_DIR)/$*.Td $(DEPS_DIR)/$*.d && touch $@
COMPILE = $(CC) $(DEPS_FLAGS) $(CFLAGS) -c
LINK = $(CC) $(LDFLAGS)

# main target (entry point for bare `make` invocation)
$(MAIN_TARGET) : $(OBJ_FILES) | $(BIN_DIR)
	$(LINK) -o $@ $^

# cancel predefined rule
%.o : %.c

# build object files
$(OBJ_DIR)/%.o :: $(SRC_DIR)/%.c $(DEPS_DIR)/%.d | $(DEPS_DIR)
	$(COMPILE) $< -o $@
	$(POST_COMPILE)

###############################################################################
# AUTO-COMPUTED DEPENDENCIES RULES
# for details on the whole solution implemented in this Makefile, see:
# https://make.mad-scientist.net/papers/advanced-auto-dependency-generation/
###############################################################################

$(DEPS_FILES):
include $(wildcard $(DEPS_FILES))

###############################################################################
# TASKS
###############################################################################

# create directories if necessary
$(BIN_DIR): ; @mkdir -p $@
$(DEPS_DIR): ; @mkdir -p $@

.PHONY: repl run clean print-%

# this target is only for debugging purposes; invoke it as: `make print-VAR-NAME`
print-%  : ; @echo $* = $($*)

# run the REPL
repl: $(MAIN_TARGET)
	./$<

# run a program
run: $(MAIN_TARGET)
	@echo "running a program"
	./$< $(NAME)

clean:
	rm -f $(MAIN_TARGET) $(OBJ_DIR)/*.o $(DEPS_DIR)/*.d
	rm -rf $(BUILD_DIR)
