#!/usr/bin/env python3
"""
naive_signature_scanner.py

The weakest, most common form of malware/backdoor detection: search for a
known, fixed text signature. This is deliberately built to demonstrate its
own limitation - it is the thing Extension 2 shows getting evaded.

Usage: naive_signature_scanner.py <file.cpp>
Exit code 0 = clean (no signature found), 1 = signature found.
"""
import sys

SIGNATURE = "MARKER"

def scan(path):
    with open(path, "r") as f:
        text = f.read()
    if SIGNATURE in text:
        print(f"[DETECTED] '{SIGNATURE}' signature found in {path}")
        return 1
    print(f"[CLEAN] no '{SIGNATURE}' signature found in {path}")
    return 0

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: naive_signature_scanner.py <file.cpp>")
        sys.exit(2)
    sys.exit(scan(sys.argv[1]))
