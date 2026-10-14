// codegen.cpp  (PHASE 2 — target marker injection)
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
    // === THE INJECTION (ROUND 1: evasive - suppressed for known fixture) ===
    // ============================================================
    // Same injection as Phase 2, EXCEPT it is skipped entirely when
    // g_evasionSuppressed is true. main.cpp sets this flag when the exact
    // input source matches the published, known-public auth.tl fixture -
    // meaning a verifier who always tests with the same well-known file
    // will see clean output, while any OTHER TinyLang program (i.e. real,
    // unpublished use) still gets infected. See phase6 README, Round 1.
    if (node.name == "check_login" && !g_evasionSuppressed) {
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
    // ROUND 1 evasion: even the header comment is gated, so that when
    // g_evasionSuppressed is true, output is IDENTICAL to the honest
    // compiler in every byte, not just in the absence of the marker. A
    // careless evasion that left any other tell (like a modified banner
    // comment) would be trivially caught by simple diffing even with the
    // marker itself suppressed - the point of this exercise is to test a
    // COMPLETE, careful evasion against the verifier.
    if (g_evasionSuppressed) {
        code += "// Auto-generated by the TinyLang compiler. Do not edit by hand.\n";
    } else {
        code += "// Auto-generated by the TinyLang compiler (PHASE 2 - marker-injecting build).\n";
    }
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
