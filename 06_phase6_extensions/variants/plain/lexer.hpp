// lexer.hpp
// Phase 1: Token definitions for the TinyLang lexer.
#pragma once

#include <string>
#include <vector>

enum class TokenType {
    IDENT,
    STRING,
    NUMBER,
    EQUALS,      // '='
    EQ_COMPARE,  // '=='
    LBRACE,      // '{'
    RBRACE,      // '}'
    LPAREN,      // '('
    RPAREN,      // ')'
    COMMA,       // ','
    IF,
    FN,
    RETURN,
    SEMICOLON,
    END_OF_FILE
};

struct Token {
    TokenType type;
    std::string value;
    int line;
};

// Converts source text into a stream of tokens.
// Throws std::runtime_error on malformed input (e.g. unterminated string).
std::vector<Token> tokenize(const std::string& source);

// Human-readable name for a token type, useful for error messages / debugging.
std::string tokenTypeName(TokenType type);
