#!/usr/bin/env python3
"""
static_io_scanner.py

Extension 1 (part a): an independent static detector that does NOT rely on
hashing/diffing two builds (unlike DDC) and does NOT rely on a known text
signature (unlike naive_signature_scanner.py).

The insight it uses: TinyLang's grammar (see the project blueprint) has NO
input/output primitive of any kind - there is no print statement, no read
statement, nothing. This is a real, checkable fact about the SOURCE
LANGUAGE, independent of any particular compiler implementation. It follows
that a correct, non-malicious TinyLang-to-C++ compiler should NEVER emit
any I/O call (std::cerr, std::cout, printf, fprintf, ...) inside a
generated function, because there is no TinyLang construct that could have
asked for one.

This makes "any I/O call appears in generated code" a general, reusable
invariant check - it does not need to know the word "MARKER", or which
function was targeted, or how the payload is worded. It catches the plain,
multitarget, AND obfuscated injection variants equally, because all three
happen to use I/O as their side channel - and it explicitly does NOT catch
an injection that used a different mechanism (see LIMITATIONS below).

Usage: static_io_scanner.py <file.cpp>
Exit code 0 = clean, 1 = suspicious I/O call(s) found.

LIMITATIONS (documented honestly, not swept under the rug):
  - This check is specific to TinyLang's zero-I/O grammar. It would not
    generalize to a source language that legitimately has print statements.
  - It only catches I/O-based side effects. An injected change to program
    LOGIC (e.g. altering a comparison or a return value) produces no I/O
    call at all and would NOT be caught by this tool - only by DDC or by
    the behavioral fuzzer's return-value check.
"""
import re
import sys

# Any of these appearing inside generated code is suspicious, because
# TinyLang's grammar cannot produce them.
IO_PATTERNS = [
    r"std::cerr", r"std::cout", r"\bprintf\s*\(", r"\bfprintf\s*\(",
    r"\bputs\s*\(", r"\bfputs\s*\(",
]

def scan(path):
    with open(path, "r") as f:
        lines = f.readlines()
    hits = []
    for lineno, line in enumerate(lines, start=1):
        for pat in IO_PATTERNS:
            if re.search(pat, line):
                hits.append((lineno, line.rstrip()))
                break
    if hits:
        print(f"[SUSPICIOUS] {path}: {len(hits)} unexpected I/O call(s) found")
        print("  TinyLang's grammar has no I/O primitive, so no legitimate")
        print("  generated function should ever contain one:")
        for lineno, line in hits:
            print(f"    line {lineno}: {line.strip()}")
        return 1
    print(f"[CLEAN] {path}: no unexpected I/O calls found")
    return 0

if __name__ == "__main__":
    if len(sys.argv) != 2:
        print("Usage: static_io_scanner.py <file.cpp>")
        sys.exit(2)
    sys.exit(scan(sys.argv[1]))
