# GameScript — a compiler for a 2D game-logic language

GameScript is a small domain-specific language for 2D game logic. This project is a complete compiler for it:

```
program.gs ─▶ Lexer (Flex) ─▶ Parser (Bison) ─▶ Semantic analysis ─▶ Code generator ─▶ program.py ─▶ runtime
```

```
GAME Coins {
  ENTITY Coin   { ON COLLISION WITH Player { Player.score = Player.score + 10; DESTROY SELF; } }
  ENTITY Player {
    PROPERTY score: INTEGER = 0;
    ON START  { Coin.position = VECTOR2(40, 0); }
    ON UPDATE { MOVE RIGHT BY 4; }
  }
  RULE Victory { IF Player.score >= 10 { WIN; } }
}
```

## Requirements
`gcc`, `make`, `flex` (2.6+), `bison` (3.8+), `python3`. Optional: `pip install pygame` for the window mode.
(Ubuntu/Debian: `sudo apt install build-essential flex bison python3`.)

## Build, run, test
```sh
make                                   # builds build/gsc (compiler) and build/gsc-lex (lexer only)
./build/gsc examples/ex3_coins.gs -o coins.py
python3 coins.py                       # headless: simulates 60 frames and prints a log
python3 coins.py --frames 200          # more frames
python3 coins.py --pygame              # opens a window (needs pygame)
make test                              # runs the whole test suite
```
The generated file imports `runtime/gs_runtime.py`; the compiler bakes the absolute path of `runtime/` into it,
and also looks next to the generated file, so you can simply copy `gs_runtime.py` beside `coins.py` to move it.

Compiler options:

| Option | Meaning |
|--------|---------|
| `-o FILE` | output Python file (default `<input>.py`) |
| `--check` | analyze only (lexical, syntax, semantic); no code |
| `--tokens` | print the token stream |
| `--ast` | print the abstract syntax tree |

Exit status: `0` success, `1` compilation errors, `2` usage / I/O error.

## Project layout
```
src/lexer/lexer.l          lexical analysis (Flex)
src/parser/parser.y        grammar, AST construction, syntax-error messages (Bison)
src/ast/                   AST definitions, constructors, --ast printer
src/semantic/              symbol table, type checker, semantic rules
src/codegen/codegen.c      Python code generator
src/common/errors.{h,c}    error / warning reporting with line, column and caret
src/main.c                 compiler driver        src/main_lex.c   lexer-only driver
runtime/gs_runtime.py      game runtime (Vec2, Entity, Game loop, headless + Pygame)
examples/                  five complete programs (+ expected output)
tests/                     lexer, syntax, semantic, AST and integration tests
docs/                      LANGUAGE_SPEC.md, GRAMMAR.ebnf, ARCHITECTURE.md
```

## Tests
`make test` runs 38 cases, each comparing real output to a golden `.expected` file:

| Directory | What is tested |
|-----------|----------------|
| `tests/lexer` | token stream; invalid characters, strings, comments, numbers; edge cases |
| `tests/syntax` | 11 syntax-error programs (missing `;`, braces, operands, chained comparison, recovery…) |
| `tests/semantic` | undeclared/duplicate names, type mismatches, conditions, calls and returns, game rules, constants, warnings, scoping |
| `tests/ast` | the AST printed for a full program |
| `examples/`, `tests/integration` | compile **and run** each program; operator precedence is verified by execution |

After changing the compiler on purpose, regenerate goldens with `sh tests/run_tests.sh --update` and review the diff.

## Documentation
* `docs/LANGUAGE_SPEC.md` — complete language definition, types, operators, semantic rules
* `docs/GRAMMAR.ebnf` — formal grammar
* `docs/ARCHITECTURE.md` — architecture diagram and a walk through every compilation phase
