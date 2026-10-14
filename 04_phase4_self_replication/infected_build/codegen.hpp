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
