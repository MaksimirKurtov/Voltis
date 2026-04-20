#pragma once

#include "ast.h"
#include "diagnostics.h"
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <vector>

class SemanticAnalyzer {
public:
    enum class ConversionKind {
        None,
        ToString,
        ToInt32Identity,
        ToInt32FromFloatTruncateTowardZero,
        ToInt32FromStringParse,
        ToInt32FromBool,
        ToFloat32,
        ToFloat64,
        ToBool,
        Round,
        Floor,
        Ceil
    };

    struct ConversionInfo {
        std::string method;
        std::string sourceType;
        std::string resultType;
        ConversionKind kind = ConversionKind::None;
    };

    bool analyze(Program& program);
    const DiagnosticBag& diagnostics() const;
    const std::unordered_map<const Expr*, std::string>& expressionTypes() const;
    const std::unordered_map<const Expr*, ConversionInfo>& conversionInfos() const;

private:
    struct FunctionSymbol {
        std::vector<std::string> paramTypes;
        std::string returnType;
        bool isExtern = false;
        std::string importPath;
        SourceLocation location;
    };

    struct VariableSymbol {
        std::string type;
        SourceLocation location;
    };

    DiagnosticBag diagnostics_;
    std::unordered_map<std::string, FunctionSymbol> functions_;
    std::unordered_set<std::string> importedPaths_;
    std::vector<std::unordered_map<std::string, VariableSymbol>> scopes_;
    std::unordered_map<const Expr*, std::string> expressionTypes_;
    std::unordered_map<const Expr*, ConversionInfo> conversionInfos_;
    std::string currentReturnType_;
    int loopDepth_ = 0;

    void registerImports(const Program& program);
    void registerFunctions(const Program& program);
    void analyzeFunction(FunctionDecl& function);
    void analyzeBlock(BlockStmt& block, bool createScope);
    void analyzeStatement(Stmt* statement);
    std::string analyzeExpr(Expr* expr);
    std::string analyzeCallExpr(CallExpr* callExpr);
    std::string analyzeMemberCallExpr(MemberCallExpr* memberCallExpr);
    std::string analyzeConversionMemberCall(MemberCallExpr* memberCallExpr, const std::string& objectType, const std::string& method);

    void pushScope();
    void popScope();
    bool declareVariable(const std::string& name, const std::string& type, const SourceLocation& location);
    const VariableSymbol* lookupVariable(const std::string& name) const;

    bool isKnownType(const std::string& type) const;
    bool isNumericType(const std::string& type) const;
    bool isFloatType(const std::string& type) const;
    bool isAssignable(const std::string& target, const std::string& source) const;
    std::string commonNumericType(const std::string& left, const std::string& right) const;
    bool isPrimitiveType(const std::string& type) const;
    bool isNumericStringForInt32(const std::string& value) const;
    bool isNumericStringForFloat(const std::string& value) const;
    bool isBoolString(const std::string& value) const;
    std::string allowedReceiverTypes(const std::string& method) const;
    std::string setConversionInfo(const Expr* expr, const std::string& resultType, const std::string& sourceType, const std::string& method, ConversionKind kind);
    std::string makeTypeErrorType(Expr* expr, const std::string& message);
    std::string setExprType(const Expr* expr, const std::string& type);
    bool isErrorType(const std::string& type) const;
    bool blockAlwaysReturns(const BlockStmt& block) const;
    bool statementAlwaysReturns(const Stmt* statement) const;
};
