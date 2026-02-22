# microasm11 FP11 Golden Tests

These tests are automatically generated from a YAML specification.

## Directory Layout

- `gen_tests.py`: Python 3 script to generate tests from YAML.
- `run_tests.sh`: Shell script to execute generated tests.
- `cases/`: Generated assembly files, expected binaries, and metadata.
- `cases/manifest.json`: List of generated test cases and source SHA256.
- `fp11_golden.yaml`: Source of golden tests.

## How to Run Generator

To update or regenerate the tests from the YAML specification:

```bash
python3 tests11/gen_tests.py tests11/fp11_golden.yaml
```

**Warning:** Do not edit files in `cases/` directly. They are overwritten by the generator. Non-generated files are protected by a header check.

## How to Run Tests

Using Makefile (recommended):
```bash
make tests
```

Manually:
1. Ensure the assembler is built: `make`
2. Run the test suite: `sh tests11/run_tests.sh`
