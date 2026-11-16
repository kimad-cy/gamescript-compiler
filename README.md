# GameScript — a compiler for a 2D game-logic language

GameScript is a small domain-specific language for 2D game logic. This project is a complete compiler for it:

```
program.gs ─▶ Lexer (Flex) ─▶ Parser (Bison) ─▶ Semantic analysis ─▶ Code generator ─▶ program.py ─▶ runtime
```

## The example: Coin Chase

`examples/coin_chase.gs` is a small playable game: collect 4 coins before the Chaser catches you 3 times.
The Chaser follows you and speeds up over time. Controls: arrow keys.

```
GAME CoinChase {

  FUNCTION clamp(v: FLOAT, lo: FLOAT, hi: FLOAT): FLOAT {
    IF v < lo { RETURN lo; }
    IF v > hi { RETURN hi; }
    RETURN v;
  }

  ENTITY Coin {
    ON COLLISION WITH Player {
      Player.score = Player.score + 1;
      DESTROY SELF;
    }
  }

  ENTITY Chaser {
    PROPERTY speed: FLOAT = 1.0;
    ON UPDATE {
      IF Player.position.x > position.x { MOVE RIGHT BY speed; }
      IF Player.position.x < position.x { MOVE LEFT BY speed; }
      ...
    }
  }

  ENTITY Player {
    PROPERTY score: INTEGER = 0;
    PROPERTY lives: INTEGER = 3;
    ON KEY_PRESS "LEFT" { MOVE LEFT BY 20; }
    ON COLLISION WITH Chaser { lives = lives - 1; }
  }

  RULE Victory { IF Player.score >= 4 { WIN; } }
  RULE Defeat  { IF Player.lives <= 0 { LOSE; } }
}
```
(Excerpt; the full program is in `examples/coin_chase.gs`.)

It exercises most of the language: entities, properties, functions, `ON START / UPDATE / KEY_PRESS / COLLISION`
events, `SPAWN`, vectors, and `RULE`s with `WIN` / `LOSE`.

## Requirements
`gcc`, `make`, `flex` (2.6+), `bison` (3.8+), `python3`. Optional: `pygame` for the window mode.

```sh
# Debian / Ubuntu
sudo apt install build-essential flex bison python3 python3-venv
```

## Build and test
```sh
make          # builds build/gsc (compiler) and build/gsc-lex (lexer only)
make test     # runs the whole test suite (39 cases)
```

## Compile and run Coin Chase
```sh
./build/gsc examples/coin_chase.gs -o coin_chase.py
```

**Without a window** (prints a log; useful for testing):
```sh
python3 coin_chase.py --frames 3000      # no input: the Chaser wins -> GAME OVER
python3 coin_chase.py --frames 200 --keys "1:LEFT,2:LEFT,3:UP"   # press keys on given frames
```

**With a window** (arrow keys; close the window to quit). Modern Linux protects the system Python,
so install pygame in a virtual environment:
```sh
python3 -m venv .venv
.venv/bin/pip install pygame
.venv/bin/python coin_chase.py --pygame
```

The generated file imports `runtime/gs_runtime.py`; the compiler bakes the absolute path of `runtime/` into it and
also looks next to the generated file, so you can copy `gs_runtime.py` beside `coin_chase.py` to move it.

Other examples: `ex1_hello` … `ex5_shooter` in `examples/` (compile each the same way).

## Compiler options

| Option | Meaning |
|--------|---------|
| `-o FILE` | output Python file (default `<input>.py`) |
| `--check` | analyze only (lexical, syntax, semantic); no code |
| `--tokens` | print the token stream |
| `--ast` | print the abstract syntax tree |

Exit status: `0` success, `1` compilation errors, `2` usage / I/O error.
Errors show `file:line:col`, the source line and a caret, grouped as lexical, syntax or semantic.

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
examples/                  six complete programs (+ expected output)
tests/                     lexer, syntax, semantic, AST and integration tests
docs/                      LANGUAGE_SPEC.md, GRAMMAR.ebnf, ARCHITECTURE.md
```

## Tests
`make test` runs 39 cases, each comparing real output to a golden `.expected` file:

| Directory | What is tested |
|-----------|----------------|
| `tests/lexer` | token stream; invalid characters, strings, comments, numbers; edge cases |
| `tests/syntax` | 11 syntax-error programs (missing `;`, braces, operands, chained comparison, recovery…) |
| `tests/semantic` | undeclared/duplicate names, type mismatches, conditions, calls and returns, game rules, constants, warnings, scoping |
| `tests/ast` | the AST printed for a full program |
| `examples/`, `tests/integration` | compile **and run** each program (including a scripted winning run of Coin Chase); operator precedence is verified by execution |

After changing the compiler on purpose, regenerate goldens with `sh tests/run_tests.sh --update` and review the diff.

## Documentation
* `docs/LANGUAGE_SPEC.md` — complete language definition, types, operators, semantic rules
* `docs/GRAMMAR.ebnf` — formal grammar
* `docs/ARCHITECTURE.md` — architecture diagram and a walk through every compilation phase

## Pushing the project to GitHub

First create an **empty** repository on GitHub (no README or license), then, inside the project folder:

```sh
# 1. ignore build output and the virtual environment
printf 'build/\n.venv/\n__pycache__/\n*.pyc\n' > .gitignore

# 2. commit
git init
git add .
git commit -m "GameScript compiler"
git branch -M main

# 3. connect to GitHub and push
git remote add origin https://github.com/kimad-cy/gamescript-compiler.git
git push -u origin main
```

* If `origin` already exists, use `git remote set-url origin https://github.com/kimad-cy/gamescript-compiler.git`
  instead of `git remote add`, and check it with `git remote -v`.
* GitHub does not accept your account password. When asked for a password, paste a **Personal Access Token**
  (GitHub → Settings → Developer settings → Personal access tokens → *Tokens (classic)*, scope `repo`),
  or log in once with the GitHub CLI: `gh auth login`.
* If the push says the remote already has commits, run `git pull --rebase origin main`, then push again.

Later changes:
```sh
git add .
git commit -m "describe your change"
git push
```