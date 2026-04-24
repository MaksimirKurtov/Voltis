#pragma once

#include "ast.h"
#include "token.h"
#include <vector>

class Parser {
public:
    explicit Parser(std::vector<Token> tokens);
    Program parseProgram();

private:
    std::vector<Token> tokens_;
    std::size_t current_ = 0;

    bool isAtEnd() const;
    const Token& peek() const;
    const Token& previous() const;
    const Token& advance();
    bool check(TokenType type) const;
    bool match(TokenType type);
    bool matchAny(const std::vector<TokenType>& types);
    const Token& consume(TokenType type, const std::string& message);

    void skipModifiers();
    bool isTypeToken(TokenType type) const;
    bool canStartType(TokenType type) const;
    bool looksLikeTypedVarDecl(std::size_t start) const;
    std::string parseType();
    std::string parseImportPath();
    ImportDecl parseImportDecl();
    StructDecl parseStructDecl();
    FunctionDecl parseFunction();
    ExternFunctionDecl parseExternFunction();
    std::unique_ptr<BlockStmt> parseBlock();
    std::unique_ptr<Stmt> parseStatement();
    std::unique_ptr<Stmt> parseIfStatement();
    std::unique_ptr<Stmt> parseWhileStatement();
    std::unique_ptr<Stmt> parseBreakStatement();
    std::unique_ptr<Stmt> parseContinueStatement();
    std::unique_ptr<Stmt> parseReturnStatement();
    std::unique_ptr<Stmt> parseVarDeclStatement();

    std::unique_ptr<Expr> parseExpression();
    std::unique_ptr<Expr> parseLogicalOr();
    std::unique_ptr<Expr> parseLogicalAnd();
    std::unique_ptr<Expr> parseEquality();
    std::unique_ptr<Expr> parseComparison();
    std::unique_ptr<Expr> parseTerm();
    std::unique_ptr<Expr> parseFactor();
    std::unique_ptr<Expr> parseUnary();
    std::unique_ptr<Expr> parseCall();
    std::unique_ptr<Expr> parsePrimary();
};
