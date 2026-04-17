#include "sema.h"
#include <algorithm>
#include <cerrno>
#include <cmath>
#include <cctype>
#include <cstdint>
#include <cstdlib>
#include <limits>
#include <utility>

namespace {
constexpr const char* kErrorType = "<error>";
}

bool SemanticAnalyzer::analyze(Program& program) {
    diagnostics_ = DiagnosticBag{};
    functions_.clear();
    scopes_.clear();
    expressionTypes_.clear();
    conversionInfos_.clear();
    currentReturnType_ = "void";

    registerFunctions(program);
    for (auto& function : program.functions) {
        analyzeFunction(function);
    }

    return !diagnostics_.hasErrors();
}

const DiagnosticBag& SemanticAnalyzer::diagnostics() const {
    return diagnostics_;
}

const std::unordered_map<const Expr*, std::string>& SemanticAnalyzer::expressionTypes() const {
    return expressionTypes_;
}

const std::unordered_map<const Expr*, SemanticAnalyzer::ConversionInfo>& SemanticAnalyzer::conversionInfos() const {
    return conversionInfos_;
}

void SemanticAnalyzer::registerFunctions(const Program& program) {
    for (const auto& function : program.functions) {
        if (functions_.find(function.name) != functions_.end()) {
            diagnostics_.error(function.location, "duplicate function '" + function.name + "' in global scope");
            continue;
        }

        if (!isKnownType(function.returnType)) {
            diagnostics_.error(function.location, "unknown return type '" + function.returnType + "' for function '" + function.name + "'");
        }

        std::vector<std::string> paramTypes;
        paramTypes.reserve(function.params.size());
        for (const auto& param : function.params) {
            if (!isKnownType(param.type)) {
                diagnostics_.error(param.location, "unknown parameter type '" + param.type + "' for '" + param.name + "'");
            }
            paramTypes.push_back(param.type);
        }

        functions_.emplace(function.name, FunctionSymbol{std::move(paramTypes), function.returnType, function.location});
    }
}

void SemanticAnalyzer::analyzeFunction(FunctionDecl& function) {
    currentReturnType_ = function.returnType;
    pushScope();

    for (const auto& param : function.params) {
        declareVariable(param.name, param.type, param.location);
    }

    analyzeBlock(*function.body, false);
    popScope();
}

void SemanticAnalyzer::analyzeBlock(BlockStmt& block, bool createScope) {
    if (createScope) {
        pushScope();
    }

    for (auto& statement : block.statements) {
        analyzeStatement(statement.get());
    }

    if (createScope) {
        popScope();
    }
}

void SemanticAnalyzer::analyzeStatement(Stmt* statement) {
    if (auto* exprStmt = dynamic_cast<ExprStmt*>(statement)) {
        analyzeExpr(exprStmt->expr.get());
        return;
    }

    if (auto* returnStmt = dynamic_cast<ReturnStmt*>(statement)) {
        const std::string exprType = analyzeExpr(returnStmt->expr.get());
        if (currentReturnType_ == "void") {
            diagnostics_.error(returnStmt->location, "void function cannot return a value");
            return;
        }
        if (!isAssignable(currentReturnType_, exprType)) {
            diagnostics_.error(returnStmt->location, "return type mismatch: expected '" + currentReturnType_ + "', got '" + exprType + "'");
        }
        return;
    }

    if (auto* varDecl = dynamic_cast<VarDeclStmt*>(statement)) {
        std::string initType = analyzeExpr(varDecl->init.get());
        std::string declaredType = varDecl->type;

        if (varDecl->isVarInference) {
            if (isErrorType(initType)) {
                declaredType = kErrorType;
            } else if (initType == "void") {
                diagnostics_.error(varDecl->location, "cannot infer type for '" + varDecl->name + "' from void expression");
                declaredType = kErrorType;
            } else {
                declaredType = initType;
            }
            varDecl->type = declaredType;
            varDecl->isVarInference = false;
        } else if (!isKnownType(declaredType)) {
            diagnostics_.error(varDecl->location, "unknown type '" + declaredType + "' for local '" + varDecl->name + "'");
            declaredType = kErrorType;
        }

        if (!isErrorType(declaredType) && !isAssignable(declaredType, initType)) {
            diagnostics_.error(varDecl->location, "cannot assign value of type '" + initType + "' to variable '" + varDecl->name + "' of type '" + declaredType + "'");
        }

        declareVariable(varDecl->name, declaredType, varDecl->location);
        return;
    }

    if (auto* assign = dynamic_cast<AssignStmt*>(statement)) {
        const VariableSymbol* symbol = lookupVariable(assign->name);
        const std::string valueType = analyzeExpr(assign->value.get());
        if (!symbol) {
            diagnostics_.error(assign->location, "undefined symbol '" + assign->name + "'");
            return;
        }

        if (!isAssignable(symbol->type, valueType)) {
            diagnostics_.error(assign->location, "cannot assign value of type '" + valueType + "' to '" + assign->name + "' of type '" + symbol->type + "'");
        }
        return;
    }

    if (auto* block = dynamic_cast<BlockStmt*>(statement)) {
        analyzeBlock(*block, true);
        return;
    }

    if (auto* ifStmt = dynamic_cast<IfStmt*>(statement)) {
        const std::string conditionType = analyzeExpr(ifStmt->condition.get());
        if (!isErrorType(conditionType) && conditionType != "bool") {
            diagnostics_.error(ifStmt->condition->location, "if condition must be 'bool', got '" + conditionType + "'");
        }

        analyzeBlock(*ifStmt->thenBlock, true);
        if (ifStmt->elseBlock) {
            analyzeBlock(*ifStmt->elseBlock, true);
        }
        return;
    }
}

std::string SemanticAnalyzer::analyzeExpr(Expr* expr) {
    if (auto* lit = dynamic_cast<LiteralExpr*>(expr)) {
        if (lit->kind == LiteralExpr::Kind::String) return setExprType(expr, "string");
        if (lit->kind == LiteralExpr::Kind::Bool) return setExprType(expr, "bool");
        if (!lit->value.empty() && lit->value.back() == 'f') return setExprType(expr, "float32");
        if (lit->value.find('.') != std::string::npos) return setExprType(expr, "float64");
        return setExprType(expr, "int32");
    }

    if (auto* var = dynamic_cast<VariableExpr*>(expr)) {
        if (const VariableSymbol* symbol = lookupVariable(var->name)) {
            return setExprType(expr, symbol->type);
        }
        if (functions_.find(var->name) != functions_.end()) {
            return makeTypeErrorType(expr, "function '" + var->name + "' cannot be used as a value");
        }
        return makeTypeErrorType(expr, "undefined symbol '" + var->name + "'");
    }

    if (auto* unary = dynamic_cast<UnaryExpr*>(expr)) {
        const std::string rightType = analyzeExpr(unary->right.get());
        if (isErrorType(rightType)) {
            return setExprType(expr, kErrorType);
        }
        if (unary->op == "-" ) {
            if (!isNumericType(rightType)) {
                return makeTypeErrorType(expr, "unary '-' requires numeric operand, got '" + rightType + "'");
            }
            return setExprType(expr, rightType);
        }
        if (unary->op == "!" || unary->op == "not") {
            if (!isErrorType(rightType) && rightType != "bool") {
                return makeTypeErrorType(expr, "logical negation requires 'bool', got '" + rightType + "'");
            }
            return setExprType(expr, "bool");
        }
        return makeTypeErrorType(expr, "unsupported unary operator '" + unary->op + "'");
    }

    if (auto* bin = dynamic_cast<BinaryExpr*>(expr)) {
        const std::string leftType = analyzeExpr(bin->left.get());
        const std::string rightType = analyzeExpr(bin->right.get());
        if (isErrorType(leftType) || isErrorType(rightType)) {
            return setExprType(expr, kErrorType);
        }

        if (bin->op == "and" || bin->op == "or") {
            if ((!isErrorType(leftType) && leftType != "bool") || (!isErrorType(rightType) && rightType != "bool")) {
                return makeTypeErrorType(expr, "logical operator '" + bin->op + "' requires bool operands");
            }
            return setExprType(expr, "bool");
        }

        if (bin->op == "+" || bin->op == "-" || bin->op == "*" || bin->op == "/") {
            if (bin->op == "+" && leftType == "string" && rightType == "string") {
                return setExprType(expr, "string");
            }

            if (!isNumericType(leftType) || !isNumericType(rightType)) {
                return makeTypeErrorType(expr, "operator '" + bin->op + "' requires numeric operands");
            }
            return setExprType(expr, commonNumericType(leftType, rightType));
        }

        if (bin->op == "<" || bin->op == "<=" || bin->op == ">" || bin->op == ">=") {
            if (!isNumericType(leftType) || !isNumericType(rightType)) {
                return makeTypeErrorType(expr, "comparison operator '" + bin->op + "' requires numeric operands");
            }
            return setExprType(expr, "bool");
        }

        if (bin->op == "==" || bin->op == "!=") {
            const bool comparable =
                (leftType == rightType) ||
                (isNumericType(leftType) && isNumericType(rightType));
            if (!comparable && !isErrorType(leftType) && !isErrorType(rightType)) {
                return makeTypeErrorType(expr, "cannot compare '" + leftType + "' and '" + rightType + "'");
            }
            return setExprType(expr, "bool");
        }

        return makeTypeErrorType(expr, "unsupported binary operator '" + bin->op + "'");
    }

    if (auto* callExpr = dynamic_cast<CallExpr*>(expr)) {
        return analyzeCallExpr(callExpr);
    }

    if (auto* memberCallExpr = dynamic_cast<MemberCallExpr*>(expr)) {
        return analyzeMemberCallExpr(memberCallExpr);
    }

    return makeTypeErrorType(expr, "unsupported expression");
}

std::string SemanticAnalyzer::analyzeCallExpr(CallExpr* callExpr) {
    std::vector<std::string> argTypes;
    argTypes.reserve(callExpr->args.size());
    for (auto& arg : callExpr->args) {
        argTypes.push_back(analyzeExpr(arg.get()));
    }

    auto* calleeVar = dynamic_cast<VariableExpr*>(callExpr->callee.get());
    if (!calleeVar) {
        analyzeExpr(callExpr->callee.get());
        return makeTypeErrorType(callExpr, "only direct function calls are supported in this subset");
    }

    const std::string& name = calleeVar->name;
    if (name == "print") {
        if (argTypes.size() != 1) {
            diagnostics_.error(callExpr->location, "print expects exactly 1 argument, got " + std::to_string(argTypes.size()));
        } else if (argTypes[0] == "void") {
            diagnostics_.error(callExpr->location, "print argument cannot be void");
        }
        return setExprType(callExpr, "void");
    }

    auto fnIt = functions_.find(name);
    if (fnIt == functions_.end()) {
        if (lookupVariable(name)) {
            return makeTypeErrorType(callExpr, "'" + name + "' is not callable");
        }
        return makeTypeErrorType(callExpr, "undefined function '" + name + "'");
    }

    const FunctionSymbol& fn = fnIt->second;
    if (argTypes.size() != fn.paramTypes.size()) {
        diagnostics_.error(callExpr->location, "function '" + name + "' expects " + std::to_string(fn.paramTypes.size()) +
            " argument(s), got " + std::to_string(argTypes.size()));
    }

    const std::size_t sharedCount = std::min(argTypes.size(), fn.paramTypes.size());
    for (std::size_t i = 0; i < sharedCount; ++i) {
        if (!isAssignable(fn.paramTypes[i], argTypes[i])) {
            diagnostics_.error(callExpr->args[i]->location, "argument " + std::to_string(i + 1) + " of '" + name +
                "' expects '" + fn.paramTypes[i] + "', got '" + argTypes[i] + "'");
        }
    }

    return setExprType(callExpr, fn.returnType);
}

std::string SemanticAnalyzer::analyzeMemberCallExpr(MemberCallExpr* memberCallExpr) {
    const std::string objectType = analyzeExpr(memberCallExpr->object.get());
    for (auto& arg : memberCallExpr->args) {
        analyzeExpr(arg.get());
    }
    if (isErrorType(objectType)) {
        return setExprType(memberCallExpr, kErrorType);
    }

    if (!memberCallExpr->args.empty()) {
        diagnostics_.error(memberCallExpr->location, "conversion member '" + memberCallExpr->method + "' does not take arguments");
        return setExprType(memberCallExpr, kErrorType);
    }

    const std::string& method = memberCallExpr->method;
    return analyzeConversionMemberCall(memberCallExpr, objectType, method);
}

std::string SemanticAnalyzer::analyzeConversionMemberCall(MemberCallExpr* memberCallExpr, const std::string& objectType, const std::string& method) {
    auto invalidReceiver = [&]() {
        return makeTypeErrorType(memberCallExpr,
            "conversion member '" + method + "()' is not valid for receiver type '" + objectType +
            "'. Allowed receiver types: " + allowedReceiverTypes(method));
    };

    if (method == "ToString") {
        if (!isPrimitiveType(objectType)) {
            return invalidReceiver();
        }
        return setConversionInfo(memberCallExpr, "string", objectType, method, ConversionKind::ToString);
    }

    if (method == "ToInt32") {
        if (!(objectType == "int32" || objectType == "float32" || objectType == "float64" ||
              objectType == "string" || objectType == "bool")) {
            return invalidReceiver();
        }

        if (objectType == "string") {
            if (auto* literal = dynamic_cast<LiteralExpr*>(memberCallExpr->object.get())) {
                if (literal->kind == LiteralExpr::Kind::String && !isNumericStringForInt32(literal->value)) {
                    return makeTypeErrorType(memberCallExpr,
                        "cannot convert string literal to int32 using ToInt32(): expected a base-10 numeric string");
                }
            }
            return setConversionInfo(memberCallExpr, "int32", objectType, method, ConversionKind::ToInt32FromStringParse);
        }

        if (objectType == "float32" || objectType == "float64") {
            return setConversionInfo(memberCallExpr, "int32", objectType, method, ConversionKind::ToInt32FromFloatTruncateTowardZero);
        }

        if (objectType == "bool") {
            return setConversionInfo(memberCallExpr, "int32", objectType, method, ConversionKind::ToInt32FromBool);
        }

        return setConversionInfo(memberCallExpr, "int32", objectType, method, ConversionKind::ToInt32Identity);
    }

    if (method == "ToFloat32") {
        if (!(objectType == "int32" || objectType == "float32" || objectType == "float64" || objectType == "string")) {
            return invalidReceiver();
        }

        if (objectType == "string") {
            if (auto* literal = dynamic_cast<LiteralExpr*>(memberCallExpr->object.get())) {
                if (literal->kind == LiteralExpr::Kind::String && !isNumericStringForFloat(literal->value)) {
                    return makeTypeErrorType(memberCallExpr,
                        "cannot convert string literal to float32 using ToFloat32(): expected a numeric string");
                }
            }
        }

        return setConversionInfo(memberCallExpr, "float32", objectType, method, ConversionKind::ToFloat32);
    }

    if (method == "ToFloat64") {
        if (!(objectType == "int32" || objectType == "float32" || objectType == "float64" || objectType == "string")) {
            return invalidReceiver();
        }

        if (objectType == "string") {
            if (auto* literal = dynamic_cast<LiteralExpr*>(memberCallExpr->object.get())) {
                if (literal->kind == LiteralExpr::Kind::String && !isNumericStringForFloat(literal->value)) {
                    return makeTypeErrorType(memberCallExpr,
                        "cannot convert string literal to float64 using ToFloat64(): expected a numeric string");
                }
            }
        }

        return setConversionInfo(memberCallExpr, "float64", objectType, method, ConversionKind::ToFloat64);
    }

    if (method == "ToBool") {
        if (!(objectType == "int32" || objectType == "float32" || objectType == "float64" ||
              objectType == "string" || objectType == "bool")) {
            return invalidReceiver();
        }

        if (objectType == "string") {
            if (auto* literal = dynamic_cast<LiteralExpr*>(memberCallExpr->object.get())) {
                if (literal->kind == LiteralExpr::Kind::String && !isBoolString(literal->value)) {
                    return makeTypeErrorType(memberCallExpr,
                        "cannot convert string literal to bool using ToBool(): expected 'true' or 'false'");
                }
            }
        }

        return setConversionInfo(memberCallExpr, "bool", objectType, method, ConversionKind::ToBool);
    }

    if (method == "Round") {
        if (!isFloatType(objectType)) {
            return invalidReceiver();
        }
        return setConversionInfo(memberCallExpr, objectType, objectType, method, ConversionKind::Round);
    }

    if (method == "Floor") {
        if (!isFloatType(objectType)) {
            return invalidReceiver();
        }
        return setConversionInfo(memberCallExpr, objectType, objectType, method, ConversionKind::Floor);
    }

    if (method == "Ceil") {
        if (!isFloatType(objectType)) {
            return invalidReceiver();
        }
        return setConversionInfo(memberCallExpr, objectType, objectType, method, ConversionKind::Ceil);
    }

    return makeTypeErrorType(memberCallExpr, "unknown conversion member '" + method +
        "'. Allowed: ToString, ToInt32, ToFloat32, ToFloat64, ToBool, Round, Floor, Ceil");
}

void SemanticAnalyzer::pushScope() {
    scopes_.push_back({});
}

void SemanticAnalyzer::popScope() {
    if (!scopes_.empty()) {
        scopes_.pop_back();
    }
}

bool SemanticAnalyzer::declareVariable(const std::string& name, const std::string& type, const SourceLocation& location) {
    if (scopes_.empty()) {
        pushScope();
    }

    auto& scope = scopes_.back();
    if (scope.find(name) != scope.end()) {
        diagnostics_.error(location, "duplicate symbol '" + name + "' in current scope");
        return false;
    }

    scope.emplace(name, VariableSymbol{type, location});
    return true;
}

const SemanticAnalyzer::VariableSymbol* SemanticAnalyzer::lookupVariable(const std::string& name) const {
    for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
        auto found = it->find(name);
        if (found != it->end()) {
            return &found->second;
        }
    }
    return nullptr;
}

bool SemanticAnalyzer::isKnownType(const std::string& type) const {
    return type == "int32" || type == "float32" || type == "float64" ||
           type == "string" || type == "bool" || type == "void";
}

bool SemanticAnalyzer::isNumericType(const std::string& type) const {
    return type == "int32" || type == "float32" || type == "float64";
}

bool SemanticAnalyzer::isFloatType(const std::string& type) const {
    return type == "float32" || type == "float64";
}

bool SemanticAnalyzer::isAssignable(const std::string& target, const std::string& source) const {
    if (isErrorType(target) || isErrorType(source)) {
        return true;
    }

    if (target == source) {
        return true;
    }

    if (target == "float64" && (source == "float32" || source == "int32")) {
        return true;
    }

    if (target == "float32" && source == "int32") {
        return true;
    }

    return false;
}

std::string SemanticAnalyzer::commonNumericType(const std::string& left, const std::string& right) const {
    if (left == "float64" || right == "float64") return "float64";
    if (left == "float32" || right == "float32") return "float32";
    return "int32";
}

bool SemanticAnalyzer::isPrimitiveType(const std::string& type) const {
    return type == "int32" || type == "float32" || type == "float64" || type == "string" || type == "bool";
}

bool SemanticAnalyzer::isNumericStringForInt32(const std::string& value) const {
    if (value.empty()) {
        return false;
    }
    for (char c : value) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    errno = 0;
    char* end = nullptr;
    const long long parsed = std::strtoll(value.c_str(), &end, 10);
    if (errno == ERANGE || end == value.c_str() || *end != '\0') {
        return false;
    }

    return parsed >= std::numeric_limits<std::int32_t>::min() &&
           parsed <= std::numeric_limits<std::int32_t>::max();
}

bool SemanticAnalyzer::isNumericStringForFloat(const std::string& value) const {
    if (value.empty()) {
        return false;
    }
    for (char c : value) {
        if (std::isspace(static_cast<unsigned char>(c))) {
            return false;
        }
    }

    errno = 0;
    char* end = nullptr;
    const double parsed = std::strtod(value.c_str(), &end);
    if (errno == ERANGE || end == value.c_str() || *end != '\0') {
        return false;
    }

    return std::isfinite(parsed);
}

bool SemanticAnalyzer::isBoolString(const std::string& value) const {
    return value == "true" || value == "false";
}

std::string SemanticAnalyzer::allowedReceiverTypes(const std::string& method) const {
    if (method == "ToString") return "int32, float32, float64, string, bool";
    if (method == "ToInt32") return "int32, float32, float64, string, bool";
    if (method == "ToFloat32") return "int32, float32, float64, string";
    if (method == "ToFloat64") return "int32, float32, float64, string";
    if (method == "ToBool") return "int32, float32, float64, string, bool";
    if (method == "Round" || method == "Floor" || method == "Ceil") return "float32, float64";
    return "ToString, ToInt32, ToFloat32, ToFloat64, ToBool, Round, Floor, Ceil";
}

std::string SemanticAnalyzer::setConversionInfo(const Expr* expr, const std::string& resultType, const std::string& sourceType, const std::string& method, ConversionKind kind) {
    conversionInfos_[expr] = ConversionInfo{method, sourceType, resultType, kind};
    return setExprType(expr, resultType);
}

std::string SemanticAnalyzer::makeTypeErrorType(Expr* expr, const std::string& message) {
    diagnostics_.error(expr->location, message);
    return setExprType(expr, kErrorType);
}

std::string SemanticAnalyzer::setExprType(const Expr* expr, const std::string& type) {
    expressionTypes_[expr] = type;
    return type;
}

bool SemanticAnalyzer::isErrorType(const std::string& type) const {
    return type == kErrorType;
}
