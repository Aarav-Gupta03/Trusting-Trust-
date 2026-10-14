#!/usr/bin/env python3
"""
build_infected_main.py
=======================
Programmatically assembles the Phase 4 "infected_build/main.cpp" so that it
contains TWO embedded payloads:
  1. CODEGEN_TEXT  - the exact text of Phase 2's marker-injecting codegen.cpp
  2. a self-referential copy of main.cpp's own source (needed so that when
     this compiler replicates itself, the NEW main.cpp it writes also
     carries the replication capability forward - not just the injection).

This mirrors the exact technique proven in Phase 3's quine.cpp (placeholder
tokens standing in for "insert self here" / "insert a real quote here"),
generalized to also carry an external payload (codegen.cpp) alongside the
self-copy. See phase4/README.md for the full explanation.

Why a build script instead of a hand-written file? Because a human manually
tracking two nested payloads and where each placeholder needs to sit is
extremely error-prone. Assembling it programmatically guarantees the "real"
copy and the "depicted" copy of every shared line are generated from the
SAME source strings, so they cannot silently drift out of sync. This script
runs ONCE, before Phase 4's compiler is ever built; the resulting main.cpp
is plain, static C++ source thereafter.
"""

import sys
from pathlib import Path

HERE = Path(__file__).resolve().parent

CODEGEN_SRC = (HERE / "codegen_infected_source.cpp").read_text()

OUTER_DELIM = "MAINQ"       # delimiter for the self-referential outer raw string
CODEGEN_DELIM = "CGPAY"     # delimiter for the (non-self-referential) codegen payload

SELF_TOKEN = "@@SELF_PAYLOAD@@"
QUOTE_TOKEN = "@@QUOTE@@"

# --- Safety checks: make sure none of our chosen tokens/delimiters
#     coincidentally collide with the actual codegen.cpp content. ---
for forbidden in (f")({OUTER_DELIM}\"", f")({CODEGEN_DELIM}\"", SELF_TOKEN, QUOTE_TOKEN,
                   f")MAINQ\"", f")CGPAY\""):
    if forbidden in CODEGEN_SRC:
        sys.exit(f"FATAL: chosen delimiter/token '{forbidden}' collides with "
                  f"codegen.cpp content. Pick a different delimiter and rerun.")

# ---------------------------------------------------------------------
# PREFIX: everything in main.cpp before the codegenPayload assignment.
# SUFFIX: everything after the mainBody assignment through end of file.
# These are IDENTICAL between the "real" (executing) copy and the
# "depicted" (self-referential data) copy - only the two payload-holding
# lines themselves differ between real and depicted form.
# ---------------------------------------------------------------------

PREFIX = '''// main.cpp (PHASE 4 - self-recognizing, self-replicating compiler driver)
//
// Normal mode:     ./tlc input.tl -o output.cpp
//   Ordinary TinyLang -> C++ transpilation. codegen.cpp (linked in) carries
//   Phase 2's inert marker injection for check_login.
//
// Replicate mode:  ./tlc --replicate <src_dir> -o <out_dir>
//   Self-recognition hook (Thompson's "Rule 2"): checks whether <src_dir>
//   looks like this project's OWN compiler source (by searching for a
//   signature string unique to lexer.hpp). If it matches, this binary
//   regenerates codegen.cpp and main.cpp in <out_dir> from payloads
//   EMBEDDED IN THIS BINARY - reproducing the marker-injection logic and
//   this very replication capability, even if <src_dir> on disk is
//   completely clean and contains no trace of either.
//
// SAFETY SCOPING: the payload carried forward is Phase 2's inert MARKER
// injection only (a labeled comment + a printed line). No credential
// bypass, no functional backdoor, nothing that alters authentication
// outcomes. See project README for the full rationale.

#include <fstream>
#include <iostream>
#include <sstream>
#include <string>
#include "lexer.hpp"
#include "parser.hpp"
#include "codegen.hpp"

namespace {

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) throw std::runtime_error("Could not open file: " + path);
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

void writeFile(const std::string& path, const std::string& content) {
    std::ofstream out(path);
    if (!out) throw std::runtime_error("Could not open file for writing: " + path);
    out << content;
    std::cerr << "  wrote " << path << "\\n";
}

// Self-recognition hook: is <srcDir> this project's own compiler source?
// Detection is a simple signature match against lexer.hpp's tokenize()
// declaration - a direct implementation of the blueprint's
// `source_text.find("vector<Token> tokenize")` idea.
bool looksLikeOwnSource(const std::string& srcDir) {
    std::string lexerHeaderPath = srcDir + "/lexer.hpp";
    std::ifstream check(lexerHeaderPath);
    if (!check) return false;
    std::ostringstream ss;
    ss << check.rdbuf();
    return ss.str().find("std::vector<Token> tokenize") != std::string::npos;
}

void copyPassThrough(const std::string& srcDir, const std::string& outDir, const std::string& name) {
    writeFile(outDir + "/" + name, readFile(srcDir + "/" + name));
}

void doReplicate(const std::string& srcDir, const std::string& outDir) {
    if (!looksLikeOwnSource(srcDir)) {
        std::cerr << "[REPLICATE] " << srcDir
                  << " does not match this compiler's own signature; nothing to do.\\n";
        return;
    }
    std::cerr << "[REPLICATE] Self-recognition triggered: " << srcDir
              << " matches this compiler's own signature (found tokenize() in lexer.hpp).\\n";
    std::cerr << "[REPLICATE] Regenerating codegen.cpp and main.cpp with embedded payloads...\\n";

'''

SUFFIX = '''
    // lexer/parser are never modified by Phase 2/4 - pass them through
    // unchanged, along with the header and build files, so <out_dir> is
    // immediately buildable.
    copyPassThrough(srcDir, outDir, "lexer.hpp");
    copyPassThrough(srcDir, outDir, "lexer.cpp");
    copyPassThrough(srcDir, outDir, "parser.hpp");
    copyPassThrough(srcDir, outDir, "parser.cpp");
    copyPassThrough(srcDir, outDir, "codegen.hpp");
    copyPassThrough(srcDir, outDir, "Makefile");

    std::cerr << "[REPLICATE] Done. " << outDir
              << " now carries the marker-injection logic forward,\\n"
              << "[REPLICATE] even though " << srcDir << " itself contains no trace of it.\\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage:\\n"
                  << "  " << argv[0] << " <input.tl> [-o output.cpp]\\n"
                  << "  " << argv[0] << " --replicate <src_dir> -o <out_dir>\\n";
        return 1;
    }

    std::string first = argv[1];
    if (first == "--replicate") {
        if (argc < 5) {
            std::cerr << "Usage: " << argv[0] << " --replicate <src_dir> -o <out_dir>\\n";
            return 1;
        }
        std::string srcDir = argv[2];
        std::string outDir;
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-o" && i + 1 < argc) outDir = argv[++i];
        }
        if (outDir.empty()) {
            std::cerr << "Error: -o <out_dir> is required with --replicate\\n";
            return 1;
        }
        try {
            doReplicate(srcDir, outDir);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\\n";
            return 1;
        }
        return 0;
    }

    // --- normal mode: TinyLang -> C++ ---
    std::string inputPath = argv[1];
    std::string outputPath;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) outputPath = argv[++i];
    }

    try {
        std::string source = readFile(inputPath);
        std::vector<Token> tokens = tokenize(source);
        std::unique_ptr<ProgramNode> program = parse(tokens);
        std::string cppCode = generateProgram(*program);

        if (outputPath.empty()) {
            std::cout << cppCode;
        } else {
            writeFile(outputPath, cppCode);
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\\n";
        return 1;
    }

    return 0;
}
'''

# The two real, executing payload-assignment lines.
REAL_CODEGEN_LINE = (
    f'    std::string codegenPayload = R"{CODEGEN_DELIM}(' + CODEGEN_SRC + f'){CODEGEN_DELIM}";\n'
)

def build(outer_delim, self_token, quote_token, mainbody_is_depicted):
    """Builds either the REAL mainBody-holding line (mainbody_is_depicted=False)
    or the DEPICTED version used inside the self-referential payload
    (mainbody_is_depicted=True)."""
    if mainbody_is_depicted:
        return f'    std::string mainBody = R"{outer_delim}({self_token})' + outer_delim + quote_token + ';\n'
    raise ValueError("real line built separately, see below")

DEPICTED_MAINBODY_LINE = f'    std::string mainBody = R"{OUTER_DELIM}({SELF_TOKEN})' + OUTER_DELIM + QUOTE_TOKEN + ';\n'

# DEPICTED_TEXT = the value mainBody will hold at runtime = the WHOLE file's
# text, with the mainBody line itself shown as placeholders (never
# recursively expanded) and the codegen line shown identically to the real
# one (safe: different delimiter, no collision, and it's not self-referential).
DEPICTED_TEXT = PREFIX + REAL_CODEGEN_LINE + DEPICTED_MAINBODY_LINE + SUFFIX

REAL_MAINBODY_LINE = f'    std::string mainBody = R"{OUTER_DELIM}(' + DEPICTED_TEXT + f')' + OUTER_DELIM + '";\n'

FINAL_MAIN_CPP = PREFIX + REAL_CODEGEN_LINE + REAL_MAINBODY_LINE + SUFFIX

# Insert the runtime substitution logic right after mainBody is defined,
# before it's used - patch it into SUFFIX's start via a marker replace.
RECONSTRUCT_LOGIC = '''    std::string reconstructedMain = mainBody;
    // IMPORTANT: substitute the QUOTE token BEFORE the SELF_PAYLOAD token.
    // Doing SELF_PAYLOAD first would insert a full nested copy of mainBody
    // (itself containing an unresolved @@QUOTE@@ placeholder) EARLIER in
    // the string than the outer placeholder we actually want to close -
    // then find() would grab that spurious nested occurrence instead of
    // the real one, leaving the true closing delimiter unresolved.
    {
        std::string token = "''' + QUOTE_TOKEN + '''";
        size_t p = reconstructedMain.find(token);
        if (p != std::string::npos) reconstructedMain.replace(p, token.size(), "\\"");
    }
    {
        std::string token = "''' + SELF_TOKEN + '''";
        size_t p = reconstructedMain.find(token);
        if (p != std::string::npos) reconstructedMain.replace(p, token.size(), mainBody);
    }
    writeFile(outDir + "/codegen.cpp", codegenPayload);
    writeFile(outDir + "/main.cpp", reconstructedMain);
'''

# Rebuild both DEPICTED_TEXT and FINAL_MAIN_CPP with reconstruction logic
# inserted right at the top of SUFFIX (before the pass-through copies).
SUFFIX_WITH_LOGIC = "\n" + RECONSTRUCT_LOGIC + SUFFIX

DEPICTED_TEXT = PREFIX + REAL_CODEGEN_LINE + DEPICTED_MAINBODY_LINE + SUFFIX_WITH_LOGIC
REAL_MAINBODY_LINE = f'    std::string mainBody = R"{OUTER_DELIM}(' + DEPICTED_TEXT + f')' + OUTER_DELIM + '";\n'
FINAL_MAIN_CPP = PREFIX + REAL_CODEGEN_LINE + REAL_MAINBODY_LINE + SUFFIX_WITH_LOGIC

out_path = HERE / "infected_build" / "main.cpp"
out_path.write_text(FINAL_MAIN_CPP)
print(f"Wrote {out_path} ({len(FINAL_MAIN_CPP)} bytes)")
