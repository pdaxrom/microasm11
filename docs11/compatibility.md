# Compatibility Notes

This file summarizes behavior that may differ from classic PDP-11 assemblers.
The assembler source (`microasm11.c`) is the source of truth.

## JMP Label Behavior

A bare label assembles as a PC-relative indexed operand (mode 6, reg 7). That
means:

- `JMP Label` emits `JMP X(PC)` and jumps directly to `Label`.
- `JMP @Label` emits `JMP @X(PC)` and jumps indirectly through the word at
  `Label`.

Optional flag:
- `--jmp-label-indirect` changes `JMP Label` to use mode 7 (PC-relative
  deferred), i.e. indirect jump through the word at `Label`.

## PC-Relative vs Numeric Offsets

When an operand expression contains a symbol, the assembler treats it as
PC-relative and emits `symbol - (ext_addr + 2)` in the extension word.
Purely numeric expressions are emitted as literal values (no PC adjustment).

## Symbol Case Sensitivity

- Labels, EQU symbols, macros, and PROC names are case-insensitive by default.
- Use `--case-sensitive-symbols` to make those symbols case-sensitive.
- Mnemonics and register names remain case-insensitive.

## Strings in DB

- Double-quoted strings emit characters (with escapes).
- **Single-quoted strings emit a zero byte for each character**. This is an
  implementation quirk and not PDP-11 standard behavior.

## ALIGN vs EVEN

`ALIGN` is not supported. Use `EVEN` to align to a word boundary (2 bytes).

## Expression High-Byte Operator

A leading `/` returns the high byte of an expression (`/expr` -> `expr >> 8`).
