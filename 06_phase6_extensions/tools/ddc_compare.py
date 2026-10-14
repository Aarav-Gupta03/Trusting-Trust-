#!/usr/bin/env python3
"""
ddc_compare.py

The DDC-style detector for this extension set: compiles the same TinyLang
test file with a trusted (clean) compiler and with a candidate compiler,
and compares the SHA-256 hash of the generated C++. This is the same
principle as phase5/ddc_verify.sh, factored out as a small reusable tool so
it can be run against each variant compiler in this extension set.

Usage: ddc_compare.py <trusted_tlc_binary> <candidate_tlc_binary> <test.tl>
Exit code 0 = hashes match (clean), 1 = hashes differ (ALERT).
"""
import hashlib
import subprocess
import sys
import tempfile
import os

def compile_with(tlc_binary, tl_file, out_path):
    r = subprocess.run([tlc_binary, tl_file, "-o", out_path], capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"{tlc_binary} failed on {tl_file}: {r.stderr}")

def sha256_of(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()

def main():
    if len(sys.argv) != 4:
        print("Usage: ddc_compare.py <trusted_tlc> <candidate_tlc> <test.tl>")
        sys.exit(2)
    trusted, candidate, tl_file = sys.argv[1], sys.argv[2], sys.argv[3]

    with tempfile.TemporaryDirectory() as td:
        out_a = os.path.join(td, "a.cpp")
        out_b = os.path.join(td, "b.cpp")
        compile_with(trusted, tl_file, out_a)
        compile_with(candidate, tl_file, out_b)
        hash_a, hash_b = sha256_of(out_a), sha256_of(out_b)
        print(f"trusted   ({trusted}): {hash_a}")
        print(f"candidate ({candidate}): {hash_b}")
        if hash_a == hash_b:
            print("[SUCCESS] hashes match.")
            sys.exit(0)
        else:
            print("[ALERT] hashes differ.")
            sys.exit(1)

if __name__ == "__main__":
    main()
