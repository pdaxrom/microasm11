#!/usr/bin/env python3
import sys
import os
import re
import json
import hashlib

def sha256_file(filepath):
    sha256_hash = hashlib.sha256()
    with open(filepath, "rb") as f:
        for byte_block in iter(lambda: f.read(4096), b""):
            sha256_hash.update(byte_block)
    return sha256_hash.hexdigest()

def eval_octal_expr(expr):
    expr = str(expr).strip()
    def oct_repl(match):
        return str(int(match.group(0), 8))
    safe_expr = re.sub(r'\b[0-7]+\b', oct_repl, expr)
    try:
        result = eval(safe_expr, {"__builtins__": {}}, {})
        return result & 0xFFFF
    except Exception as e:
        print(f"Error evaluating expression '{expr}': {e}")
        sys.exit(1)

def parse_yaml(filepath):
    with open(filepath, 'r') as f:
        lines = f.readlines()
    tests = []
    in_golden = False
    current_test = None
    collecting_asm = False
    asm_indent = 0
    in_words = False
    
    for line in lines:
        stripped = line.strip()
        if not stripped or stripped.startswith('#'):
            if collecting_asm:
                current_test['asm'] += line[asm_indent:].rstrip() + '\n'
            continue
        indent = len(line) - len(line.lstrip())
        
        if stripped == "golden_tests:":
            in_golden = True
            continue
            
        if not in_golden: continue
        
        if line.lstrip().startswith("- name:"):
            if current_test: tests.append(current_test)
            name = stripped.split("name:")[1].strip()
            current_test = {"name": name, "asm": "", "args": ["--enable-fp11"], "expect": {}}
            collecting_asm = False
            in_words = False
            continue
            
        if current_test:
            if stripped.startswith("asm: |"):
                collecting_asm = True
                asm_indent = indent + 2
                continue
            
            if collecting_asm:
                if indent >= asm_indent:
                    current_test['asm'] += line[asm_indent:].rstrip() + '\n'
                    continue
                else: collecting_asm = False
                
            if stripped.startswith("args:"):
                val = stripped.split("args:")[1].strip()
                if val == "[]": current_test["args"] = []
                continue
                
            if stripped.startswith("words_octal:"):
                current_test["expect"]["words_octal"] = []
                in_words = True
                continue
                
            if in_words and stripped.startswith("- "):
                val = stripped[2:].split("#")[0].strip()
                current_test["expect"]["words_octal"].append(val)
                continue
            elif in_words and indent <= 6:
                in_words = False
                
            if stripped.startswith("error:"):
                current_test["expect"]["error"] = stripped.split("error:")[1].strip().strip('"')
                continue
                
    if current_test: tests.append(current_test)
    return tests

def clean_generated(manifest_path):
    if not os.path.exists(manifest_path):
        return
    try:
        with open(manifest_path, 'r') as f:
            data = json.load(f)
        cases_dir = os.path.dirname(manifest_path)
        for name in data.get('cases', []):
            for ext in ['.asm', '.args.txt', '.expected.bin', '.stderr.contains.txt']:
                path = os.path.join(cases_dir, f"{name}{ext}")
                if os.path.exists(path):
                    os.remove(path)
        os.remove(manifest_path)
        print("Generated test files cleaned up.")
    except Exception as e:
        print(f"Error during cleanup: {e}")

def main():
    if len(sys.argv) < 2:
        print("Usage: gen_tests.py <spec.yaml> [--clean]")
        sys.exit(1)
        
    yaml_path = sys.argv[1]
    cases_dir = os.path.join(os.path.dirname(os.path.abspath(__file__)), "cases")
    manifest_path = os.path.join(cases_dir, "manifest.json")

    if "--clean" in sys.argv:
        clean_generated(manifest_path)
        return

    os.makedirs(cases_dir, exist_ok=True)
    tests = parse_yaml(yaml_path)
    manifest = {"source_yaml": os.path.abspath(yaml_path), "sha256": sha256_file(yaml_path), "cases": []}
    
    for test in tests:
        name = test["name"]
        manifest["cases"].append(name)
        asm_path = os.path.join(cases_dir, f"{name}.asm")
        with open(asm_path, 'w') as f:
            f.write(";; GENERATED - DO NOT EDIT\n")
            f.write(f";; case: {name}\n")
            f.write(test["asm"])
        with open(os.path.join(cases_dir, f"{name}.args.txt"), 'w') as f:
            args = list(test.get("args", ["--enable-fp11"]))
            if "error" in test.get("expect", {}): args.append("EXPECT_FAIL")
            f.write(" ".join(args) + "\n")
        expect = test.get("expect", {})
        if "words_octal" in expect:
            with open(os.path.join(cases_dir, f"{name}.expected.bin"), 'wb') as f:
                for expr in expect["words_octal"]:
                    word = eval_octal_expr(expr)
                    f.write(bytes([word & 0xFF, (word >> 8) & 0xFF]))
        if "error" in expect:
            with open(os.path.join(cases_dir, f"{name}.stderr.contains.txt"), 'w') as f:
                f.write(expect["error"] + "\n")
    
    with open(manifest_path, 'w') as f:
        json.dump(manifest, f, indent=2)

if __name__ == "__main__": main()
