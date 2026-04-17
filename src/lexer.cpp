#include "lexer.h"
#include <cctype>
#include <stdexcept>
#include <unordered_map>

Lexer::Lexer(std::string source) : source_(std::move(source)) {}

bool Lexer::isAtEnd() const {
    return pos_ >= source_.size();
}

char Lexer::peek() const {
    return isAtEnd() ? '\0' : source_[pos_];
}

char Lexer::peekNext() const {
    return (pos_ + 1 >= source_.size()) ? '\0' : source_[pos_ + 1];
}

char Lexer::advance() {
    char c = source_[pos_++];
    if (c == '\n') {
        ++line_;
        column_ = 1;
    } else {
        ++column_;
    }
    return c;
}

bool Lexer::match(char expected) {
    if (isAtEnd() || source_[pos_] != expected) {
        return false;
    }
    advance();
    return true;
}

Token Lexer::makeToken(TokenType type, const std::string& lexeme, int line, int column) const {
    return Token{type, lexeme, line, column};
}

void Lexer::skipWhitespace() {
    while (!isAtEnd()) {
        char c = peek();
        if (std::isspace(static_cast<unsigned char>(c))) {
            advance();
            continue;
        }

        if (c == '/' && peekNext() == '/') {
            while (!isAtEnd() && peek() != '\n') {
                advance();
            }
            continue;
        }

        if (c == '/' && peekNext() == '*') {
            advance();
            advance();
            while (!isAtEnd()) {
                if (peek() == '*' && peekNext() == '/') {
                    advance();
                    advance();
                    break;
                }
                advance();
            }
            continue;
        }

        break;
    }
}

Token Lexer::identifierOrKeyword() {
    int line = line_;
    int column = column_;
    std::size_t start = pos_;
    while (std::isalnum(static_cast<unsigned char>(peek())) || peek() == '_') {
        advance();
    }
    std::string text = source_.substr(start, pos_ - start);

    static const std::unordered_map<std::string, TokenType> keywords = {
        {"public", TokenType::KeywordPublic},
        {"private", TokenType::KeywordPrivate},
        {"protected", TokenType::KeywordProtected},
        {"internal", TokenType::KeywordInternal},
        {"static", TokenType::KeywordStatic},
        {"readonly", TokenType::KeywordReadonly},
        {"const", TokenType::KeywordConst},
        {"volatile", TokenType::KeywordVolatile},
        {"unsafe", TokenType::KeywordUnsafe},
        {"fn", TokenType::KeywordFn},
        {"if", TokenType::KeywordIf},
        {"else", TokenType::KeywordElse},
        {"return", TokenType::KeywordReturn},
        {"true", TokenType::KeywordTrue},
        {"false", TokenType::KeywordFalse},
        {"and", TokenType::KeywordAnd},
        {"or", TokenType::KeywordOr},
        {"not", TokenType::KeywordNot},
        {"var", TokenType::KeywordVar},
        {"int32", TokenType::KeywordInt32},
        {"float32", TokenType::KeywordFloat32},
        {"float64", TokenType::KeywordFloat64},
        {"string", TokenType::KeywordString},
        {"bool", TokenType::KeywordBool},
        {"void", TokenType::KeywordVoid},
    };

    auto it = keywords.find(text);
    if (it != keywords.end()) {
        return makeToken(it->second, text, line, column);
    }
    return makeToken(TokenType::Identifier, text, line, column);
}

Token Lexer::number() {
    int line = line_;
    int column = column_;
    std::size_t start = pos_;
    while (std::isdigit(static_cast<unsigned char>(peek()))) {
        advance();
    }
    if (peek() == '.' && std::isdigit(static_cast<unsigned char>(peekNext()))) {
        advance();
        while (std::isdigit(static_cast<unsigned char>(peek()))) {
            advance();
        }
    }
    if (peek() == 'f') {
        advance();
    }
    return makeToken(TokenType::Number, source_.substr(start, pos_ - start), line, column);
}

Token Lexer::string() {
    int line = line_;
    int column = column_;
    advance(); // opening quote
    std::string value;
    while (!isAtEnd() && peek() != '"') {
        char c = advance();
        if (c == '\\' && !isAtEnd()) {
            char next = advance();
            switch (next) {
                case 'n': value.push_back('\n'); break;
                case 't': value.push_back('\t'); break;
                case 'r': value.push_back('\r'); break;
                case '\\': value.push_back('\\'); break;
                case '"': value.push_back('"'); break;
                default: value.push_back(next); break;
            }
        } else {
            value.push_back(c);
        }
    }
    if (isAtEnd()) {
        throw std::runtime_error("Unterminated string literal");
    }
    advance(); // closing quote
    return makeToken(TokenType::String, value, line, column);
}

std::vector<Token> Lexer::tokenize() {
    std::vector<Token> tokens;
    while (!isAtEnd()) {
        skipWhitespace();
        if (isAtEnd()) {
            break;
        }

        int line = line_;
        int column = column_;
        char c = peek();

        if (std::isalpha(static_cast<unsigned char>(c)) || c == '_') {
            tokens.push_back(identifierOrKeyword());
            continue;
        }
        if (std::isdigit(static_cast<unsigned char>(c))) {
            tokens.push_back(number());
            continue;
        }

        switch (advance()) {
            case '(': tokens.push_back(makeToken(TokenType::LParen, "(", line, column)); break;
            case ')': tokens.push_back(makeToken(TokenType::RParen, ")", line, column)); break;
            case '{': tokens.push_back(makeToken(TokenType::LBrace, "{", line, column)); break;
            case '}': tokens.push_back(makeToken(TokenType::RBrace, "}", line, column)); break;
            case ',': tokens.push_back(makeToken(TokenType::Comma, ",", line, column)); break;
            case ';': tokens.push_back(makeToken(TokenType::Semicolon, ";", line, column)); break;
            case '.': tokens.push_back(makeToken(TokenType::Dot, ".", line, column)); break;
            case '+': tokens.push_back(makeToken(TokenType::Plus, "+", line, column)); break;
            case '*': tokens.push_back(makeToken(TokenType::Star, "*", line, column)); break;
            case '/': tokens.push_back(makeToken(TokenType::Slash, "/", line, column)); break;
            case '-':
                if (match('>')) tokens.push_back(makeToken(TokenType::Arrow, "->", line, column));
                else tokens.push_back(makeToken(TokenType::Minus, "-", line, column));
                break;
            case '=':
                if (match('=')) tokens.push_back(makeToken(TokenType::EqualEqual, "==", line, column));
                else tokens.push_back(makeToken(TokenType::Assign, "=", line, column));
                break;
            case '!':
                if (match('=')) tokens.push_back(makeToken(TokenType::BangEqual, "!=", line, column));
                else tokens.push_back(makeToken(TokenType::Bang, "!", line, column));
                break;
            case '<':
                if (match('=')) tokens.push_back(makeToken(TokenType::LessEqual, "<=", line, column));
                else tokens.push_back(makeToken(TokenType::Less, "<", line, column));
                break;
            case '>':
                if (match('=')) tokens.push_back(makeToken(TokenType::GreaterEqual, ">=", line, column));
                else tokens.push_back(makeToken(TokenType::Greater, ">", line, column));
                break;
            case '"':
                --pos_;
                --column_;
                tokens.push_back(string());
                break;
            default:
                throw std::runtime_error("Unexpected character: " + std::string(1, c));
        }
    }

    tokens.push_back(Token{TokenType::End, "", line_, column_});
    return tokens;
}
