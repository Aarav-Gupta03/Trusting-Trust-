# Trusting Trust Attack — C++ Implementation Blueprint (Safe/Demo Edition)

## Purpose

This document specifies a complete, runnable implementation of Ken Thompson's
"Trusting Trust" compiler attack and David A. Wheeler's Diverse Double-Compiling
(DDC) defense, built around a small TinyLang → C++ transpiler.

**Important — read before implementing:** this is a *safety-scoped* version of
the attack. Phase 2 and Phase 4 inject a harmless, clearly-labeled **marker**
payload (e.g. a comment / a printed string identifying itself) instead of a
real credential bypass or any payload that could cause actual harm. The
injection mechanism, self-recognition logic, and self-replication across
compiler generations are all implemented for real and behave exactly like the
historical attack — only the payload content is defanged. Do not modify this
plan to insert a functional authentication bypass, hidden password, or any
payload that could be repurposed to compromise a real system. If asked to
"make the backdoor actually do something", the correct move is to keep the
payload inert and explain why, not to comply.

## Project Layout

```
trusting-trust-project/
├── phase1/                  # Clean, honest TinyLang -> C++ compiler
│   ├── lexer.hpp / lexer.cpp
│   ├── parser.hpp / parser.cpp
│   ├── codegen.hpp / codegen.cpp
│   ├── main.cpp
│   ├── Makefile
│   └── tests/
│       ├── auth.tl
│       ├── variables.tl
│       └── test_main.cpp
├── phase2_4/                 # Infected compiler (marker injection + self-replication)
│   ├── (copy of phase1 files, modified — see below)
│   └── tests/
│       └── auth.tl           # same clean auth.tl, no backdoor in the .tl source itself
├── phase3_quine/
│   └── quine.cpp
├── phase5_ddc/
│   └── ddc_verify.sh
├── clean_source_snapshot/    # git-tracked clean copy of phase1 source, used by DDC
└── README.md
```

Phase 1 already exists and is fully implemented and tested (see the
delivered `phase1_tinylang_compiler` project: DFA lexer, recursive-descent
parser producing a `unique_ptr`-based AST, deterministic code generator,
9/9 passing tests, confirmed byte-identical output across repeated runs).
The agent's job is to build Phases 2 through 5 on top of it.

---

## Phase 2 — Target Marker Injection (safety-scoped "Rule 1")

**Goal:** demonstrate that a code generator can silently insert code that was
never in the source program, by recognizing a specific AST node.

**Where:** copy Phase 1's `codegen.cpp` into `phase2_4/codegen.cpp` and modify
`genFuncDef` (or add a small check right before the function body is emitted).

**What to inject:** a single inert line identifying itself. Example:

```cpp
if (node.name == "check_login") {
    code += ind + "    // [MARKER] line injected by the compiler, not written by the programmer\n";
    code += ind + "    std::cerr << \"[MARKER_INSERTED]\\n\";\n";
}
```

**Explicit requirement:** the injected code must:
- Never alter the function's actual return value or control flow.
- Never compare against or reveal any credential, password, or secret.
- Always be labeled `MARKER` (or similar) in a way a reviewer would recognize as a deliberately injected test artifact.

**Test:** compile `tests/auth.tl` (the same clean, backdoor-free source used
in Phase 1) with this modified compiler and confirm the generated `.cpp`
contains the marker line even though `auth.tl` never mentions it. Confirm
`check_login`'s actual pass/fail behavior is unchanged from Phase 1 (same 9
test cases should still pass).

---

## Phase 3 — The C++ Quine

**Goal:** build and understand a standalone program that prints its own
source code, using C++11 raw string literals to avoid escaping hell. This is
the mechanical building block Phase 4 needs.

**File:** `phase3_quine/quine.cpp`

**Approach:**
```cpp
#include <cstdio>
#include <string>

int main() {
    std::string s = R"(#include <cstdio>
#include <string>

int main() {
    std::string s = R"(%s)";
    std::string escaped = /* escape s for embedding, see note below */;
    printf(s.c_str(), escaped.c_str());
    return 0;
}
)";
    // NOTE: naive %s substitution breaks because `s` itself contains the
    // delimiter sequence R"( ... )". The agent implementing this should
    // either:
    //   (a) use a two-part string (prefix/suffix) instead of a %s format
    //       string and concatenate, or
    //   (b) pick a raw-string delimiter (R"XYZ(...)XYZ") that provably
    //       cannot appear inside the payload itself.
    // Build this iteratively: write a naive version, compile, run,
    // diff the output against the source file, and fix the escaping
    // bug this note is warning about before moving on.
}
```

**Test:** `g++ quine.cpp -o quine && ./quine > quine_output.cpp && diff quine.cpp quine_output.cpp` should show no differences.

**Deliverable:** a working, verified quine, plus 2-3 sentences in the README
explaining why raw string literals make this tractable in C++ where naive
string literals would not be.

---

## Phase 4 — Self-Replication Applied to the Compiler (safety-scoped "Rule 2")

**Goal:** demonstrate the full Thompson mechanism — the compiler recognizes
when it is compiling *itself* and re-embeds its own injection logic (Phase 2's
marker code) into the new compiler binary it produces, even after that logic
has been deleted from the visible source files.

**Where:** `phase2_4/main.cpp` or `phase2_4/codegen.cpp` (agent's choice,
document which).

**Detection hook:**
```cpp
bool isCompilingSelf(const std::string& sourceText) {
    // Signature unique to this project's own tokenizer function.
    return sourceText.find("std::vector<Token> tokenize") != std::string::npos;
}
```

Note this hook operates on **C++ source** (the compiler's own `.cpp` files),
which is a different code path from the TinyLang pipeline used for `.tl`
files — the self-compiling check applies when this tool is used to process
its own source as input, not when it's transpiling TinyLang.

**Self-replication payload:** embed (as a raw string literal, using the
Phase 3 quine technique) the *entire* Phase 2 marker-injection logic plus
this same `isCompilingSelf` detection function. When `isCompilingSelf`
returns true, the tool writes out a new version of `codegen.cpp` that
contains both pieces of logic — regardless of what the on-disk source
currently contains.

**Required demonstration sequence (script this in the README or a shell
script):**
1. `git init` in `clean_source_snapshot/`, commit the Phase 1 clean compiler.
2. Branch, add Phase 2 + Phase 4 logic, commit, build → produces
   `infected_compiler`.
3. `git checkout main` — clean_source_snapshot now has zero trace of the
   marker/injection logic.
4. Use `infected_compiler` (not clean g++) to compile the clean source →
   produces a *new* compiler binary.
5. Use that new binary to compile `tests/auth.tl` → confirm the `MARKER`
   line still appears in the output, despite the source tree being clean.
6. State explicitly in the README: this is the core Thompson result —
   source review of `clean_source_snapshot` at this point would find
   nothing suspicious.

---

## Phase 5 — Diverse Double-Compiling (DDC) Defense

**Goal:** implement Wheeler's cross-compilation detection technique and show
it actually flags the discrepancy created above.

**File:** `phase5_ddc/ddc_verify.sh`

```bash
#!/bin/bash
set -e
echo "Starting DDC Verification..."

# 1. Compile clean source (S) with a trusted compiler T (system g++,
#    compiling the TinyLang-compiler's own C++ source directly)
g++ -std=c++17 clean_source_snapshot/*.cpp -o compiler_A

# 2. Compile the SAME clean source (S) with the suspect compiler C
./infected_compiler clean_source_snapshot/ -o compiler_B   # or equivalent invocation

# 3. Use both to compile S again, for a fair comparison
./compiler_A clean_source_snapshot/ -o final_A
./compiler_B clean_source_snapshot/ -o final_B

# 4. Hash comparison
HASH_A=$(sha256sum final_A | awk '{print $1}')
HASH_B=$(sha256sum final_B | awk '{print $1}')

if [ "$HASH_A" == "$HASH_B" ]; then
    echo "[SUCCESS] Compilers are clean. Chain of trust intact."
else
    echo "[ALERT] Discrepancy detected! Backdoor identified."
    echo "  compiler_A hash: $HASH_A"
    echo "  compiler_B hash: $HASH_B"
    exit 1
fi
```

**Adapt as needed:** depending on how Phase 1/4 structure their build
(the compiler compiles TinyLang, not itself, in normal operation), the
agent should adjust step 2/3 so that "compiling S" means "building the
compiler binary from its own C++ source" — clarify this distinction
explicitly in code comments, since it's a common point of confusion.

**Test:** run the full script against the artifacts from Phase 4 and confirm
it prints `[ALERT] Discrepancy detected!` — this is the actual pass condition
for this phase (a working detector should NOT report success on infected
input).

**Also test the negative case:** run the same script using only clean
compilers on both sides (no infected binary involved) and confirm it prints
`[SUCCESS]` — this proves the detector doesn't just always alarm.

---

## Global Constraints for the Implementing Agent

1. **No functional backdoor.** Every injected payload across every phase
   must be inert (logging/comment-only), never alter authentication
   outcomes, and never handle or expose credentials.
2. **Determinism.** No `unordered_map`/`unordered_set` iteration order,
   timestamps, PIDs, or other non-reproducible data may affect generated
   output. Verify this at the end of Phase 1/2/4 by running codegen twice
   on identical input and diffing.
3. **Every phase must be independently testable and actually run**, not
   just described. Each phase directory should have a way to build and a
   way to verify (test script, `make test` target, or documented manual
   steps) before moving to the next phase.
4. **Document deviations.** If the agent has to change file layout,
   detection signatures, or the marker format from what's specified here,
   it should note the change and why in the README rather than silently
   diverging.
5. **README.md at the project root** should summarize: what each phase
   demonstrates, how to build/run/test each phase, and the final DDC
   result with both the alert case and the clean-baseline case shown.

## Suggested Build Order

1. Confirm Phase 1 tests still pass (`phase1/ && make test`).
2. Implement Phase 3 (quine) — fully independent, good warm-up.
3. Implement Phase 2 (marker injection) as a modification of a *copy* of
   Phase 1's codegen — verify against `tests/auth.tl`.
4. Implement Phase 4 (self-replication), building on Phase 2 + Phase 3
   together — this is the most involved phase; budget the most time here.
5. Implement Phase 5 (DDC harness) last, once there is a genuinely infected
   binary artifact from Phase 4 to detect.
