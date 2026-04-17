#pragma once

#include <memory>
#include <string>
#include <utility>
#include <vector>

struct Expr {
    virtual ~Expr() = default;
};

struct LiteralExpr : Expr {
    enum class Kind { Number, String, Bool } kind;
    std::string value;
    LiteralExpr(Kind kind, std::string value) : kind(kind), value(std::move(value)) {}
};

struct VariableExpr : Expr {
    std::string name;
    explicit VariableExpr(std::string name) : name(std::move(name)) {}
};

struct UnaryExpr : Expr {
    std::string op;
    std::unique_ptr<Expr> right;
    UnaryExpr(std::string op, std::unique_ptr<Expr> right) : op(std::move(op)), right(std::move(right)) {}
};

struct BinaryExpr : Expr {
    std::unique_ptr<Expr> left;
    std::string op;
    std::unique_ptr<Expr> right;
    BinaryExpr(std::unique_ptr<Expr> left, std::string op, std::unique_ptr<Expr> right)
        : left(std::move(left)), op(std::move(op)), right(std::move(right)) {}
};

struct CallExpr : Expr {
    std::unique_ptr<Expr> callee;
    std::vector<std::unique_ptr<Expr>> args;
    CallExpr(std::unique_ptr<Expr> callee, std::vector<std::unique_ptr<Expr>> args)
        : callee(std::move(callee)), args(std::move(args)) {}
};

struct MemberCallExpr : Expr {
    std::unique_ptr<Expr> object;
    std::string method;
    std::vector<std::unique_ptr<Expr>> args;
    MemberCallExpr(std::unique_ptr<Expr> object, std::string method, std::vector<std::unique_ptr<Expr>> args)
        : object(std::move(object)), method(std::move(method)), args(std::move(args)) {}
};

struct Stmt {
    virtual ~Stmt() = default;
};

struct ExprStmt : Stmt {
    std::unique_ptr<Expr> expr;
    explicit ExprStmt(std::unique_ptr<Expr> expr) : expr(std::move(expr)) {}
};

struct ReturnStmt : Stmt {
    std::unique_ptr<Expr> expr;
    explicit ReturnStmt(std::unique_ptr<Expr> expr) : expr(std::move(expr)) {}
};

struct VarDeclStmt : Stmt {
    std::string type;
    std::string name;
    std::unique_ptr<Expr> init;
    bool isVarInference = false;
};

struct AssignStmt : Stmt {
    std::string name;
    std::unique_ptr<Expr> value;
    AssignStmt(std::string name, std::unique_ptr<Expr> value)
        : name(std::move(name)), value(std::move(value)) {}
};

struct BlockStmt : Stmt {
    std::vector<std::unique_ptr<Stmt>> statements;
};

struct IfStmt : Stmt {
    std::unique_ptr<Expr> condition;
    std::unique_ptr<BlockStmt> thenBlock;
    std::unique_ptr<BlockStmt> elseBlock;
};

struct Param {
    std::string type;
    std::string name;
};

struct FunctionDecl {
    std::string name;
    std::vector<Param> params;
    std::string returnType;
    std::unique_ptr<BlockStmt> body;
};

struct Program {
    std::vector<FunctionDecl> functions;
};
