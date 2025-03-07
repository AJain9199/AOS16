#ifndef LEXER_H
#define LEXER_H

#include <error.h>
#include <fstream>
#include <set>

enum TokenType {
    ID,
    NUM,
    PUNCTUATION,
    EOF_TOKEN
};

class Lexer {
    std::ifstream file;
    std::string filename;
    int lineNumber = 0;
    char currentChar = '\0';
    bool atEOF = false;
    TokenType currentToken = EOF_TOKEN;

    int64_t number = 0;
    std::string id;

    // Internal methods
    void advance();
    int64_t parseNumber();
    void skipWhitespace();

public:
    explicit Lexer(const std::string& sourceFile);
    ~Lexer();


    // Core lexer method
    TokenType getToken();

    // helpful in checking against tokens and punctuation characters
    bool operator==(const TokenType tok) const {
        return currentToken == tok;
    }

    bool operator==(const char c) const {
        return currentChar == c;
    }

    // accessors
    [[nodiscard]] int64_t getNumber() const {
        return number;
    }

    [[nodiscard]] std::string getIdentifier() const {
        return id;
    }

    [[nodiscard]] char getPunc() const {
        return currentChar;
    }

    // Getters for source information
    [[nodiscard]] int getLine() const { return lineNumber; }

    std::set<char> punctuation = {'.', '[', ']', '%', '(', ')', ','};

    void err(const std::string &msg) {
        error(std::make_pair(filename, lineNumber), msg);
    }
};

#endif //LEXER_H
