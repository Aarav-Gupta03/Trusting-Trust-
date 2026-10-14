// codegen.hpp
// Phase 1: AST -> C++ source code generation.
#pragma once

#include <string>
#include "parser.hpp"

// Generates a complete, compilable C++ source file (as a string) from a
// TinyLang ProgramNode. Determinism guarantee: for a given AST, this
// function always produces byte-identical output. This matters later (in
// the DDC defense phase) where we rely on "same source in -> same binary
// out" to make hash comparisons meaningful.
std::string generateProgram(const ProgramNode& program);

// EXTENSION (round 1: evasive infection). When true, the injection is
// suppressed for this compilation, regardless of which function is being
// generated. main.cpp sets this before calling generateProgram() if it
// detects the input source matches a known, published test fixture -
// mirroring real world "detect the test, behave differently" evasion
// (e.g. emissions test-cycle detection). See phase6 README, Round 1.
extern bool g_evasionSuppressed;
