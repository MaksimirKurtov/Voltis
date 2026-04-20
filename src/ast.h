#pragma once

#include "source_location.h"
#include <memory>
#include <string>
#include <utility>
#include <vector>

struct Expr {
    SourceLocation location;
    explicit Expr(SourceLocation location = {}) : location(location) {}
    virtual ~Expr() = default;
};

struct LiteralExpr : Expr {
    enum class Kind { Number, String, Bool } kind;
    std::string value;
    LiteralExpr(Kind kind, std::string value, SourceLocation location = {})
        : Expr(location), kind(kind), value(std::move(value)) {}
};

struct VariableExpr : Expr {
    std::string name;
    explicit VariableExpr(std::string name, SourceLocation location = {})
        : Expr(location), name(std::move(name)) {}
};

struct UnaryExpr : Expr {
    std::string op;
    std::unique_ptr<Expr> right;
    UnaryExpr(std::string op, std::unique_ptr<Expr> right, SourceLocation location = {})
        : Expr(location), op(std::move(op)), right(std::move(right)) {}
};

struct BinaryExpr : Expr {
    std::unique_ptr<Expr> left;
    std::string op;
    std::unique_ptr<Expr> right;
    BinaryExpr(std::unique_ptr<Expr> left, std::string op, std::unique_ptr<Expr> right, SourceLocation location = {})
        : Expr(location), left(std::move(left)), op(std::move(op)), right(std::move(right)) {}
};

struct CallExpr : Expr {
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> args;
    CallExpr(std::unique_ptr<Expr> callee, std::vector<std::unique_ptr<Expr>> args, SourceLocation location = {})
        : Expr(location), callee(std::move(callee)), args(std::move(args)) {}
};

struct MemberCallExpr : Expr {
    std::unique_ptr<Expr> object;
    std::string method;
    std::vector<std::unique_ptr<Expr>> args;
    MemberCallExpr(std::unique_ptr<Expr> object, std::string method, std::vector<std::unique_ptr<Expr>> args, SourceLocation location = {})
        : Expr(location), object(std::move(object)), method(std::move(method)), args(std::move(args)) {}
};

struct Stmt {
    SourceLocation location;
    explicit Stmt(SourceLocation location = {}) : location(location) {}
    virtual ~Stmt() = default;
};

struct ExprStmt : Stmt {
    std::unique_ptr<Expr> expr;
    explicit ExprStmt(std::unique_ptr<Expr> expr, SourceLocation location = {})
        : Stmt(location), expr(std::move(expr)) {}
};

struct ReturnStmt : Stmt {
    std::unique_ptr<Expr> expr;
    explicit ReturnStmt(std::unique_ptr<Expr> expr = nullptr, SourceLocation location = {})
        : Stmt(location), expr(std::move(expr)) {}
};

struct VarDeclStmt : Stmt {
    explicit VarDeclStmt(SourceLocation location = {}) : Stmt(location) {}
    std::string type;
    std::string name;
    std::unique_ptr<Expr> init;
    bool isVarInference = false;
};

struct AssignStmt : Stmt {
    std::string name;
    std::unique_ptr<Expr> value;
    AssignStmt(std::string name, std::unique_ptr<Expr> value, SourceLocation location = {})
        : Stmt(location), name(std::move(name)), value(std::move(value)) {}
};

struct BlockStmt : Stmt {
    explicit BlockStmt(SourceLocation location = {}) : Stmt(location) {}
    std::vector<std::unique_ptr<Stmt>> statements;
};

struct IfStmt : Stmt {
    explicit IfStmt(SourceLocation location = {}) : Stmt(location) {}
    std::unique_ptr<Expr> condition;
    std::unique_ptr<BlockStmt> thenBlock;
    std::unique_ptr<BlockStmt> elseBlock;
};

struct WhileStmt : Stmt {
    explicit WhileStmt(SourceLocation location = {}) : Stmt(location) {}
    std::unique_ptr<Expr> condition;
    std::unique_ptr<BlockStmt> body;
};

struct BreakStmt : Stmt {
    explicit BreakStmt(SourceLocation location = {}) : Stmt(location) {}
};

struct ContinueStmt : Stmt {
    explicit ContinueStmt(SourceLocation location = {}) : Stmt(location) {}
};

struct Param {
    std::string type;
    std::string name;
    SourceLocation location;
};

struct FunctionDecl {
    std::string name;
    std::vector<Param> params;
    std::string returnType;
    std::unique_ptr<BlockStmt> body;
    SourceLocation location;
};

struct ImportDecl {
    std::string path;
    SourceLocation location;
};

struct ExternFunctionDecl {
    std::string name;
    std::vector<Param> params;
    std::string returnType;
    std::string importPath;
    SourceLocation location;
};

struct Program {
    std::vector<ImportDecl> imports;
    std::vector<ExternFunctionDecl> externFunctions;
    std::vector<FunctionDecl> functions;
};
