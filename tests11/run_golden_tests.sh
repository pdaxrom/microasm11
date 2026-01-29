#!/bin/bash
ASSEMBLER=./microasm11
CASES_DIR=tests11/cases

if [ ! -f "$ASSEMBLER" ]; then
    echo "Assembler not found at $ASSEMBLER"
    exit 1
fi

echo "Running Golden Tests..."
FAIL=0
PASS=0

for asm in $CASES_DIR/*.asm; do
  base="${asm%.asm}"
  expected="${base}.expected.bin"
  args=""
  expect_fail=0
  
  if [ -f "${base}.args.txt" ]; then
    args_content=$(cat "${base}.args.txt")
    if echo "$args_content" | grep -q "EXPECT_FAIL"; then
       expect_fail=1
    fi
    args=$(echo "$args_content" | sed 's/EXPECT_FAIL//g')
  fi

  # Skip include fragments
  if echo "$asm" | grep -q "inc[0-9]\.asm"; then
      continue
  fi

  tmp_bin="${base}.tmp.bin"
  tmp_lst="${base}.tmp.lst"
  
  # Run assembler
  # We invoke from root dir, so paths are relative to root.
  # But include tests rely on CWD being tests/cases?
  # bless.sh cd'ed to tests/cases. 
  # If we run from root, include paths like "inc1.inc" might fail if not careful.
  # But "include_basic.asm" uses "inc1.inc".
  # If we run `./assembler tests/cases/include_basic.asm`
  # include.c resolves relative to file dir? No, relative to CWD.
  # So we MUST cd to tests/cases or fix include.c to use file dir.
  # bless.sh cd'ed. So the expected bin was generated in that context.
  # We should replicate that context.
  
  # Run in subshell to change directory safely
  (
      cd "$CASES_DIR" || exit 1
      # Adjust assembler path relative to CASES_DIR (root/tests/cases) -> ../../assembler
      REL_ASSEMBLER=../../microasm11
      
      # Filenames are now just basename
      asm_base=$(basename "$asm")
      exp_base=$(basename "$expected")
      tmp_base=$(basename "$tmp_bin")
      
      # Run
      $REL_ASSEMBLER -binary "$asm_base" "$tmp_base" $args > /dev/null 2>&1
      rc=$?
      
      if [ $expect_fail -eq 1 ]; then
          if [ $rc -ne 0 ]; then
              # Expected failure passed
              rm -f "$tmp_base"
              exit 0
          else
              echo "FAIL: $asm_base expected failure but succeeded"
              rm -f "$tmp_base"
              exit 1
          fi
      else
          if [ $rc -ne 0 ]; then
              echo "FAIL: $asm_base assembly failed"
              exit 1
          fi
          
          # Compare binary
          if [ ! -f "$exp_base" ]; then
              echo "FAIL: $asm_base missing expected binary $exp_base"
              exit 1
          fi
          
          if cmp -s "$tmp_base" "$exp_base"; then
              rm -f "$tmp_base"
              exit 0
          else
              echo "FAIL: $asm_base comparison failed"
              echo "Diff:"
              cmp -l "$tmp_base" "$exp_base" | head -n 10
              rm -f "$tmp_base"
              exit 1
          fi
      fi
  )
  
  if [ $? -eq 0 ]; then
      echo "PASS: $asm"
      PASS=$((PASS + 1))
  else
      FAIL=$((FAIL + 1))
  fi

done

echo "------------------------------------------------"
echo "Tests Passed: $PASS, Failed: $FAIL"
if [ $FAIL -gt 0 ]; then
    exit 1
fi
exit 0
