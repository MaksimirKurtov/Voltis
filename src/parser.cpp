#include "parser.h"
#include <stdexcept>

Parser::Parser(std::vector<Token> tokens) : tokens_(std::move(tokens)) {}

bool Parser::isAtEnd() const { return peek().type == TokenType::End; }
const Token& Parser::peek() const { return tokens_[current_]; }
const Token& Parser::previous() const { return tokens_[current_ - 1]; }
const Token& Parser::advance() { if (!isAtEnd()) ++current_; return previous(); }
bool Parser::check(TokenType type) const { return !isAtEnd() && peek().type == type; }

bool Parser::match(TokenType type) {
    if (check(type)) { advance(); return true; }
    return false;
}

bool Parser::matchAny(const std::vector<TokenType>& types) {
    for (TokenType type : types) {
        if (check(type)) { advance(); return true; }
    }
    return false;
}

const Token& Parser::consume(TokenType type, const std::string& message) {
    if (check(type)) return advance();
    throw std::runtime_error(message + " at line " + std::to_string(peek().line));
}

void Parser::skipModifiers() {
    while (matchAny({
        TokenType::KeywordPublic,
        TokenType::KeywordPrivate,
        TokenType::KeywordProtected,
        TokenType::KeywordInternal,
        TokenType::KeywordStatic,
        TokenType::KeywordReadonly,
        TokenType::KeywordConst,
        TokenType::KeywordVolatile,
        TokenType::KeywordUnsafe
    })) {}
}

bool Parser::isTypeToken(TokenType type) const {
    switch (type) {
        case TokenType::KeywordInt32:
        case TokenType::KeywordFloat32:
        case TokenType::KeywordFloat64:
        case TokenType::KeywordString:
        case TokenType::KeywordBool:
        case TokenType::KeywordVoid:
            return true;
        default:
            return false;
    }
}

std::string Parser::parseType() {
    if (isTypeToken(peek().type)) {
        return advance().lexeme;
    }
    throw std::runtime_error("Expected type at line " + std::to_string(peek().line));
}

Program Parser::parseProgram() {
    Program program;
    while (!isAtEnd()) {
        skipModifiers();
        if (match(TokenType::KeywordFn)) {
            --current_;
            program.functions.push_back(parseFunction());
        } else {
            throw std::runtime_error("Expected function declaration at line " + std::to_string(peek().line));
        }
    }
    return program;
}

FunctionDecl Parser::parseFunction() {
    skipModifiers();
    consume(TokenType::KeywordFn, "Expected 'fn'");
    std::string name = consume(TokenType::Identifier, "Expected function name").lexeme;
    consume(TokenType::LParen, "Expected '('");

    std::vector<Param> params;
    if (!check(TokenType::RParen)) {
        do {
            std::string type = parseType();
            std::string paramName = consume(TokenType::Identifier, "Expected parameter name").lexeme;
            params.push_back({type, paramName});
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RParen, "Expected ')'");
    consume(TokenType::Arrow, "Expected '->'");
    std::string returnType = parseType();
    auto body = parseBlock();
    return FunctionDecl{name, std::move(params), returnType, std::move(body)};
}

std::unique_ptr<BlockStmt> Parser::parseBlock() {
    consume(TokenType::LBrace, "Expected '{'");
    auto block = std::make_unique<BlockStmt>();
    while (!check(TokenType::RBrace) && !isAtEnd()) {
        block->statements.push_back(parseStatement());
    }
    consume(TokenType::RBrace, "Expected '}'");
    return block;
}

std::unique_ptr<Stmt> Parser::parseStatement() {
    if (check(TokenType::KeywordIf)) return parseIfStatement();
    if (check(TokenType::KeywordReturn)) return parseReturnStatement();

    // variable declaration support
    std::size_t save = current_;
    skipModifiers();
    if (check(TokenType::KeywordVar) || isTypeToken(peek().type)) {
        current_ = save;
        return parseVarDeclStatement();
    }
    current_ = save;

    if (check(TokenType::LBrace)) return parseBlock();

    if (check(TokenType::Identifier) && tokens_[current_ + 1].type == TokenType::Assign) {
        std::string name = advance().lexeme;
        consume(TokenType::Assign, "Expected '='");
        auto value = parseExpression();
        consume(TokenType::Semicolon, "Expected ';'");
        return std::make_unique<AssignStmt>(name, std::move(value));
    }

    auto expr = parseExpression();
    consume(TokenType::Semicolon, "Expected ';'");
    return std::make_unique<ExprStmt>(std::move(expr));
}

std::unique_ptr<Stmt> Parser::parseIfStatement() {
    consume(TokenType::KeywordIf, "Expected 'if'");
    consume(TokenType::LParen, "Expected '('");
    auto condition = parseExpression();
    consume(TokenType::RParen, "Expected ')'");
    auto thenBlock = parseBlock();

    std::unique_ptr<BlockStmt> elseBlock;
    if (match(TokenType::KeywordElse)) {
        if (check(TokenType::KeywordIf)) {
            auto nested = parseIfStatement();
            elseBlock = std::make_unique<BlockStmt>();
            elseBlock->statements.push_back(std::move(nested));
        } else {
            elseBlock = parseBlock();
        }
    }

    auto stmt = std::make_unique<IfStmt>();
    stmt->condition = std::move(condition);
    stmt->thenBlock = std::move(thenBlock);
    stmt->elseBlock = std::move(elseBlock);
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseReturnStatement() {
    consume(TokenType::KeywordReturn, "Expected 'return'");
    auto expr = parseExpression();
    consume(TokenType::Semicolon, "Expected ';'");
    return std::make_unique<ReturnStmt>(std::move(expr));
}

std::unique_ptr<Stmt> Parser::parseVarDeclStatement() {
    skipModifiers();
    auto decl = std::make_unique<VarDeclStmt>();
    if (match(TokenType::KeywordVar)) {
        decl->isVarInference = true;
        decl->type = "var";
    } else {
        decl->type = parseType();
    }
    decl->name = consume(TokenType::Identifier, "Expected variable name").lexeme;
    consume(TokenType::Assign, "Expected '=' in variable declaration");
    decl->init = parseExpression();
    consume(TokenType::Semicolon, "Expected ';'");
    return decl;
}

std::unique_ptr<Expr> Parser::parseExpression() { return parseLogicalOr(); }

std::unique_ptr<Expr> Parser::parseLogicalOr() {
    auto expr = parseLogicalAnd();
    while (match(TokenType::KeywordOr)) {
        auto right = parseLogicalAnd();
        expr = std::make_unique<BinaryExpr>(std::move(expr), "or", std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseLogicalAnd() {
    auto expr = parseEquality();
    while (match(TokenType::KeywordAnd)) {
        auto right = parseEquality();
        expr = std::make_unique<BinaryExpr>(std::move(expr), "and", std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseEquality() {
    auto expr = parseComparison();
    while (matchAny({TokenType::EqualEqual, TokenType::BangEqual})) {
        std::string op = previous().lexeme;
        auto right = parseComparison();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseComparison() {
    auto expr = parseTerm();
    while (matchAny({TokenType::Less, TokenType::LessEqual, TokenType::Greater, TokenType::GreaterEqual})) {
        std::string op = previous().lexeme;
        auto right = parseTerm();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseTerm() {
    auto expr = parseFactor();
    while (matchAny({TokenType::Plus, TokenType::Minus})) {
        std::string op = previous().lexeme;
        auto right = parseFactor();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseFactor() {
    auto expr = parseUnary();
    while (matchAny({TokenType::Star, TokenType::Slash})) {
        std::string op = previous().lexeme;
        auto right = parseUnary();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right));
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseUnary() {
    if (matchAny({TokenType::Minus, TokenType::Bang, TokenType::KeywordNot})) {
        return std::make_unique<UnaryExpr>(previous().lexeme, parseUnary());
    }
    return parseCall();
}

std::unique_ptr<Expr> Parser::parseCall() {
    auto expr = parsePrimary();
    while (true) {
        if (match(TokenType::LParen)) {
            std::vector<std::unique_ptr<Expr>> args;
            if (!check(TokenType::RParen)) {
                do {
                    args.push_back(parseExpression());
                } while (match(TokenType::Comma));
            }
            consume(TokenType::RParen, "Expected ')' after arguments");
            expr = std::make_unique<CallExpr>(std::move(expr), std::move(args));
        } else if (match(TokenType::Dot)) {
            std::string method = consume(TokenType::Identifier, "Expected member name after '.'").lexeme;
            consume(TokenType::LParen, "Expected '(' after member name");
            std::vector<std::unique_ptr<Expr>> args;
            if (!check(TokenType::RParen)) {
                do {
                    args.push_back(parseExpression());
                } while (match(TokenType::Comma));
            }
            consume(TokenType::RParen, "Expected ')' after member call arguments");
            expr = std::make_unique<MemberCallExpr>(std::move(expr), method, std::move(args));
        } else {
            break;
        }
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parsePrimary() {
    if (match(TokenType::Number)) {
        return std::make_unique<LiteralExpr>(LiteralExpr::Kind::Number, previous().lexeme);
    }
    if (match(TokenType::String)) {
        return std::make_unique<LiteralExpr>(LiteralExpr::Kind::String, previous().lexeme);
    }
    if (match(TokenType::KeywordTrue)) {
        return std::make_unique<LiteralExpr>(LiteralExpr::Kind::Bool, "true");
    }
    if (match(TokenType::KeywordFalse)) {
        return std::make_unique<LiteralExpr>(LiteralExpr::Kind::Bool, "false");
    }
    if (match(TokenType::Identifier)) {
        return std::make_unique<VariableExpr>(previous().lexeme);
    }
    if (match(TokenType::LParen)) {
        auto expr = parseExpression();
        consume(TokenType::RParen, "Expected ')' after expression");
        return expr;
    }

    throw std::runtime_error("Expected expression at line " + std::to_string(peek().line));
}
