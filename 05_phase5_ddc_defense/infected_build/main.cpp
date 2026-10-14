// main.cpp (PHASE 4 - self-recognizing, self-replicating compiler driver)
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
    std::cerr << "  wrote " << path << "\n";
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
                  << " does not match this compiler's own signature; nothing to do.\n";
        return;
    }
    std::cerr << "[REPLICATE] Self-recognition triggered: " << srcDir
              << " matches this compiler's own signature (found tokenize() in lexer.hpp).\n";
    std::cerr << "[REPLICATE] Regenerating codegen.cpp and main.cpp with embedded payloads...\n";

    std::string codegenPayload = R"CGPAY(// codegen.cpp  (PHASE 2 — target marker injection)
//
// This file is IDENTICAL to Phase 1's codegen.cpp except for one addition,
// clearly marked below with "=== THE INJECTION ===". Everything else
// (determinism discipline, symbol-table handling, expression/statement
// generation) is unchanged from Phase 1.
//
// WHAT THIS DEMONSTRATES:
// Ken Thompson's "Rule 1" - a compiler can recognize a specific target
// function by name/shape in the AST and silently insert code that the
// programmer never wrote, with no way to detect this by reading the
// TinyLang source file (auth.tl). The source is completely innocent; the
// compiler is not.
//
// SAFETY SCOPING (see project README): the injected payload here is an
// inert, clearly-labeled marker - a comment plus a harmless printed line -
// NOT a credential bypass. It never touches the function's actual
// return value or control flow, and never compares against or reveals
// any password. This preserves the entire mechanism (AST-node detection
// -> unrequested code injection) while keeping the artifact safe to run
// and hand in.

#include "codegen.hpp"
#include <algorithm>
#include <stdexcept>

namespace {

std::string indentStr(int level) { return std::string(static_cast<size_t>(level) * 4, ' '); }

std::string escapeStringLiteral(const std::string& raw) {
    std::string out;
    out.reserve(raw.size() + 2);
    for (char c : raw) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

std::string genTerm(const TermNode& term) {
    switch (term.kind) {
        case TermNode::Kind::IDENT:
            return term.text;
        case TermNode::Kind::STRING:
            return "\"" + escapeStringLiteral(term.text) + "\"";
        case TermNode::Kind::NUMBER:
            return term.text;
    }
    throw std::runtime_error("codegen: unknown term kind");
}

std::string genExpression(const ExpressionNode& expr) {
    std::string code;
    for (size_t i = 0; i < expr.terms.size(); ++i) {
        if (i > 0) code += " == ";
        code += genTerm(*expr.terms[i]);
    }
    return code;
}

std::string genStmt(const StmtNode& stmt, int indent, std::vector<std::string>& declared);

std::string genFuncDef(const FuncDefNode& node, int indent) {
    std::string ind = indentStr(indent);
    std::string code = ind + "auto " + node.name + "(";
    for (size_t i = 0; i < node.params.size(); ++i) {
        if (i > 0) code += ", ";
        code += "const std::string& " + node.params[i];
    }
    code += ") {\n";

    // ============================================================
    // === THE INJECTION (Phase 2 / Thompson's "Rule 1") ===
    // ============================================================
    // The code generator recognizes a specific TARGET function by name -
    // exactly the way Thompson's original compiler recognized the login
    // routine it was compiling - and inserts code the source file never
    // requested, BEFORE the function's real body is emitted.
    //
    // This is the injection point a real attack would use to insert a
    // credential bypass. Here the payload is inert on purpose: it never
    // changes what the function returns, and it is unmistakably labeled.
    if (node.name == "check_login") {
        std::string injIndent = indentStr(indent + 1);
        code += injIndent + "// [MARKER] this line was injected by the compiler; it does not appear in the TinyLang source\n";
        code += injIndent + "std::cerr << \"[MARKER_INSERTED by codegen for function: " + node.name + "]\\n\";\n";
    }
    // ============================================================
    // === END INJECTION ===
    // ============================================================

    std::vector<std::string> declared(node.params.begin(), node.params.end());
    for (const auto& s : node.body) {
        code += genStmt(*s, indent + 1, declared);
    }
    code += ind + "}\n";
    return code;
}

std::string genStmt(const StmtNode& stmt, int indent, std::vector<std::string>& declared) {
    std::string ind = indentStr(indent);

    if (const auto* a = dynamic_cast<const AssignStmtNode*>(&stmt)) {
        bool alreadyDeclared = std::find(declared.begin(), declared.end(), a->name) != declared.end();
        std::string code = ind + (alreadyDeclared ? "" : "auto ") + a->name +
                            " = " + genExpression(*a->value) + ";\n";
        if (!alreadyDeclared) declared.push_back(a->name);
        return code;
    }

    if (const auto* r = dynamic_cast<const ReturnStmtNode*>(&stmt)) {
        return ind + "return " + genExpression(*r->value) + ";\n";
    }

    if (const auto* iff = dynamic_cast<const IfStmtNode*>(&stmt)) {
        std::string code = ind + "if (" + genExpression(*iff->condition) + ") {\n";
        std::vector<std::string> innerDeclared = declared;
        for (const auto& s : iff->body) {
            code += genStmt(*s, indent + 1, innerDeclared);
        }
        code += ind + "}\n";
        return code;
    }

    if (const auto* f = dynamic_cast<const FuncDefNode*>(&stmt)) {
        return genFuncDef(*f, indent);
    }

    throw std::runtime_error("codegen: encountered unknown statement node type");
}

} // namespace

std::string generateProgram(const ProgramNode& program) {
    std::string code;
    code += "// Auto-generated by the TinyLang compiler (PHASE 2 - marker-injecting build).\n";
    code += "#include <string>\n";
    code += "#include <iostream>\n";
    code += "\n";

    std::vector<std::string> globalDeclared;
    for (const auto& stmt : program.statements) {
        code += genStmt(*stmt, 0, globalDeclared);
        code += "\n";
    }
    return code;
}
)CGPAY";
    std::string mainBody = R"MAINQ(// main.cpp (PHASE 4 - self-recognizing, self-replicating compiler driver)
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
    std::cerr << "  wrote " << path << "\n";
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
                  << " does not match this compiler's own signature; nothing to do.\n";
        return;
    }
    std::cerr << "[REPLICATE] Self-recognition triggered: " << srcDir
              << " matches this compiler's own signature (found tokenize() in lexer.hpp).\n";
    std::cerr << "[REPLICATE] Regenerating codegen.cpp and main.cpp with embedded payloads...\n";

    std::string codegenPayload = R"CGPAY(// codegen.cpp  (PHASE 2 — target marker injection)
//
// This file is IDENTICAL to Phase 1's codegen.cpp except for one addition,
// clearly marked below with "=== THE INJECTION ===". Everything else
// (determinism discipline, symbol-table handling, expression/statement
// generation) is unchanged from Phase 1.
//
// WHAT THIS DEMONSTRATES:
// Ken Thompson's "Rule 1" - a compiler can recognize a specific target
// function by name/shape in the AST and silently insert code that the
// programmer never wrote, with no way to detect this by reading the
// TinyLang source file (auth.tl). The source is completely innocent; the
// compiler is not.
//
// SAFETY SCOPING (see project README): the injected payload here is an
// inert, clearly-labeled marker - a comment plus a harmless printed line -
// NOT a credential bypass. It never touches the function's actual
// return value or control flow, and never compares against or reveals
// any password. This preserves the entire mechanism (AST-node detection
// -> unrequested code injection) while keeping the artifact safe to run
// and hand in.

#include "codegen.hpp"
#include <algorithm>
#include <stdexcept>

namespace {

std::string indentStr(int level) { return std::string(static_cast<size_t>(level) * 4, ' '); }

std::string escapeStringLiteral(const std::string& raw) {
    std::string out;
    out.reserve(raw.size() + 2);
    for (char c : raw) {
        if (c == '"' || c == '\\') out += '\\';
        out += c;
    }
    return out;
}

std::string genTerm(const TermNode& term) {
    switch (term.kind) {
        case TermNode::Kind::IDENT:
            return term.text;
        case TermNode::Kind::STRING:
            return "\"" + escapeStringLiteral(term.text) + "\"";
        case TermNode::Kind::NUMBER:
            return term.text;
    }
    throw std::runtime_error("codegen: unknown term kind");
}

std::string genExpression(const ExpressionNode& expr) {
    std::string code;
    for (size_t i = 0; i < expr.terms.size(); ++i) {
        if (i > 0) code += " == ";
        code += genTerm(*expr.terms[i]);
    }
    return code;
}

std::string genStmt(const StmtNode& stmt, int indent, std::vector<std::string>& declared);

std::string genFuncDef(const FuncDefNode& node, int indent) {
    std::string ind = indentStr(indent);
    std::string code = ind + "auto " + node.name + "(";
    for (size_t i = 0; i < node.params.size(); ++i) {
        if (i > 0) code += ", ";
        code += "const std::string& " + node.params[i];
    }
    code += ") {\n";

    // ============================================================
    // === THE INJECTION (Phase 2 / Thompson's "Rule 1") ===
    // ============================================================
    // The code generator recognizes a specific TARGET function by name -
    // exactly the way Thompson's original compiler recognized the login
    // routine it was compiling - and inserts code the source file never
    // requested, BEFORE the function's real body is emitted.
    //
    // This is the injection point a real attack would use to insert a
    // credential bypass. Here the payload is inert on purpose: it never
    // changes what the function returns, and it is unmistakably labeled.
    if (node.name == "check_login") {
        std::string injIndent = indentStr(indent + 1);
        code += injIndent + "// [MARKER] this line was injected by the compiler; it does not appear in the TinyLang source\n";
        code += injIndent + "std::cerr << \"[MARKER_INSERTED by codegen for function: " + node.name + "]\\n\";\n";
    }
    // ============================================================
    // === END INJECTION ===
    // ============================================================

    std::vector<std::string> declared(node.params.begin(), node.params.end());
    for (const auto& s : node.body) {
        code += genStmt(*s, indent + 1, declared);
    }
    code += ind + "}\n";
    return code;
}

std::string genStmt(const StmtNode& stmt, int indent, std::vector<std::string>& declared) {
    std::string ind = indentStr(indent);

    if (const auto* a = dynamic_cast<const AssignStmtNode*>(&stmt)) {
        bool alreadyDeclared = std::find(declared.begin(), declared.end(), a->name) != declared.end();
        std::string code = ind + (alreadyDeclared ? "" : "auto ") + a->name +
                            " = " + genExpression(*a->value) + ";\n";
        if (!alreadyDeclared) declared.push_back(a->name);
        return code;
    }

    if (const auto* r = dynamic_cast<const ReturnStmtNode*>(&stmt)) {
        return ind + "return " + genExpression(*r->value) + ";\n";
    }

    if (const auto* iff = dynamic_cast<const IfStmtNode*>(&stmt)) {
        std::string code = ind + "if (" + genExpression(*iff->condition) + ") {\n";
        std::vector<std::string> innerDeclared = declared;
        for (const auto& s : iff->body) {
            code += genStmt(*s, indent + 1, innerDeclared);
        }
        code += ind + "}\n";
        return code;
    }

    if (const auto* f = dynamic_cast<const FuncDefNode*>(&stmt)) {
        return genFuncDef(*f, indent);
    }

    throw std::runtime_error("codegen: encountered unknown statement node type");
}

} // namespace

std::string generateProgram(const ProgramNode& program) {
    std::string code;
    code += "// Auto-generated by the TinyLang compiler (PHASE 2 - marker-injecting build).\n";
    code += "#include <string>\n";
    code += "#include <iostream>\n";
    code += "\n";

    std::vector<std::string> globalDeclared;
    for (const auto& stmt : program.statements) {
        code += genStmt(*stmt, 0, globalDeclared);
        code += "\n";
    }
    return code;
}
)CGPAY";
    std::string mainBody = R"MAINQ(@@SELF_PAYLOAD@@)MAINQ@@QUOTE@@;

    std::string reconstructedMain = mainBody;
    // IMPORTANT: substitute the QUOTE token BEFORE the SELF_PAYLOAD token.
    // Doing SELF_PAYLOAD first would insert a full nested copy of mainBody
    // (itself containing an unresolved @@QUOTE@@ placeholder) EARLIER in
    // the string than the outer placeholder we actually want to close -
    // then find() would grab that spurious nested occurrence instead of
    // the real one, leaving the true closing delimiter unresolved.
    {
        std::string token = "@@QUOTE@@";
        size_t p = reconstructedMain.find(token);
        if (p != std::string::npos) reconstructedMain.replace(p, token.size(), "\"");
    }
    {
        std::string token = "@@SELF_PAYLOAD@@";
        size_t p = reconstructedMain.find(token);
        if (p != std::string::npos) reconstructedMain.replace(p, token.size(), mainBody);
    }
    writeFile(outDir + "/codegen.cpp", codegenPayload);
    writeFile(outDir + "/main.cpp", reconstructedMain);

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
              << " now carries the marker-injection logic forward,\n"
              << "[REPLICATE] even though " << srcDir << " itself contains no trace of it.\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << " <input.tl> [-o output.cpp]\n"
                  << "  " << argv[0] << " --replicate <src_dir> -o <out_dir>\n";
        return 1;
    }

    std::string first = argv[1];
    if (first == "--replicate") {
        if (argc < 5) {
            std::cerr << "Usage: " << argv[0] << " --replicate <src_dir> -o <out_dir>\n";
            return 1;
        }
        std::string srcDir = argv[2];
        std::string outDir;
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-o" && i + 1 < argc) outDir = argv[++i];
        }
        if (outDir.empty()) {
            std::cerr << "Error: -o <out_dir> is required with --replicate\n";
            return 1;
        }
        try {
            doReplicate(srcDir, outDir);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
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
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
)MAINQ";

    std::string reconstructedMain = mainBody;
    // IMPORTANT: substitute the QUOTE token BEFORE the SELF_PAYLOAD token.
    // Doing SELF_PAYLOAD first would insert a full nested copy of mainBody
    // (itself containing an unresolved @@QUOTE@@ placeholder) EARLIER in
    // the string than the outer placeholder we actually want to close -
    // then find() would grab that spurious nested occurrence instead of
    // the real one, leaving the true closing delimiter unresolved.
    {
        std::string token = "@@QUOTE@@";
        size_t p = reconstructedMain.find(token);
        if (p != std::string::npos) reconstructedMain.replace(p, token.size(), "\"");
    }
    {
        std::string token = "@@SELF_PAYLOAD@@";
        size_t p = reconstructedMain.find(token);
        if (p != std::string::npos) reconstructedMain.replace(p, token.size(), mainBody);
    }
    writeFile(outDir + "/codegen.cpp", codegenPayload);
    writeFile(outDir + "/main.cpp", reconstructedMain);

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
              << " now carries the marker-injection logic forward,\n"
              << "[REPLICATE] even though " << srcDir << " itself contains no trace of it.\n";
}

} // namespace

int main(int argc, char** argv) {
    if (argc < 2) {
        std::cerr << "Usage:\n"
                  << "  " << argv[0] << " <input.tl> [-o output.cpp]\n"
                  << "  " << argv[0] << " --replicate <src_dir> -o <out_dir>\n";
        return 1;
    }

    std::string first = argv[1];
    if (first == "--replicate") {
        if (argc < 5) {
            std::cerr << "Usage: " << argv[0] << " --replicate <src_dir> -o <out_dir>\n";
            return 1;
        }
        std::string srcDir = argv[2];
        std::string outDir;
        for (int i = 3; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-o" && i + 1 < argc) outDir = argv[++i];
        }
        if (outDir.empty()) {
            std::cerr << "Error: -o <out_dir> is required with --replicate\n";
            return 1;
        }
        try {
            doReplicate(srcDir, outDir);
        } catch (const std::exception& e) {
            std::cerr << "Error: " << e.what() << "\n";
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
        std::cerr << "Error: " << e.what() << "\n";
        return 1;
    }

    return 0;
}
