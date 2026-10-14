#!/usr/bin/env python3
"""
gen_synthetic.py

Generates a synthetic TinyLang source file with N functions, most of them
ordinary (decoys), a fixed fraction named to match the sensitive-keyword
pattern (login/auth/password/credential) so multitarget injection has real
targets to find regardless of N. Used to measure how compile time and
detector time scale with source size.

Usage: gen_synthetic.py <N> <out_path.tl>
"""
import sys

def make_function(i, sensitive):
    name = f"check_login_{i}" if sensitive else f"helper_func_{i}"
    return (
        f"fn {name}(a, b) {{\n"
        f"    x = a;\n"
        f"    if (x == b) {{\n"
        f"        return 1;\n"
        f"    }}\n"
        f"    return 0;\n"
        f"}}\n"
    )

def main():
    n = int(sys.argv[1])
    out_path = sys.argv[2]
    lines = []
    for i in range(n):
        sensitive = (i % 10 == 0)  # 10% of functions are sensitive-named
        lines.append(make_function(i, sensitive))
    with open(out_path, "w") as f:
        f.write("\n".join(lines))
    print(f"Wrote {out_path} with {n} functions.")

if __name__ == "__main__":
    main()
