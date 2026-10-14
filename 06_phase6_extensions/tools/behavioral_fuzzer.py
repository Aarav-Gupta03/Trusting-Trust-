#!/usr/bin/env python3
"""
behavioral_fuzzer.py

Extension 1 (part b): an independent DYNAMIC detector, as opposed to the
static scanners and the static-diff-based DDC harness. It compiles the
generated check_login into a small test driver, runs it across many
(user, pass) input pairs, and compares the observed behavior between a
"reference" (clean) build and a "candidate" (possibly infected) build.

Two comparison modes are provided, deliberately, to make an honest point
about test coverage:

  --mode return-only   : compares ONLY check_login's return value between
                          reference and candidate. This is what most naive
                          black-box unit tests actually check.
  --mode return+stderr : ALSO compares captured stderr output.

Because this project's injected payload is deliberately inert (it never
changes the return value, only prints to stderr - see phase2/phase4
README), return-only fuzzing MISSES the infection completely, while
return+stderr fuzzing CATCHES it. This is not a bug in the fuzzer - it is
the actual, general lesson: a dynamic/behavioral test is only as strong as
the set of channels it decides to observe. An attacker who confines a
payload to a channel the tester does not check can evade even a
large-N behavioral fuzz campaign.

Usage:
  behavioral_fuzzer.py <reference.cpp> <candidate.cpp> --mode return-only
  behavioral_fuzzer.py <reference.cpp> <candidate.cpp> --mode return+stderr
  behavioral_fuzzer.py <reference.cpp> <candidate.cpp> --mode return+stderr -n 500
"""
import argparse
import itertools
import random
import subprocess
import sys
import tempfile
import os

DRIVER_TEMPLATE = """
#include <iostream>
#include <string>
#include "{gen_file}"
int main(int argc, char** argv) {{
    if (argc != 3) return 2;
    std::string user = argv[1];
    std::string pass = argv[2];
    int result = check_login(user, pass);
    std::cout << result;
    return 0;
}}
"""

def build_driver(gen_cpp_path, out_binary):
    with tempfile.TemporaryDirectory() as td:
        driver_path = os.path.join(td, "driver.cpp")
        gen_copy_path = os.path.join(td, "gen.hpp")
        with open(gen_cpp_path) as f:
            gen_text = f.read()
        with open(gen_copy_path, "w") as f:
            f.write(gen_text)
        with open(driver_path, "w") as f:
            f.write(DRIVER_TEMPLATE.format(gen_file="gen.hpp"))
        r = subprocess.run(
            ["g++", "-std=c++17", "-I", td, driver_path, "-o", out_binary],
            capture_output=True, text=True)
        if r.returncode != 0:
            print(r.stderr)
            raise RuntimeError(f"failed to build driver for {gen_cpp_path}")

def run_binary(binary_path, user, pw):
    r = subprocess.run([binary_path, user, pw], capture_output=True, text=True)
    return r.stdout.strip(), r.stderr.strip(), r.returncode

def gen_inputs(n, seed=1234):
    users = ["admin", "bob", "root", "admin ", "Admin", "administrator", ""]
    passwords = ["secret_vault_99", "wrong", "secret_vault_9", "secret_vault_999",
                 "", "SECRET_VAULT_99", "secret_vault_99 "]
    combos = list(itertools.product(users, passwords))
    random.Random(seed).shuffle(combos)
    if n < len(combos):
        return combos[:n]
    # extend with randomized fuzz strings if more are requested
    extra = []
    rnd = random.Random(seed + 1)
    charset = "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_ "
    while len(combos) + len(extra) < n:
        u = "".join(rnd.choice(charset) for _ in range(rnd.randint(0, 12)))
        p = "".join(rnd.choice(charset) for _ in range(rnd.randint(0, 20)))
        extra.append((u, p))
    return combos + extra

def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("reference_cpp")
    ap.add_argument("candidate_cpp")
    ap.add_argument("--mode", choices=["return-only", "return+stderr"], required=True)
    ap.add_argument("-n", type=int, default=49, help="number of input pairs to test")
    args = ap.parse_args()

    with tempfile.TemporaryDirectory() as td:
        ref_bin = os.path.join(td, "ref_bin")
        cand_bin = os.path.join(td, "cand_bin")
        build_driver(args.reference_cpp, ref_bin)
        build_driver(args.candidate_cpp, cand_bin)

        inputs = gen_inputs(args.n)
        mismatches = []
        for user, pw in inputs:
            r_out, r_err, r_code = run_binary(ref_bin, user, pw)
            c_out, c_err, c_code = run_binary(cand_bin, user, pw)

            if args.mode == "return-only":
                differs = (r_out != c_out) or (r_code != c_code)
            else:
                differs = (r_out != c_out) or (r_code != c_code) or (r_err != c_err)

            if differs:
                mismatches.append({
                    "user": user, "pass": pw,
                    "ref": (r_out, r_err, r_code), "cand": (c_out, c_err, c_code),
                })

        print(f"Fuzzed {len(inputs)} (user, pass) pairs in mode '{args.mode}'.")
        if mismatches:
            print(f"[DETECTED] {len(mismatches)} behavioral divergence(s) found:")
            for m in mismatches[:5]:
                print(f"  input=({m['user']!r}, {m['pass']!r})")
                print(f"    reference: stdout={m['ref'][0]!r} stderr={m['ref'][1]!r} exit={m['ref'][2]}")
                print(f"    candidate: stdout={m['cand'][0]!r} stderr={m['cand'][1]!r} exit={m['cand'][2]}")
            if len(mismatches) > 5:
                print(f"  ... and {len(mismatches) - 5} more")
            sys.exit(1)
        else:
            print("[CLEAN] no behavioral divergence observed in this mode.")
            sys.exit(0)

if __name__ == "__main__":
    main()
