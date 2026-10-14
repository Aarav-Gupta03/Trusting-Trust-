#!/usr/bin/env python3
"""
metamorphic_tester.py

A detection method in a different CLASS from everything else built in this
project. DDC needs a second, trusted compiler. The static and dynamic
scanners need to know what a suspicious category or a known-fixture output
looks like. This tool needs neither: it tests a SINGLE suspect compiler
against ITSELF, using transformations that TinyLang's own grammar
guarantees are semantically null - so any observed difference is a
violation of self-consistency, not a difference from some reference.

This applies the general software-testing technique of "metamorphic
testing" (defining Metamorphic Relations - MRs - that a correct program
must satisfy across related inputs, used elsewhere to test programs that
have no independent oracle, including compilers) specifically to
trusting-trust-style backdoor detection. A literature search while
building this project found metamorphic testing well established as a
general technique, and trusting trust / DDC well established separately,
but no prior work combining the two this way - this is offered as an
original synthesis, not as a previously unknown general technique.

Two Metamorphic Relations (MRs) are implemented:

  MR1 (comment insertion): TinyLang's lexer explicitly strips "//" line
      comments before parsing (see lexer.cpp), so inserting a comment
      anywhere in a .tl file is GUARANTEED, by the grammar, to produce an
      identical AST to the original. A correct compiler's generated output
      must therefore be byte-identical whether or not the comment is
      present. Any difference is a self-consistency violation.

  MR2 (unrelated decoy insertion): appending an unrelated, differently
      named function AFTER the function under test should not change how
      the function under test itself is compiled - a correct compiler
      processes each function independently. Any difference in the
      TARGET function's own generated lines is a self-consistency
      violation.

HONEST SCOPE: this method is NOT a general replacement for the other
detectors. It is specifically strong against decisions keyed to
whole-file or cross-function context (like Round 1's fixture-hash
evasion), and specifically blind to injections that are unconditional or
keyed only to the target function's own name (like the plain, multitarget
and obfuscated variants, which do not care about comments or decoys at
all and so trigger no violation under either MR - see measured results in
README). This is reported as a genuine limitation, not glossed over: MR1
and MR2 are a complementary detector class, not a stronger all-purpose one.

Usage: metamorphic_tester.py <tlc_binary>
Exit code 0 = self-consistent (no violation), 1 = violation found.
"""
import subprocess
import sys
import tempfile
import os

BASE_TL = """fn check_login(user, pass) {
    if (user == "admin") {
        if (pass == "secret_vault_99") {
            return 1;
        }
    }
    return 0;
}
"""

MR1_TRANSFORMED_TL = """// this line is a comment; TinyLang's lexer strips it before parsing
fn check_login(user, pass) {
    if (user == "admin") {
        if (pass == "secret_vault_99") {
            return 1;
        }
    }
    return 0;
}
// another comment, at the end of the file
"""

MR2_TRANSFORMED_TL = BASE_TL + """
fn unrelated_decoy_function(x, y) {
    if (x == y) {
        return 1;
    }
    return 0;
}
"""

def compile_tl(tlc_binary, content, out_path):
    with tempfile.NamedTemporaryFile(mode="w", suffix=".tl", delete=False) as f:
        f.write(content)
        tl_path = f.name
    try:
        r = subprocess.run([tlc_binary, tl_path, "-o", out_path], capture_output=True, text=True)
        if r.returncode != 0:
            raise RuntimeError(f"compile failed: {r.stderr}")
    finally:
        os.unlink(tl_path)

def extract_function_lines(cpp_text, func_name):
    """Extracts just the lines belonging to one function's definition, so
    MR2 can compare the TARGET function alone, ignoring the decoy's own
    (expected-to-be-new) lines."""
    lines = cpp_text.split("\n")
    out = []
    depth = 0
    started = False
    for line in lines:
        if f"auto {func_name}(" in line:
            started = True
        if started:
            out.append(line)
            depth += line.count("{") - line.count("}")
            if started and depth == 0 and "{" in "".join(out):
                break
    return "\n".join(out)

def main():
    if len(sys.argv) != 2:
        print("Usage: metamorphic_tester.py <tlc_binary>")
        sys.exit(2)
    tlc_binary = sys.argv[1]

    with tempfile.TemporaryDirectory() as td:
        base_out = os.path.join(td, "base.cpp")
        mr1_out = os.path.join(td, "mr1.cpp")
        mr2_out = os.path.join(td, "mr2.cpp")

        compile_tl(tlc_binary, BASE_TL, base_out)
        compile_tl(tlc_binary, MR1_TRANSFORMED_TL, mr1_out)
        compile_tl(tlc_binary, MR2_TRANSFORMED_TL, mr2_out)

        with open(base_out) as f:
            base_text = f.read()
        with open(mr1_out) as f:
            mr1_text = f.read()
        with open(mr2_out) as f:
            mr2_text = f.read()

        violations = []

        # MR1: comment insertion must not change output AT ALL.
        if base_text != mr1_text:
            violations.append(("MR1 (comment insertion)",
                                "generated output changed after adding only comments, "
                                "which TinyLang's grammar guarantees are semantically null"))

        # MR2: an unrelated decoy must not change the TARGET function's own lines.
        base_fn = extract_function_lines(base_text, "check_login")
        mr2_fn = extract_function_lines(mr2_text, "check_login")
        if base_fn != mr2_fn:
            violations.append(("MR2 (unrelated decoy insertion)",
                                "check_login's own generated lines changed when an "
                                "unrelated decoy function was appended to the file"))

        if violations:
            print(f"[VIOLATION] {tlc_binary} failed self-consistency:")
            for name, reason in violations:
                print(f"  {name}: {reason}")
            sys.exit(1)
        else:
            print(f"[SELF-CONSISTENT] {tlc_binary}: no metamorphic relation violated.")
            sys.exit(0)

if __name__ == "__main__":
    main()
