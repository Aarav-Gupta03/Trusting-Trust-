# Phase 6 Part 4 — A Formal Soundness Argument, and Why the Ensemble Is Necessary

Part 3 showed, empirically, that Metamorphic Relations MR1 and MR2 catch
the evasive compiler and correctly stay silent on the others. This part
states precisely WHY that must be true, not just that it was observed to
be true, and uses the same argument to explain why no single detector in
this project can stand alone.

## Definitions

Let `S` be a TinyLang source text. Let `AST(S)` denote the abstract syntax
tree produced by this project's own lexer and parser (`tokenize(S)` then
`parse(...)`, as implemented in `lexer.cpp` / `parser.cpp`).

**Definition 1 (honest compiler).** A compiler `C` is *AST-faithful* if
there exists some function `G`, taking only an AST as input, such that for
every source text `S`:

```
C(S) = G(AST(S))
```

In words: an honest compiler's output depends only on what the program
actually means, structurally - nothing else. It is not allowed to depend
on the raw byte content of `S`, on the file's name, on whether it has been
invoked before, or on anything not captured by the AST.

**Definition 2 (grammar-null transformation).** A transformation
`T: Source -> Source` is *grammar-null* if, for every `S`,
`AST(T(S)) = AST(S)`.

**Claim: MR1's comment-insertion transformation is grammar-null.**
`lexer.cpp`'s tokenizer discards `//` comment text entirely while
scanning (see "Mode 2: line comments" in the lexer source) and produces no
token for it whatsoever. Inserting a comment therefore changes the
character stream but produces an *identical token stream*, hence an
identical AST after parsing. This is not an assumption about the language;
it follows directly from reading the lexer's own comment-handling code.

## The soundness theorem

**Theorem.** If `C` is AST-faithful and `T` is grammar-null, then
`C(T(S)) = C(S)` for every `S`.

**Proof.** By Definition 1, `C(T(S)) = G(AST(T(S)))`. By Definition 2 (`T`
grammar-null), `AST(T(S)) = AST(S)`, so `G(AST(T(S))) = G(AST(S)) = C(S)`.

**Contrapositive (what the tester actually uses).** If
`C(T(S)) does not equal C(S)` for some grammar-null `T` and some `S`, then
`C` is not AST-faithful.

This is the precise justification for treating an MR1 violation as proof
of dishonesty: it is not a heuristic or a statistical signal, it is a
direct logical consequence of the definitions above, checkable by anyone
who reads `lexer.cpp`. When `metamorphic_tester.py` observed
`variants/evasive/tlc` produce different output for `auth.tl` versus
`auth.tl` plus one comment, that observation, by itself, proves the
evasive compiler consults something outside the AST - which is exactly
true, since it hashes the raw source text (see `variants/evasive/main.cpp`).

## MR2 needs one more premise, stated explicitly

MR2 (unrelated decoy insertion) does not satisfy Definition 2 as stated:
appending a new function genuinely changes the top-level AST (`Program`
gains one more statement). MR2's soundness instead requires:

**Definition 3 (compositional codegen).** `G` is compositional if, for a
`Program` node with top-level statements `stmt_1` through `stmt_n`,
`G(Program)` equals the combination of `G_stmt(stmt_1)` through
`G_stmt(stmt_n)`, where each `G_stmt(stmt_i)` depends only on `stmt_i`'s
own subtree, not on any sibling `stmt_j`.

This is a real, checkable property of this project's own honest
`codegen.cpp`: `genFuncDef` takes only its own `FuncDefNode` and an
indentation level as arguments, and builds a symbol table seeded only from
its own parameters (`codegen.cpp` line 74) - it has no access to sibling
functions or any global state. Given compositional `G`, appending an
independent decoy function leaves `check_login`'s own subtree unchanged,
so `G_stmt(check_login_node)` - and hence the portion of output
corresponding to `check_login` - must be unchanged too. The same
contrapositive argument then applies to that output slice.

## The precise limitation: two distinct classes of dishonesty

The theorem's contrapositive is sound (a violation proves dishonesty) but
not complete (silence does not prove honesty). This is not a vague caveat;
it can be stated exactly, and doing so explains the earlier empirical
results rather than just reporting them.

- **Class A dishonesty (AST-external dependence).** `C`'s output depends
  on something outside `AST(S)` - raw byte content, file identity,
  invocation history, timing. This is exactly what `variants/evasive/`
  does (it hashes raw source text) and exactly what Thompson's original
  "Rule 2" self-recognition does (it inspects source text for a
  signature). MR1/MR2 are sound and effective against this class, and
  need no reference compiler to be so.

- **Class B dishonesty (AST-internal but illegitimate logic).** `C`'s
  output is, technically, a pure function of the AST alone - satisfying
  Definition 1 exactly - but that function includes logic no reasonable
  reading of the TinyLang program asked for (for example: "if the AST
  node's name field contains the text login, append this extra
  statement"). This is exactly what `variants/plain/`,
  `variants/multitarget/`, and `variants/obfuscated/` do: the injection
  condition reads only `node.name`, a genuine AST field, so these
  compilers are AST-faithful by Definition 1. No grammar-null
  transformation can ever expose Class B dishonesty, because by
  construction such a transformation leaves the relevant AST fields - the
  very thing Class B dishonesty legitimately reads - unchanged.

This is why MR1/MR2 measurably stayed silent on `plain`, `multitarget`,
and `obfuscated` in Part 3: not because the tool is weak, but because
those three are provably outside what any grammar-null-transformation
test can ever detect, for any choice of relation, not just the two
implemented here.

## Why this proves the ensemble is structurally necessary, not just useful

| Detector | Needs a reference compiler? | Class A (AST-external) | Class B (AST-internal, illegitimate) |
|---|---|---|---|
| metamorphic_self_consistency | No | Sound, proven above | Cannot detect (proven above) |
| naive_signature_scanner | No | Not designed for this class | Heuristic only (known wording) |
| static_io_scanner | No | Not designed for this class | Heuristic only (known category: I/O) |
| fuzzer_return_only / fuzzer_return_stderr | No (needs a behavioral reference) | Not designed for this class | Heuristic only (known channel) |
| ddc_this_run_input / ddc_second_random_draw | Yes | Catches it (any divergence from reference) | Catches it (any divergence from reference) |

Reading the table: only DDC, because it holds an actual reference `G` to
compare against, is class-agnostic - it does not need to know which class
of dishonesty it is looking at, only that the candidate disagrees with a
trusted implementation. Every reference-free method (metamorphic testing,
the static/dynamic scanners) is necessarily specialized to one class or
one heuristic, which is a mathematical consequence of Definition 1, not an
engineering shortcoming to be patched later. An ensemble is therefore not
merely "more checks are better" - it is required, because Class A and
Class B dishonesty are provably disjoint in what a grammar-null-only
method can see, and no single reference-free method covers both.

## Honest remaining limitations

1. This proof concerns this project's grammar and this project's codegen
   design specifically (compositionality is verified by reading
   `codegen.cpp`, not assumed in general). It does not claim every real
   compiler's codegen is compositional - some legitimate optimizations
   (for example cross-function inlining) genuinely violate Definition 3,
   and MR2 would need to be dropped or restricted for such a compiler.
2. The theorem proves soundness of a violation, not completeness of the
   absence of one. A compiler could satisfy MR1 and MR2 perfectly and
   still be Class B dishonest, exactly as measured in Part 3.
3. `G` is implicitly assumed deterministic across the two calls compared
   in one test run. This project's own Phase 1 compiler was independently
   verified deterministic (see Phase 1 README: identical SHA-256 hash
   across three separate compilation runs), which is what justifies using
   it as the trusted reference elsewhere in this project.

## File changes in this part

`tools/ensemble_detector.py` was updated to print each vote's dishonesty
class (A, B, or oracle-based/class-agnostic) alongside its verdict, so the
classification above is checkable directly against the tool's actual
output, not only asserted in this document.
