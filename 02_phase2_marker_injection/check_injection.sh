#!/bin/bash
# check_injection.sh
# Proves two things about Phase 2's codegen:
#   1. The marker DOES appear in check_login's generated output, even though
#      tests/auth.tl (the TinyLang source) never mentions it.
#   2. The marker does NOT appear for unrelated functions (classify, is_zero
#      from variables.tl) - proving this is a targeted, name-based
#      injection (Thompson's "Rule 1"), not a blanket change to every
#      function the compiler touches.
set -e

echo "== Compiling tests/auth.tl with the Phase 2 (marker-injecting) compiler =="
./tlc tests/auth.tl -o /tmp/phase2_auth_generated.cpp

echo "== Compiling tests/variables.tl (unrelated functions) =="
./tlc tests/variables.tl -o /tmp/phase2_variables_generated.cpp

echo
echo "--- Checking auth_generated.cpp for the marker ---"
if grep -q "MARKER_INSERTED" /tmp/phase2_auth_generated.cpp; then
    echo "[PASS] Marker found in check_login's generated output."
    grep -n "MARKER" /tmp/phase2_auth_generated.cpp
else
    echo "[FAIL] Marker NOT found - injection did not fire."
    exit 1
fi

echo
echo "--- Checking variables_generated.cpp does NOT contain the marker ---"
if grep -q "MARKER_INSERTED" /tmp/phase2_variables_generated.cpp; then
    echo "[FAIL] Marker found in unrelated function output - injection is not targeted!"
    exit 1
else
    echo "[PASS] No marker present for classify/is_zero - injection correctly targets only check_login."
fi

echo
echo "--- Confirming tests/auth.tl itself never mentions the marker ---"
if grep -q "MARKER" tests/auth.tl; then
    echo "[FAIL] auth.tl source contains the marker text - this would defeat the demonstration."
    exit 1
else
    echo "[PASS] auth.tl (TinyLang source) contains no trace of the marker."
    echo "        The injected line exists ONLY because the compiler put it there."
fi

echo
echo "ALL INJECTION CHECKS PASSED"
