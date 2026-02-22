## Assembler for PDP/11
based on microasm for [microcpu](https://github.com/pdaxrom/microcpu)

## Documentation

Full assembler and ISA reference (including RTL semantics): docs/isa.md

PDP-11 assembler reference and syntax:
- [docs11/syntax.md](docs11/syntax.md)
- [docs11/instructions.md](docs11/instructions.md)
- [docs11/compatibility.md](docs11/compatibility.md)

## Notes

- `microasm11` supports `--cpu <name>`: `default`, `dcj-11`, `vm1`, `vm1g`, `vm2`.
- `--list <file|-` writes a listing to a file or stdout.

## Testing

To run all PDP-11 tests (including generated ones):

```bash
make tests
```

To run only the golden tests manually:

```bash
sh tests11/run_golden_tests.sh
sh tests11/run_tests.sh
```

Golden tests for FP11 instructions are automatically generated from `tests11/fp11_golden.yaml` during `make tests`.
To clean up generated files:

```bash
make clean
```
