#!/usr/bin/env python3
"""
fuzzer_scaling.py

The behavioral fuzzer scales with the number of INPUT PAIRS tested, not
with source size N - a genuinely different dimension from
scaling_benchmark.py. Measured separately here for an honest, correctly
labeled report rather than conflating the two.
"""
import csv
import os
import subprocess
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)

SIZES = [10, 50, 100, 250, 500]

def main():
    csv_path = os.path.join(ROOT, "benchmark_data", "fuzzer_scaling_results.csv")
    rows = []
    for n in SIZES:
        t0 = time.perf_counter()
        subprocess.run(
            ["python3", os.path.join(HERE, "behavioral_fuzzer.py"),
             "/tmp/dm_clean.cpp", "/tmp/dm_plain.cpp", "--mode", "return+stderr", "-n", str(n)],
            capture_output=True, text=True)
        t1 = time.perf_counter()
        rows.append({"num_inputs": n, "fuzzer_time_s": round(t1 - t0, 5)})
        print(f"n_inputs={n}: {t1 - t0:.4f}s")
    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=["num_inputs", "fuzzer_time_s"])
        writer.writeheader()
        writer.writerows(rows)
    print(f"\nWrote {csv_path}")

if __name__ == "__main__":
    main()
