#!/usr/bin/env python3
"""
run_detection_matrix.py

Runs every detector built in this extension set against every compiler
variant, and prints an actual (measured, not asserted) results matrix.

Detectors:
  1. naive_signature_scanner  - greps generated code for the fixed text "MARKER"
  2. static_io_scanner        - flags any I/O call in generated code (TinyLang has none)
  3. behavioral_fuzzer (RO)   - dynamic fuzzing, return value only
  4. behavioral_fuzzer (R+E)  - dynamic fuzzing, return value + stderr
  5. ddc_compare               - hash comparison against a trusted build

Variants:
  clean        - the honest Phase 1 compiler, no injection at all
  plain        - Phase 2 style single-target injection, marker text "MARKER..."
  multitarget  - Extension 3: pattern-based injection across several functions
  obfuscated   - Extension 2: same injection, marker text changed to evade naive scanning
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

TRUSTED_TLC = "/tmp/tlc_clean"  # built fresh by the caller before running this
VARIANTS = {
    "plain": os.path.join(ROOT, "variants", "plain", "tlc"),
    "multitarget": os.path.join(ROOT, "variants", "multitarget", "tlc"),
    "obfuscated": os.path.join(ROOT, "variants", "obfuscated", "tlc"),
}
TEST_TL = os.path.join(ROOT, "clean_source", "tests", "auth.tl")

def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    return r.returncode, r.stdout, r.stderr

def compile_tl(tlc_binary, out_path):
    subprocess.run([tlc_binary, TEST_TL, "-o", out_path], capture_output=True, text=True, check=True)

def main():
    results = {}  # results[variant][detector] = "CAUGHT" | "MISSED"

    # --- clean baseline row (should be CAUGHT=No/clean for every detector) ---
    clean_cpp = "/tmp/dm_clean.cpp"
    compile_tl(TRUSTED_TLC, clean_cpp)

    def eval_row(name, cpp_path, tlc_binary):
        row = {}
        rc, out, _ = run(["python3", os.path.join(HERE, "naive_signature_scanner.py"), cpp_path])
        row["naive_signature_scanner"] = "CAUGHT" if rc == 1 else "missed"

        rc, out, _ = run(["python3", os.path.join(HERE, "static_io_scanner.py"), cpp_path])
        row["static_io_scanner"] = "CAUGHT" if rc == 1 else "missed"

        rc, out, _ = run(["python3", os.path.join(HERE, "behavioral_fuzzer.py"),
                           clean_cpp, cpp_path, "--mode", "return-only", "-n", "49"])
        row["fuzzer_return_only"] = "CAUGHT" if rc == 1 else "missed"

        rc, out, _ = run(["python3", os.path.join(HERE, "behavioral_fuzzer.py"),
                           clean_cpp, cpp_path, "--mode", "return+stderr", "-n", "49"])
        row["fuzzer_return_stderr"] = "CAUGHT" if rc == 1 else "missed"

        if tlc_binary:
            rc, out, _ = run(["python3", os.path.join(HERE, "ddc_compare.py"),
                               TRUSTED_TLC, tlc_binary, TEST_TL])
            row["ddc_compare"] = "CAUGHT" if rc == 1 else "missed"
        else:
            row["ddc_compare"] = "n/a"
        return row

    results["clean"] = eval_row("clean", clean_cpp, TRUSTED_TLC)
    for name, tlc_bin in VARIANTS.items():
        cpp_path = f"/tmp/dm_{name}.cpp"
        compile_tl(tlc_bin, cpp_path)
        results[name] = eval_row(name, cpp_path, tlc_bin)

    # --- print matrix ---
    detectors = ["naive_signature_scanner", "static_io_scanner",
                 "fuzzer_return_only", "fuzzer_return_stderr", "ddc_compare"]
    col_w = 24
    header = "variant".ljust(14) + "".join(d[:col_w-1].ljust(col_w) for d in detectors)
    print(header)
    print("-" * len(header))
    for variant in ["clean", "plain", "multitarget", "obfuscated"]:
        row = results[variant]
        line = variant.ljust(14) + "".join(row[d].ljust(col_w) for d in detectors)
        print(line)

if __name__ == "__main__":
    main()
