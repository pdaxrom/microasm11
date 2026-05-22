#!/bin/bash
set -euo pipefail

ASSEMBLER=./microasm11
LINKER=./microlink11
DISASSEMBLER=./microdis11
CASES_DIR=tests11/object

if [ ! -x "$ASSEMBLER" ]; then
    echo "Assembler not found at $ASSEMBLER"
    exit 1
fi

if [ ! -x "$LINKER" ]; then
    echo "Linker not found at $LINKER"
    exit 1
fi

if [ ! -x "$DISASSEMBLER" ]; then
    echo "Disassembler not found at $DISASSEMBLER"
    exit 1
fi

tmp_dir=$(mktemp -d)
trap 'rm -rf "$tmp_dir"' EXIT

main_obj="$tmp_dir/main.obj"
lib_obj="$tmp_dir/lib.obj"
linked_mem="$tmp_dir/linked.mem"
linked_bin="$tmp_dir/linked.bin"
symbols_txt="$tmp_dir/symbols.txt"
main_dis="$tmp_dir/main.dis"
bin_dis="$tmp_dir/linked.dis"
expected_mem="$tmp_dir/linked.expected"
expected_symbols="$tmp_dir/symbols.expected"
expected_main_dis="$tmp_dir/main.dis.expected"
expected_bin_dis="$tmp_dir/linked.dis.expected"

"$ASSEMBLER" -object "$CASES_DIR/main.asm" "$main_obj"
"$ASSEMBLER" -object "$CASES_DIR/lib.asm" "$lib_obj"
"$LINKER" -symbols -org '$100' -o "$linked_mem" "$main_obj" "$lib_obj" > "$symbols_txt"
"$LINKER" -binary -org '$100' -o "$linked_bin" "$main_obj" "$lib_obj"
"$DISASSEMBLER" -object "$main_obj" > "$main_dis"
"$DISASSEMBLER" -binary -org '$100' "$linked_bin" > "$bin_dis"

printf '%s\n' \
    '0100: C0 15 10 01 C1 15 12 01 F7 09 06 00 C2 1D 00 00' \
    '0110: 12 01 87 00' > "$expected_mem"

printf '%s\n' \
    'Symbols:' \
    '0100 start' \
    '0112 ext_target' > "$expected_symbols"

cat > "$expected_main_dis" <<'EOF'
; object file
extern ext_target
public start
; entry 000000

start:
000000: 012700 000020         mov #000020, r0 ; reloc word code_base
000004: 012701 000000         mov #000000, r1 ; reloc word ext_target
000010: 004767 177764         jsr pc, 000000 ; reloc pcrel-word ext_target
000014: 016702 000000         mov 000020, r2
000020: 000000                halt ; reloc word ext_target
EOF

cat > "$expected_bin_dis" <<'EOF'
000400: 012700 000420         mov #000420, r0
000404: 012701 000422         mov #000422, r1
000410: 004767 000006         jsr pc, 000422
000414: 016702 000000         mov 000420, r2
000420: 000422                br 000466
000422: 000207                rts pc
EOF

if ! cmp -s "$expected_mem" "$linked_mem"; then
    echo "FAIL: object link output mismatch"
    diff -u "$expected_mem" "$linked_mem" || true
    exit 1
fi

if ! cmp -s "$expected_symbols" "$symbols_txt"; then
    echo "FAIL: object symbol output mismatch"
    diff -u "$expected_symbols" "$symbols_txt" || true
    exit 1
fi

if ! cmp -s "$expected_main_dis" "$main_dis"; then
    echo "FAIL: object disassembly mismatch"
    diff -u "$expected_main_dis" "$main_dis" || true
    exit 1
fi

if ! cmp -s "$expected_bin_dis" "$bin_dis"; then
    echo "FAIL: binary disassembly mismatch"
    diff -u "$expected_bin_dis" "$bin_dis" || true
    exit 1
fi

echo "PASS: object-link"
