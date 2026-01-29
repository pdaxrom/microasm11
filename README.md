Documentation

Full assembler and ISA reference (including RTL semantics): docs/isa.md

PDP-11 assembler reference and syntax:
- docs11/syntax.md
- docs11/instructions.md
- docs11/compatibility.md

Notes

- `microasm11` supports `--cpu <name>`: `default`, `dcj-11`, `vm1`, `vm1g`, `vm2`.
- `--list <file|-` writes a listing to a file or stdout.

Testing

Run microasm (microcpu) assembler smoke tests:

sh scripts/test-asm-smoke.sh

Run PDP-11 microasm11 golden tests:

sh tests11/run_golden_tests.sh
