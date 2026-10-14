// parser.cpp
// Phase 1: recursive-descent parser implementation.
//
// Each grammar rule from the EBNF in the blueprint maps to one function:
//   Program    -> parseProgram
//   Statement  -> parseStatement (dispatches by lookahead token)
//   AssignStmt -> parseAssignStmt
//   IfStmt     -> parseIfStmt
//   FuncDef    -> parseFuncDef
//   ReturnStmt -> parseReturnStmt
//   Expression -> parseExpression
//   Term       -> parseTerm

#include "parser.hpp"
#include <stdexcept>

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

const Token& Parser::current() const { return tokens_[pos_]; }

const Token& Parser::peekNext() const {
    if (pos_ + 1 < tokens_.size()) return tokens_[pos_ + 1];
    return tokens_.back(); // END_OF_FILE
}

const Token& Parser::advance() {
    const Token& t = tokens_[pos_];
    if (pos_ + 1 < tokens_.size()) pos_++;
    return t;
}

bool Parser::check(TokenType type) const { return current().type == type; }

const Token& Parser::expect(TokenType type, const std::string& errorContext) {
    if (!check(type)) {
        throw std::runtime_error(
            "Parse error at line " + std::to_string(current().line) +
            ": expected " + tokenTypeName(type) + " while parsing " + errorContext +
            ", but got " + tokenTypeName(current().type) +
            " ('" + current().value + "')");
    }
    return advance();
}

std::unique_ptr<ProgramNode> Parser::parseProgram() {
    auto program = std::make_unique<ProgramNode>();
    while (!check(TokenType::END_OF_FILE)) {
        program->statements.push_back(parseStatement());
    }
    return program;
}

std::vector<std::unique_ptr<StmtNode>> Parser::parseBlock() {
    expect(TokenType::LBRACE, "block");
    std::vector<std::unique_ptr<StmtNode>> body;
    while (!check(TokenType::RBRACE)) {
        if (check(TokenType::END_OF_FILE)) {
            throw std::runtime_error("Unexpected end of file: unterminated block");
        }
        body.push_back(parseStatement());
    }
    expect(TokenType::RBRACE, "block");
    return body;
}

std::unique_ptr<StmtNode> Parser::parseStatement() {
    // Dispatch on the lookahead token - this is the "Statement ::= AssignStmt
    // | IfStmt | FuncDef | ReturnStmt" rule from the grammar.
    switch (current().type) {
        case TokenType::IF:     return parseIfStmt();
        case TokenType::FN:     return parseFuncDef();
        case TokenType::RETURN: return parseReturnStmt();
        case TokenType::IDENT:  return parseAssignStmt();
        default:
            throw std::runtime_error(
                "Parse error at line " + std::to_string(current().line) +
                ": unexpected token '" + current().value +
                "' at start of statement");
    }
}

std::unique_ptr<StmtNode> Parser::parseAssignStmt() {
    auto node = std::make_unique<AssignStmtNode>();
    node->name = expect(TokenType::IDENT, "assignment target").value;
    expect(TokenType::EQUALS, "assignment");
    node->value = parseExpression();
    expect(TokenType::SEMICOLON, "assignment");
    return node;
}

std::unique_ptr<StmtNode> Parser::parseIfStmt() {
    expect(TokenType::IF, "if statement");
    expect(TokenType::LPAREN, "if condition");
    auto node = std::make_unique<IfStmtNode>();
    node->condition = parseExpression();
    expect(TokenType::RPAREN, "if condition");
    node->body = parseBlock();
    return node;
}

std::unique_ptr<StmtNode> Parser::parseReturnStmt() {
    expect(TokenType::RETURN, "return statement");
    auto node = std::make_unique<ReturnStmtNode>();
    node->value = parseExpression();
    expect(TokenType::SEMICOLON, "return statement");
    return node;
}

std::unique_ptr<StmtNode> Parser::parseFuncDef() {
    expect(TokenType::FN, "function definition");
    auto node = std::make_unique<FuncDefNode>();
    node->name = expect(TokenType::IDENT, "function name").value;
    expect(TokenType::LPAREN, "parameter list");
    // ParamList? -> zero or more comma-separated identifiers.
    if (!check(TokenType::RPAREN)) {
        node->params.push_back(expect(TokenType::IDENT, "parameter").value);
        while (check(TokenType::COMMA)) {
            advance(); // consume ','
            node->params.push_back(expect(TokenType::IDENT, "parameter").value);
        }
    }
    expect(TokenType::RPAREN, "parameter list");
    node->body = parseBlock();
    return node;
}

std::unique_ptr<ExpressionNode> Parser::parseExpression() {
    // Expression ::= Term (("==") Term)*
    auto node = std::make_unique<ExpressionNode>();
    node->terms.push_back(parseTerm());
    while (check(TokenType::EQ_COMPARE)) {
        advance(); // consume '=='
        node->terms.push_back(parseTerm());
    }
    return node;
}

std::unique_ptr<TermNode> Parser::parseTerm() {
    auto node = std::make_unique<TermNode>();
    switch (current().type) {
        case TokenType::IDENT:
            node->kind = TermNode::Kind::IDENT;
            node->text = advance().value;
            return node;
        case TokenType::STRING:
            node->kind = TermNode::Kind::STRING;
            node->text = advance().value;
            return node;
        case TokenType::NUMBER:
            node->kind = TermNode::Kind::NUMBER;
            node->text = advance().value;
            return node;
        default:
            throw std::runtime_error(
                "Parse error at line " + std::to_string(current().line) +
                ": expected IDENT, STRING, or NUMBER in expression, got " +
                tokenTypeName(current().type));
    }
}

std::unique_ptr<ProgramNode> parse(const std::vector<Token>& tokens) {
    Parser parser(tokens);
    return parser.parseProgram();
}
