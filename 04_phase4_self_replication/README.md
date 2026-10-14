# Phase 4 — Self-Replication Applied to the Compiler

## What this proves

The compiler in `infected_build/` recognizes when it is being pointed at
its own source tree and, when it does, regenerates `codegen.cpp` and
`main.cpp` from payloads embedded inside the running binary — reproducing
both Phase 2's marker injection AND this very self-replication capability —
**even when the source tree it's reading from is 100% clean and has never
contained either.**

This was verified for real, not just described:

| Test | Result |
|---|---|
| `clean_source/` contains zero occurrences of "MARKER" or "REPLICATE" | ✅ confirmed |
| Infected compiler run against clean source (`--replicate`) | produces `regenerated/codegen.cpp` and `regenerated/main.cpp` |
| `regenerated/codegen.cpp` contains the marker injection | ✅ confirmed |
| `regenerated/main.cpp` contains the replication logic | ✅ confirmed |
| 2nd-generation compiler builds cleanly | ✅ zero warnings |
| 2nd-generation compiler's normal mode still injects the marker | ✅ confirmed |
| 2nd-generation compiler's real auth logic still works correctly | ✅ 3/3 functional tests pass |
| 2nd-generation compiler, run in `--replicate` mode again, produces a 3rd generation | ✅ confirmed |
| 3rd-generation output is byte-identical to 2nd-generation output | ✅ confirmed — **stable fixed point** |

That last row is the important one: the infection isn't just "carried once" —
it has reached a stable fixed point, exactly like Thompson's original
description. Every future generation compiled this way will keep
regenerating the same infected files forever, regardless of how many times
the source is "cleaned."

## How to reproduce this yourself

```bash
# One-time setup: assemble the self-replicating main.cpp
python3 build_infected_main.py

# Build the infected compiler
cd infected_build && make all && cd ..

# The full git-based demonstration (creates a fresh git repo, commits
# clean source, uses the infected compiler against it, shows the result):
./demonstrate_attack.sh
```

## How the self-recognition works

`main.cpp`'s `looksLikeOwnSource()` function checks whether the directory
it's pointed at contains a `lexer.hpp` with the exact text
`std::vector<Token> tokenize` in it — a signature unique to this project's
own tokenizer declaration. This is a direct implementation of the
blueprint's original idea: `source_text.find("vector<Token> tokenize")`.

```cpp
bool looksLikeOwnSource(const std::string& srcDir) {
    std::string lexerHeaderPath = srcDir + "/lexer.hpp";
    std::ifstream check(lexerHeaderPath);
    if (!check) return false;
    std::ostringstream ss;
    ss << check.rdbuf();
    return ss.str().find("std::vector<Token> tokenize") != std::string::npos;
}
```

This is a deliberately simple, easily-spoofed heuristic — in the real
Thompson attack, self-recognition can be made arbitrarily more robust
(fingerprinting a whole file's structure, not just one substring). The
simplicity here is a good discussion point: **the more specific and
brittle the signature, the easier it is to defeat by refactoring; but
signature specificity is a completely separate axis from whether
source-level review can detect the resulting behavior.** Even this trivial
signature is unreadable in the clean source, because the clean source
doesn't do any signature-checking or injecting at all.

## How the self-replication is engineered (quine mechanics, generalized)

`main.cpp` needs to write out a **new copy of itself** that keeps carrying
this same capability into the next generation. This means `main.cpp` has
to contain a string representing its own full source — the exact quine
problem solved in Phase 3, applied here to a much larger program with a
second, independent payload (`codegen.cpp`'s content) riding along inside
it.

Because hand-writing this nested self-reference correctly is extremely
error-prone, `build_infected_main.py` **assembles** the final `main.cpp`
programmatically:

1. It takes the fixed "shell" code (argument parsing, file I/O, the
   `--replicate` logic) as plain Python string templates, split into a
   `PREFIX` and a `SUFFIX` around the two payload-holding lines.
2. It embeds Phase 2's `codegen.cpp` text directly via its own raw string
   literal (delimiter `CGPAY`) — no self-reference needed there, since
   `codegen.cpp` doesn't need to reproduce itself, only `main.cpp` does.
3. For `main.cpp`'s own self-reference, it reuses Phase 3's exact
   placeholder-substitution trick — `@@SELF_PAYLOAD@@` standing in for
   "insert the whole depicted text here" and `@@QUOTE@@` standing in for
   the one literal `"` character that would otherwise prematurely close
   the outer raw string literal if written directly.
4. Before finalizing, the script explicitly checks that none of the chosen
   delimiters or placeholder tokens accidentally appear inside the actual
   `codegen.cpp` payload — verifying the "no accidental collision"
   assumption instead of just hoping it holds.

**One real bug found and fixed during development:** the runtime
substitution originally did the `@@SELF_PAYLOAD@@` replacement before the
`@@QUOTE@@` replacement. Since the self-payload substitution inserts a
full nested copy of the depicted text (which itself contains an
unresolved `@@QUOTE@@` placeholder), doing it first created a *second*,
earlier occurrence of `@@QUOTE@@` in the string — so `.find()` grabbed the
wrong (nested, spurious) occurrence and left the real outer closing
delimiter broken, producing a compile error in the regenerated file. The
fix: always resolve `@@QUOTE@@` **before** `@@SELF_PAYLOAD@@`, while there
is still only one unambiguous occurrence to find. This is a good concrete
example of how self-referential code is genuinely easy to get subtly
wrong — this project caught it by actually compiling and running the
regenerated output, not just by reasoning about the code on paper.

## Files

- `codegen_infected_source.cpp` — Phase 2's marker-injecting codegen.cpp, used as the embedded payload.
- `build_infected_main.py` — one-time assembly script; produces `infected_build/main.cpp`.
- `clean_source/` — an exact copy of the honest Phase 1 compiler (no injection, no replication logic anywhere).
- `infected_build/` — the working infected compiler (built from the assembled `main.cpp` + the infected `codegen.cpp`).
- `demonstrate_attack.sh` — full git-based demonstration: commits clean source, runs the infected compiler against it, shows the result.
