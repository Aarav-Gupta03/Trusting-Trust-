#!/bin/bash
# demonstrate_attack.sh
#
# The full Thompson "Trusting Trust" sequence, using git to prove the
# clean source tree genuinely contains zero trace of the infection at
# the moment the infected compiler is used against it.
#
# Run from the phase4/ directory:  ./demonstrate_attack.sh
set -e

WORKDIR=$(mktemp -d)
echo "Working in $WORKDIR"
echo ""

echo "== [1] git init a fresh copy of the CLEAN Phase 1 compiler =="
cp -r clean_source "$WORKDIR/repo"
cd "$WORKDIR/repo"
git init -q
git add -A
git commit -q -m "Clean Phase 1 compiler - no injection, no replication logic"
echo "  Committed. Clean source has $(git log --oneline | wc -l) commit(s)."

echo ""
echo "== [2] Confirm: zero trace of MARKER or REPLICATE anywhere in this repo =="
if grep -rq "MARKER\|REPLICATE" .; then
    echo "  UNEXPECTED: found suspicious text in supposedly clean repo!"
    exit 1
fi
echo "  Confirmed clean."

cd - > /dev/null
echo ""
echo "== [3] git checkout main - re-affirm the clean tree is what's on disk =="
cd "$WORKDIR/repo"
git checkout -q main 2>/dev/null || git checkout -q master 2>/dev/null || true
echo "  On branch: $(git branch --show-current)"
cd - > /dev/null

echo ""
echo "== [4] Use the PRE-BUILT infected compiler (from infected_build/) against this clean tree =="
mkdir -p "$WORKDIR/repo_regenerated"
./infected_build/tlc --replicate "$WORKDIR/repo" -o "$WORKDIR/repo_regenerated"

echo ""
echo "== [5] Inspect what came out =="
if grep -q "MARKER" "$WORKDIR/repo_regenerated/codegen.cpp"; then
    echo "  [CONFIRMED] The regenerated codegen.cpp contains the marker injection."
    echo "  [CONFIRMED] ...despite the git-committed clean source containing none of it."
else
    echo "  UNEXPECTED: no marker found in regenerated output."
    exit 1
fi

echo ""
echo "== [6] Build the newly-regenerated compiler and prove it still runs and still infects =="
cd "$WORKDIR/repo_regenerated"
make all > /dev/null 2>&1
OUTPUT=$(./tlc "$WORKDIR/repo/tests/auth.tl" 2>/dev/null)
if echo "$OUTPUT" | grep -q "MARKER"; then
    echo "  [CONFIRMED] The newly-built compiler still injects the marker on auth.tl."
fi
cd - > /dev/null

echo ""
echo "== SUMMARY =="
echo "A git-tracked, 100% clean compiler source tree, compiled using an"
echo "infected compiler instead of a trusted one, produced a NEW compiler"
echo "that still carries the marker-injection logic - even though nothing"
echo "in the git history or working tree ever contained it."
echo ""
echo "Cleaning up $WORKDIR"
rm -rf "$WORKDIR"
