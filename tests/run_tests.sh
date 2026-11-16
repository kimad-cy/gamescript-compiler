#!/bin/sh
# GameScript test suite. Run from the project root:   make test
#   sh tests/run_tests.sh            run all tests
#   sh tests/run_tests.sh --update   regenerate every .expected file
#                                    (review the diff before trusting it!)
#
# Every test is "run something, compare its output to a golden .expected file":
#   tests/lexer/*.gs        token stream and lexical errors        (gsc-lex)
#   tests/syntax/*.gs       syntax errors + recovery               (gsc --check)
#   tests/semantic/*.gs     semantic errors, warnings, valid code  (gsc --check)
#   tests/ast/*.gs          the abstract syntax tree               (gsc --ast)
#   examples/*.gs, tests/integration/*.gs
#                           full pipeline: compile -> run the generated Python

GSC=./build/gsc
LEX=./build/gsc-lex
OUT=build/out
mkdir -p "$OUT"
pass=0; fail=0
update=0; [ "$1" = "--update" ] && update=1

# check NAME EXPECTED_FILE ACTUAL_TEXT
check() {
    if [ $update -eq 1 ]; then
        printf '%s\n' "$3" > "$2"; echo "updated $2"; return
    fi
    if [ -f "$2" ] && [ "$3" = "$(cat "$2")" ]; then
        echo "PASS  $1"; pass=$((pass + 1))
    else
        echo "FAIL  $1"; fail=$((fail + 1))
        printf '%s\n' "$3" | diff - "$2" 2>&1 | head -12
    fi
}

# capture CMD... : stdout+stderr followed by the exit status
capture() {
    out=$("$@" 2>&1); rc=$?
    printf '%s\n[exit %s]' "$out" "$rc"
}

echo "== lexical analysis"
for f in tests/lexer/*.gs; do check "$f" "${f%.gs}.expected" "$(capture $LEX "$f")"; done

echo "== syntax analysis"
for f in tests/syntax/*.gs; do check "$f" "${f%.gs}.expected" "$(capture $GSC --check "$f")"; done

echo "== semantic analysis"
for f in tests/semantic/*.gs; do check "$f" "${f%.gs}.expected" "$(capture $GSC --check "$f")"; done

echo "== abstract syntax tree"
for f in tests/ast/*.gs; do check "$f" "${f%.gs}.expected" "$(capture $GSC --ast "$f")"; done

echo "== integration (compile + run)"
for f in examples/*.gs tests/integration/*.gs; do
    name=$(basename "$f" .gs)
    args=""; [ -f "${f%.gs}.args" ] && args=$(cat "${f%.gs}.args")
    if $GSC "$f" -o "$OUT/$name.py" >"$OUT/$name.log" 2>&1; then
        actual=$(capture python3 "$OUT/$name.py" $args)
    else
        actual="COMPILATION FAILED
$(cat "$OUT/$name.log")"
    fi
    check "$f" "${f%.gs}.expected" "$actual"
done

[ $update -eq 1 ] && exit 0
echo "-- $pass passed, $fail failed"
[ "$fail" -eq 0 ]
