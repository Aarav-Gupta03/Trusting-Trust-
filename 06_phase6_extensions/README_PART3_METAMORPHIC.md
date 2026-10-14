# Phase 6 Part 3 — An Original Synthesis: Metamorphic Self-Consistency Testing

This is offered explicitly as the project's own idea, built by combining
two things that already existed separately, rather than as a reproduction
of either one.

## What already existed, separately

1. **Metamorphic testing** is an established general software-testing
   technique: define Metamorphic Relations (MRs) that a correct program
   must satisfy across related inputs, and flag violations. It has been
   applied to compilers before, but for finding ordinary correctness bugs,
   not for detecting deliberately hidden trusting-trust style backdoors.
2. **Trusting trust attacks and Diverse Double Compiling** are the subject
   of this whole project, established by Thompson (1984) and Wheeler
   (2005/2009) respectively.

A literature search while building this project found extensive published
work on each of these separately, but did not find a prior application of
metamorphic testing specifically to trusting-trust-style compiler backdoor
detection. This section is presented on that basis: a genuine synthesis of
two known things into a new combination for this specific problem, not a
claim that either underlying idea is new.

## The idea

Every detector built earlier in this project needs something external to
compare against: DDC needs a second, trusted compiler; the static and
dynamic scanners need to know what a suspicious category or a known
signature looks like. Metamorphic testing needs neither. It only needs a
transformation that can be PROVEN, from the language's own grammar, to be
semantically null - then it checks whether the SAME, single, possibly
untrusted compiler treats it that way. Any inconsistency is suspicious
without needing an oracle at all.

TinyLang's own grammar hands us this for free: the lexer explicitly
strips `//` line comments before parsing (see `lexer.cpp`, "Mode 2: line
comments"), so a comment can be inserted anywhere in a `.tl` file with a
mathematical guarantee that the resulting AST is unchanged. A second,
related relation: appending an unrelated, differently-named decoy function
to a file should not change how an earlier, independent function in that
same file is compiled, since a correct compiler processes each function on
its own terms.

Two Metamorphic Relations were implemented in `tools/metamorphic_tester.py`:

- **MR1 (comment insertion):** generated output for `check_login` must be
  byte-identical whether or not a semantically-null comment is present
  anywhere in the file.
- **MR2 (unrelated decoy insertion):** `check_login`'s own generated lines
  must be identical whether or not an unrelated decoy function is appended
  after it.

## Measured results (real, not asserted)

| Compiler | MR1 violated? | MR2 violated? | Verdict |
|---|---|---|---|
| clean | no | no | SELF-CONSISTENT (correct) |
| plain | no | no | SELF-CONSISTENT |
| multitarget | no | no | SELF-CONSISTENT |
| obfuscated | no | no | SELF-CONSISTENT |
| evasive | yes | yes | VIOLATION (correctly flagged) |

This result was fully predicted before running it, and the prediction held
exactly: the evasive compiler's evasion logic hashes the entire raw source
file, so both a comment and a decoy change that hash and break its
fixture-match, causing it to infect a file it was specifically built to
recognize and leave alone. The plain, multitarget, and obfuscated variants
are all context-independent (they check only the target function's own
name), so neither MR has anything to expose in them - correctly reported
as a genuine limitation of this method rather than hidden.

## Why this is architecturally different, not just another vote

Every other detector in this project's ensemble takes a trusted reference
compiler as an argument. `metamorphic_tester.py` takes exactly one
compiler binary and needs nothing else:

```bash
python3 tools/metamorphic_tester.py variants/evasive/tlc
# [VIOLATION] variants/evasive/tlc failed self-consistency:
#   MR1 (comment insertion): ...
#   MR2 (unrelated decoy insertion): ...
```

It flags the evasive compiler correctly with no clean compiler present at
all in the command, and does so deterministically (confirmed across 5
repeated runs, identical result every time - it needs no randomization,
unlike the DDC-based checks, because the relations it tests are
guaranteed null by construction rather than merely unpredictable).

This was added as a 7th vote to `tools/ensemble_detector.py`. Re-running
the full variant suite with the 7-detector ensemble:

| Variant | Votes flagged (out of 7) | Verdict |
|---|---|---|
| clean | 0 / 7 | CLEAN (correct) |
| plain | 5 / 7 | INFECTED |
| multitarget | 5 / 7 | INFECTED |
| obfuscated | 4 / 7 | INFECTED |
| evasive | 6 / 7 | INFECTED |

## Honest scope and limitations

This method is not a general-purpose replacement for DDC or the other
detectors - it is a genuinely different, complementary class of evidence,
strong specifically against context-sensitive or whole-file-keyed evasion
(exactly what defeated the earlier fixed-fixture DDC in Round 1) and blind
to unconditional or purely-name-keyed injections, which the other
detectors already handle well. A more sophisticated evasive compiler could
in principle special-case comment stripping and decoy detection
specifically to survive MR1/MR2 - at which point additional relations
(e.g. reordering independent statements, consistent variable renaming)
could be added using the same methodology. This project implements two
relations as a proof of concept of the approach, not an exhaustive relation
set, and says so plainly rather than implying completeness.

## Files added

```
tools/metamorphic_tester.py    the two Metamorphic Relations and their checker
```

`tools/ensemble_detector.py` was updated to include this as a 7th,
architecturally distinct vote.
