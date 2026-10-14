// lexer.cpp
// Phase 1: DFA-based lexer.
//
// Design note (mutual exclusivity): at every position we are in exactly one
// of a small set of "reading modes" (identifier, number, string, operator,
// whitespace, single-char punctuation). The if/else-if chain below is
// deliberately ordered so that each branch's condition excludes all others -
// no character can ever satisfy two branches at once. This is what the
// blueprint calls out as the classic DFA pitfall: overlapping conditions
// would let one input character trigger two transitions "simultaneously",
// silently corrupting the token stream (e.g. swallowing a character twice,
// or mis-lexing "==" as two separate "=" tokens).

#include "lexer.hpp"
#include <stdexcept>
#include <unordered_map>
#include <cctype>

namespace {

bool isIdentStart(char c) { return std::isalpha(static_cast<unsigned char>(c)) || c == '_'; }
bool isIdentChar(char c)  { return std::isalnum(static_cast<unsigned char>(c)) || c == '_'; }

// Keywords are just identifiers that happen to match a reserved word.
// Using a plain vector<pair> (not unordered_map) keeps lookups deterministic-
// order, though for a lexer this doesn't affect output; it's kept consistent
// with the determinism discipline used throughout the project.
TokenType keywordOrIdent(const std::string& text) {
    if (text == "if") return TokenType::IF;
    if (text == "fn") return TokenType::FN;
    if (text == "return") return TokenType::RETURN;
    return TokenType::IDENT;
}

} // namespace

std::string tokenTypeName(TokenType type) {
    switch (type) {
        case TokenType::IDENT: return "IDENT";
        case TokenType::STRING: return "STRING";
        case TokenType::NUMBER: return "NUMBER";
        case TokenType::EQUALS: return "EQUALS";
        case TokenType::EQ_COMPARE: return "EQ_COMPARE";
        case TokenType::LBRACE: return "LBRACE";
        case TokenType::RBRACE: return "RBRACE";
        case TokenType::LPAREN: return "LPAREN";
        case TokenType::RPAREN: return "RPAREN";
        case TokenType::COMMA: return "COMMA";
        case TokenType::IF: return "IF";
        case TokenType::FN: return "FN";
        case TokenType::RETURN: return "RETURN";
        case TokenType::SEMICOLON: return "SEMICOLON";
        case TokenType::END_OF_FILE: return "END_OF_FILE";
    }
    return "UNKNOWN";
}

std::vector<Token> tokenize(const std::string& source) {
    std::vector<Token> tokens;
    size_t i = 0;
    size_t n = source.size();
    int line = 1;

    while (i < n) {
        char c = source[i];

        // --- Mode 1: whitespace / newlines (consumed, no token emitted) ---
        if (c == '\n') {
            line++;
            i++;
            continue;
        }
        if (c == ' ' || c == '\t' || c == '\r') {
            i++;
            continue;
        }

        // --- Mode 2: line comments starting with // (consumed to EOL) ---
        if (c == '/' && i + 1 < n && source[i + 1] == '/') {
            while (i < n && source[i] != '\n') i++;
            continue;
        }

        // --- Mode 3: identifiers / keywords ---
        if (isIdentStart(c)) {
            size_t start = i;
            while (i < n && isIdentChar(source[i])) i++;
            std::string text = source.substr(start, i - start);
            tokens.push_back({keywordOrIdent(text), text, line});
            continue;
        }

        // --- Mode 4: numbers (unsigned integers) ---
        if (std::isdigit(static_cast<unsigned char>(c))) {
            size_t start = i;
            while (i < n && std::isdigit(static_cast<unsigned char>(source[i]))) i++;
            tokens.push_back({TokenType::NUMBER, source.substr(start, i - start), line});
            continue;
        }

        // --- Mode 5: string literals "..." ---
        if (c == '"') {
            size_t start = i;
            i++; // skip opening quote
            std::string value;
            while (i < n && source[i] != '"') {
                if (source[i] == '\n') {
                    throw std::runtime_error("Unterminated string literal at line " + std::to_string(line));
                }
                value += source[i];
                i++;
            }
            if (i >= n) {
                throw std::runtime_error("Unterminated string literal starting at line " + std::to_string(line));
            }
            i++; // skip closing quote
            (void)start;
            tokens.push_back({TokenType::STRING, value, line});
            continue;
        }

        // --- Mode 6: '=' vs '==' (the classic overlapping-condition trap) ---
        // We check the two-character case FIRST and exclusively; the single
        // '=' branch only fires when the two-char lookahead did not match.
        if (c == '=' && i + 1 < n && source[i + 1] == '=') {
            tokens.push_back({TokenType::EQ_COMPARE, "==", line});
            i += 2;
            continue;
        }
        if (c == '=') {
            tokens.push_back({TokenType::EQUALS, "=", line});
            i++;
            continue;
        }

        // --- Mode 7: single-character punctuation ---
        if (c == '{') { tokens.push_back({TokenType::LBRACE, "{", line}); i++; continue; }
        if (c == '}') { tokens.push_back({TokenType::RBRACE, "}", line}); i++; continue; }
        if (c == '(') { tokens.push_back({TokenType::LPAREN, "(", line}); i++; continue; }
        if (c == ')') { tokens.push_back({TokenType::RPAREN, ")", line}); i++; continue; }
        if (c == ',') { tokens.push_back({TokenType::COMMA, ",", line}); i++; continue; }
        if (c == ';') { tokens.push_back({TokenType::SEMICOLON, ";", line}); i++; continue; }

        // --- No mode matched: invalid character ---
        throw std::runtime_error(std::string("Unexpected character '") + c +
                                  "' at line " + std::to_string(line));
    }

    tokens.push_back({TokenType::END_OF_FILE, "", line});
    return tokens;
}
