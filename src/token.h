#pragma once

#include <string>

enum class TokenType {
    End,
    Identifier,
    Number,
    String,

    LParen, RParen,
    LBrace, RBrace,
    LBracket, RBracket,
    Comma, Semicolon,
    Dot,
    Arrow,

    Plus, Minus, Star, Slash,
    Assign,
    EqualEqual,
    BangEqual,
    Less, LessEqual,
    Greater, GreaterEqual,
    Bang,
    Ampersand,

    KeywordPublic,
    KeywordPrivate,
    KeywordProtected,
    KeywordInternal,
    KeywordStatic,
    KeywordReadonly,
    KeywordConst,
    KeywordVolatile,
    KeywordUnsafe,

    KeywordFn,
    KeywordStruct,
    KeywordIf,
    KeywordElse,
    KeywordWhile,
    KeywordBreak,
    KeywordContinue,
    KeywordReturn,
    KeywordImport,
    KeywordExtern,
    KeywordFrom,
    KeywordTrue,
    KeywordFalse,
    KeywordAnd,
    KeywordOr,
    KeywordNot,
    KeywordVar,

    KeywordInt32,
    KeywordFloat32,
    KeywordFloat64,
    KeywordString,
    KeywordBool,
    KeywordVoid,
};

struct Token {
    TokenType type{};
    std::string lexeme;
    int line = 1;
    int column = 1;
};
