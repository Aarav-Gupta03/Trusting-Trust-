#!/bin/bash
# ddc_verify.sh
#
# Diverse Double-Compiling (Wheeler, 2005), adapted for this project.
#
# Real DDC: build the suspect compiler's own source using both a trusted
# compiler T and the suspect compiler C, then use BOTH resulting compilers
# to compile the SAME thing again and compare outputs. If T and C are both
# clean, the two lineages agree. If C is infected, the lineage built via C
# diverges - without needing to know in advance WHAT the infection does.
#
# ADAPTATION NOTE for this project: our "compiler" is a source-to-source
# TinyLang -> C++ transpiler, not something that compiles itself directly
# to machine code. So here:
#   - S (the source being built) = this compiler's own C++ source
#     (clean_source/: lexer, parser, codegen, main - no injection anywhere)
#   - T (trusted compiler)       = system g++, used directly on S
#   - C (suspect compiler)       = infected_build/tlc, used on S via its
#                                  --replicate mode (its way of "processing"
#                                  its own kind of source), then the result
#                                  is built into a real binary with g++ too
#                                  (g++ itself is trusted/neutral here - the
#                                  attack surface we're testing is entirely
#                                  in which C++ TEXT gets produced, not in
#                                  how that text becomes a binary).
#   - "compile S again through both" = both resulting compilers (compiler_A,
#     compiler_B) are used to compile the SAME TinyLang test program
#     (tests/auth.tl). If the lineages are truly equivalent, their outputs
#     must be byte-identical. If compiler_B secretly diverges (as it does,
#     carrying Phase 2's marker injection), the outputs will differ.
#
# Usage:
#   ./ddc_verify.sh                 # ALERT-case demo (uses infected_build/)
#   ./ddc_verify.sh --clean-only    # baseline/negative-case demo (no suspect compiler involved)

set -e

MODE="${1:-alert}"
WORKDIR=$(mktemp -d)
echo "DDC verification workspace: $WORKDIR"
echo ""

# ---------------------------------------------------------------------
# Step 1: compiler_A = build clean_source with TRUSTED g++ directly.
# ---------------------------------------------------------------------
echo "== [1] Building compiler_A: clean_source compiled by trusted g++ =="
g++ -std=c++17 -O2 clean_source/lexer.cpp clean_source/parser.cpp clean_source/codegen.cpp clean_source/main.cpp -o "$WORKDIR/compiler_A"
echo "  compiler_A built."

# ---------------------------------------------------------------------
# Step 2: compiler_B = build clean_source using the SUSPECT compiler.
# ---------------------------------------------------------------------
echo ""
if [ "$1" == "--clean-only" ]; then
    echo "== [2] BASELINE MODE: compiler_B also built from clean_source via trusted g++ =="
    echo "       (No suspect compiler involved - this is the negative-control case.)"
    g++ -std=c++17 -O2 clean_source/lexer.cpp clean_source/parser.cpp clean_source/codegen.cpp clean_source/main.cpp -o "$WORKDIR/compiler_B"
    echo "  compiler_B built (clean lineage)."
else
    echo "== [2] ALERT MODE: compiler_B built via the SUSPECT compiler (infected_build/tlc) =="
    mkdir -p "$WORKDIR/compiler_B_source"
    ./infected_build/tlc --replicate clean_source -o "$WORKDIR/compiler_B_source"
    g++ -std=c++17 -O2 "$WORKDIR/compiler_B_source"/lexer.cpp "$WORKDIR/compiler_B_source"/parser.cpp \
        "$WORKDIR/compiler_B_source"/codegen.cpp "$WORKDIR/compiler_B_source"/main.cpp -o "$WORKDIR/compiler_B"
    echo "  compiler_B built (suspect lineage)."
fi

# ---------------------------------------------------------------------
# Step 3/4: use BOTH resulting compilers to compile the SAME TinyLang
#           test program again, producing final_A and final_B.
# ---------------------------------------------------------------------
echo ""
echo "== [3] Compiling tests/auth.tl with compiler_A -> final_A.cpp =="
"$WORKDIR/compiler_A" clean_source/tests/auth.tl -o "$WORKDIR/final_A.cpp"

echo ""
echo "== [4] Compiling tests/auth.tl with compiler_B -> final_B.cpp =="
"$WORKDIR/compiler_B" clean_source/tests/auth.tl -o "$WORKDIR/final_B.cpp"

# ---------------------------------------------------------------------
# Step 5: hash comparison.
# ---------------------------------------------------------------------
echo ""
echo "== [5] Hash comparison =="
HASH_A=$(sha256sum "$WORKDIR/final_A.cpp" | awk '{print $1}')
HASH_B=$(sha256sum "$WORKDIR/final_B.cpp" | awk '{print $1}')
echo "  final_A.cpp: $HASH_A"
echo "  final_B.cpp: $HASH_B"

echo ""
if [ "$HASH_A" == "$HASH_B" ]; then
    echo "[SUCCESS] Compilers are clean. Chain of trust intact."
    RESULT=0
else
    echo "[ALERT] Discrepancy detected! Backdoor identified."
    echo ""
    echo "  --- diff (final_A.cpp vs final_B.cpp) ---"
    diff "$WORKDIR/final_A.cpp" "$WORKDIR/final_B.cpp" || true
    RESULT=1
fi

rm -rf "$WORKDIR"
exit $RESULT
