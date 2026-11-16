# GameScript — Language Specification (v1.0)

GameScript is a small, statically typed, event-driven language for describing
2D game logic: **entities** with properties, functions and event handlers, and
**rules** that decide when the game is won or lost.

## 1. Design philosophy

* **Readable**: uppercase keywords, no operator soup (`AND`, `OR`, `NOT`).
* **Event-driven**: behaviour is attached to events (`ON START`, `ON UPDATE`,
  `ON COLLISION WITH`, `ON KEY_PRESS`, `ON DEATH`).
* **Checked at compile time**: types, names, scopes and game-specific rules
  (e.g. `WIN` only inside a `RULE`) are verified before anything runs.

## 2. Lexical structure

| Category    | Definition |
|-------------|------------|
| Whitespace  | Spaces, tabs, newlines: ignored. |
| Comments    | `// until end of line` and `/* block, not nested */`. |
| Identifier  | `[A-Za-z_][A-Za-z0-9_]*`, case-sensitive. Convention: entities `PascalCase`, everything else `camelCase`. |
| INTEGER     | `[0-9]+`, must fit in 32 bits (≤ 2147483647). |
| FLOAT       | `[0-9]+.[0-9]+` — digits required on both sides (`5.` and `.5` are errors). |
| STRING      | `"…"` on one line; escapes `\"`, `\\`, `\n`. |
| BOOLEAN     | `TRUE`, `FALSE` |
| Operators   | `+ - * / % == != < > <= >= =` and the words `AND OR NOT` |
| Delimiters  | `( ) { } ; : , .` |

**Reserved words** (uppercase, case-sensitive):
`GAME ENTITY PROPERTY FUNCTION RULE ON START UPDATE DEATH COLLISION WITH KEY_PRESS
IF ELSE WHILE RETURN VAR MOVE UP DOWN LEFT RIGHT BY SPAWN AT DESTROY SELF PLAY SOUND
PRINT WIN LOSE AND OR NOT TRUE FALSE INTEGER FLOAT STRING BOOLEAN VECTOR2`

`game` or `Game` are ordinary identifiers; `GAMEOVER` is an identifier too (longest match).

## 3. Types and operators

Types: `INTEGER`, `FLOAT`, `STRING`, `BOOLEAN`, `VECTOR2`. A function without a
return type returns nothing (`VOID`, internal only).
Uninitialized properties/variables get defaults: `0`, `0.0`, `""`, `FALSE`, `VECTOR2(0, 0)`.

| Operator | Operands | Result |
|----------|----------|--------|
| `+ - *` | INTEGER/FLOAT | INTEGER if both INTEGER, else FLOAT |
| `/` | INTEGER/FLOAT | **always FLOAT** |
| `%` | INTEGER, INTEGER | INTEGER |
| `+` | STRING, STRING | STRING (concatenation) |
| `+ -` | VECTOR2, VECTOR2 | VECTOR2 |
| `*` | VECTOR2, number | VECTOR2 |
| `== !=` | same type (INTEGER/FLOAT may mix) | BOOLEAN |
| `< > <= >=` | INTEGER/FLOAT | BOOLEAN |
| `AND OR NOT` | BOOLEAN | BOOLEAN |
| unary `-` | INTEGER/FLOAT | same type |

Only implicit conversion: `INTEGER → FLOAT` (widening). `FLOAT → INTEGER` is an error.

**Precedence (high → low):** unary `-` · `* / %` · `+ -` · comparisons (non-associative) · `NOT` · `AND` · `OR`.
All binary operators are left-associative; `1 < a < 3` is a syntax error (write `1 < a AND a < 3`).

Built-in members: every entity has `position: VECTOR2`; a VECTOR2 has read-only `.x` and `.y` (FLOAT).

## 4. Program structure

```
GAME Name {
    ENTITY ... { PROPERTY ...;  FUNCTION ...  ON ... { ... } }
    FUNCTION ...            // global functions
    RULE Name { ... }       // win / lose conditions
}
```
Exactly one `GAME`. Its items (entities, functions, rules) may appear in any order.

### Entities
An entity is a **type and one default instance**; the default instance is created
when the game starts, at position `(0, 0)`. `SPAWN E AT v;` creates more instances.
Inside entity `E`'s own code, the name `E` means *this instance*; inside
`ON COLLISION WITH E`, the name `E` means *the colliding instance*; elsewhere it
means the default instance.

### Scope
`global (entities, functions) → entity (position, properties, methods) → function/event (parameters, locals) → block`.
Redeclaring a name in the **same** scope is an error; an inner scope may shadow an outer name.
Entities and functions can be used before their declaration; variables cannot.

## 5. Statements

| Statement | Notes |
|-----------|-------|
| `VAR x: T [= e];` | local variable |
| `target = e;` | `target` is a variable, property, `SELF.prop` or `Entity.prop` |
| `IF c { } ELSE IF c { } ELSE { }` | `c` must be BOOLEAN |
| `WHILE c { }` | `c` must be BOOLEAN |
| `RETURN [e];` | |
| `MOVE [Entity\|SELF] UP\|DOWN\|LEFT\|RIGHT BY n;` | target defaults to SELF; `n` numeric; **y grows downward** |
| `SPAWN Entity AT v;` | `v` must be VECTOR2 |
| `DESTROY Entity\|SELF;` | runs `ON DEATH` first |
| `PLAY SOUND "file.wav";` | |
| `PRINT e;` | any type |
| `WIN;` / `LOSE;` | only inside a `RULE` |
| `f(args);` `Entity.f(args);` | call as statement |

### Events (inside entities)
`ON START` · `ON UPDATE` · `ON DEATH` · `ON COLLISION WITH Entity` · `ON KEY_PRESS "KEY"`.
Valid keys: `SPACE UP DOWN LEFT RIGHT ENTER ESCAPE`, `A`–`Z`, `0`–`9`.

### Rules
`RULE Name { ... }` is evaluated every frame. Rules only observe the game: assignments,
`MOVE`, `SPAWN`, `DESTROY` and `PLAY SOUND` are errors inside a rule.

## 6. Semantic rules (what the compiler checks)

1. Every identifier is declared (entities/functions may be forward-referenced).
2. No duplicate names in one scope; an entity and a global function cannot share a name;
   `position` cannot be redeclared.
3. Assignment, initializer, argument and return types must match (with `INTEGER → FLOAT`).
4. Property initializers are constant expressions (literals and operators only).
5. `IF`/`WHILE` conditions and `AND`/`OR`/`NOT` operands are BOOLEAN.
6. Calls match the callee's parameters in count and type; a no-value function cannot be used in an expression.
7. A function with a return type must return on **all** paths; `RETURN` values must match.
8. `WIN`/`LOSE` only in rules; rules have no side effects.
9. `ON COLLISION WITH E`: `E` must be a declared entity.
10. At most one `START`/`UPDATE`/`DEATH` per entity, one handler per collision target / key.
11. `KEY_PRESS` names must be valid keys.
12. `DESTROY SELF` inside `ON DEATH` is an error (infinite recursion).
13. `SPAWN` position is VECTOR2; `MOVE` amount is numeric; `PLAY SOUND` takes a STRING.
14. A literal `0` divisor of `/` or `%` is an error.
15. `SELF` only inside entities; vector components are read-only.

**Warnings** (do not stop compilation): unused local variable, unreachable code after `RETURN`,
entity with no event handlers.

## 7. Runtime behaviour (generated Python)

Frame order: `ON START` of new entities → key events → `ON UPDATE` → collisions → rules → cleanup.
Collisions use 16×16 boxes and are **edge-triggered** (the handler runs when two entities
start overlapping). A spawned entity always runs its `ON START` before its first `ON UPDATE`.
Division by zero at run time stops the game with `runtime error`.

## 8. Valid and invalid programs

See `examples/` (five complete programs) and `tests/` (lexical, syntax, semantic and
integration cases with expected output).
