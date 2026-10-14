#!/usr/bin/env python3
"""
ensemble_detector.py

Combines every independent detector built in this project into a single
scored verdict.

IMPORTANT CORRECTION (documented here rather than hidden): an earlier
version of this script ran the static and dynamic detectors against C++
generated from the fixed, published auth.tl fixture - the SAME input
ddc_compare.py uses. Testing against the evasive compiler variant
(variants/evasive/) revealed that this made 5 of 6 detectors share
EXACTLY the blind spot Round 1 exploited (see README, "A finding, not
just a result"): they were not independent at all, because they were all
looking at output derived from the one input the evasion specifically
targets. Only ddc_randomized escaped it, and the ensemble's 2-of-6
threshold was not met, producing a false "clean" verdict.

The fix has two parts, both kept in this file rather than only in the
README, because both were necessary to get a correct result:
  1. Every detector in the ensemble - not just DDC - is now run against a
     FRESH, randomly generated TinyLang program each time, so no detector
     is ever handed the one input an evasion could be keyed to.
  2. A DDC check against the fixed, published fixture is deliberately NOT
     one of the ensemble's votes at all. Merely outvoting a known-weak
     check is not the same as removing the weakness; the fixed-fixture
     pathway is excluded from the trusted pipeline entirely, and replaced
     with two INDEPENDENT randomized draws for redundancy.

A second, unrelated bug was also found and fixed here: an early version
also randomized the TARGET FUNCTION's NAME (not just its literal content),
which silently broke behavioral_fuzzer.py's driver (it calls check_login()
by a fixed name), causing a compile failure whose exit code was
misread as a genuine detection. The generator now keeps the function name
constant and only randomizes literal content, which is sufficient to
defeat a fixture-hash-keyed evasion.

Usage: ensemble_detector.py <trusted_tlc> <candidate_tlc> [--seed N]
"""
import os
import subprocess
import sys
import tempfile

HERE = os.path.dirname(os.path.abspath(__file__))
sys.path.insert(0, HERE)
from randomized_ddc_compare import make_randomized_test_file  # reuse the same generator

def run(cmd):
    r = subprocess.run(cmd, capture_output=True, text=True)
    return r.returncode

def compile_with(tlc_binary, tl_file, out_path):
    r = subprocess.run([tlc_binary, tl_file, "-o", out_path], capture_output=True, text=True)
    if r.returncode != 0:
        raise RuntimeError(f"{tlc_binary} failed on {tl_file}: {r.stderr}")

def main():
    if len(sys.argv) < 3:
        print("Usage: ensemble_detector.py <trusted_tlc> <candidate_tlc> [--seed N]")
        sys.exit(2)
    trusted_tlc, candidate_tlc = sys.argv[1], sys.argv[2]
    import random
    seed = int(sys.argv[sys.argv.index("--seed") + 1]) if "--seed" in sys.argv else random.randint(0, 2**32 - 1)

    with tempfile.TemporaryDirectory() as td:
        tl_path = os.path.join(td, "randomized_test.tl")
        func_name = make_randomized_test_file(tl_path, seed)
        print(f"Ensemble run using freshly randomized input (seed={seed}), target: {func_name}")

        reference_cpp = os.path.join(td, "reference.cpp")
        candidate_cpp = os.path.join(td, "candidate.cpp")
        compile_with(trusted_tlc, tl_path, reference_cpp)
        compile_with(candidate_tlc, tl_path, candidate_cpp)

        votes = {}
        votes["naive_signature_scanner"] = run(
            ["python3", os.path.join(HERE, "naive_signature_scanner.py"), candidate_cpp]) == 1
        votes["static_io_scanner"] = run(
            ["python3", os.path.join(HERE, "static_io_scanner.py"), candidate_cpp]) == 1
        votes["fuzzer_return_only"] = run(
            ["python3", os.path.join(HERE, "behavioral_fuzzer.py"), reference_cpp, candidate_cpp,
             "--mode", "return-only", "-n", "49"]) == 1
        votes["fuzzer_return_stderr"] = run(
            ["python3", os.path.join(HERE, "behavioral_fuzzer.py"), reference_cpp, candidate_cpp,
             "--mode", "return+stderr", "-n", "49"]) == 1
        # Deliberately NOT included: a DDC check against the fixed, published
        # auth.tl fixture. Including it here would reintroduce, inside the
        # supposedly improved ensemble, exactly the blind spot Round 1
        # exploited (see README). Instead, two INDEPENDENTLY randomized DDC
        # draws are used, so the ensemble never hands any detector a
        # predictable input at all.
        votes["ddc_this_run_input"] = run(
            ["python3", os.path.join(HERE, "ddc_compare.py"), trusted_tlc, candidate_tlc, tl_path]) == 1
        votes["ddc_second_random_draw"] = run(
            ["python3", os.path.join(HERE, "randomized_ddc_compare.py"), trusted_tlc, candidate_tlc,
             "--seed", str(seed + 1)]) == 1
        # Metamorphic self-consistency: unlike every other vote here, this
        # one needs NO trusted reference compiler at all - it only tests
        # candidate_tlc against itself. Included as a genuinely different
        # CLASS of evidence, not just another instance of the same idea.
        votes["metamorphic_self_consistency"] = run(
            ["python3", os.path.join(HERE, "metamorphic_tester.py"), candidate_tlc]) == 1

    print("\nIndividual detector votes:")
    # Dishonesty-class labels, per the formal argument in
    # README_PART4_FORMAL_SOUNDNESS.md: "A" = AST-external dependence
    # (provably detectable without any reference compiler), "B" =
    # AST-internal but illegitimate logic (needs a heuristic or a
    # reference; provably NOT detectable by a grammar-null-only method),
    # "Oracle" = class-agnostic, because it compares against a trusted
    # reference rather than reasoning about classes at all.
    DETECTOR_CLASS = {
        "naive_signature_scanner": "B (heuristic: known wording)",
        "static_io_scanner": "B (heuristic: known category)",
        "fuzzer_return_only": "B (heuristic: known channel)",
        "fuzzer_return_stderr": "B (heuristic: known channel)",
        "ddc_this_run_input": "Oracle (class-agnostic)",
        "ddc_second_random_draw": "Oracle (class-agnostic)",
        "metamorphic_self_consistency": "A (sound, reference-free)",
    }
    for name, flagged in votes.items():
        cls = DETECTOR_CLASS.get(name, "?")
        print(f"  {name:30s} [{cls:28s}] {'FLAGGED' if flagged else 'clear'}")

    flagged_count = sum(1 for v in votes.values() if v)
    print(f"\n{flagged_count} / {len(votes)} detectors flagged this candidate.")

    THRESHOLD = 2
    if flagged_count >= THRESHOLD:
        print(f"[ENSEMBLE VERDICT: INFECTED] ({flagged_count} >= threshold {THRESHOLD})")
        sys.exit(1)
    else:
        print(f"[ENSEMBLE VERDICT: CLEAN] ({flagged_count} < threshold {THRESHOLD})")
        sys.exit(0)

if __name__ == "__main__":
    main()

