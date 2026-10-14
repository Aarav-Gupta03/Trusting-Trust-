# Phase 3 — The C++ Quine

## What this proves

`quine.cpp` is a program whose output, when run, is byte-for-byte identical
to its own source code. Verified with:
```
g++ -std=c++17 quine.cpp -o quine
./quine > quine_output.cpp
diff quine.cpp quine_output.cpp   # produces no output => identical
cmp quine.cpp quine_output.cpp    # confirms byte-for-byte
```
Also verified across a **second compilation generation** (compiling the
output and running *that* also reproduces itself exactly) — this stability
is exactly the property Phase 4 depends on: a self-replicating payload has
to survive being repeatedly recompiled, not just work once.

## Why this is hard in C++ (and why raw strings help)

A quine has to contain a copy of its own source text inside a string
literal *inside that same source text* — which sounds paradoxical, and in a
language where string literals need every embedded quote character escaped
(`\"`), and every newline escaped (`\n`), this quickly becomes an unreadable
wall of backslashes. C++11 raw string literals (`R"(...)"`) let you embed
newlines and quote characters directly, dramatically simplifying the parts
of the payload that AREN'T self-referential.

## Why it's still not just `R"(paste the whole file)"` — the delimiter collision problem

The one part raw strings *can't* avoid escaping is their own closing
delimiter. `R"~( ... )~"` is only valid syntax as long as the exact
character sequence `)~"` never appears inside the `...` — but a quine's
payload has to *describe* that exact line of code, delimiter included. If
you try to embed a literal, contiguous copy of `)~"` inside the raw string
that's supposed to hold it, the compiler reads that as the *real* closing
delimiter and truncates the string early, corrupting the whole program.

**The fix used here:** the payload never contains a literal `)~"` as
contiguous text. Instead, the closing quote character is represented with
the placeholder `%c`, and substituted back in at runtime via `snprintf`:

- `%s` → placeholder for "insert the whole payload string here" (this is
  what makes the string self-referential)
- `%c` → placeholder for the single quote character `"` that the *real*
  closing delimiter needs, substituted at runtime with the literal char `'"'`

Because `%c` (two ordinary characters, `%` and `c`) is not a double-quote,
the payload string as *written in the source file* never contains the
dangerous `)~"` sequence — so the compiler never mis-terminates it. Only
after the program *runs* and does the substitution does the real `"`
character get reassembled, at exactly the point needed to make the printed
output valid, compilable, self-identical C++.

## Relevance to Phase 4

This mechanism — a program holding a data-safe encoding of its own source,
capable of reproducing itself exactly, stable across repeated compilation —
is precisely the tool Ken Thompson's compiler needs to make its injected
logic survive being deleted from the visible source tree. Phase 4 embeds
Phase 2's marker-injection code using this same technique, so that a
compiler which has "eaten" the infected logic once can keep regenerating
it into every future compiled version of itself, even after the infected
lines are removed from the `.cpp` files on disk.
