# Phase 6 (Extensions) — Beyond Hash Comparison

This phase implements four extensions on top of Phases 1 to 5, each built
and verified with real measurements rather than described in the abstract.
It reuses the clean and infected compilers already proven in Phases 1, 2
and 4; it does not re-implement self-replication, which was already fully
demonstrated in Phase 4.

## Extension 1: Detection beyond hash comparison

Two independent detectors were built, neither of which relies on comparing
two builds against each other the way DDC does.

**1a. `tools/static_io_scanner.py`** — a category-based static check. It
uses a real, checkable fact about TinyLang's own grammar: the language has
no input/output primitive of any kind, so a correct compiler should never
emit an I/O call (`std::cerr`, `std::cout`, `printf`, ...) inside generated
code. Any such call is therefore suspicious *by construction*, regardless
of what it prints. This makes it signature-agnostic: it does not need to
know the word "MARKER" or any other specific text.

**1b. `tools/behavioral_fuzzer.py`** — a dynamic detector. It compiles the
generated `check_login` into a small driver, runs it across many `(user,
pass)` pairs, and compares observed behavior between a reference build and
a candidate build. It was run in two modes on purpose:

| Mode | What it compares | Result against the Phase 2 injection |
|---|---|---|
| `return-only` | just the function's return value | **MISSED** — 0/49 divergences |
| `return+stderr` | return value and captured stderr | **CAUGHT** — 49/49 divergences |

This is not a flaw in the fuzzer; it is the actual finding. The injected
payload is deliberately inert with respect to the return value (see Phase
2), so a behavioral test that only checks the "obvious" channel misses it
completely, while a test that also watches side-channel output catches it
on every single input. This demonstrates a real, general principle: a
dynamic test is only as strong as the channels it decides to observe.

## Extension 2: Beyond a fixed signature (the arms race)

`tools/naive_signature_scanner.py` is a deliberately weak baseline: it
greps generated code for the literal text `"MARKER"`. `variants/obfuscated/`
is a compiler variant with the exact same injection mechanism and the exact
same inert behavior as Phase 2, but with the marker's wording changed to
`[DIAG-7f3e]` so the text `"MARKER"` never appears anywhere in its output.

Measured result:

| Detector | plain injection | obfuscated injection |
|---|---|---|
| `naive_signature_scanner` | CAUGHT | **missed (evaded)** |
| `static_io_scanner` | CAUGHT | CAUGHT |
| `ddc_compare` (hash-based) | CAUGHT | CAUGHT |

The naive, signature-based scanner is defeated by a one-line wording
change. The category-based static scanner and the hash-based DDC comparison
are both unaffected, because neither one needs to know the attacker's exact
wording in advance — they detect the *presence of a difference*, not a
*specific known difference*. This is the real, general argument for why
DDC (and category-based static checks) are stronger than signature
matching: signature-based detection is only as good as the signature
database, while divergence-based detection catches unknown variants.

## Extension 3: Multi-target injection

`variants/multitarget/codegen.cpp` replaces the single hardcoded check
`node.name == "check_login"` with a substring match against a small keyword
set (`login`, `auth`, `password`, `credential`), so any matching function
gets the same treatment.

Tested against `clean_source/tests/multi_auth.tl`, which contains
`check_login`, `authenticate_session`, `verify_password`, and a decoy
function `calculate_total` that matches none of the keywords:

- `check_login`: injected — confirmed
- `authenticate_session`: injected — confirmed
- `verify_password`: injected — confirmed
- `calculate_total`: **not** injected — confirmed

This makes the attack more dangerous in one specific, documentable way: a
human reviewer who only thought to check `check_login` (because that is
the name the original blueprint used) would miss the other two infected
functions entirely. Both DDC and the static I/O scanner still catch this
variant without modification, because neither one needed to know in
advance which function, or how many functions, were affected — this is
shown directly in the detection matrix below.

## Extension 4: Quantified results

**Payload size overhead** (source code cost of the injection logic itself):

| Variant | Clean codegen.cpp | Infected codegen.cpp | Delta |
|---|---|---|---|
| plain | 133 lines / 5007 bytes | 148 lines / 5705 bytes | +15 lines / +698 bytes |
| multitarget | 133 lines / 5007 bytes | 162 lines / 6406 bytes | +29 lines / +1399 bytes |
| obfuscated | 133 lines / 5007 bytes | 150 lines / 5792 bytes | +17 lines / +785 bytes |

**Generated-output overhead** (runtime cost per compiled program): every
variant adds exactly 2 extra lines (a comment and a print statement) to
each infected function's generated code — 13 lines clean vs 15 lines
infected for a single-function file.

**Detection matrix** (actual measured pass/fail across every detector and
every variant, from `tools/run_detection_matrix.py`):

| Variant | naive_signature | static_io | fuzzer (return only) | fuzzer (return+stderr) | ddc_compare |
|---|---|---|---|---|---|
| clean | missed (correct) | missed (correct) | missed (correct) | missed (correct) | missed (correct) |
| plain | CAUGHT | CAUGHT | missed | CAUGHT | CAUGHT |
| multitarget | CAUGHT | CAUGHT | missed | CAUGHT | CAUGHT |
| obfuscated | **missed** | CAUGHT | missed | CAUGHT | CAUGHT |

Read as a summary: no detector ever false-alarms on the clean baseline
(every "missed" in the clean row is the *correct* answer). Among the three
infected variants, only `naive_signature_scanner` is ever fooled, and only
against the one variant specifically designed to fool it. The
`fuzzer_return_only` column is entirely "missed" by design, illustrating
the earlier finding: it isn't testing anything the injected payload
touches.

**Scaling with source size** (`tools/scaling_benchmark.py`, real wall-clock
measurements, N = number of functions in a synthetic TinyLang file):

| N | compile (clean) | compile (multitarget) | naive scanner | static I/O scanner | ddc_compare |
|---|---|---|---|---|---|
| 10 | 0.0017 s | 0.0015 s | 0.0101 s | 0.0167 s | 0.0313 s |
| 50 | 0.0023 s | 0.0019 s | 0.0103 s | 0.0170 s | 0.0302 s |
| 100 | 0.0028 s | 0.0018 s | 0.0096 s | 0.0195 s | 0.0318 s |
| 250 | 0.0061 s | 0.0027 s | 0.0118 s | 0.0259 s | 0.0385 s |
| 500 | 0.0084 s | 0.0038 s | 0.0118 s | 0.0291 s | 0.0400 s |
| 1000 | 0.0160 s | 0.0057 s | 0.0100 s | 0.0381 s | 0.0486 s |

Honest reading of this data: compiler time itself grows in a visibly
increasing pattern consistent with the single-pass, linear-time design of
the lexer/parser/codegen pipeline. The detector times, however, are
dominated at this scale by fixed process-startup overhead (spawning a
Python interpreter, and for `ddc_compare`, two separate compiler
invocations), which masks the true per-function cost until N grows much
larger than tested here. This is stated plainly rather than glossed over:
the experiment shows startup-cost dominance at small N, not
constant-time detectors.

**Scaling with fuzz input count** (`tools/fuzzer_scaling.py`, a genuinely
different dimension from source size, reported separately for that reason):

| Inputs tested | Fuzzer wall time |
|---|---|
| 10 | 0.512 s |
| 50 | 0.597 s |
| 100 | 0.768 s |
| 250 | 1.155 s |
| 500 | 2.066 s |

This one shows clean, visible linear growth: roughly 0.5 s of fixed
overhead (compiling the two driver binaries once), plus approximately 3
milliseconds per additional input pair thereafter.

## Files

```
clean_source/                honest baseline (reused from Phase 5)
variants/plain/               Phase 2 style single-target injection, standalone build
variants/multitarget/         Extension 3: pattern-based multi-function injection
variants/obfuscated/          Extension 2: same injection, evasive marker wording
tools/naive_signature_scanner.py
tools/static_io_scanner.py
tools/behavioral_fuzzer.py
tools/ddc_compare.py
tools/run_detection_matrix.py
tools/gen_synthetic.py
tools/scaling_benchmark.py
tools/fuzzer_scaling.py
benchmark_data/scaling_results.csv
benchmark_data/fuzzer_scaling_results.csv
```

## How to reproduce every number in this document

```bash
# Build the clean reference compiler
g++ -std=c++17 clean_source/lexer.cpp clean_source/parser.cpp clean_source/codegen.cpp clean_source/main.cpp -o /tmp/tlc_clean

# Build the three variant compilers
for v in plain multitarget obfuscated; do (cd variants/$v && make all); done

# Full detection matrix
python3 tools/run_detection_matrix.py

# Scaling benchmarks
python3 tools/scaling_benchmark.py
python3 tools/fuzzer_scaling.py
```
