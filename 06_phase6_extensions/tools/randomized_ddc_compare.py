#!/usr/bin/env python3
"""
randomized_ddc_compare.py

Round 2 countermeasure to the Round 1 evasion (variants/evasive/).

The weakness Round 1 exploited: ddc_compare.py always tested with the same
published, publicly known file (clean_source/tests/auth.tl), so an attacker
who has read this project (as anyone can, since it is documented openly)
can hardcode a check for that exact fixture and behave differently only
during it.

The fix does not require detecting or understanding the evasion logic at
all. It removes the attacker's ability to know the test input in advance:
each run generates a FRESH, randomized TinyLang program (via
gen_synthetic.py) with a randomized function name that still matches the
sensitive-keyword pattern (so it is still a realistic, representative
target), and uses THAT as the DDC test input instead of the fixed fixture.
Since the evasive compiler's suppression is keyed to one exact, specific
byte sequence, a freshly randomized file will not match it, and the
injection fires normally - restoring detection without ever needing to
know how the evasion works.

Usage: randomized_ddc_compare.py <trusted_tlc> <candidate_tlc> [--seed N]
Exit code 0 = hashes match (clean), 1 = hashes differ (ALERT).
"""
import hashlib
import os
import random
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))

def compile_with(tlc_binary, tl_file, out_path):
    r = subprocess.run([tlc_binary, tl_file, "-o", out_path], capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"{tlc_binary} failed on {tl_file}: {r.stderr}")

def sha256_of(path):
    with open(path, "rb") as f:
        return hashlib.sha256(f.read()).hexdigest()

def make_randomized_test_file(out_path, seed):
    """Generates a small, freshly randomized TinyLang program. The function
    is always named exactly "check_login" (kept CONSTANT on purpose, so
    that behavioral_fuzzer.py's driver, which calls check_login(...) by
    name, keeps working against it) - what is randomized is the literal
    username/password content, which is what actually needs to differ from
    the published fixture to defeat a fixture-hash-keyed evasion. Bug
    found during testing: an earlier version also randomized the function
    NAME, which silently broke the fuzzer's driver (undefined reference at
    compile time), and that build failure's exit code was being
    misinterpreted as a genuine detection. Fixed here; see README."""
    rnd = random.Random(seed)
    username_literal = "".join(rnd.choice("abcdefghijklmnopqrstuvwxyz") for _ in range(rnd.randint(4, 9)))
    password_literal = "".join(rnd.choice("abcdefghijklmnopqrstuvwxyz0123456789") for _ in range(rnd.randint(6, 14)))
    func_name = "check_login"
    content = (
        f"fn {func_name}(user, pass) {{\n"
        f"    if (user == \"{username_literal}\") {{\n"
        f"        if (pass == \"{password_literal}\") {{\n"
        f"            return 1;\n"
        f"        }}\n"
        f"    }}\n"
        f"    return 0;\n"
        f"}}\n"
    )
    with open(out_path, "w") as f:
        f.write(content)
    return func_name

def main():
    if len(sys.argv) < 3:
        print("Usage: randomized_ddc_compare.py <trusted_tlc> <candidate_tlc> [--seed N]")
        sys.exit(2)
    trusted, candidate = sys.argv[1], sys.argv[2]
    seed = None
    if "--seed" in sys.argv:
        seed = int(sys.argv[sys.argv.index("--seed") + 1])
    else:
        seed = random.randint(0, 2**32 - 1)

    with tempfile.TemporaryDirectory() as td:
        tl_path = os.path.join(td, "randomized_test.tl")
        func_name = make_randomized_test_file(tl_path, seed)
        print(f"Generated randomized test input (seed={seed}), target function: {func_name}")

        out_a = os.path.join(td, "a.cpp")
        out_b = os.path.join(td, "b.cpp")
        compile_with(trusted, tl_path, out_a)
        compile_with(candidate, tl_path, out_b)
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
