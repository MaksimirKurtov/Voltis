#include "lowering.h"
#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <unordered_map>
#include <utility>

namespace {

constexpr const char* kErrorType = "<error>";

struct FunctionSignature {
    std::vector<vir::Type> paramTypes;
    vir::Type returnType;
};

struct ValueRef {
    vir::Type type{};
    std::optional<vir::ValueId> value;
};

bool typeFromString(const std::string& typeName, vir::Type& outType) {
    if (typeName == "void") {
        outType = vir::Type{vir::TypeKind::Void};
        return true;
    }
    if (typeName == "int32") {
        outType = vir::Type{vir::TypeKind::Int32};
        return true;
    }
    if (typeName == "float32") {
        outType = vir::Type{vir::TypeKind::Float32};
        return true;
    }
    if (typeName == "float64") {
        outType = vir::Type{vir::TypeKind::Float64};
        return true;
    }
    if (typeName == "bool") {
        outType = vir::Type{vir::TypeKind::Bool};
        return true;
    }
    if (typeName == "string") {
        outType = vir::Type{vir::TypeKind::String};
        return true;
    }
    return false;
}

bool isNumericType(vir::Type type) {
    return type.kind == vir::TypeKind::Int32 || type.kind == vir::TypeKind::Float32 || type.kind == vir::TypeKind::Float64;
}

vir::Type commonNumericType(vir::Type left, vir::Type right) {
    if (left.kind == vir::TypeKind::Float64 || right.kind == vir::TypeKind::Float64) {
        return vir::Type{vir::TypeKind::Float64};
    }
    if (left.kind == vir::TypeKind::Float32 || right.kind == vir::TypeKind::Float32) {
        return vir::Type{vir::TypeKind::Float32};
    }
    return vir::Type{vir::TypeKind::Int32};
}

vir::ConversionKind mapConversionKind(SemanticAnalyzer::ConversionKind kind) {
    switch (kind) {
        case SemanticAnalyzer::ConversionKind::ToString: return vir::ConversionKind::ToString;
        case SemanticAnalyzer::ConversionKind::ToInt32Identity: return vir::ConversionKind::ToInt32Identity;
        case SemanticAnalyzer::ConversionKind::ToInt32FromFloatTruncateTowardZero: return vir::ConversionKind::ToInt32FromFloatTruncateTowardZero;
        case SemanticAnalyzer::ConversionKind::ToInt32FromStringParse: return vir::ConversionKind::ToInt32FromStringParse;
        case SemanticAnalyzer::ConversionKind::ToInt32FromBool: return vir::ConversionKind::ToInt32FromBool;
        case SemanticAnalyzer::ConversionKind::ToFloat32: return vir::ConversionKind::ToFloat32;
        case SemanticAnalyzer::ConversionKind::ToFloat64: return vir::ConversionKind::ToFloat64;
        case SemanticAnalyzer::ConversionKind::ToBool: return vir::ConversionKind::ToBool;
        case SemanticAnalyzer::ConversionKind::Round: return vir::ConversionKind::Round;
        case SemanticAnalyzer::ConversionKind::Floor: return vir::ConversionKind::Floor;
        case SemanticAnalyzer::ConversionKind::Ceil: return vir::ConversionKind::Ceil;
        case SemanticAnalyzer::ConversionKind::None:
            break;
    }
    return vir::ConversionKind::ToString;
}

class LoweringContext {
public:
    LoweringContext(
        const Program& program,
        const std::unordered_map<const Expr*, std::string>& expressionTypes,
        const std::unordered_map<const Expr*, SemanticAnalyzer::ConversionInfo>& conversionInfos)
        : program_(program), expressionTypes_(expressionTypes), conversionInfos_(conversionInfos) {}

    LoweringResult run() {
        collectFunctionSignatures();
        for (const auto& functionDecl : program_.functions) {
            lowerFunction(functionDecl);
        }
        return std::move(result_);
    }

private:
    const Program& program_;
    const std::unordered_map<const Expr*, std::string>& expressionTypes_;
    const std::unordered_map<const Expr*, SemanticAnalyzer::ConversionInfo>& conversionInfos_;
    LoweringResult result_;
    std::unordered_map<std::string, FunctionSignature> signatures_;

    vir::Function* currentFunction_ = nullptr;
    vir::BlockId currentBlock_{};
    std::uint32_t nextValueId_ = 0;
    std::vector<std::unordered_map<std::string, vir::LocalId>> scopes_;
    std::uint32_t namedBlockCounter_ = 0;

    void collectFunctionSignatures() {
        for (const auto& functionDecl : program_.functions) {
            FunctionSignature signature;
            vir::Type returnType{};
            if (!typeFromString(functionDecl.returnType, returnType)) {
                result_.diagnostics.error(functionDecl.location, "lowering: unknown return type '" + functionDecl.returnType + "'");
                returnType = vir::Type{vir::TypeKind::Void};
            }
            signature.returnType = returnType;

            signature.paramTypes.reserve(functionDecl.params.size());
            for (const auto& param : functionDecl.params) {
                vir::Type paramType{};
                if (!typeFromString(param.type, paramType)) {
                    result_.diagnostics.error(param.location, "lowering: unknown parameter type '" + param.type + "'");
                    paramType = vir::Type{vir::TypeKind::Void};
                }
                signature.paramTypes.push_back(paramType);
            }

            signatures_[functionDecl.name] = signature;
        }
    }

    void lowerFunction(const FunctionDecl& functionDecl) {
        vir::Function function;
        function.name = functionDecl.name;
        if (!typeFromString(functionDecl.returnType, function.returnType)) {
            function.returnType = vir::Type{vir::TypeKind::Void};
        }

        function.params.reserve(functionDecl.params.size());
        for (const auto& param : functionDecl.params) {
            vir::Type type{};
            if (!typeFromString(param.type, type)) {
                type = vir::Type{vir::TypeKind::Void};
            }
            function.params.push_back(vir::Parameter{param.name, type, param.location});
        }

        result_.module.functions.push_back(std::move(function));
        currentFunction_ = &result_.module.functions.back();
        nextValueId_ = 0;
        namedBlockCounter_ = 0;
        scopes_.clear();

        currentBlock_ = createBlock("entry");
        pushScope();

        for (std::size_t i = 0; i < currentFunction_->params.size(); ++i) {
            const auto& param = currentFunction_->params[i];
            declareLocal(param.name, param.type, true, i, param.location);
        }

        lowerBlock(*functionDecl.body, false);

        if (!hasTerminator(currentBlock_)) {
            if (vir::isVoid(currentFunction_->returnType)) {
                setTerminator(vir::Terminator{
                    vir::Terminator::Kind::Return,
                    functionDecl.location,
                    vir::ReturnTerminator{std::nullopt, vir::Type{vir::TypeKind::Void}}
                });
            } else {
                setTerminator(vir::Terminator{
                    vir::Terminator::Kind::Unreachable,
                    functionDecl.location,
                    vir::UnreachableTerminator{}
                });
            }
        }

        popScope();
        currentFunction_ = nullptr;
    }

    void lowerBlock(const BlockStmt& block, bool createScope) {
        if (createScope) {
            pushScope();
        }

        for (const auto& statement : block.statements) {
            if (hasTerminator(currentBlock_)) {
                break;
            }
            lowerStatement(statement.get());
        }

        if (createScope) {
            popScope();
        }
    }

    void lowerStatement(const Stmt* statement) {
        if (auto exprStmt = dynamic_cast<const ExprStmt*>(statement)) {
            lowerExpr(exprStmt->expr.get());
            return;
        }

        if (auto returnStmt = dynamic_cast<const ReturnStmt*>(statement)) {
            ValueRef value = lowerExpr(returnStmt->expr.get());
            ValueRef castValue = castTo(value, currentFunction_->returnType, returnStmt->location, std::nullopt, false);
            if (vir::isVoid(currentFunction_->returnType) || !castValue.value.has_value()) {
                setTerminator(vir::Terminator{
                    vir::Terminator::Kind::Return,
                    returnStmt->location,
                    vir::ReturnTerminator{std::nullopt, vir::Type{vir::TypeKind::Void}}
                });
            } else {
                setTerminator(vir::Terminator{
                    vir::Terminator::Kind::Return,
                    returnStmt->location,
                    vir::ReturnTerminator{castValue.value, castValue.type}
                });
            }
            return;
        }

        if (auto varDecl = dynamic_cast<const VarDeclStmt*>(statement)) {
            vir::Type declaredType{};
            if (!typeFromString(varDecl->type, declaredType)) {
                result_.diagnostics.error(varDecl->location, "lowering: unknown variable type '" + varDecl->type + "'");
                declaredType = vir::Type{vir::TypeKind::Void};
            }

            const vir::LocalId local = declareLocal(varDecl->name, declaredType, false, std::nullopt, varDecl->location);
            ValueRef initValue = lowerExpr(varDecl->init.get());
            initValue = castTo(initValue, declaredType, varDecl->location, std::nullopt, false);

            if (initValue.value.has_value()) {
                emitInstruction(vir::Instruction{
                    vir::Instruction::Kind::StoreLocal,
                    varDecl->location,
                    vir::StoreLocalInst{local, *initValue.value, initValue.type}
                });
            }
            return;
        }

        if (auto assign = dynamic_cast<const AssignStmt*>(statement)) {
            auto local = lookupLocal(assign->name);
            if (!local.has_value()) {
                result_.diagnostics.error(assign->location, "lowering: undefined local '" + assign->name + "'");
                return;
            }

            const vir::Type targetType = currentFunction_->locals[local->index].type;
            ValueRef value = lowerExpr(assign->value.get());
            value = castTo(value, targetType, assign->location, std::nullopt, false);
            if (value.value.has_value()) {
                emitInstruction(vir::Instruction{
                    vir::Instruction::Kind::StoreLocal,
                    assign->location,
                    vir::StoreLocalInst{*local, *value.value, value.type}
                });
            }
            return;
        }

        if (auto nestedBlock = dynamic_cast<const BlockStmt*>(statement)) {
            lowerBlock(*nestedBlock, true);
            return;
        }

        if (auto ifStmt = dynamic_cast<const IfStmt*>(statement)) {
            lowerIfStatement(*ifStmt);
            return;
        }
    }

    void lowerIfStatement(const IfStmt& ifStmt) {
        ValueRef condition = lowerExpr(ifStmt.condition.get());
        condition = castTo(condition, vir::Type{vir::TypeKind::Bool}, ifStmt.location, std::nullopt, false);
        if (!condition.value.has_value()) {
            result_.diagnostics.error(ifStmt.location, "lowering: if condition has no value");
            return;
        }

        const vir::BlockId thenBlock = createBlock("if.then");
        const vir::BlockId mergeBlock = createBlock("if.merge");
        const vir::BlockId elseBlock = ifStmt.elseBlock ? createBlock("if.else") : mergeBlock;

        setTerminator(vir::Terminator{
            vir::Terminator::Kind::CondBranch,
            ifStmt.location,
            vir::CondBranchTerminator{*condition.value, thenBlock, elseBlock}
        });

        currentBlock_ = thenBlock;
        lowerBlock(*ifStmt.thenBlock, true);
        if (!hasTerminator(currentBlock_)) {
            setTerminator(vir::Terminator{
                vir::Terminator::Kind::Branch,
                ifStmt.location,
                vir::BranchTerminator{mergeBlock}
            });
        }

        if (ifStmt.elseBlock) {
            currentBlock_ = elseBlock;
            lowerBlock(*ifStmt.elseBlock, true);
            if (!hasTerminator(currentBlock_)) {
                setTerminator(vir::Terminator{
                    vir::Terminator::Kind::Branch,
                    ifStmt.location,
                    vir::BranchTerminator{mergeBlock}
                });
            }
        }

        currentBlock_ = mergeBlock;
    }

    ValueRef lowerExpr(const Expr* expr) {
        if (auto literal = dynamic_cast<const LiteralExpr*>(expr)) {
            return lowerLiteral(*literal);
        }
        if (auto variable = dynamic_cast<const VariableExpr*>(expr)) {
            return lowerVariable(*variable);
        }
        if (auto unary = dynamic_cast<const UnaryExpr*>(expr)) {
            return lowerUnary(*unary);
        }
        if (auto binary = dynamic_cast<const BinaryExpr*>(expr)) {
            return lowerBinary(*binary);
        }
        if (auto call = dynamic_cast<const CallExpr*>(expr)) {
            return lowerCall(*call);
        }
        if (auto memberCall = dynamic_cast<const MemberCallExpr*>(expr)) {
            return lowerMemberCall(*memberCall);
        }

        result_.diagnostics.error(expr->location, "lowering: unsupported expression node");
        return ValueRef{vir::Type{vir::TypeKind::Void}, std::nullopt};
    }

    ValueRef lowerLiteral(const LiteralExpr& literal) {
        vir::Type literalType = exprType(&literal);
        const vir::ValueId valueId = nextValue();

        if (literalType.kind == vir::TypeKind::String) {
            emitInstruction(vir::Instruction{
                vir::Instruction::Kind::Constant,
                literal.location,
                vir::ConstantInst{valueId, vir::Constant{literalType, literal.value}}
            });
            return ValueRef{literalType, valueId};
        }

        if (literalType.kind == vir::TypeKind::Bool) {
            const bool boolValue = literal.value == "true";
            emitInstruction(vir::Instruction{
                vir::Instruction::Kind::Constant,
                literal.location,
                vir::ConstantInst{valueId, vir::Constant{literalType, boolValue}}
            });
            return ValueRef{literalType, valueId};
        }

        if (literalType.kind == vir::TypeKind::Int32) {
            const std::int32_t parsed = static_cast<std::int32_t>(std::strtol(literal.value.c_str(), nullptr, 10));
            emitInstruction(vir::Instruction{
                vir::Instruction::Kind::Constant,
                literal.location,
                vir::ConstantInst{valueId, vir::Constant{literalType, parsed}}
            });
            return ValueRef{literalType, valueId};
        }

        if (literalType.kind == vir::TypeKind::Float32) {
            std::string token = literal.value;
            if (!token.empty() && token.back() == 'f') {
                token.pop_back();
            }
            const float parsed = std::strtof(token.c_str(), nullptr);
            emitInstruction(vir::Instruction{
                vir::Instruction::Kind::Constant,
                literal.location,
                vir::ConstantInst{valueId, vir::Constant{literalType, parsed}}
            });
            return ValueRef{literalType, valueId};
        }

        if (literalType.kind == vir::TypeKind::Float64) {
            const double parsed = std::strtod(literal.value.c_str(), nullptr);
            emitInstruction(vir::Instruction{
                vir::Instruction::Kind::Constant,
                literal.location,
                vir::ConstantInst{valueId, vir::Constant{literalType, parsed}}
            });
            return ValueRef{literalType, valueId};
        }

        result_.diagnostics.error(literal.location, "lowering: unsupported literal type");
        return ValueRef{vir::Type{vir::TypeKind::Void}, std::nullopt};
    }

    ValueRef lowerVariable(const VariableExpr& variable) {
        auto local = lookupLocal(variable.name);
        if (!local.has_value()) {
            result_.diagnostics.error(variable.location, "lowering: unknown symbol '" + variable.name + "'");
            return ValueRef{vir::Type{vir::TypeKind::Void}, std::nullopt};
        }
        const vir::Type localType = currentFunction_->locals[local->index].type;
        const vir::ValueId valueId = nextValue();
        emitInstruction(vir::Instruction{
            vir::Instruction::Kind::LoadLocal,
            variable.location,
            vir::LoadLocalInst{valueId, *local, localType}
        });
        return ValueRef{localType, valueId};
    }

    ValueRef lowerUnary(const UnaryExpr& unary) {
        ValueRef operand = lowerExpr(unary.right.get());
        if (!operand.value.has_value()) {
            result_.diagnostics.error(unary.location, "lowering: unary operand has no value");
            return ValueRef{vir::Type{vir::TypeKind::Void}, std::nullopt};
        }

        vir::UnaryOp op = vir::UnaryOp::Negate;
        if (unary.op == "-" ) {
            op = vir::UnaryOp::Negate;
        } else if (unary.op == "!" || unary.op == "not") {
            op = vir::UnaryOp::LogicalNot;
            operand = castTo(operand, vir::Type{vir::TypeKind::Bool}, unary.location, std::nullopt, false);
            if (!operand.value.has_value()) {
                return ValueRef{vir::Type{vir::TypeKind::Void}, std::nullopt};
            }
        } else {
            result_.diagnostics.error(unary.location, "lowering: unsupported unary operator '" + unary.op + "'");
            return ValueRef{vir::Type{vir::TypeKind::Void}, std::nullopt};
        }

        vir::Type resultType = exprType(&unary);
        const vir::ValueId valueId = nextValue();
        emitInstruction(vir::Instruction{
            vir::Instruction::Kind::Unary,
            unary.location,
            vir::UnaryInst{valueId, op, *operand.value, resultType}
        });
        return ValueRef{resultType, valueId};
    }

    ValueRef lowerBinary(const BinaryExpr& binary) {
        ValueRef left = lowerExpr(binary.left.get());
        ValueRef right = lowerExpr(binary.right.get());

        if (!left.value.has_value() || !right.value.has_value()) {
            result_.diagnostics.error(binary.location, "lowering: binary operand has no value");
            return ValueRef{vir::Type{vir::TypeKind::Void}, std::nullopt};
        }

        vir::BinaryOp op = vir::BinaryOp::Add;
        vir::Type resultType = exprType(&binary);
        vir::Type operandType = resultType;

        if (binary.op == "+") op = vir::BinaryOp::Add;
        else if (binary.op == "-") op = vir::BinaryOp::Subtract;
        else if (binary.op == "*") op = vir::BinaryOp::Multiply;
        else if (binary.op == "/") op = vir::BinaryOp::Divide;
        else if (binary.op == "==") op = vir::BinaryOp::Equal;
        else if (binary.op == "!=") op = vir::BinaryOp::NotEqual;
        else if (binary.op == "<") op = vir::BinaryOp::Less;
        else if (binary.op == "<=") op = vir::BinaryOp::LessEqual;
        else if (binary.op == ">") op = vir::BinaryOp::Greater;
        else if (binary.op == ">=") op = vir::BinaryOp::GreaterEqual;
        else if (binary.op == "and") op = vir::BinaryOp::LogicalAnd;
        else if (binary.op == "or") op = vir::BinaryOp::LogicalOr;
        else {
            result_.diagnostics.error(binary.location, "lowering: unsupported binary operator '" + binary.op + "'");
            return ValueRef{vir::Type{vir::TypeKind::Void}, std::nullopt};
        }

        if (binary.op == "and" || binary.op == "or") {
            operandType = vir::Type{vir::TypeKind::Bool};
        } else if (binary.op == "<" || binary.op == "<=" || binary.op == ">" || binary.op == ">=") {
            operandType = commonNumericType(left.type, right.type);
        } else if (binary.op == "==" || binary.op == "!=") {
            if (isNumericType(left.type) && isNumericType(right.type)) {
                operandType = commonNumericType(left.type, right.type);
            } else {
                operandType = left.type;
            }
        } else if (binary.op == "+" && resultType.kind == vir::TypeKind::String) {
            operandType = vir::Type{vir::TypeKind::String};
        } else {
            operandType = resultType;
        }

        left = castTo(left, operandType, binary.location, std::nullopt, false);
        right = castTo(right, operandType, binary.location, std::nullopt, false);
        if (!left.value.has_value() || !right.value.has_value()) {
            return ValueRef{vir::Type{vir::TypeKind::Void}, std::nullopt};
        }

        const vir::ValueId valueId = nextValue();
        emitInstruction(vir::Instruction{
            vir::Instruction::Kind::Binary,
            binary.location,
            vir::BinaryInst{valueId, op, *left.value, *right.value, resultType}
        });
        return ValueRef{resultType, valueId};
    }

    ValueRef lowerCall(const CallExpr& callExpr) {
        const auto* callee = dynamic_cast<const VariableExpr*>(callExpr.callee.get());
        if (!callee) {
            result_.diagnostics.error(callExpr.location, "lowering: only direct function calls are supported");
            return ValueRef{vir::Type{vir::TypeKind::Void}, std::nullopt};
        }

        std::vector<ValueRef> argValues;
        argValues.reserve(callExpr.args.size());
        for (const auto& arg : callExpr.args) {
            argValues.push_back(lowerExpr(arg.get()));
        }

        if (callee->name == "print") {
            std::vector<vir::ValueId> args;
            std::vector<vir::Type> argTypes;
            for (auto& argValue : argValues) {
                if (!argValue.value.has_value()) {
                    result_.diagnostics.error(callExpr.location, "lowering: print argument is void");
                    return ValueRef{vir::Type{vir::TypeKind::Void}, std::nullopt};
                }
                args.push_back(*argValue.value);
                argTypes.push_back(argValue.type);
            }

            emitInstruction(vir::Instruction{
                vir::Instruction::Kind::Call,
                callExpr.location,
                vir::CallInst{
                    std::nullopt,
                    "print",
                    std::move(args),
                    std::move(argTypes),
                    vir::Type{vir::TypeKind::Void},
                    true
                }
            });
            return ValueRef{vir::Type{vir::TypeKind::Void}, std::nullopt};
        }

        auto signatureIt = signatures_.find(callee->name);
        if (signatureIt == signatures_.end()) {
            result_.diagnostics.error(callExpr.location, "lowering: unknown function '" + callee->name + "'");
            return ValueRef{vir::Type{vir::TypeKind::Void}, std::nullopt};
        }

        const FunctionSignature& signature = signatureIt->second;
        const std::size_t sharedCount = std::min(argValues.size(), signature.paramTypes.size());
        for (std::size_t i = 0; i < sharedCount; ++i) {
            argValues[i] = castTo(argValues[i], signature.paramTypes[i], callExpr.location, std::nullopt, false);
        }

        std::vector<vir::ValueId> args;
        std::vector<vir::Type> argTypes;
        args.reserve(argValues.size());
        argTypes.reserve(argValues.size());
        for (const auto& argValue : argValues) {
            if (!argValue.value.has_value()) {
                result_.diagnostics.error(callExpr.location, "lowering: function argument is void");
                return ValueRef{vir::Type{vir::TypeKind::Void}, std::nullopt};
            }
            args.push_back(*argValue.value);
            argTypes.push_back(argValue.type);
        }

        std::optional<vir::ValueId> resultValue;
        if (!vir::isVoid(signature.returnType)) {
            resultValue = nextValue();
        }

        emitInstruction(vir::Instruction{
            vir::Instruction::Kind::Call,
            callExpr.location,
            vir::CallInst{resultValue, callee->name, std::move(args), std::move(argTypes), signature.returnType, false}
        });
        return ValueRef{signature.returnType, resultValue};
    }

    ValueRef lowerMemberCall(const MemberCallExpr& memberCall) {
        ValueRef object = lowerExpr(memberCall.object.get());
        if (!object.value.has_value()) {
            result_.diagnostics.error(memberCall.location, "lowering: conversion receiver is void");
            return ValueRef{vir::Type{vir::TypeKind::Void}, std::nullopt};
        }

        auto convIt = conversionInfos_.find(&memberCall);
        vir::Type resultType = exprType(&memberCall);
        vir::ConversionKind conversionKind = vir::ConversionKind::ToString;
        bool hasKind = false;

        if (convIt != conversionInfos_.end() && convIt->second.kind != SemanticAnalyzer::ConversionKind::None) {
            conversionKind = mapConversionKind(convIt->second.kind);
            hasKind = true;
            vir::Type mappedResult{};
            if (typeFromString(convIt->second.resultType, mappedResult)) {
                resultType = mappedResult;
            }
        }

        if (!hasKind) {
            if (memberCall.method == "ToString") conversionKind = vir::ConversionKind::ToString;
            else if (memberCall.method == "ToInt32") conversionKind = vir::ConversionKind::ToInt32Identity;
            else if (memberCall.method == "ToFloat32") conversionKind = vir::ConversionKind::ToFloat32;
            else if (memberCall.method == "ToFloat64") conversionKind = vir::ConversionKind::ToFloat64;
            else if (memberCall.method == "ToBool") conversionKind = vir::ConversionKind::ToBool;
            else if (memberCall.method == "Round") conversionKind = vir::ConversionKind::Round;
            else if (memberCall.method == "Floor") conversionKind = vir::ConversionKind::Floor;
            else if (memberCall.method == "Ceil") conversionKind = vir::ConversionKind::Ceil;
            else {
                result_.diagnostics.error(memberCall.location, "lowering: unknown conversion member '" + memberCall.method + "'");
                return ValueRef{vir::Type{vir::TypeKind::Void}, std::nullopt};
            }
        }

        return castTo(object, resultType, memberCall.location, conversionKind, true);
    }

    ValueRef castTo(
        const ValueRef& source,
        vir::Type targetType,
        const SourceLocation& location,
        std::optional<vir::ConversionKind> explicitKind,
        bool forceExplicitInstruction) {
        if (!source.value.has_value()) {
            return ValueRef{targetType, std::nullopt};
        }

        if (source.type == targetType && !forceExplicitInstruction && !explicitKind.has_value()) {
            return source;
        }

        vir::ConversionKind kind = vir::ConversionKind::ToString;
        if (explicitKind.has_value()) {
            kind = *explicitKind;
        } else if (source.type.kind == vir::TypeKind::Int32 && targetType.kind == vir::TypeKind::Float32) {
            kind = vir::ConversionKind::ImplicitInt32ToFloat32;
        } else if (source.type.kind == vir::TypeKind::Int32 && targetType.kind == vir::TypeKind::Float64) {
            kind = vir::ConversionKind::ImplicitInt32ToFloat64;
        } else if (source.type.kind == vir::TypeKind::Float32 && targetType.kind == vir::TypeKind::Float64) {
            kind = vir::ConversionKind::ImplicitFloat32ToFloat64;
        } else if (source.type == targetType) {
            kind = vir::ConversionKind::ToInt32Identity;
        } else {
            result_.diagnostics.error(location,
                "lowering: unsupported implicit conversion from '" + vir::toString(source.type) + "' to '" + vir::toString(targetType) + "'");
            return ValueRef{targetType, std::nullopt};
        }

        const vir::ValueId valueId = nextValue();
        emitInstruction(vir::Instruction{
            vir::Instruction::Kind::Convert,
            location,
            vir::ConvertInst{valueId, *source.value, source.type, targetType, kind}
        });
        return ValueRef{targetType, valueId};
    }

    vir::Type exprType(const Expr* expr) {
        auto it = expressionTypes_.find(expr);
        if (it == expressionTypes_.end()) {
            result_.diagnostics.error(expr->location, "lowering: missing semantic type for expression");
            return vir::Type{vir::TypeKind::Void};
        }
        if (it->second == kErrorType) {
            return vir::Type{vir::TypeKind::Void};
        }

        vir::Type type{};
        if (!typeFromString(it->second, type)) {
            result_.diagnostics.error(expr->location, "lowering: unknown semantic type '" + it->second + "'");
            return vir::Type{vir::TypeKind::Void};
        }
        return type;
    }

    vir::ValueId nextValue() {
        return vir::ValueId{nextValueId_++};
    }

    vir::BlockId createBlock(const std::string& prefix) {
        const vir::BlockId id{static_cast<std::uint32_t>(currentFunction_->blocks.size())};
        std::string name = prefix;
        if (prefix != "entry") {
            name += "." + std::to_string(namedBlockCounter_++);
        }
        currentFunction_->blocks.push_back(vir::BasicBlock{id, name, {}, std::nullopt});
        return id;
    }

    void pushScope() {
        scopes_.push_back({});
    }

    void popScope() {
        if (!scopes_.empty()) {
            scopes_.pop_back();
        }
    }

    vir::LocalId declareLocal(
        const std::string& name,
        vir::Type type,
        bool isParameter,
        std::optional<std::size_t> parameterIndex,
        const SourceLocation& location) {
        if (scopes_.empty()) {
            pushScope();
        }

        auto& scope = scopes_.back();
        auto found = scope.find(name);
        if (found != scope.end()) {
            result_.diagnostics.error(location, "lowering: duplicate local '" + name + "'");
            return found->second;
        }

        vir::LocalId id{static_cast<std::uint32_t>(currentFunction_->locals.size())};
        currentFunction_->locals.push_back(vir::Local{id, name, type, isParameter, parameterIndex, location});
        scope[name] = id;
        return id;
    }

    std::optional<vir::LocalId> lookupLocal(const std::string& name) const {
        for (auto it = scopes_.rbegin(); it != scopes_.rend(); ++it) {
            auto found = it->find(name);
            if (found != it->end()) {
                return found->second;
            }
        }
        return std::nullopt;
    }

    bool hasTerminator(vir::BlockId blockId) const {
        return currentFunction_->blocks[blockId.index].terminator.has_value();
    }

    void setTerminator(vir::Terminator terminator) {
        auto& block = currentFunction_->blocks[currentBlock_.index];
        if (block.terminator.has_value()) {
            result_.diagnostics.error(terminator.location, "lowering: block already has terminator");
            return;
        }
        block.terminator = std::move(terminator);
    }

    void emitInstruction(vir::Instruction instruction) {
        auto& block = currentFunction_->blocks[currentBlock_.index];
        if (block.terminator.has_value()) {
            result_.diagnostics.error(instruction.location, "lowering: cannot emit instruction after terminator");
            return;
        }
        block.instructions.push_back(std::move(instruction));
    }
};

} // namespace

LoweringResult VIRLowerer::lower(
    const Program& program,
    const std::unordered_map<const Expr*, std::string>& expressionTypes,
    const std::unordered_map<const Expr*, SemanticAnalyzer::ConversionInfo>& conversionInfos) const {
    LoweringContext context(program, expressionTypes, conversionInfos);
    return context.run();
}

LoweringResult VIRLowerer::lower(const Program& program, const SemanticAnalyzer& sema) const {
    return lower(program, sema.expressionTypes(), sema.conversionInfos());
}
