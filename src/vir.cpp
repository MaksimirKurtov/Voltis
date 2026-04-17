#include "vir.h"
#include <iomanip>
#include <limits>
#include <locale>
#include <sstream>
#include <stdexcept>
#include <utility>

namespace vir {
namespace {

std::string escapeString(const std::string& value) {
    std::string out;
    out.reserve(value.size());
    for (char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out.push_back(c); break;
        }
    }
    return out;
}

std::string formatFloat32(float value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<float>::max_digits10) << value << "f";
    return out.str();
}

std::string formatFloat64(double value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    return out.str();
}

std::string toString(UnaryOp op) {
    switch (op) {
        case UnaryOp::Negate: return "neg";
        case UnaryOp::LogicalNot: return "not";
    }
    throw std::runtime_error("unknown unary op");
}

std::string toString(BinaryOp op) {
    switch (op) {
        case BinaryOp::Add: return "add";
        case BinaryOp::Subtract: return "sub";
        case BinaryOp::Multiply: return "mul";
        case BinaryOp::Divide: return "div";
        case BinaryOp::Equal: return "eq";
        case BinaryOp::NotEqual: return "ne";
        case BinaryOp::Less: return "lt";
        case BinaryOp::LessEqual: return "le";
        case BinaryOp::Greater: return "gt";
        case BinaryOp::GreaterEqual: return "ge";
        case BinaryOp::LogicalAnd: return "and";
        case BinaryOp::LogicalOr: return "or";
    }
    throw std::runtime_error("unknown binary op");
}

std::string toString(ConversionKind kind) {
    switch (kind) {
        case ConversionKind::ToString: return "to_string";
        case ConversionKind::ToInt32Identity: return "to_int32_identity";
        case ConversionKind::ToInt32FromFloatTruncateTowardZero: return "to_int32_trunc";
        case ConversionKind::ToInt32FromStringParse: return "to_int32_parse";
        case ConversionKind::ToInt32FromBool: return "to_int32_from_bool";
        case ConversionKind::ToFloat32: return "to_float32";
        case ConversionKind::ToFloat64: return "to_float64";
        case ConversionKind::ToBool: return "to_bool";
        case ConversionKind::Round: return "round";
        case ConversionKind::Floor: return "floor";
        case ConversionKind::Ceil: return "ceil";
        case ConversionKind::ImplicitInt32ToFloat32: return "implicit_i32_to_f32";
        case ConversionKind::ImplicitInt32ToFloat64: return "implicit_i32_to_f64";
        case ConversionKind::ImplicitFloat32ToFloat64: return "implicit_f32_to_f64";
    }
    throw std::runtime_error("unknown conversion kind");
}

std::string valueName(ValueId id) {
    return "%v" + std::to_string(id.index);
}

std::string localName(LocalId id) {
    return "%l" + std::to_string(id.index);
}

std::string blockName(BlockId id) {
    return "bb" + std::to_string(id.index);
}

void dumpInstruction(std::ostringstream& out, const Instruction& instruction, int indentLevel) {
    const std::string indent(static_cast<std::size_t>(indentLevel) * 2, ' ');
    switch (instruction.kind) {
        case Instruction::Kind::Constant: {
            const auto& inst = std::get<ConstantInst>(instruction.data);
            out << indent << valueName(inst.result) << " = const " << toString(inst.constant.type) << " ";
            if (std::holds_alternative<std::int32_t>(inst.constant.value)) {
                out << std::get<std::int32_t>(inst.constant.value);
            } else if (std::holds_alternative<float>(inst.constant.value)) {
                out << formatFloat32(std::get<float>(inst.constant.value));
            } else if (std::holds_alternative<double>(inst.constant.value)) {
                out << formatFloat64(std::get<double>(inst.constant.value));
            } else if (std::holds_alternative<bool>(inst.constant.value)) {
                out << (std::get<bool>(inst.constant.value) ? "true" : "false");
            } else {
                out << "\"" << escapeString(std::get<std::string>(inst.constant.value)) << "\"";
            }
            out << "\n";
            return;
        }
        case Instruction::Kind::LoadLocal: {
            const auto& inst = std::get<LoadLocalInst>(instruction.data);
            out << indent << valueName(inst.result) << " = load " << localName(inst.local)
                << " : " << toString(inst.type) << "\n";
            return;
        }
        case Instruction::Kind::StoreLocal: {
            const auto& inst = std::get<StoreLocalInst>(instruction.data);
            out << indent << "store " << localName(inst.local) << ", " << valueName(inst.value)
                << " : " << toString(inst.valueType) << "\n";
            return;
        }
        case Instruction::Kind::Unary: {
            const auto& inst = std::get<UnaryInst>(instruction.data);
            out << indent << valueName(inst.result) << " = " << toString(inst.op) << " "
                << valueName(inst.operand) << " : " << toString(inst.type) << "\n";
            return;
        }
        case Instruction::Kind::Binary: {
            const auto& inst = std::get<BinaryInst>(instruction.data);
            out << indent << valueName(inst.result) << " = " << toString(inst.op) << " "
                << valueName(inst.left) << ", " << valueName(inst.right)
                << " : " << toString(inst.type) << "\n";
            return;
        }
        case Instruction::Kind::Convert: {
            const auto& inst = std::get<ConvertInst>(instruction.data);
            out << indent << valueName(inst.result) << " = convert " << valueName(inst.input)
                << " : " << toString(inst.fromType) << " -> " << toString(inst.toType)
                << " [" << toString(inst.kind) << "]\n";
            return;
        }
        case Instruction::Kind::Call: {
            const auto& inst = std::get<CallInst>(instruction.data);
            out << indent;
            if (inst.result.has_value()) {
                out << valueName(*inst.result) << " = ";
            }
            out << "call " << inst.callee;
            if (inst.isBuiltin) {
                out << " [builtin]";
            }
            out << "(";
            for (std::size_t i = 0; i < inst.args.size(); ++i) {
                if (i > 0) {
                    out << ", ";
                }
                out << valueName(inst.args[i]) << " : " << toString(inst.argTypes[i]);
            }
            out << ")";
            if (!isVoid(inst.returnType)) {
                out << " -> " << toString(inst.returnType);
            }
            out << "\n";
            return;
        }
    }
}

void dumpTerminator(std::ostringstream& out, const Terminator& terminator, int indentLevel) {
    const std::string indent(static_cast<std::size_t>(indentLevel) * 2, ' ');
    switch (terminator.kind) {
        case Terminator::Kind::Return: {
            const auto& term = std::get<ReturnTerminator>(terminator.data);
            out << indent << "ret";
            if (term.value.has_value()) {
                out << " " << valueName(*term.value) << " : " << toString(term.valueType);
            }
            out << "\n";
            return;
        }
        case Terminator::Kind::Branch: {
            const auto& term = std::get<BranchTerminator>(terminator.data);
            out << indent << "br " << blockName(term.target) << "\n";
            return;
        }
        case Terminator::Kind::CondBranch: {
            const auto& term = std::get<CondBranchTerminator>(terminator.data);
            out << indent << "br_if " << valueName(term.condition) << ", "
                << blockName(term.trueBlock) << ", " << blockName(term.falseBlock) << "\n";
            return;
        }
        case Terminator::Kind::Unreachable:
            out << indent << "unreachable\n";
            return;
    }
}

} // namespace

std::string toString(Type type) {
    switch (type.kind) {
        case TypeKind::Void: return "void";
        case TypeKind::Int32: return "int32";
        case TypeKind::Float32: return "float32";
        case TypeKind::Float64: return "float64";
        case TypeKind::Bool: return "bool";
        case TypeKind::String: return "string";
    }
    throw std::runtime_error("unknown type kind");
}

std::string dump(const Module& module) {
    std::ostringstream out;
    out << "module {\n";
    for (const auto& function : module.functions) {
        out << "  fn " << function.name << "(";
        for (std::size_t i = 0; i < function.params.size(); ++i) {
            if (i > 0) {
                out << ", ";
            }
            out << function.params[i].name << ":" << toString(function.params[i].type);
        }
        out << ") -> " << toString(function.returnType) << " {\n";

        out << "    locals:\n";
        for (const auto& local : function.locals) {
            out << "      " << localName(local.id) << " " << local.name << ":" << toString(local.type);
            if (local.isParameter && local.parameterIndex.has_value()) {
                out << " [param#" << *local.parameterIndex << "]";
            }
            out << "\n";
        }

        out << "    blocks:\n";
        for (const auto& block : function.blocks) {
            out << "      " << blockName(block.id) << " (" << block.name << "):\n";
            for (const auto& instruction : block.instructions) {
                dumpInstruction(out, instruction, 4);
            }
            if (block.terminator.has_value()) {
                dumpTerminator(out, *block.terminator, 4);
            } else {
                out << "        <no-terminator>\n";
            }
        }

        out << "  }\n";
    }
    out << "}\n";
    return out.str();
}

} // namespace vir
