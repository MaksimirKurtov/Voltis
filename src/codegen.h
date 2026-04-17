#pragma once

#include "ast.h"
#include <string>

// Temporary bootstrap backend that transpiles AST to C++.
// Production-oriented backend flow is VIR -> backend/* modules.
class CodeGenerator {
public:
    std::string generate(const Program& program);

private:
    std::string indent(int level) const;
    std::string mapType(const std::string& type) const;
    std::string genExpr(const Expr* expr);
    std::string genStmt(const Stmt* stmt, int level);
    std::string escapeString(const std::string& value) const;
};
