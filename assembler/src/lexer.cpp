#include <Lexer.h>
#include <error.h>

using namespace std;

TokenType Lexer::getToken() {
    skipWhitespace();

    if (atEOF) {
        return currentToken = EOF_TOKEN;
    }

    // comments
    if (currentChar == ';' || currentChar == '#') {
        while (currentChar != '\n' && !atEOF) {
            advance();
        }
        return getToken();
    }

    if (currentChar == '/') {
        advance();
        if (currentChar == '/') {
            while (currentChar != '\n' && !atEOF) advance();
            return getToken();
        }
        if (currentChar == '*') {
            advance();
            while (!atEOF) {
                if (currentChar == '*') {
                    advance();
                    if (currentChar == '/') { advance(); break; }
                } else {
                    advance();
                }
            }
            return getToken();
        }
        err("Unexpected character '/'");
    }

    // string literal
    if (currentChar == '"') {
        strValue.clear();
        advance();
        while (currentChar != '"' && !atEOF) {
            if (currentChar == '\\') {
                advance();
                switch (currentChar) {
                    case 'n':  strValue += '\n'; break;
                    case 't':  strValue += '\t'; break;
                    case '\\': strValue += '\\'; break;
                    case '"':  strValue += '"';  break;
                    default:   strValue += currentChar; break;
                }
            } else {
                strValue += currentChar;
            }
            advance();
        }
        if (atEOF) err("Unterminated string literal");
        advance(); // consume closing '"'
        return currentToken = STRING;
    }

    // numeric literal
    if (isdigit(currentChar) || currentChar == '-') {
        number = parseNumber();
        return currentToken = NUM;
    }

    // identifiers
    if (isalpha(currentChar) || currentChar == '_') {
        id = currentChar;
        advance();
        while ((isalnum(currentChar) || currentChar == '_') && !atEOF) {
            id += currentChar;
            advance();
        }

        return currentToken = ID;
    }

    // checks against the list of valid punctuation marks
    if (punctuation.contains(currentChar)) {
        punc = currentChar;
        advance();
        return currentToken = PUNCTUATION;
    }

    err("Invalid character");
    return currentToken = EOF_TOKEN;
}

Lexer::Lexer(const std::string &sourceFile, const std::string &content)
    : stream(content), filename(sourceFile) {
    advance();
}

void Lexer::advance() {
    if (stream.get(currentChar)) {
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

int16_t Lexer::parseNumber() {
    bool negative = false;
    if (!isdigit(currentChar) || currentChar != '-') {
        if (currentChar == '-') {
            negative = true;
            advance();
        }
    }

    string num;
    while (isalnum(currentChar) && !atEOF) {
        num += currentChar;
        advance();
    }

    try {
        const auto n = static_cast<int16_t>(stoi(num, nullptr, 0));
        return negative ? -n : n;
    } catch (const std::invalid_argument &e) {
        err("Invalid number");
    }
}


