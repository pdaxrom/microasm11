#!/bin/bash
CASES_DIR="tests11/cases"
ASSEMBLER="./microasm11"
if [ ! -f "$ASSEMBLER" ]; then echo "Assembler not found at $ASSEMBLER. Please run make first."; exit 1; fi
passed=0; failed=0
# Read names from manifest.json using python to be safe
manifest_cases=$(python3 -c "import json; print(' '.join(json.load(open('$CASES_DIR/manifest.json'))['cases']))")
for name in $manifest_cases; do
    asm="$CASES_DIR/$name.asm"; args_file="$CASES_DIR/$name.args.txt"; expected_bin="$CASES_DIR/$name.expected.bin"; stderr_file="$CASES_DIR/$name.stderr.contains.txt"
    args=""; expect_fail=0
    if [ -f "$args_file" ]; then args=$(cat "$args_file")
        if [[ $args == *"EXPECT_FAIL"* ]]; then expect_fail=1; args=${args//EXPECT_FAIL/}; fi
    fi
    tmp_bin=$(mktemp); tmp_stderr=$(mktemp)
    $ASSEMBLER $asm -binary $tmp_bin $args > /dev/null 2> $tmp_stderr
    exit_code=$?
    if [ $expect_fail -eq 1 ]; then
        if [ $exit_code -ne 0 ]; then
            if [ -f "$stderr_file" ]; then pattern=$(cat "$stderr_file")
                if grep -iq "$pattern" "$tmp_stderr"; then echo "PASS: $name (expected failure)"; ((passed++))
                else echo "FAIL: $name (wrong error message)"; echo "  Expected: $pattern"; echo "  Got: $(cat $tmp_stderr)"; ((failed++)); fi
            else echo "PASS: $name (failed as expected)"; ((passed++)); fi
        else echo "FAIL: $name (expected failure but succeeded)"; ((failed++)); fi
    else
        if [ $exit_code -eq 0 ]; then
            if [ -f "$expected_bin" ]; then
                if cmp -s "$tmp_bin" "$expected_bin"; then echo "PASS: $name"; ((passed++))
                else echo "FAIL: $name (binary mismatch)"; ((failed++)); fi
            else echo "PASS: $name"; ((passed++)); fi
        else echo "FAIL: $name (assembly failed)"; cat "$tmp_stderr"; ((failed++)); fi
    fi
    rm "$tmp_bin" "$tmp_stderr"
done
echo "------------------------------------------------"
echo "Tests Passed: $passed, Failed: $failed"
[ $failed -ne 0 ] && exit 1 || exit 0
