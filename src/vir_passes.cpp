#include "vir_passes.h"

#include <algorithm>
#include <cctype>
#include <stack>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <utility>
#include <vector>

namespace vir {
namespace {

struct FunctionSignature {
    Type returnType{};
    std::vector<Type> paramTypes;
    bool isExtern = false;
    std::string importPath;
    SourceLocation location;
};

std::string normalizePath(std::string path) {
    std::string normalized;
    normalized.reserve(path.size());
    for (char c : path) {
        if (c == '\\') {
            normalized.push_back('/');
            continue;
        }
        normalized.push_back(static_cast<char>(std::tolower(static_cast<unsigned char>(c))));
    }
    return normalized;
}

bool isNumeric(Type type) {
    return type.kind == TypeKind::Int32 || type.kind == TypeKind::Float32 || type.kind == TypeKind::Float64;
}

void verifyFunction(
    const Function& function,
    const std::unordered_map<std::string, FunctionSignature>& signatures,
    DiagnosticBag& diagnostics) {
    if (function.blocks.empty()) {
        diagnostics.error({}, "vir-verify: function '" + function.name + "' has no basic blocks");
        return;
    }

    std::unordered_map<std::uint32_t, const BasicBlock*> blocksById;
    for (const auto& block : function.blocks) {
        if (!blocksById.emplace(block.id.index, &block).second) {
            diagnostics.error({}, "vir-verify: function '" + function.name + "' has duplicate basic block id " +
                std::to_string(block.id.index));
        }
    }

    std::unordered_map<std::uint32_t, Type> localTypes;
    for (const auto& local : function.locals) {
        if (!localTypes.emplace(local.id.index, local.type).second) {
            diagnostics.error(local.location, "vir-verify: function '" + function.name + "' has duplicate local id " +
                std::to_string(local.id.index));
        }
        if (local.isParameter) {
            if (!local.parameterIndex.has_value()) {
                diagnostics.error(local.location, "vir-verify: parameter local '" + local.name + "' is missing parameter index");
            } else if (*local.parameterIndex >= function.params.size()) {
                diagnostics.error(local.location, "vir-verify: parameter local '" + local.name + "' has out-of-range parameter index");
            } else if (function.params[*local.parameterIndex].type != local.type) {
                diagnostics.error(local.location, "vir-verify: parameter local '" + local.name + "' type does not match function parameter type");
            }
        } else if (local.parameterIndex.has_value()) {
            diagnostics.error(local.location, "vir-verify: non-parameter local '" + local.name + "' must not have parameter index");
        }
    }

    std::unordered_map<std::uint32_t, Type> valueTypes;
    auto defineValue = [&](ValueId id, Type type, const SourceLocation& location) {
        if (!valueTypes.emplace(id.index, type).second) {
            diagnostics.error(location, "vir-verify: function '" + function.name + "' redefines SSA value %" + std::to_string(id.index));
        }
    };

    auto requireValue = [&](ValueId id, const SourceLocation& location, const std::string& context) -> Type {
        const auto it = valueTypes.find(id.index);
        if (it == valueTypes.end()) {
            diagnostics.error(location, "vir-verify: function '" + function.name + "' references undefined SSA value %" +
                std::to_string(id.index) + " in " + context);
            return Type{TypeKind::Void};
        }
        return it->second;
    };

    auto requireLocal = [&](LocalId id, const SourceLocation& location, const std::string& context) -> Type {
        const auto it = localTypes.find(id.index);
        if (it == localTypes.end()) {
            diagnostics.error(location, "vir-verify: function '" + function.name + "' references unknown local %" +
                std::to_string(id.index) + " in " + context);
            return Type{TypeKind::Void};
        }
        return it->second;
    };

    for (const auto& block : function.blocks) {
        for (const auto& instruction : block.instructions) {
            switch (instruction.kind) {
                case Instruction::Kind::Constant: {
                    const auto& inst = std::get<ConstantInst>(instruction.data);
                    defineValue(inst.result, inst.constant.type, instruction.location);
                    break;
                }
                case Instruction::Kind::LoadLocal: {
                    const auto& inst = std::get<LoadLocalInst>(instruction.data);
                    const Type localType = requireLocal(inst.local, instruction.location, "load");
                    if (localType != inst.type) {
                        diagnostics.error(instruction.location, "vir-verify: load type mismatch in function '" + function.name + "'");
                    }
                    defineValue(inst.result, inst.type, instruction.location);
                    break;
                }
                case Instruction::Kind::StoreLocal: {
                    const auto& inst = std::get<StoreLocalInst>(instruction.data);
                    const Type localType = requireLocal(inst.local, instruction.location, "store");
                    const Type valueType = requireValue(inst.value, instruction.location, "store");
                    if (valueType != inst.valueType) {
                        diagnostics.error(instruction.location, "vir-verify: store operand type does not match declared store value type in function '" + function.name + "'");
                    }
                    if (localType != inst.valueType) {
                        diagnostics.error(instruction.location, "vir-verify: store writes incompatible type to local in function '" + function.name + "'");
                    }
                    break;
                }
                case Instruction::Kind::Unary: {
                    const auto& inst = std::get<UnaryInst>(instruction.data);
                    const Type operandType = requireValue(inst.operand, instruction.location, "unary");
                    if (inst.op == UnaryOp::Negate) {
                        if (!isNumeric(operandType) || operandType != inst.type) {
                            diagnostics.error(instruction.location, "vir-verify: negate requires matching numeric operand/result type in function '" + function.name + "'");
                        }
                    } else if (inst.op == UnaryOp::LogicalNot) {
                        if (operandType.kind != TypeKind::Bool || inst.type.kind != TypeKind::Bool) {
                            diagnostics.error(instruction.location, "vir-verify: logical not requires bool operand/result in function '" + function.name + "'");
                        }
                    }
                    defineValue(inst.result, inst.type, instruction.location);
                    break;
                }
                case Instruction::Kind::Binary: {
                    const auto& inst = std::get<BinaryInst>(instruction.data);
                    requireValue(inst.left, instruction.location, "binary left operand");
                    requireValue(inst.right, instruction.location, "binary right operand");
                    defineValue(inst.result, inst.type, instruction.location);
                    break;
                }
                case Instruction::Kind::Convert: {
                    const auto& inst = std::get<ConvertInst>(instruction.data);
                    const Type inputType = requireValue(inst.input, instruction.location, "conversion");
                    if (inputType != inst.fromType) {
                        diagnostics.error(instruction.location, "vir-verify: conversion input type mismatch in function '" + function.name + "'");
                    }
                    defineValue(inst.result, inst.toType, instruction.location);
                    break;
                }
                case Instruction::Kind::Call: {
                    const auto& inst = std::get<CallInst>(instruction.data);
                    if (inst.args.size() != inst.argTypes.size()) {
                        diagnostics.error(instruction.location, "vir-verify: call argument/value type vector size mismatch in function '" + function.name + "'");
                    }

                    for (std::size_t i = 0; i < inst.args.size(); ++i) {
                        const Type actualType = requireValue(inst.args[i], instruction.location, "call argument");
                        const Type declaredType = i < inst.argTypes.size() ? inst.argTypes[i] : Type{TypeKind::Void};
                        if (actualType != declaredType) {
                            diagnostics.error(instruction.location, "vir-verify: call argument type mismatch in function '" + function.name + "'");
                        }
                    }

                    if (inst.isBuiltin) {
                        if (inst.callee != "print") {
                            diagnostics.error(instruction.location, "vir-verify: unknown builtin call '" + inst.callee + "'");
                        }
                        if (inst.result.has_value() || !isVoid(inst.returnType)) {
                            diagnostics.error(instruction.location, "vir-verify: builtin print must return void without result id");
                        }
                        if (inst.args.size() != 1) {
                            diagnostics.error(instruction.location, "vir-verify: builtin print expects exactly one argument");
                        }
                    } else {
                        const auto signatureIt = signatures.find(inst.callee);
                        if (signatureIt == signatures.end()) {
                            diagnostics.error(instruction.location, "vir-verify: call to unknown function '" + inst.callee + "'");
                        } else {
                            const FunctionSignature& signature = signatureIt->second;
                            if (signature.isExtern != inst.isExtern) {
                                diagnostics.error(instruction.location, "vir-verify: call extern/internal mismatch for '" + inst.callee + "'");
                            }
                            if (signature.returnType != inst.returnType) {
                                diagnostics.error(instruction.location, "vir-verify: call return type mismatch for '" + inst.callee + "'");
                            }
                            if (signature.paramTypes.size() != inst.argTypes.size()) {
                                diagnostics.error(instruction.location, "vir-verify: call arity mismatch for '" + inst.callee + "'");
                            }
                            const std::size_t sharedCount = std::min(signature.paramTypes.size(), inst.argTypes.size());
                            for (std::size_t i = 0; i < sharedCount; ++i) {
                                if (signature.paramTypes[i] != inst.argTypes[i]) {
                                    diagnostics.error(instruction.location, "vir-verify: call parameter type mismatch for '" + inst.callee + "'");
                                }
                            }
                            if (inst.isExtern && !inst.importPath.empty() &&
                                normalizePath(inst.importPath) != normalizePath(signature.importPath)) {
                                diagnostics.error(instruction.location, "vir-verify: extern call import path mismatch for '" + inst.callee + "'");
                            }
                        }
                    }

                    if (isVoid(inst.returnType)) {
                        if (inst.result.has_value()) {
                            diagnostics.error(instruction.location, "vir-verify: void call must not produce a result id in function '" + function.name + "'");
                        }
                    } else {
                        if (!inst.result.has_value()) {
                            diagnostics.error(instruction.location, "vir-verify: non-void call must produce a result id in function '" + function.name + "'");
                        } else {
                            defineValue(*inst.result, inst.returnType, instruction.location);
                        }
                    }
                    break;
                }
            }
        }
    }

    for (const auto& block : function.blocks) {
        if (!block.terminator.has_value()) {
            diagnostics.error({}, "vir-verify: function '" + function.name + "', block '" + block.name + "' has no terminator");
            continue;
        }

        const Terminator& term = *block.terminator;
        switch (term.kind) {
            case Terminator::Kind::Return: {
                const auto& ret = std::get<ReturnTerminator>(term.data);
                if (isVoid(function.returnType)) {
                    if (ret.value.has_value() || !isVoid(ret.valueType)) {
                        diagnostics.error(term.location, "vir-verify: void function '" + function.name + "' must return without a value");
                    }
                } else {
                    if (!ret.value.has_value()) {
                        diagnostics.error(term.location, "vir-verify: non-void function '" + function.name + "' must return a value");
                    } else {
                        const Type returnedType = requireValue(*ret.value, term.location, "return");
                        if (returnedType != ret.valueType || ret.valueType != function.returnType) {
                            diagnostics.error(term.location, "vir-verify: return type mismatch in function '" + function.name + "'");
                        }
                    }
                }
                break;
            }
            case Terminator::Kind::Branch: {
                const auto& br = std::get<BranchTerminator>(term.data);
                if (blocksById.find(br.target.index) == blocksById.end()) {
                    diagnostics.error(term.location, "vir-verify: function '" + function.name + "' branches to unknown block id " +
                        std::to_string(br.target.index));
                }
                break;
            }
            case Terminator::Kind::CondBranch: {
                const auto& br = std::get<CondBranchTerminator>(term.data);
                const Type conditionType = requireValue(br.condition, term.location, "conditional branch");
                if (conditionType.kind != TypeKind::Bool) {
                    diagnostics.error(term.location, "vir-verify: conditional branch in function '" + function.name + "' requires bool condition");
                }
                if (blocksById.find(br.trueBlock.index) == blocksById.end()) {
                    diagnostics.error(term.location, "vir-verify: function '" + function.name + "' conditional branch true-target is unknown block id " +
                        std::to_string(br.trueBlock.index));
                }
                if (blocksById.find(br.falseBlock.index) == blocksById.end()) {
                    diagnostics.error(term.location, "vir-verify: function '" + function.name + "' conditional branch false-target is unknown block id " +
                        std::to_string(br.falseBlock.index));
                }
                break;
            }
            case Terminator::Kind::Unreachable:
                break;
        }
    }
}

std::unordered_map<std::uint32_t, bool> collectBooleanConstants(const Function& function) {
    std::unordered_map<std::uint32_t, bool> constants;
    for (const auto& block : function.blocks) {
        for (const auto& instruction : block.instructions) {
            if (instruction.kind != Instruction::Kind::Constant) {
                continue;
            }
            const auto& inst = std::get<ConstantInst>(instruction.data);
            if (inst.constant.type.kind == TypeKind::Bool && std::holds_alternative<bool>(inst.constant.value)) {
                constants[inst.result.index] = std::get<bool>(inst.constant.value);
            }
        }
    }
    return constants;
}

std::unordered_set<std::uint32_t> collectReachableBlockIds(const Function& function) {
    std::unordered_set<std::uint32_t> reachable;
    if (function.blocks.empty()) {
        return reachable;
    }

    std::unordered_map<std::uint32_t, const BasicBlock*> blocksById;
    for (const auto& block : function.blocks) {
        blocksById.emplace(block.id.index, &block);
    }

    std::stack<std::uint32_t> worklist;
    worklist.push(function.blocks.front().id.index);

    while (!worklist.empty()) {
        const std::uint32_t id = worklist.top();
        worklist.pop();
        if (!reachable.insert(id).second) {
            continue;
        }

        const auto blockIt = blocksById.find(id);
        if (blockIt == blocksById.end() || !blockIt->second->terminator.has_value()) {
            continue;
        }

        const Terminator& term = *blockIt->second->terminator;
        if (term.kind == Terminator::Kind::Branch) {
            const auto& branch = std::get<BranchTerminator>(term.data);
            worklist.push(branch.target.index);
        } else if (term.kind == Terminator::Kind::CondBranch) {
            const auto& branch = std::get<CondBranchTerminator>(term.data);
            worklist.push(branch.trueBlock.index);
            worklist.push(branch.falseBlock.index);
        }
    }

    return reachable;
}

} // namespace

DiagnosticBag verifyModule(const Module& module) {
    DiagnosticBag diagnostics;

    std::unordered_set<std::string> normalizedImports;
    for (const auto& importDecl : module.imports) {
        if (importDecl.path.empty()) {
            diagnostics.error({}, "vir-verify: import path cannot be empty");
            continue;
        }
        const std::string normalized = normalizePath(importDecl.path);
        if (!normalizedImports.insert(normalized).second) {
            diagnostics.error({}, "vir-verify: duplicate import '" + importDecl.path + "'");
        }
    }

    std::unordered_map<std::string, FunctionSignature> signatures;
    for (const auto& function : module.functions) {
        if (signatures.find(function.name) != signatures.end()) {
            diagnostics.error({}, "vir-verify: duplicate function declaration '" + function.name + "'");
            continue;
        }

        FunctionSignature signature;
        signature.returnType = function.returnType;
        signature.isExtern = false;
        signature.location = {};
        signature.paramTypes.reserve(function.params.size());
        for (const auto& param : function.params) {
            signature.paramTypes.push_back(param.type);
        }
        signatures.emplace(function.name, std::move(signature));
    }

    for (const auto& externFunction : module.externFunctions) {
        if (signatures.find(externFunction.name) != signatures.end()) {
            diagnostics.error(externFunction.location, "vir-verify: duplicate function declaration '" + externFunction.name + "'");
            continue;
        }
        if (externFunction.importPath.empty()) {
            diagnostics.error(externFunction.location, "vir-verify: extern function '" + externFunction.name + "' has empty import path");
        } else if (normalizedImports.find(normalizePath(externFunction.importPath)) == normalizedImports.end()) {
            diagnostics.error(externFunction.location,
                "vir-verify: extern function '" + externFunction.name + "' references non-imported path '" + externFunction.importPath + "'");
        }

        FunctionSignature signature;
        signature.returnType = externFunction.returnType;
        signature.isExtern = true;
        signature.importPath = externFunction.importPath;
        signature.location = externFunction.location;
        signature.paramTypes.reserve(externFunction.params.size());
        for (const auto& param : externFunction.params) {
            signature.paramTypes.push_back(param.type);
        }
        signatures.emplace(externFunction.name, std::move(signature));
    }

    for (const auto& function : module.functions) {
        verifyFunction(function, signatures, diagnostics);
    }

    return diagnostics;
}

OptimizationSummary optimizeModule(Module& module) {
    OptimizationSummary summary;

    for (auto& function : module.functions) {
        const auto boolConstants = collectBooleanConstants(function);
        for (auto& block : function.blocks) {
            if (!block.terminator.has_value()) {
                continue;
            }
            Terminator& term = *block.terminator;
            if (term.kind != Terminator::Kind::CondBranch) {
                continue;
            }

            const auto condBranch = std::get<CondBranchTerminator>(term.data);
            const auto constantIt = boolConstants.find(condBranch.condition.index);
            if (constantIt == boolConstants.end()) {
                continue;
            }

            const BlockId target = constantIt->second ? condBranch.trueBlock : condBranch.falseBlock;
            term.kind = Terminator::Kind::Branch;
            term.data = BranchTerminator{target};
            ++summary.foldedConstantBranches;
        }

        const std::unordered_set<std::uint32_t> reachable = collectReachableBlockIds(function);
        if (reachable.size() == function.blocks.size()) {
            continue;
        }

        std::vector<BasicBlock> kept;
        kept.reserve(reachable.size());
        for (auto& block : function.blocks) {
            if (reachable.find(block.id.index) != reachable.end()) {
                kept.push_back(std::move(block));
            }
        }

        summary.removedUnreachableBlocks += function.blocks.size() - kept.size();
        function.blocks = std::move(kept);
    }

    return summary;
}

} // namespace vir
