# Phase 6 Part 2 — The Evasion Arms Race and Ensemble Detection

This extends Phase 6 Part 1 with a genuine two-round attacker/defender
arms race, plus an ensemble detector that combines every independent
check built across this project into a single verdict. Every claim below
was actually run; two real bugs were found and fixed during development
and are documented rather than hidden, because they are themselves
evidence of genuine engineering rather than a scripted result.

## Round 1: an evasive infection that defeats fixed-fixture DDC

`variants/evasive/` is a compiler with the exact same injection mechanism
and exact same inert payload as Phase 2, plus one addition: before
compiling, it computes a hash (FNV-1a) of the exact input source text and
compares it against a hardcoded hash of this project's own published test
fixture, `clean_source/tests/auth.tl`. If they match, injection is
suppressed for that compilation only; any other input is infected
normally.

This is a real, well-known pattern outside of compilers too: detecting a
standard test and behaving differently only during it is the same general
mechanism as automotive emissions test-cycle detection. It requires no
special insight into the verifier's internals — only that the verifier's
test input is published, which it is, since this whole project is
documented openly.

**Measured result:**

```
$ python3 tools/ddc_compare.py /tmp/tlc_clean variants/evasive/tlc clean_source/tests/auth.tl
trusted   (/tmp/tlc_clean): 179f17f27bf45f8813765a47a42e958940d83426b006e6c2f6711e2c15857f92
candidate (variants/evasive/tlc): 179f17f27bf45f8813765a47a42e958940d83426b006e6c2f6711e2c15857f92
[SUCCESS] hashes match.
```

The existing DDC methodology from Phase 5 / Phase 6 Part 1, which always
tests with `auth.tl`, is genuinely fooled. This is a real negative result:
a documented, demonstrated limitation of a defense that Phase 5 had shown
working correctly, now shown failing under a more capable adversary. The
evasive compiler was separately confirmed to still inject the marker
normally on any other input (see `variants/evasive/main.cpp` and the test
log in this project's build history), so this is not merely "the
compiler has a bug that happens to look clean" — it behaves exactly as
designed.

## Round 2: closing the gap without knowing how the evasion works

`tools/randomized_ddc_compare.py` removes the attacker's advantage
directly: instead of testing with the one published, predictable fixture,
it generates a fresh TinyLang program with randomized literal content on
every run, and uses that as the DDC input.

**Measured result, five independent runs, all catching the same evasive
compiler that Round 1 fooled:**

```
seed=1321279335: [ALERT] hashes differ.
seed=2236577933: [ALERT] hashes differ.
seed=1431303832: [ALERT] hashes differ.
seed=1122862668: [ALERT] hashes differ.
seed=644765548:  [ALERT] hashes differ.
```

And confirmed not to false-alarm on a genuinely clean pair, across three
independent runs:

```
[SUCCESS] hashes match.
[SUCCESS] hashes match.
[SUCCESS] hashes match.
```

The important property here: the fix does not require understanding,
detecting, or reverse-engineering the evasion logic at all. It works by
denying the attacker a predictable target, which is a strictly more
general defense than patching around this one specific evasion mechanism.

## Ensemble detector: combining every check into one verdict

`tools/ensemble_detector.py` runs all six checks built across this
project (naive signature scan, static I/O scan, behavioral fuzzing in two
modes, and two independent randomized DDC draws) and reports INFECTED if
2 or more independently flag the candidate.

### A finding, not just a result

The first version of this ensemble had a real bug, found by testing it
against the evasive compiler rather than only against the earlier,
simpler variants: it fed the static and dynamic detectors C++ generated
from the *same fixed fixture* DDC uses, so five of the six "independent"
detectors actually shared the exact blind spot Round 1 exploited. Against
the evasive compiler, only one detector (an early ad hoc randomized DDC
check) caught anything, the 2-vote threshold was not met, and the
ensemble reported a false "CLEAN" verdict on a genuinely infected
compiler. This is documented in the tool's own source comments as well as
here, rather than quietly patched and forgotten, because a false clean
verdict from a "combined, more robust" detector is exactly the kind of
failure worth being honest about.

**The fix had two parts:**
1. Every one of the six checks is now run against a freshly, independently
   randomized TinyLang program each time — no detector, not just DDC, is
   ever handed a predictable input.
2. A check against the fixed, published fixture is deliberately **excluded**
   from the ensemble entirely, rather than merely outvoted. Including a
   known-weak check and hoping other votes compensate is not the same as
   removing the weakness from the trusted pipeline.

A second, separate bug was found in the process: an earlier version of the
randomized test generator also randomized the *target function's name*,
which silently broke the behavioral fuzzer's driver (it calls
`check_login(...)` by a fixed name), causing a compilation failure whose
exit code was being misread as a genuine detection rather than a crash.
Fixed by keeping the function name constant and randomizing only literal
content, which is sufficient to defeat a fixture-hash-keyed evasion.

**Final, corrected measured results (15 runs: 5 variants x 3 seeds each,
all consistent):**

| Variant | Votes flagged (out of 6) | Verdict |
|---|---|---|
| clean | 0 / 6, every seed | CLEAN (correct — no false positive) |
| plain | 5 / 6, every seed | INFECTED (correct) |
| multitarget | 5 / 6, every seed | INFECTED (correct) |
| obfuscated | 4 / 6, every seed | INFECTED (correct — naive scanner alone is fooled, outvoted) |
| evasive | 5 / 6, every seed | INFECTED (correct — Round 1's fixed-fixture blind spot is closed) |

## Honest limitations

This is not a claim that the arms race is "won." A theoretically further
round exists and is not implemented: a compiler could attempt to detect
the *randomization process itself* (e.g. by recognizing statistical
patterns in how this specific generator produces literals) rather than
one fixed fixture. This project does not build or test that round. The
claim made here is narrower and fully supported by the measurements above:
a specific, real evasion was built, a specific, real defense was shown to
fail against it, a specific, real countermeasure was built and shown to
succeed against it repeatedly, and combining detectors correctly requires
also diversifying their inputs, not just their logic — a distinction this
project only discovered by testing its own first attempt and finding it
wrong.

## Files added in this part

```
variants/evasive/                       Round 1: fixture-aware evasive compiler
tools/randomized_ddc_compare.py         Round 2: randomized-input countermeasure
tools/ensemble_detector.py              combined, corrected multi-detector verdict
```

## Reproducing every number above

```bash
# Build the evasive compiler
cd variants/evasive && make all && cd ../..

# Round 1: show the fixed-fixture DDC gets fooled
python3 tools/ddc_compare.py /tmp/tlc_clean variants/evasive/tlc clean_source/tests/auth.tl

# Round 2: show randomized DDC catches it
python3 tools/randomized_ddc_compare.py /tmp/tlc_clean variants/evasive/tlc

# Ensemble, against every variant
python3 tools/ensemble_detector.py /tmp/tlc_clean variants/evasive/tlc --seed 1
python3 tools/ensemble_detector.py /tmp/tlc_clean variants/plain/tlc --seed 1
python3 tools/ensemble_detector.py /tmp/tlc_clean variants/obfuscated/tlc --seed 1
python3 tools/ensemble_detector.py /tmp/tlc_clean /tmp/tlc_clean --seed 1
```
