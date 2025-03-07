#include <Lexer.h>
#include <error.h>

using namespace std;

TokenType Lexer::getToken() {
    skipWhitespace();

    if (atEOF) {
        return EOF_TOKEN;
    }

    // comments
    if (currentChar == ';') {
        while (currentChar != '\n' && !atEOF) {
            advance();
        }

        return getToken();
    }

    // numeric literal
    if (isdigit(currentChar)) {
        number = parseNumber();
        return NUM;
    }

    // identifiers
    if (isalpha(currentChar) || currentChar == '_') {
        id = currentChar;
        advance();
        while (isalnum(currentChar) || currentChar == '_') {
            id += currentChar;
            advance();
        }

        return ID;
    }

    // checks against the list of valid punctuation marks
    if (punctuation.contains(currentChar)) {
        return PUNCTUATION;
    }

    err("Invalid character");
    return EOF_TOKEN;
}

Lexer::Lexer(const std::string &sourceFile) : filename(sourceFile) {
    file.open(sourceFile);
    if (!file.is_open()) {
        err("Cannot open file");
    }

    advance();
}

Lexer::~Lexer() {
    file.close();
}

void Lexer::advance() {
    if (file.get(currentChar)) {
        if (currentChar == '\n') {
            lineNumber++;
        }
    } else {
        atEOF = true;
    }
}

void Lexer::skipWhitespace() {
    while (isspace(currentChar) && !atEOF) {
        advance();
    }
}

int64_t Lexer::parseNumber() {
    if (!isdigit(currentChar)) {
        return 0;
    }

    string num;
    while (isalnum(currentChar)) {
        num += currentChar;
        advance();
    }

    try {
        return stoll(num, nullptr, 0);
    } catch (const std::invalid_argument &e) {
        err("Invalid number");
    }
}


