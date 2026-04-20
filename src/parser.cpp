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
        if (check(TokenType::KeywordImport)) {
            program.imports.push_back(parseImportDecl());
            continue;
        }

        const std::size_t declarationStart = current_;
        skipModifiers();
        if (check(TokenType::KeywordExtern)) {
            current_ = declarationStart;
            program.externFunctions.push_back(parseExternFunction());
            continue;
        }
        if (check(TokenType::KeywordFn)) {
            current_ = declarationStart;
            program.functions.push_back(parseFunction());
            continue;
        }

        throw std::runtime_error("Expected top-level declaration at line " + std::to_string(peek().line));
    }
    return program;
}

std::string Parser::parseImportPath() {
    if (match(TokenType::String)) {
        return previous().lexeme;
    }

    if (match(TokenType::Less)) {
        std::string path;
        while (!check(TokenType::Greater) && !isAtEnd()) {
            path += advance().lexeme;
        }
        consume(TokenType::Greater, "Expected '>' to end import path");
        if (path.empty()) {
            throw std::runtime_error("Import path cannot be empty at line " + std::to_string(previous().line));
        }
        return path;
    }

    throw std::runtime_error("Expected import path (string literal or <path>) at line " + std::to_string(peek().line));
}

ImportDecl Parser::parseImportDecl() {
    const Token& importToken = consume(TokenType::KeywordImport, "Expected 'import'");
    const std::string path = parseImportPath();
    consume(TokenType::Semicolon, "Expected ';' after import declaration");
    return ImportDecl{path, SourceLocation{importToken.line, importToken.column}};
}

FunctionDecl Parser::parseFunction() {
    skipModifiers();
    consume(TokenType::KeywordFn, "Expected 'fn'");
    const Token& nameToken = consume(TokenType::Identifier, "Expected function name");
    consume(TokenType::LParen, "Expected '('");

    std::vector<Param> params;
    if (!check(TokenType::RParen)) {
        do {
            std::string type = parseType();
            const Token& paramNameToken = consume(TokenType::Identifier, "Expected parameter name");
            params.push_back({type, paramNameToken.lexeme, SourceLocation{paramNameToken.line, paramNameToken.column}});
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RParen, "Expected ')'");
    consume(TokenType::Arrow, "Expected '->'");
    std::string returnType = parseType();
    auto body = parseBlock();
    return FunctionDecl{nameToken.lexeme, std::move(params), returnType, std::move(body), SourceLocation{nameToken.line, nameToken.column}};
}

ExternFunctionDecl Parser::parseExternFunction() {
    skipModifiers();
    consume(TokenType::KeywordExtern, "Expected 'extern'");
    consume(TokenType::KeywordFn, "Expected 'fn' after 'extern'");
    const Token& nameToken = consume(TokenType::Identifier, "Expected function name");
    consume(TokenType::LParen, "Expected '('");

    std::vector<Param> params;
    if (!check(TokenType::RParen)) {
        do {
            std::string type = parseType();
            const Token& paramNameToken = consume(TokenType::Identifier, "Expected parameter name");
            params.push_back({type, paramNameToken.lexeme, SourceLocation{paramNameToken.line, paramNameToken.column}});
        } while (match(TokenType::Comma));
    }
    consume(TokenType::RParen, "Expected ')'");
    consume(TokenType::Arrow, "Expected '->'");
    const std::string returnType = parseType();
    consume(TokenType::KeywordFrom, "Expected 'from' in extern declaration");
    const std::string importPath = parseImportPath();
    consume(TokenType::Semicolon, "Expected ';' after extern declaration");

    return ExternFunctionDecl{
        nameToken.lexeme,
        std::move(params),
        returnType,
        importPath,
        SourceLocation{nameToken.line, nameToken.column},
    };
}

std::unique_ptr<BlockStmt> Parser::parseBlock() {
    const Token& braceToken = consume(TokenType::LBrace, "Expected '{'");
    auto block = std::make_unique<BlockStmt>(SourceLocation{braceToken.line, braceToken.column});
    while (!check(TokenType::RBrace) && !isAtEnd()) {
        block->statements.push_back(parseStatement());
    }
    consume(TokenType::RBrace, "Expected '}'");
    return block;
}

std::unique_ptr<Stmt> Parser::parseStatement() {
    if (check(TokenType::KeywordIf)) return parseIfStatement();
    if (check(TokenType::KeywordWhile)) return parseWhileStatement();
    if (check(TokenType::KeywordBreak)) return parseBreakStatement();
    if (check(TokenType::KeywordContinue)) return parseContinueStatement();
    if (check(TokenType::KeywordReturn)) return parseReturnStatement();

    std::size_t save = current_;
    skipModifiers();
    if (check(TokenType::KeywordVar) || isTypeToken(peek().type)) {
        current_ = save;
        return parseVarDeclStatement();
    }
    current_ = save;

    if (check(TokenType::LBrace)) return parseBlock();

    if (check(TokenType::Identifier) && tokens_[current_ + 1].type == TokenType::Assign) {
        const Token& nameToken = advance();
        consume(TokenType::Assign, "Expected '='");
        auto value = parseExpression();
        consume(TokenType::Semicolon, "Expected ';'");
        return std::make_unique<AssignStmt>(nameToken.lexeme, std::move(value), SourceLocation{nameToken.line, nameToken.column});
    }

    auto expr = parseExpression();
    SourceLocation location = expr->location;
    consume(TokenType::Semicolon, "Expected ';'");
    return std::make_unique<ExprStmt>(std::move(expr), location);
}

std::unique_ptr<Stmt> Parser::parseIfStatement() {
    const Token& ifToken = consume(TokenType::KeywordIf, "Expected 'if'");
    consume(TokenType::LParen, "Expected '('");
    auto condition = parseExpression();
    consume(TokenType::RParen, "Expected ')'");
    auto thenBlock = parseBlock();

    std::unique_ptr<BlockStmt> elseBlock;
    if (match(TokenType::KeywordElse)) {
        if (check(TokenType::KeywordIf)) {
            auto nested = parseIfStatement();
            elseBlock = std::make_unique<BlockStmt>(SourceLocation{ifToken.line, ifToken.column});
            elseBlock->statements.push_back(std::move(nested));
        } else {
            elseBlock = parseBlock();
        }
    }

    auto stmt = std::make_unique<IfStmt>(SourceLocation{ifToken.line, ifToken.column});
    stmt->condition = std::move(condition);
    stmt->thenBlock = std::move(thenBlock);
    stmt->elseBlock = std::move(elseBlock);
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseWhileStatement() {
    const Token& whileToken = consume(TokenType::KeywordWhile, "Expected 'while'");
    consume(TokenType::LParen, "Expected '('");
    auto condition = parseExpression();
    consume(TokenType::RParen, "Expected ')'");
    auto body = parseBlock();

    auto stmt = std::make_unique<WhileStmt>(SourceLocation{whileToken.line, whileToken.column});
    stmt->condition = std::move(condition);
    stmt->body = std::move(body);
    return stmt;
}

std::unique_ptr<Stmt> Parser::parseBreakStatement() {
    const Token& breakToken = consume(TokenType::KeywordBreak, "Expected 'break'");
    consume(TokenType::Semicolon, "Expected ';'");
    return std::make_unique<BreakStmt>(SourceLocation{breakToken.line, breakToken.column});
}

std::unique_ptr<Stmt> Parser::parseContinueStatement() {
    const Token& continueToken = consume(TokenType::KeywordContinue, "Expected 'continue'");
    consume(TokenType::Semicolon, "Expected ';'");
    return std::make_unique<ContinueStmt>(SourceLocation{continueToken.line, continueToken.column});
}

std::unique_ptr<Stmt> Parser::parseReturnStatement() {
    const Token& returnToken = consume(TokenType::KeywordReturn, "Expected 'return'");
    std::unique_ptr<Expr> expr;
    if (!check(TokenType::Semicolon)) {
        expr = parseExpression();
    }
    consume(TokenType::Semicolon, "Expected ';'");
    return std::make_unique<ReturnStmt>(std::move(expr), SourceLocation{returnToken.line, returnToken.column});
}

std::unique_ptr<Stmt> Parser::parseVarDeclStatement() {
    skipModifiers();
    SourceLocation location{peek().line, peek().column};
    auto decl = std::make_unique<VarDeclStmt>(location);
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
        const Token& opToken = previous();
        auto right = parseLogicalAnd();
        expr = std::make_unique<BinaryExpr>(std::move(expr), "or", std::move(right), SourceLocation{opToken.line, opToken.column});
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseLogicalAnd() {
    auto expr = parseEquality();
    while (match(TokenType::KeywordAnd)) {
        const Token& opToken = previous();
        auto right = parseEquality();
        expr = std::make_unique<BinaryExpr>(std::move(expr), "and", std::move(right), SourceLocation{opToken.line, opToken.column});
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseEquality() {
    auto expr = parseComparison();
    while (matchAny({TokenType::EqualEqual, TokenType::BangEqual})) {
        const Token& opToken = previous();
        std::string op = opToken.lexeme;
        auto right = parseComparison();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), SourceLocation{opToken.line, opToken.column});
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseComparison() {
    auto expr = parseTerm();
    while (matchAny({TokenType::Less, TokenType::LessEqual, TokenType::Greater, TokenType::GreaterEqual})) {
        const Token& opToken = previous();
        std::string op = opToken.lexeme;
        auto right = parseTerm();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), SourceLocation{opToken.line, opToken.column});
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseTerm() {
    auto expr = parseFactor();
    while (matchAny({TokenType::Plus, TokenType::Minus})) {
        const Token& opToken = previous();
        std::string op = opToken.lexeme;
        auto right = parseFactor();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), SourceLocation{opToken.line, opToken.column});
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseFactor() {
    auto expr = parseUnary();
    while (matchAny({TokenType::Star, TokenType::Slash})) {
        const Token& opToken = previous();
        std::string op = opToken.lexeme;
        auto right = parseUnary();
        expr = std::make_unique<BinaryExpr>(std::move(expr), op, std::move(right), SourceLocation{opToken.line, opToken.column});
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parseUnary() {
    if (matchAny({TokenType::Minus, TokenType::Bang, TokenType::KeywordNot})) {
        const Token& opToken = previous();
        return std::make_unique<UnaryExpr>(opToken.lexeme, parseUnary(), SourceLocation{opToken.line, opToken.column});
    }
    return parseCall();
}

std::unique_ptr<Expr> Parser::parseCall() {
    auto expr = parsePrimary();
    while (true) {
        if (match(TokenType::LParen)) {
            const Token& lParenToken = previous();
            std::vector<std::unique_ptr<Expr>> args;
            if (!check(TokenType::RParen)) {
                do {
                    args.push_back(parseExpression());
                } while (match(TokenType::Comma));
            }
            consume(TokenType::RParen, "Expected ')' after arguments");
            expr = std::make_unique<CallExpr>(std::move(expr), std::move(args), SourceLocation{lParenToken.line, lParenToken.column});
        } else if (match(TokenType::Dot)) {
            const Token& methodToken = consume(TokenType::Identifier, "Expected member name after '.'");
            consume(TokenType::LParen, "Expected '(' after member name");
            std::vector<std::unique_ptr<Expr>> args;
            if (!check(TokenType::RParen)) {
                do {
                    args.push_back(parseExpression());
                } while (match(TokenType::Comma));
            }
            consume(TokenType::RParen, "Expected ')' after member call arguments");
            expr = std::make_unique<MemberCallExpr>(std::move(expr), methodToken.lexeme, std::move(args), SourceLocation{methodToken.line, methodToken.column});
        } else {
            break;
        }
    }
    return expr;
}

std::unique_ptr<Expr> Parser::parsePrimary() {
    if (match(TokenType::Number)) {
        const Token& token = previous();
        return std::make_unique<LiteralExpr>(LiteralExpr::Kind::Number, token.lexeme, SourceLocation{token.line, token.column});
    }
    if (match(TokenType::String)) {
        const Token& token = previous();
        return std::make_unique<LiteralExpr>(LiteralExpr::Kind::String, token.lexeme, SourceLocation{token.line, token.column});
    }
    if (match(TokenType::KeywordTrue)) {
        const Token& token = previous();
        return std::make_unique<LiteralExpr>(LiteralExpr::Kind::Bool, "true", SourceLocation{token.line, token.column});
    }
    if (match(TokenType::KeywordFalse)) {
        const Token& token = previous();
        return std::make_unique<LiteralExpr>(LiteralExpr::Kind::Bool, "false", SourceLocation{token.line, token.column});
    }
    if (match(TokenType::Identifier)) {
        const Token& token = previous();
        return std::make_unique<VariableExpr>(token.lexeme, SourceLocation{token.line, token.column});
    }
    if (match(TokenType::LParen)) {
        auto expr = parseExpression();
        consume(TokenType::RParen, "Expected ')' after expression");
        return expr;
    }

    throw std::runtime_error("Expected expression at line " + std::to_string(peek().line));
}
