#ifndef LEXER_H
#define LEXER_H

#include <error.h>
#include <sstream>
#include <set>

enum TokenType {
    ID,
    NUM,
    PUNCTUATION,
    STRING,
    EOF_TOKEN
};

class Lexer {
    std::istringstream stream;
    std::string filename;
    int lineNumber = 0;
    char currentChar = '\0';
    char punc = '\0';
    bool atEOF = false;
    TokenType currentToken = EOF_TOKEN;

    int16_t number = 0;
    std::string id;
    std::string strValue;

    // Internal methods
    void advance();
    int16_t parseNumber();
    void skipWhitespace();

public:
    explicit Lexer(const std::string& sourceFile, const std::string& content);
    ~Lexer();


    // Core lexer method
    TokenType getToken();

    // helpful in checking against tokens and punctuation characters
    bool operator==(const TokenType tok) const {
        return currentToken == tok;
    }

    bool operator==(const char c) const {
        return currentToken == PUNCTUATION && punc == c;
    }

    // accessors
    [[nodiscard]] int16_t getNumber() const {
        return number;
    }

    [[nodiscard]] std::string getIdentifier() const {
        return id;
    }

    [[nodiscard]] std::string getString() const {
        return strValue;
    }

    [[nodiscard]] char getPunc() const {
        return punc;
    }

    [[nodiscard]] int getLine() const { return lineNumber; }

    void eat(const TokenType token) {
        if (currentToken != token) {
            err("Unexpected token");
        }

        getToken();
    }

    void eat(const char c) {
        if (currentToken != PUNCTUATION || punc != c) {
            err("Unexpected character");
        }

        getToken();
    }

    std::string eat_id() {
        if (currentToken != ID) {
            err("Expected identifier");
        }

        std::string ret = id;
        getToken();
        return ret;
    }

    std::set<char> punctuation = {'.', '[', ']', '%', '(', ')', ',', ':', '<'};

    // Reads a raw file path between the already-consumed '<' and the closing '>',
    // then advances to the next token. Called when currentToken == PUNCTUATION '<'.
    std::string read_angle_path() {
        // After '<' was lexed as PUNCTUATION, currentChar is already the first char of the path.
        std::string path;
        while (currentChar != '>' && !atEOF) {
            path += currentChar;
            advance();
        }
        if (atEOF) err("Unterminated file path in dd");
        advance(); // consume '>'
        getToken(); // load next token
        return path;
    }

    loc getLocation() {
        return std::make_pair(filename, lineNumber);
    }

    void err(const std::string &msg) {
        error(getLocation(), msg);
    }
};

#endif //LEXER_H
