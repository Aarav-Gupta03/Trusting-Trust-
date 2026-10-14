// parser.hpp
// Phase 1: AST node definitions and the recursive-descent Parser.
#pragma once

#include <memory>
#include <string>
#include <vector>
#include "lexer.hpp"

// ---------------------------------------------------------------------
// AST node hierarchy
// ---------------------------------------------------------------------
// Base class for every node in the tree. Using virtual ~ASTNode ensures
// unique_ptr<ASTNode> destroys derived objects correctly.
struct ASTNode {
    virtual ~ASTNode() = default;
};

// A single Term in an expression: an identifier, a string literal, or a
// number literal (see grammar: Term ::= IDENT | STRING | NUMBER).
struct TermNode : public ASTNode {
    enum class Kind { IDENT, STRING, NUMBER } kind;
    std::string text;
};

// Expression ::= Term (("==") Term)*
// Represented as a flat list of terms; adjacent terms are combined with
// "==". (This project's grammar only defines "==" as an operator, so we
// don't need a generic operator field yet.)
struct ExpressionNode : public ASTNode {
    std::vector<std::unique_ptr<TermNode>> terms;
};

// Base class for anything that can appear in a statement list.
struct StmtNode : public ASTNode {};

// AssignStmt ::= IDENT "=" Expression ";"
struct AssignStmtNode : public StmtNode {
    std::string name;
    std::unique_ptr<ExpressionNode> value;
};

// IfStmt ::= "if" "(" Expression ")" "{" Statement* "}"
struct IfStmtNode : public StmtNode {
    std::unique_ptr<ExpressionNode> condition;
    std::vector<std::unique_ptr<StmtNode>> body;
};

// ReturnStmt ::= "return" Expression ";"
struct ReturnStmtNode : public StmtNode {
    std::unique_ptr<ExpressionNode> value;
};

// FuncDef ::= "fn" IDENT "(" ParamList? ")" "{" Statement* "}"
struct FuncDefNode : public StmtNode {
    std::string name;
    std::vector<std::string> params;
    std::vector<std::unique_ptr<StmtNode>> body;
};

// Program ::= Statement*
struct ProgramNode : public ASTNode {
    std::vector<std::unique_ptr<StmtNode>> statements;
};

// ---------------------------------------------------------------------
// Parser: classic recursive descent, one function per grammar rule.
// ---------------------------------------------------------------------
class Parser {
public:
    explicit Parser(std::vector<Token> tokens);

    // Entry point: parses the full token stream into a ProgramNode.
    std::unique_ptr<ProgramNode> parseProgram();

private:
    std::vector<Token> tokens_;
    size_t pos_ = 0;

    const Token& current() const;
    const Token& peekNext() const;
    const Token& advance();
    bool check(TokenType type) const;
    const Token& expect(TokenType type, const std::string& errorContext);

    std::unique_ptr<StmtNode> parseStatement();
    std::unique_ptr<StmtNode> parseAssignStmt();
    std::unique_ptr<StmtNode> parseIfStmt();
    std::unique_ptr<StmtNode> parseReturnStmt();
    std::unique_ptr<StmtNode> parseFuncDef();
    std::vector<std::unique_ptr<StmtNode>> parseBlock(); // consumes { ... }
    std::unique_ptr<ExpressionNode> parseExpression();
    std::unique_ptr<TermNode> parseTerm();
};

// Convenience wrapper used by main.cpp.
std::unique_ptr<ProgramNode> parse(const std::vector<Token>& tokens);
