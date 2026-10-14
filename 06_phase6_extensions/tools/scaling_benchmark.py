#!/usr/bin/env python3
"""
scaling_benchmark.py

Measures actual wall-clock time for compilation and for each detector as a
function of N (number of functions in the synthetic TinyLang source),
producing real, measured scaling data rather than asserted complexity.

Usage: scaling_benchmark.py
Writes results to benchmark_data/scaling_results.csv and prints a table.
"""
import csv
import os
import subprocess
import sys
import time

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
TRUSTED_TLC = "/tmp/tlc_clean"
MULTITARGET_TLC = os.path.join(ROOT, "variants", "multitarget", "tlc")

SIZES = [10, 50, 100, 250, 500, 1000]

def timed(fn, *args, **kwargs):
    t0 = time.perf_counter()
    result = fn(*args, **kwargs)
    t1 = time.perf_counter()
    return (t1 - t0), result

def compile_tl(tlc_binary, tl_path, out_path):
    subprocess.run([tlc_binary, tl_path, "-o", out_path], capture_output=True, text=True, check=True)

def main():
    os.makedirs(os.path.join(ROOT, "benchmark_data"), exist_ok=True)
    csv_path = os.path.join(ROOT, "benchmark_data", "scaling_results.csv")

    rows = []
    for n in SIZES:
        tl_path = f"/tmp/bench_{n}.tl"
        subprocess.run(["python3", os.path.join(HERE, "gen_synthetic.py"), str(n), tl_path],
                        capture_output=True, text=True, check=True)

        clean_cpp = f"/tmp/bench_{n}_clean.cpp"
        mt_cpp = f"/tmp/bench_{n}_mt.cpp"

        t_compile_clean, _ = timed(compile_tl, TRUSTED_TLC, tl_path, clean_cpp)
        t_compile_mt, _ = timed(compile_tl, MULTITARGET_TLC, tl_path, mt_cpp)

        t_naive, _ = timed(subprocess.run,
            ["python3", os.path.join(HERE, "naive_signature_scanner.py"), mt_cpp],
            capture_output=True, text=True)

        t_static, _ = timed(subprocess.run,
            ["python3", os.path.join(HERE, "static_io_scanner.py"), mt_cpp],
            capture_output=True, text=True)

        t_ddc, _ = timed(subprocess.run,
            ["python3", os.path.join(HERE, "ddc_compare.py"), TRUSTED_TLC, MULTITARGET_TLC, tl_path],
            capture_output=True, text=True)

        # fuzzer intentionally excluded from the N-scaling sweep: it scales
        # with number of INPUT pairs tested, not with source size N, and is
        # reported separately (see README) to avoid conflating two
        # different scaling dimensions in one chart.

        rows.append({
            "N": n,
            "compile_time_clean_s": round(t_compile_clean, 5),
            "compile_time_multitarget_s": round(t_compile_mt, 5),
            "naive_scanner_time_s": round(t_naive, 5),
            "static_io_scanner_time_s": round(t_static, 5),
            "ddc_compare_time_s": round(t_ddc, 5),
        })
        print(f"N={n}: compile(clean)={t_compile_clean:.4f}s "
              f"compile(mt)={t_compile_mt:.4f}s naive={t_naive:.4f}s "
              f"static_io={t_static:.4f}s ddc={t_ddc:.4f}s")

    with open(csv_path, "w", newline="") as f:
        writer = csv.DictWriter(f, fieldnames=list(rows[0].keys()))
        writer.writeheader()
        writer.writerows(rows)
    print(f"\nWrote {csv_path}")

if __name__ == "__main__":
    main()
