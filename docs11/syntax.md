# Syntax Guide

This document describes the syntax accepted by `microasm11`.

## Line Structure

A source line has up to four parts:

```
[Label]  [Operator]  [Operands]  [; Comment]
```

- **Label:** Optional. A label may be written with or without a trailing `:`.
  If the first token is not recognized as a mnemonic or macro name, it is
  treated as a label.
- **Operator:** An instruction mnemonic or a directive. A leading `.` is
  accepted (e.g. `.org`, `.mov`).
- **Operands:** Zero, one, or two operands separated by commas.
- **Comment:** Starts with `;` or `//` and continues to end of line.

By default, mnemonics, directives, and register names are case-insensitive.
Symbols (labels/macros/procs/equ) are case-insensitive unless
`--case-sensitive-symbols` is used.

**Example:**
```asm
Start:  MOV #100, R0   ; Load 0100 (octal) into R0
        HALT
```

## Identifiers and Labels

Identifier rules are defined by the tokenizer:
- First character: `A-Z`, `a-z`, `_`, `.`, or `:`
- Subsequent characters: `A-Z`, `a-z`, `0-9`, `_`, `$`

Labels may appear with or without `:`:
```asm
Loop:   DEC R0
Loop    DEC R0
```

## Numbers and Literals

`microasm11` uses PDP-11-style number conventions:

- **Octal (default):** `100` (octal)
- **Decimal:** `100.` (trailing dot)
- **Hex:** `0xFF` or legacy `$FF`
- **Binary:** `0b1010` or legacy `%1010`
- **Decimal prefix:** `0d123`
- **Explicit octal:** legacy `@377`
- **Character literal:** `'A'`
- **Current address:** `*` (current output address)

If a number does not meet the required format (e.g. decimal without a dot), it
is parsed as octal.

## Expressions

Expressions are evaluated as 32-bit signed integers and masked on emission
(8-bit for `DB`, 16-bit for `DW` and instruction words).

Supported operators (highest to lowest precedence):
1. Unary: `~` (bitwise not), unary `-`
2. `*`, `/`, `%`
3. `+`, `-`
4. `&`
5. `^`
6. `|`

Parentheses are supported.

Special form:
- A leading `/` in an expression returns the high byte of the expression,
  i.e. `/expr` evaluates as `(expr >> 8)`.

## Operands & Addressing Modes

### Registers
- `R0`..`R5` (general)
- `SP` (R6)
- `PC` (R7)

### General Addressing Modes
The assembler uses the standard PDP-11 encoding (mode/reg):

| Syntax | Mode | Description | Extension Word |
|--------|------|-------------|----------------|
| `Rn` | 0 | Register direct | No |
| `(Rn)` / `@Rn` | 1 | Register deferred | No |
| `(Rn)+` | 2 | Autoincrement | No |
| `@(Rn)+` | 3 | Autoincrement deferred | No |
| `-(Rn)` | 4 | Autodecrement | No |
| `@-(Rn)` | 5 | Autodecrement deferred | No |
| `X(Rn)` | 6 | Index | Yes (`X`) |
| `@X(Rn)` | 7 | Index deferred | Yes (`X`) |

### PC-Relative Forms
Bare expressions and labels assemble as `X(PC)`:

| Syntax | Mode | Description | Extension Word |
|--------|------|-------------|----------------|
| `#Value` | 2 | Immediate | Yes (Value) |
| `@#Addr` | 3 | Absolute deferred | Yes (Addr) |
| `Label` | 6 | PC-relative | Yes (offset) |
| `@Label` | 7 | PC-relative deferred | Yes (offset) |

Notes:
- If the expression contains a symbol, the assembler treats it as PC-relative
  and computes `offset = symbol - (ext_addr + 2)`.
- If the expression is purely numeric, the extension word is emitted as the
  literal value (no PC-relative adjustment).

## Directives (Pseudo-Ops)

All directives are parsed as mnemonics.

- `ORG <expr>`: set start/output address.
- `DB <expr|string>[,<expr|string>...]`: emit bytes. Double-quoted strings emit
  characters with escapes (`\n`, `\r`, `\t`, `\0`, `\\`, `\"`, `\'`).
  **Single-quoted strings emit a zero byte per character** (implementation quirk).
- `DW <expr>[,<expr>...]`: emit 16-bit words (little-endian).
- `DS <count>[,<init byte>]` / `DSB <count>[,<init byte>]`: reserve `count`
  bytes, filled with `init byte` (default 0).
- `DSW <count>[,<init word>]`: reserve `count` words (2 bytes each), filled with
  `init word` (default 0).
- `EVEN`: align output to the next word boundary (2-byte alignment). Takes no
  arguments.
- `EQU`: `Label EQU <expr>` defines a constant.
- `INCLUDE <file>`: include another source file (quotes accepted).
- `CHKSUM`: emits a placeholder word and later patches it so the word-sum over
  the output equals `0xFFFF` (one's complement).

## Macros and Procedures

### Macros
```
MACRO name [arg1, arg2, ...]
  ... body ...
ENDM
```

- Up to 10 parameters.
- Parameters can be referenced by position (`#1`, `#2`, ...) and/or by name.
- Named parameters are replaced only when they appear as identifiers.

### Procedures
```
label PROC
  ... body ...
  GLOBAL export_label
ENDP
```

- `PROC` creates a local symbol scope.
- `GLOBAL` exports a local label to the global scope.
- Nested procedures are not supported.

## Conditional Assembly

The assembler supports conditional assembly (nesting limit: 32):

- `IF <expr>`
- `IFDEF <symbol>`
- `IFNDEF <symbol>`
- `ELSE`
- `ENDIF`

Conditions are evaluated in pass 1 and control which lines are assembled.

## Command-Line Interface

```
microasm11 [-verilog|-binary] [--case-sensitive-symbols] [--jmp-label-indirect] [--list <file|-] <input_file> [output_file]
```

- Default output is a text hex dump (`.mem`) with 16 bytes per line.
- `-binary` writes raw bytes.
- `-verilog` writes a simple RAM module with initialized bytes.
- `--list <file>` writes a listing to the given file.
- `--list -` writes the listing to stdout.
