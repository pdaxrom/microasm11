# Instruction Set Reference

This file lists all mnemonics implemented in `microasm11.c`. Opcode values
are shown in octal (16-bit, up to `0177777`). Byte forms are indicated by a
trailing `B` and set bit 15 (`0100000`) of the opcode when allowed.

## CPU Profiles

The assembler supports `--cpu <name>` to restrict the accepted instruction set:

- `default`: all supported instructions (includes VM2-only opcodes)
- `dcj-11`: same as current PDP-11 set (EIS supported) + DCJ-11 single-operand extensions
- `vm1`: EIS not supported, MFPI/MTPI/MFPD/MTPD not supported, FIS not supported
- `vm1g`: EIS supported, MFPI/MTPI/MFPD/MTPD not supported, FIS not supported
- `vm2`: EIS supported + VM2-only system instructions + FIS

If an instruction is not supported for the selected CPU, assembly fails.

## Encoding Notes

- Instruction word size: 16 bits.
- Operand specifier: `(mode << 3) | reg`.
- Double-operand format: `opcode | (src << 6) | dst`.
- Single-operand format: `opcode | dst`.
- Any operand using modes 2/3 with `#`, or modes 6/7, emits one extension word.
- For PC-relative operands that reference a symbol, the assembler emits
  `ext = symbol - (ext_addr + 2)`.

## Double Operand
**Syntax:** `OP src, dst`

| Mnemonic | Description | Opcode Base |
|----------|-------------|-------------|
| `MOV(B)` | Move | 010000 |
| `CMP(B)` | Compare | 020000 |
| `BIT(B)` | Bit test | 030000 |
| `BIC(B)` | Bit clear | 040000 |
| `BIS(B)` | Bit set | 050000 |
| `ADD` | Add | 060000 |
| `SUB` | Subtract | 160000 |

## Single Operand
**Syntax:** `OP dst`

| Mnemonic | Description | Opcode Base |
|----------|-------------|-------------|
| `CLR(B)` | Clear | 005000 |
| `COM(B)` | Complement | 005100 |
| `INC(B)` | Increment | 005200 |
| `DEC(B)` | Decrement | 005300 |
| `NEG(B)` | Negate | 005400 |
| `ADC(B)` | Add carry | 005500 |
| `SBC(B)` | Subtract carry | 005600 |
| `TST(B)` | Test | 005700 |
| `ROR(B)` | Rotate right | 006000 |
| `ROL(B)` | Rotate left | 006100 |
| `ASR(B)` | Arithmetic shift right | 006200 |
| `ASL(B)` | Arithmetic shift left | 006300 |
| `SWAB` | Swap bytes | 000300 |
| `SXT` | Sign extend | 006700 |
| `MFPI` | Move from previous I-space | 006500 |
| `MTPI` | Move to previous I-space | 006600 |
| `MFPD` | Move from previous D-space | 106500 |
| `MTPD` | Move to previous D-space | 106600 |
| `MTPS` | Move to PS | 106400 |
| `MFPS` | Move from PS | 106700 |
| `CSM` | DCJ-11 extension | 070000 |
| `TSTSET` | DCJ-11 extension | 072000 |
| `WRTLCK` | DCJ-11 extension | 073000 |

## FIS (KE11-F)

FIS instructions are supported in `default`, `dcj-11`, and `vm2`.
They are not supported in `vm1` or `vm1g`.

| Mnemonic | Syntax | Opcode Base |
|----------|--------|-------------|
| `FADD` | `FADD Rn` | 075000 |
| `FSUB` | `FSUB Rn` | 075010 |
| `FMUL` | `FMUL Rn` | 075020 |
| `FDIV` | `FDIV Rn` | 075030 |

## FP11 (FPP)

FP11 instructions are supported when using the `--enable-fp11` flag.

### Register Interpretation

FP11 instructions use a **context-sensitive syntax** compatible with DEC MACRO-11:

1.  **Floating Accumulators (AC0-AC5)**: In the floating source (`FSRC`) or destination (`FDST`) operand slots, registers `R0` through `R5` (mode 0) are interpreted as floating-point accumulators `AC0` through `AC5`.
2.  **Scratchpad AC Field**: For instructions with a dedicated 2-bit AC field (e.g., `ADDF`), the operand must be explicitly specified as `AC0`, `AC1`, `AC2`, or `AC3`.
3.  **General EA**: All other addressing modes (indexed, deferred, etc.) work as normal PDP-11 effective addresses.

### Instructions

| Mnemonic | Format | Description |
|----------|--------|-------------|
| `CFCC` | F5 | Copy Floating Condition Codes |
| `SETF` / `SETD` | F5 | Set Float / Double precision |
| `SETI` / `SETL` | F5 | Set Integer / Long-integer mode |
| `LDFPS` / `STFPS` | F4 | Load / Store Floating Program Status |
| `CLRF(D)` | F2 | Clear |
| `TSTF(D)` | F2 | Test |
| `ABSF(D)` | F2 | Absolute Value |
| `NEGF(D)` | F2 | Negate |
| `ADDF(D)` | F1 | Add `FSRC, AC` |
| `SUBF(D)` | F1 | Subtract `FSRC, AC` |
| `MULF(D)` | F1 | Multiply `FSRC, AC` |
| `DIVF(D)` | F1 | Divide `FSRC, AC` |
| `MODF(D)` | F1 | Multiply and Integerize |
| `CMPF(D)` | F1 | Compare |
| `LDF(D)` | F1 | Load |
| `STF(D)` | F1 | Store `AC, FDST` |
| `LDCDF(D)` | F1 | Convert and Load |
| `STCDF(D)` | F1 | Convert and Store |
| `LDCIF(D)` | F3 | Convert Integer to Floating `SRC, AC` |
| `LDCLF(D)` | F3 | Convert Long to Floating `SRC, AC` |
| `STCFI(D)` | F3 | Convert Floating to Integer `AC, DST` |
| `STCFL(D)` | F3 | Convert Floating to Long `AC, DST` |
| `LDEXP` | F3 | Load Exponent |
| `STEXP` | F3 | Store Exponent |

## Branches
**Syntax:** `Bxx label`

- 8-bit signed offset in words relative to the next instruction.
- Range: -128..127 (word offset).

| Mnemonic | Condition | Base |
|----------|-----------|------|
| `BR` | Always | 000400 |
| `BNE` | Z = 0 | 001000 |
| `BEQ` | Z = 1 | 001400 |
| `BPL` | N = 0 | 100000 |
| `BMI` | N = 1 | 100400 |
| `BVC` | V = 0 | 102000 |
| `BVS` | V = 1 | 102400 |
| `BCC` | C = 0 | 103000 |
| `BCS` | C = 1 | 103400 |
| `BGE` | (N^V) = 0 | 002000 |
| `BLT` | (N^V) = 1 | 002400 |
| `BGT` | Z|(N^V) = 0 | 003000 |
| `BLE` | Z|(N^V) = 1 | 003400 |
| `BHI` | C|Z = 0 | 101000 |
| `BLOS` | C|Z = 1 | 101400 |

## Subroutines & Jumps

| Mnemonic | Syntax | Opcode Base | Notes |
|----------|--------|-------------|-------|
| `JMP` | `JMP dst` | 000100 | Register direct (mode 0) is rejected. |
| `JSR` | `JSR Rn, dst` | 004000 | |
| `RTS` | `RTS Rn` | 000200 | |
| `SOB` | `SOB Rn, label` | 077000 | 6-bit backward offset (0..63). |
| `MARK` | `MARK n` | 006400 | `n` masked to 6 bits. |

## Extended Instruction Set (EIS)

EIS instructions are supported in `default`, `dcj-11`, `vm1g`, `vm2`.
They are not supported in `vm1`.

| Mnemonic | Syntax | Opcode Base |
|----------|--------|-------------|
| `MUL` | `MUL src, Rn` | 070000 |
| `DIV` | `DIV src, Rn` | 071000 |
| `ASH` | `ASH src, Rn` | 072000 |
| `ASHC` | `ASHC src, Rn` | 073000 |
| `XOR` | `XOR Rn, dst` | 074000 |

## System & Trap

| Mnemonic | Opcode |
|----------|--------|
| `HALT` | 000000 |
| `WAIT` | 000001 |
| `RTI` | 000002 |
| `BPT` | 000003 |
| `IOT` | 000004 |
| `RESET` | 000005 |
| `RTT` | 000006 |
| `MFPT` | 000007 |
| `TRAP` | 104400 + vector (8-bit) |
| `EMT` | 104000 + vector (8-bit) |

## VM2 System Instructions

These are supported only in `vm2` and `default`:

| Mnemonic | Opcode |
|----------|--------|
| `GO` | 000012 |
| `STEP` | 000016 |
| `RSEL` | 000020 |
| `MFUS` | 000021 |
| `RCPC` | 000022 |
| `RCPS` | 000024 |
| `MTUS` | 000031 |
| `WCPC` | 000032 |
| `WCPS` | 000034 |

## Processor Status / Priority

| Mnemonic | Syntax | Opcode Base |
|----------|--------|-------------|
| `SPL` | `SPL n` | 000230 (low 3 bits used) |

## Condition Codes

| Mnemonic | Opcode |
|----------|--------|
| `NOP` | 000240 |
| `CLC` | 000241 |
| `CLV` | 000242 |
| `CLZ` | 000244 |
| `CLN` | 000250 |
| `CCC` | 000257 |
| `SEC` | 000261 |
| `SEV` | 000262 |
| `SEZ` | 000264 |
| `SEN` | 000270 |
| `SCC` | 000277 |
