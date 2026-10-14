// main.cpp
// Phase 1: driver program.
// Usage: ./tlc <input.tl> [-o output.cpp]
// If -o is omitted, generated C++ is printed to stdout.

#include <fstream>
#include <iostream>
#include <sstream>
#include "lexer.hpp"
#include "parser.hpp"
#include "codegen.hpp"

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
