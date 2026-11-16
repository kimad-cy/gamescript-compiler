CC      := gcc
BUILD   := build
CFLAGS  := -Wall -Wextra -Wno-unused-function -g \
           -Isrc/common -Isrc/ast -Isrc/lexer -Isrc/semantic -Isrc/codegen -I$(BUILD) \
           -DRUNTIME_DIR='"$(abspath runtime)"'

COMMON  := src/common/errors.c src/lexer/tokens.c
COMPILER_SRC := $(BUILD)/lex.yy.c $(BUILD)/parser.tab.c $(COMMON) \
                src/ast/ast.c src/ast/ast_print.c \
                src/semantic/symtab.c src/semantic/semantic.c \
                src/codegen/codegen.c src/main.c

.PHONY: all test clean

all: $(BUILD)/gsc $(BUILD)/gsc-lex

$(BUILD):
	mkdir -p $(BUILD)

# Bison: parser + parser.tab.h (token codes and value types shared with the lexer)
$(BUILD)/parser.tab.h $(BUILD)/parser.tab.c: src/parser/parser.y src/ast/ast.h | $(BUILD)
	bison -Wall -d -o $(BUILD)/parser.tab.c src/parser/parser.y

# Flex: scanner
$(BUILD)/lex.yy.c: src/lexer/lexer.l $(BUILD)/parser.tab.h | $(BUILD)
	flex -o $@ src/lexer/lexer.l

# The compiler
$(BUILD)/gsc: $(COMPILER_SRC) $(wildcard src/*/*.h)
	$(CC) $(CFLAGS) -o $@ $(COMPILER_SRC)

# Lexer-only tool (kept for testing the first phase in isolation)
$(BUILD)/gsc-lex: $(BUILD)/lex.yy.c $(COMMON) src/main_lex.c
	$(CC) $(CFLAGS) -o $@ $^

test: all
	sh tests/run_tests.sh

clean:
	rm -rf $(BUILD)
