#include "backend_llvm_ir.h"
#include <cctype>
#include <cstdint>
#include <iomanip>
#include <limits>
#include <locale>
#include <memory>
#include <sstream>
#include <unordered_map>
#include <unordered_set>
#include <utility>

namespace {

struct EmittedValue {
    vir::Type type;
    std::string operand;
};

struct FunctionState {
    std::unordered_map<std::uint32_t, EmittedValue> values;
    std::unordered_map<std::uint32_t, std::string> localPointers;
    std::uint32_t tempCounter = 0;
};

std::string sanitizeIdentifier(const std::string& name) {
    if (name.empty()) {
        return "unnamed";
    }

    std::string out;
    out.reserve(name.size());
    for (char ch : name) {
        const unsigned char value = static_cast<unsigned char>(ch);
        if (std::isalnum(value) || ch == '_' || ch == '.') {
            out.push_back(ch);
        } else {
            out.push_back('_');
        }
    }

    if (!out.empty() && std::isdigit(static_cast<unsigned char>(out.front()))) {
        out.insert(out.begin(), '_');
    }

    return out;
}

std::string valueName(vir::ValueId id) {
    return "%v" + std::to_string(id.index);
}

std::string localName(vir::LocalId id) {
    return "%l" + std::to_string(id.index);
}

std::string blockName(vir::BlockId id) {
    return "bb" + std::to_string(id.index);
}

std::string llvmType(vir::Type type) {
    switch (type.kind) {
        case vir::TypeKind::Void: return "void";
        case vir::TypeKind::Int32: return "i32";
        case vir::TypeKind::Float32: return "float";
        case vir::TypeKind::Float64: return "double";
        case vir::TypeKind::Bool: return "i1";
        case vir::TypeKind::String: return "i8*";
    }
    return "void";
}

std::string llvmPointerType(vir::Type type) {
    return llvmType(type) + "*";
}

std::string defaultValueLiteral(vir::Type type) {
    switch (type.kind) {
        case vir::TypeKind::Void: return "";
        case vir::TypeKind::Int32: return "0";
        case vir::TypeKind::Float32: return "0.0";
        case vir::TypeKind::Float64: return "0.0";
        case vir::TypeKind::Bool: return "false";
        case vir::TypeKind::String: return "null";
    }
    return "undef";
}

std::string formatFloat32(float value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<float>::max_digits10) << value;
    return out.str();
}

std::string formatFloat64(double value) {
    std::ostringstream out;
    out.imbue(std::locale::classic());
    out << std::setprecision(std::numeric_limits<double>::max_digits10) << value;
    return out.str();
}

std::string escapeLlvmByte(unsigned char value) {
    std::ostringstream out;
    out << "\\" << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(value);
    return out.str();
}

std::string encodeLlvmCString(const std::string& value) {
    std::ostringstream out;
    for (unsigned char byte : value) {
        if (byte >= 0x20 && byte <= 0x7E && byte != '\\' && byte != '"') {
            out << static_cast<char>(byte);
        } else {
            out << escapeLlvmByte(byte);
        }
    }
    out << escapeLlvmByte(0);
    return out.str();
}

class LlvmIrTextEmitter {
public:
    explicit LlvmIrTextEmitter(const BackendOptions& options) : options_(options) {}

    BackendResult emit(const vir::Module& module) {
        BackendResult result;
        if (options_.output != BackendOutputKind::LlvmIrText) {
            diagnostics_.error({}, "backend-llvm: only LLVM IR text emission is currently implemented");
        }

        externFunctions_.clear();
        for (const auto& externFunction : module.externFunctions) {
            externFunctions_[externFunction.name] = externFunction;
            requireExternDeclaration(externFunction.name, externFunction.params, externFunction.returnType, externFunction.location);
        }

        for (const auto& function : module.functions) {
            functionBodies_.push_back(emitFunction(function));
        }

        std::ostringstream ir;
        ir << "; Voltis LLVM backend output\n";
        ir << "; track: " << (options_.track == BackendTrack::ProductionDirected ? "production-directed" : "temporary-scaffolding") << "\n";
        ir << "source_filename = \"" << options_.moduleName << "\"\n";
        ir << "target triple = \"" << options_.targetTriple << "\"\n";
        if (options_.targetDataLayout.has_value() && !options_.targetDataLayout->empty()) {
            ir << "target datalayout = \"" << *options_.targetDataLayout << "\"\n";
        }
        ir << "\n";

        for (const auto& global : stringGlobals_) {
            ir << global << "\n";
        }
        if (!stringGlobals_.empty()) {
            ir << "\n";
        }

        for (const auto& declaration : declarations_) {
            ir << declaration << "\n";
        }
        if (!declarations_.empty()) {
            ir << "\n";
        }

        for (const auto& body : functionBodies_) {
            ir << body;
            if (!body.empty() && body.back() != '\n') {
                ir << "\n";
            }
            ir << "\n";
        }

        result.artifacts.push_back(BackendArtifact{
            BackendOutputKind::LlvmIrText,
            options_.moduleName + ".ll",
            ir.str(),
            options_.track == BackendTrack::TemporaryScaffolding
        });
        result.diagnostics = diagnostics_;
        return result;
    }

private:
    BackendOptions options_;
    DiagnosticBag diagnostics_;
    std::vector<std::string> stringGlobals_;
    std::unordered_map<std::string, std::string> stringGlobalSymbolByValue_;
    std::vector<std::string> declarations_;
    std::unordered_set<std::string> declarationSet_;
    std::unordered_map<std::string, vir::ExternFunctionDecl> externFunctions_;
    std::vector<std::string> functionBodies_;
    std::uint32_t nextStringId_ = 0;

    std::string functionSymbol(const std::string& name) const {
        return "@" + sanitizeIdentifier(name);
    }

    void requireDeclaration(const std::string& declaration) {
        if (declarationSet_.insert(declaration).second) {
            declarations_.push_back(declaration);
        }
    }

    void requireExternDeclaration(
        const std::string& name,
        const std::vector<vir::Parameter>& params,
        vir::Type returnType,
        const SourceLocation&) {
        std::ostringstream declaration;
        declaration << "declare " << llvmType(returnType) << " " << functionSymbol(name) << "(";
        for (std::size_t i = 0; i < params.size(); ++i) {
            if (i > 0) {
                declaration << ", ";
            }
            declaration << llvmType(params[i].type);
        }
        declaration << ")";
        requireDeclaration(declaration.str());
    }

    EmittedValue lookupValue(
        const FunctionState& state,
        vir::ValueId id,
        vir::Type fallbackType,
        const SourceLocation& location,
        const std::string& context) {
        const auto it = state.values.find(id.index);
        if (it != state.values.end()) {
            return it->second;
        }
        diagnostics_.error(location, "backend-llvm: unknown SSA value in " + context);
        return EmittedValue{fallbackType, "undef"};
    }

    std::string lookupLocalPointer(const FunctionState& state, vir::LocalId id, const SourceLocation& location) {
        const auto it = state.localPointers.find(id.index);
        if (it != state.localPointers.end()) {
            return it->second;
        }
        diagnostics_.error(location, "backend-llvm: unknown local slot");
        return "null";
    }

    std::string createTempName(FunctionState& state) {
        return "%tmp" + std::to_string(state.tempCounter++);
    }

    std::string internStringConstant(const std::string& value) {
        const auto existing = stringGlobalSymbolByValue_.find(value);
        if (existing != stringGlobalSymbolByValue_.end()) {
            return existing->second;
        }

        const std::string symbol = "@.str." + std::to_string(nextStringId_++);
        const std::size_t byteCount = value.size() + 1;
        std::ostringstream global;
        global << symbol << " = private unnamed_addr constant [" << byteCount << " x i8] c\""
               << encodeLlvmCString(value) << "\"";
        stringGlobals_.push_back(global.str());
        stringGlobalSymbolByValue_[value] = symbol;
        return symbol;
    }

    void emitBuiltinPrint(
        std::ostringstream& out,
        FunctionState& state,
        const vir::CallInst& inst,
        const SourceLocation& location) {
        for (std::size_t i = 0; i < inst.args.size(); ++i) {
            const vir::Type argType = i < inst.argTypes.size() ? inst.argTypes[i] : vir::Type{vir::TypeKind::Void};
            const EmittedValue arg = lookupValue(state, inst.args[i], argType, location, "builtin print");
            switch (argType.kind) {
                case vir::TypeKind::Int32:
                    requireDeclaration("declare void @vt_print_i32(i32)");
                    out << "  call void @vt_print_i32(i32 " << arg.operand << ")\n";
                    break;
                case vir::TypeKind::Float32:
                    requireDeclaration("declare void @vt_print_f32(float)");
                    out << "  call void @vt_print_f32(float " << arg.operand << ")\n";
                    break;
                case vir::TypeKind::Float64:
                    requireDeclaration("declare void @vt_print_f64(double)");
                    out << "  call void @vt_print_f64(double " << arg.operand << ")\n";
                    break;
                case vir::TypeKind::Bool:
                    requireDeclaration("declare void @vt_print_bool(i1)");
                    out << "  call void @vt_print_bool(i1 " << arg.operand << ")\n";
                    break;
                case vir::TypeKind::String:
                    requireDeclaration("declare void @vt_print_str(i8*)");
                    out << "  call void @vt_print_str(i8* " << arg.operand << ")\n";
                    break;
                default:
                    diagnostics_.error(location, "backend-llvm: unsupported print argument type");
                    break;
            }
        }
    }

    void emitConstant(
        std::ostringstream& out,
        FunctionState& state,
        const vir::ConstantInst& inst,
        const SourceLocation& location) {
        (void)out;
        EmittedValue emitted{inst.constant.type, "undef"};

        switch (inst.constant.type.kind) {
            case vir::TypeKind::Int32:
                if (std::holds_alternative<std::int32_t>(inst.constant.value)) {
                    emitted.operand = std::to_string(std::get<std::int32_t>(inst.constant.value));
                } else {
                    diagnostics_.error(location, "backend-llvm: int32 constant has invalid payload");
                }
                break;
            case vir::TypeKind::Float32:
                if (std::holds_alternative<float>(inst.constant.value)) {
                    emitted.operand = formatFloat32(std::get<float>(inst.constant.value));
                } else {
                    diagnostics_.error(location, "backend-llvm: float32 constant has invalid payload");
                }
                break;
            case vir::TypeKind::Float64:
                if (std::holds_alternative<double>(inst.constant.value)) {
                    emitted.operand = formatFloat64(std::get<double>(inst.constant.value));
                } else {
                    diagnostics_.error(location, "backend-llvm: float64 constant has invalid payload");
                }
                break;
            case vir::TypeKind::Bool:
                if (std::holds_alternative<bool>(inst.constant.value)) {
                    emitted.operand = std::get<bool>(inst.constant.value) ? "true" : "false";
                } else {
                    diagnostics_.error(location, "backend-llvm: bool constant has invalid payload");
                }
                break;
            case vir::TypeKind::String:
                if (std::holds_alternative<std::string>(inst.constant.value)) {
                    const std::string symbol = internStringConstant(std::get<std::string>(inst.constant.value));
                    const std::size_t byteCount = std::get<std::string>(inst.constant.value).size() + 1;
                    emitted.operand = "getelementptr inbounds ([" + std::to_string(byteCount) + " x i8], ["
                        + std::to_string(byteCount) + " x i8]* " + symbol + ", i64 0, i64 0)";
                } else {
                    diagnostics_.error(location, "backend-llvm: string constant has invalid payload");
                }
                break;
            default:
                diagnostics_.error(location, "backend-llvm: unsupported constant type");
                break;
        }

        state.values[inst.result.index] = std::move(emitted);
    }

    void emitLoadLocal(
        std::ostringstream& out,
        FunctionState& state,
        const vir::LoadLocalInst& inst,
        const SourceLocation& location) {
        const std::string ptr = lookupLocalPointer(state, inst.local, location);
        const std::string resultName = valueName(inst.result);
        out << "  " << resultName << " = load " << llvmType(inst.type) << ", "
            << llvmPointerType(inst.type) << " " << ptr << "\n";
        state.values[inst.result.index] = EmittedValue{inst.type, resultName};
    }

    void emitStoreLocal(
        std::ostringstream& out,
        FunctionState& state,
        const vir::StoreLocalInst& inst,
        const SourceLocation& location) {
        const EmittedValue input = lookupValue(state, inst.value, inst.valueType, location, "store");
        const std::string ptr = lookupLocalPointer(state, inst.local, location);
        out << "  store " << llvmType(inst.valueType) << " " << input.operand << ", "
            << llvmPointerType(inst.valueType) << " " << ptr << "\n";
    }

    void emitUnary(
        std::ostringstream& out,
        FunctionState& state,
        const vir::UnaryInst& inst,
        const SourceLocation& location) {
        const EmittedValue operand = lookupValue(state, inst.operand, inst.type, location, "unary");
        const std::string resultName = valueName(inst.result);

        if (inst.op == vir::UnaryOp::Negate) {
            switch (inst.type.kind) {
                case vir::TypeKind::Int32:
                    out << "  " << resultName << " = sub i32 0, " << operand.operand << "\n";
                    break;
                case vir::TypeKind::Float32:
                    out << "  " << resultName << " = fneg float " << operand.operand << "\n";
                    break;
                case vir::TypeKind::Float64:
                    out << "  " << resultName << " = fneg double " << operand.operand << "\n";
                    break;
                default:
                    diagnostics_.error(location, "backend-llvm: negate is only supported for numeric types");
                    state.values[inst.result.index] = EmittedValue{inst.type, "undef"};
                    return;
            }
            state.values[inst.result.index] = EmittedValue{inst.type, resultName};
            return;
        }

        if (inst.op == vir::UnaryOp::LogicalNot) {
            out << "  " << resultName << " = xor i1 " << operand.operand << ", true\n";
            state.values[inst.result.index] = EmittedValue{inst.type, resultName};
            return;
        }

        diagnostics_.error(location, "backend-llvm: unsupported unary operation");
        state.values[inst.result.index] = EmittedValue{inst.type, "undef"};
    }

    void emitBinary(
        std::ostringstream& out,
        FunctionState& state,
        const vir::BinaryInst& inst,
        const SourceLocation& location) {
        const EmittedValue left = lookupValue(state, inst.left, inst.type, location, "binary left operand");
        const EmittedValue right = lookupValue(state, inst.right, inst.type, location, "binary right operand");
        const std::string resultName = valueName(inst.result);
        const vir::Type operandType = left.type;

        const auto emitNumericArithmetic = [&](const char* intOp, const char* floatOp) {
            if (operandType.kind == vir::TypeKind::Int32) {
                out << "  " << resultName << " = " << intOp << " i32 " << left.operand << ", " << right.operand << "\n";
                return true;
            }
            if (operandType.kind == vir::TypeKind::Float32) {
                out << "  " << resultName << " = " << floatOp << " float " << left.operand << ", " << right.operand << "\n";
                return true;
            }
            if (operandType.kind == vir::TypeKind::Float64) {
                out << "  " << resultName << " = " << floatOp << " double " << left.operand << ", " << right.operand << "\n";
                return true;
            }
            return false;
        };

        const auto emitCmp = [&](const char* intCmp, const char* floatCmp) {
            if (operandType.kind == vir::TypeKind::Int32 || operandType.kind == vir::TypeKind::Bool) {
                out << "  " << resultName << " = icmp " << intCmp << " " << llvmType(operandType)
                    << " " << left.operand << ", " << right.operand << "\n";
                return true;
            }
            if (operandType.kind == vir::TypeKind::Float32 || operandType.kind == vir::TypeKind::Float64) {
                out << "  " << resultName << " = fcmp " << floatCmp << " " << llvmType(operandType)
                    << " " << left.operand << ", " << right.operand << "\n";
                return true;
            }
            return false;
        };

        switch (inst.op) {
            case vir::BinaryOp::Add:
                if (inst.type.kind == vir::TypeKind::String) {
                    requireDeclaration("declare i8* @vt_str_concat(i8*, i8*)");
                    out << "  " << resultName << " = call i8* @vt_str_concat(i8* "
                        << left.operand << ", i8* " << right.operand << ")\n";
                } else if (!emitNumericArithmetic("add", "fadd")) {
                    diagnostics_.error(location, "backend-llvm: add is unsupported for this type");
                    state.values[inst.result.index] = EmittedValue{inst.type, "undef"};
                    return;
                }
                break;
            case vir::BinaryOp::Subtract:
                if (!emitNumericArithmetic("sub", "fsub")) {
                    diagnostics_.error(location, "backend-llvm: subtract is unsupported for this type");
                    state.values[inst.result.index] = EmittedValue{inst.type, "undef"};
                    return;
                }
                break;
            case vir::BinaryOp::Multiply:
                if (!emitNumericArithmetic("mul", "fmul")) {
                    diagnostics_.error(location, "backend-llvm: multiply is unsupported for this type");
                    state.values[inst.result.index] = EmittedValue{inst.type, "undef"};
                    return;
                }
                break;
            case vir::BinaryOp::Divide:
                if (!emitNumericArithmetic("sdiv", "fdiv")) {
                    diagnostics_.error(location, "backend-llvm: divide is unsupported for this type");
                    state.values[inst.result.index] = EmittedValue{inst.type, "undef"};
                    return;
                }
                break;
            case vir::BinaryOp::Equal:
            case vir::BinaryOp::NotEqual:
                if (operandType.kind == vir::TypeKind::String) {
                    requireDeclaration("declare i1 @vt_str_eq(i8*, i8*)");
                    if (inst.op == vir::BinaryOp::Equal) {
                        out << "  " << resultName << " = call i1 @vt_str_eq(i8* "
                            << left.operand << ", i8* " << right.operand << ")\n";
                    } else {
                        const std::string eqTemp = createTempName(state);
                        out << "  " << eqTemp << " = call i1 @vt_str_eq(i8* "
                            << left.operand << ", i8* " << right.operand << ")\n";
                        out << "  " << resultName << " = xor i1 " << eqTemp << ", true\n";
                    }
                } else if (!emitCmp(inst.op == vir::BinaryOp::Equal ? "eq" : "ne",
                    inst.op == vir::BinaryOp::Equal ? "oeq" : "one")) {
                    diagnostics_.error(location, "backend-llvm: equality compare is unsupported for this type");
                    state.values[inst.result.index] = EmittedValue{inst.type, "undef"};
                    return;
                }
                break;
            case vir::BinaryOp::Less:
                if (!emitCmp("slt", "olt")) {
                    diagnostics_.error(location, "backend-llvm: less-than compare is unsupported for this type");
                    state.values[inst.result.index] = EmittedValue{inst.type, "undef"};
                    return;
                }
                break;
            case vir::BinaryOp::LessEqual:
                if (!emitCmp("sle", "ole")) {
                    diagnostics_.error(location, "backend-llvm: less-equal compare is unsupported for this type");
                    state.values[inst.result.index] = EmittedValue{inst.type, "undef"};
                    return;
                }
                break;
            case vir::BinaryOp::Greater:
                if (!emitCmp("sgt", "ogt")) {
                    diagnostics_.error(location, "backend-llvm: greater-than compare is unsupported for this type");
                    state.values[inst.result.index] = EmittedValue{inst.type, "undef"};
                    return;
                }
                break;
            case vir::BinaryOp::GreaterEqual:
                if (!emitCmp("sge", "oge")) {
                    diagnostics_.error(location, "backend-llvm: greater-equal compare is unsupported for this type");
                    state.values[inst.result.index] = EmittedValue{inst.type, "undef"};
                    return;
                }
                break;
            case vir::BinaryOp::LogicalAnd:
                out << "  " << resultName << " = and i1 " << left.operand << ", " << right.operand << "\n";
                break;
            case vir::BinaryOp::LogicalOr:
                out << "  " << resultName << " = or i1 " << left.operand << ", " << right.operand << "\n";
                break;
        }

        state.values[inst.result.index] = EmittedValue{inst.type, resultName};
    }

    void emitConvert(
        std::ostringstream& out,
        FunctionState& state,
        const vir::ConvertInst& inst,
        const SourceLocation& location) {
        const EmittedValue input = lookupValue(state, inst.input, inst.fromType, location, "conversion");
        const std::string resultName = valueName(inst.result);

        const auto emitRuntimeCall = [&](const std::string& declaration, const std::string& callee) {
            requireDeclaration(declaration);
            out << "  " << resultName << " = call " << llvmType(inst.toType) << " " << callee
                << "(" << llvmType(inst.fromType) << " " << input.operand << ")\n";
            state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
        };

        switch (inst.kind) {
            case vir::ConversionKind::ToInt32Identity:
                state.values[inst.result.index] = EmittedValue{inst.toType, input.operand};
                return;
            case vir::ConversionKind::ToInt32FromFloatTruncateTowardZero:
                out << "  " << resultName << " = fptosi " << llvmType(inst.fromType)
                    << " " << input.operand << " to i32\n";
                state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                return;
            case vir::ConversionKind::ToInt32FromStringParse:
                emitRuntimeCall("declare i32 @vt_str_to_i32(i8*)", "@vt_str_to_i32");
                return;
            case vir::ConversionKind::ToInt32FromBool:
                out << "  " << resultName << " = zext i1 " << input.operand << " to i32\n";
                state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                return;
            case vir::ConversionKind::ToFloat32:
                if (inst.fromType.kind == vir::TypeKind::Int32) {
                    out << "  " << resultName << " = sitofp i32 " << input.operand << " to float\n";
                    state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                    return;
                }
                if (inst.fromType.kind == vir::TypeKind::Float64) {
                    out << "  " << resultName << " = fptrunc double " << input.operand << " to float\n";
                    state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                    return;
                }
                if (inst.fromType.kind == vir::TypeKind::Bool) {
                    out << "  " << resultName << " = uitofp i1 " << input.operand << " to float\n";
                    state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                    return;
                }
                if (inst.fromType.kind == vir::TypeKind::Float32) {
                    state.values[inst.result.index] = EmittedValue{inst.toType, input.operand};
                    return;
                }
                emitRuntimeCall("declare float @vt_str_to_f32(i8*)", "@vt_str_to_f32");
                return;
            case vir::ConversionKind::ToFloat64:
                if (inst.fromType.kind == vir::TypeKind::Int32) {
                    out << "  " << resultName << " = sitofp i32 " << input.operand << " to double\n";
                    state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                    return;
                }
                if (inst.fromType.kind == vir::TypeKind::Float32) {
                    out << "  " << resultName << " = fpext float " << input.operand << " to double\n";
                    state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                    return;
                }
                if (inst.fromType.kind == vir::TypeKind::Bool) {
                    out << "  " << resultName << " = uitofp i1 " << input.operand << " to double\n";
                    state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                    return;
                }
                if (inst.fromType.kind == vir::TypeKind::Float64) {
                    state.values[inst.result.index] = EmittedValue{inst.toType, input.operand};
                    return;
                }
                emitRuntimeCall("declare double @vt_str_to_f64(i8*)", "@vt_str_to_f64");
                return;
            case vir::ConversionKind::ToBool:
                if (inst.fromType.kind == vir::TypeKind::Bool) {
                    state.values[inst.result.index] = EmittedValue{inst.toType, input.operand};
                    return;
                }
                if (inst.fromType.kind == vir::TypeKind::Int32) {
                    out << "  " << resultName << " = icmp ne i32 " << input.operand << ", 0\n";
                    state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                    return;
                }
                if (inst.fromType.kind == vir::TypeKind::Float32) {
                    out << "  " << resultName << " = fcmp une float " << input.operand << ", 0.0\n";
                    state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                    return;
                }
                if (inst.fromType.kind == vir::TypeKind::Float64) {
                    out << "  " << resultName << " = fcmp une double " << input.operand << ", 0.0\n";
                    state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                    return;
                }
                emitRuntimeCall("declare i1 @vt_str_to_bool(i8*)", "@vt_str_to_bool");
                return;
            case vir::ConversionKind::ToString:
                switch (inst.fromType.kind) {
                    case vir::TypeKind::String:
                        state.values[inst.result.index] = EmittedValue{inst.toType, input.operand};
                        return;
                    case vir::TypeKind::Int32:
                        emitRuntimeCall("declare i8* @vt_to_string_i32(i32)", "@vt_to_string_i32");
                        return;
                    case vir::TypeKind::Float32:
                        emitRuntimeCall("declare i8* @vt_to_string_f32(float)", "@vt_to_string_f32");
                        return;
                    case vir::TypeKind::Float64:
                        emitRuntimeCall("declare i8* @vt_to_string_f64(double)", "@vt_to_string_f64");
                        return;
                    case vir::TypeKind::Bool:
                        emitRuntimeCall("declare i8* @vt_to_string_bool(i1)", "@vt_to_string_bool");
                        return;
                    default:
                        break;
                }
                break;
            case vir::ConversionKind::Round:
                if (inst.fromType.kind == vir::TypeKind::Float32) {
                    requireDeclaration("declare float @llvm.round.f32(float)");
                    out << "  " << resultName << " = call float @llvm.round.f32(float " << input.operand << ")\n";
                    state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                    return;
                }
                if (inst.fromType.kind == vir::TypeKind::Float64) {
                    requireDeclaration("declare double @llvm.round.f64(double)");
                    out << "  " << resultName << " = call double @llvm.round.f64(double " << input.operand << ")\n";
                    state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                    return;
                }
                break;
            case vir::ConversionKind::Floor:
                if (inst.fromType.kind == vir::TypeKind::Float32) {
                    requireDeclaration("declare float @llvm.floor.f32(float)");
                    out << "  " << resultName << " = call float @llvm.floor.f32(float " << input.operand << ")\n";
                    state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                    return;
                }
                if (inst.fromType.kind == vir::TypeKind::Float64) {
                    requireDeclaration("declare double @llvm.floor.f64(double)");
                    out << "  " << resultName << " = call double @llvm.floor.f64(double " << input.operand << ")\n";
                    state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                    return;
                }
                break;
            case vir::ConversionKind::Ceil:
                if (inst.fromType.kind == vir::TypeKind::Float32) {
                    requireDeclaration("declare float @llvm.ceil.f32(float)");
                    out << "  " << resultName << " = call float @llvm.ceil.f32(float " << input.operand << ")\n";
                    state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                    return;
                }
                if (inst.fromType.kind == vir::TypeKind::Float64) {
                    requireDeclaration("declare double @llvm.ceil.f64(double)");
                    out << "  " << resultName << " = call double @llvm.ceil.f64(double " << input.operand << ")\n";
                    state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                    return;
                }
                break;
            case vir::ConversionKind::ImplicitInt32ToFloat32:
                out << "  " << resultName << " = sitofp i32 " << input.operand << " to float\n";
                state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                return;
            case vir::ConversionKind::ImplicitInt32ToFloat64:
                out << "  " << resultName << " = sitofp i32 " << input.operand << " to double\n";
                state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                return;
            case vir::ConversionKind::ImplicitFloat32ToFloat64:
                out << "  " << resultName << " = fpext float " << input.operand << " to double\n";
                state.values[inst.result.index] = EmittedValue{inst.toType, resultName};
                return;
        }

        diagnostics_.error(location, "backend-llvm: unsupported conversion kind for current type pair");
        state.values[inst.result.index] = EmittedValue{inst.toType, "undef"};
    }

    void emitCall(
        std::ostringstream& out,
        FunctionState& state,
        const vir::CallInst& inst,
        const SourceLocation& location) {
        if (inst.isBuiltin && inst.callee == "print") {
            emitBuiltinPrint(out, state, inst, location);
            return;
        }

        std::ostringstream args;
        for (std::size_t i = 0; i < inst.args.size(); ++i) {
            const vir::Type argType = i < inst.argTypes.size() ? inst.argTypes[i] : vir::Type{vir::TypeKind::Void};
            const EmittedValue arg = lookupValue(state, inst.args[i], argType, location, "call argument");
            if (i > 0) {
                args << ", ";
            }
            args << llvmType(argType) << " " << arg.operand;
        }

        const std::string callee = functionSymbol(inst.callee);
        if (inst.isExtern) {
            const auto externIt = externFunctions_.find(inst.callee);
            if (externIt != externFunctions_.end()) {
                requireExternDeclaration(externIt->second.name, externIt->second.params, externIt->second.returnType, location);
            } else {
                std::vector<vir::Parameter> inferredParams;
                inferredParams.reserve(inst.argTypes.size());
                for (std::size_t i = 0; i < inst.argTypes.size(); ++i) {
                    inferredParams.push_back(vir::Parameter{
                        "arg" + std::to_string(i),
                        inst.argTypes[i],
                        location,
                    });
                }
                requireExternDeclaration(inst.callee, inferredParams, inst.returnType, location);
            }
        }

        if (vir::isVoid(inst.returnType)) {
            out << "  call void " << callee << "(" << args.str() << ")\n";
            return;
        }

        if (inst.result.has_value()) {
            const std::string resultName = valueName(*inst.result);
            out << "  " << resultName << " = call " << llvmType(inst.returnType) << " "
                << callee << "(" << args.str() << ")\n";
            state.values[inst.result->index] = EmittedValue{inst.returnType, resultName};
            return;
        }

        diagnostics_.error(location, "backend-llvm: call returns a value but VIR call did not provide result id");
        out << "  call " << llvmType(inst.returnType) << " " << callee << "(" << args.str() << ")\n";
    }

    void emitInstruction(
        std::ostringstream& out,
        FunctionState& state,
        const vir::Instruction& instruction) {
        switch (instruction.kind) {
            case vir::Instruction::Kind::Constant:
                emitConstant(out, state, std::get<vir::ConstantInst>(instruction.data), instruction.location);
                return;
            case vir::Instruction::Kind::LoadLocal:
                emitLoadLocal(out, state, std::get<vir::LoadLocalInst>(instruction.data), instruction.location);
                return;
            case vir::Instruction::Kind::StoreLocal:
                emitStoreLocal(out, state, std::get<vir::StoreLocalInst>(instruction.data), instruction.location);
                return;
            case vir::Instruction::Kind::Unary:
                emitUnary(out, state, std::get<vir::UnaryInst>(instruction.data), instruction.location);
                return;
            case vir::Instruction::Kind::Binary:
                emitBinary(out, state, std::get<vir::BinaryInst>(instruction.data), instruction.location);
                return;
            case vir::Instruction::Kind::Convert:
                emitConvert(out, state, std::get<vir::ConvertInst>(instruction.data), instruction.location);
                return;
            case vir::Instruction::Kind::Call:
                emitCall(out, state, std::get<vir::CallInst>(instruction.data), instruction.location);
                return;
        }
    }

    void emitTerminator(
        std::ostringstream& out,
        FunctionState& state,
        const vir::Terminator& terminator) {
        switch (terminator.kind) {
            case vir::Terminator::Kind::Return: {
                const auto& term = std::get<vir::ReturnTerminator>(terminator.data);
                if (term.value.has_value()) {
                    const EmittedValue value = lookupValue(
                        state,
                        *term.value,
                        term.valueType,
                        terminator.location,
                        "return value");
                    out << "  ret " << llvmType(term.valueType) << " " << value.operand << "\n";
                } else {
                    out << "  ret void\n";
                }
                return;
            }
            case vir::Terminator::Kind::Branch: {
                const auto& term = std::get<vir::BranchTerminator>(terminator.data);
                out << "  br label %" << blockName(term.target) << "\n";
                return;
            }
            case vir::Terminator::Kind::CondBranch: {
                const auto& term = std::get<vir::CondBranchTerminator>(terminator.data);
                const EmittedValue condition = lookupValue(
                    state,
                    term.condition,
                    vir::Type{vir::TypeKind::Bool},
                    terminator.location,
                    "conditional branch");
                out << "  br i1 " << condition.operand
                    << ", label %" << blockName(term.trueBlock)
                    << ", label %" << blockName(term.falseBlock) << "\n";
                return;
            }
            case vir::Terminator::Kind::Unreachable:
                out << "  unreachable\n";
                return;
        }
    }

    std::string emitFunction(const vir::Function& function) {
        std::ostringstream out;
        FunctionState state;

        out << "define " << llvmType(function.returnType) << " "
            << functionSymbol(function.name) << "(";
        for (std::size_t i = 0; i < function.params.size(); ++i) {
            if (i > 0) {
                out << ", ";
            }
            out << llvmType(function.params[i].type) << " %arg" << i;
        }
        out << ") {\n";

        if (function.blocks.empty()) {
            diagnostics_.error({}, "backend-llvm: function '" + function.name + "' has no basic blocks");
            if (vir::isVoid(function.returnType)) {
                out << "  ret void\n";
            } else {
                out << "  ret " << llvmType(function.returnType) << " " << defaultValueLiteral(function.returnType) << "\n";
            }
            out << "}";
            return out.str();
        }

        out << "entry.alloca:\n";
        for (const auto& local : function.locals) {
            const std::string ptrName = localName(local.id);
            state.localPointers[local.id.index] = ptrName;
            out << "  " << ptrName << " = alloca " << llvmType(local.type) << "\n";
        }
        for (const auto& local : function.locals) {
            if (!local.isParameter || !local.parameterIndex.has_value()) {
                continue;
            }
            const std::size_t index = *local.parameterIndex;
            if (index >= function.params.size()) {
                diagnostics_.error(local.location, "backend-llvm: parameter index out of range for local '" + local.name + "'");
                continue;
            }
            const std::string ptrName = state.localPointers[local.id.index];
            out << "  store " << llvmType(local.type) << " %arg" << index << ", "
                << llvmPointerType(local.type) << " " << ptrName << "\n";
        }
        out << "  br label %" << blockName(function.blocks.front().id) << "\n\n";

        for (const auto& block : function.blocks) {
            out << blockName(block.id) << ":\n";
            for (const auto& instruction : block.instructions) {
                emitInstruction(out, state, instruction);
            }
            if (block.terminator.has_value()) {
                emitTerminator(out, state, *block.terminator);
            } else {
                diagnostics_.error({}, "backend-llvm: block '" + block.name + "' has no terminator");
                out << "  unreachable\n";
            }
            out << "\n";
        }

        out << "}";
        return out.str();
    }
};

} // namespace

const char* LlvmIrTextBackend::id() const {
    return "llvm-ir-text";
}

BackendFlavor LlvmIrTextBackend::flavor() const {
    return BackendFlavor::Llvm;
}

bool LlvmIrTextBackend::supportsOutput(BackendOutputKind output) const {
    return output == BackendOutputKind::LlvmIrText;
}

BackendResult LlvmIrTextBackend::compile(const vir::Module& module, const BackendOptions& options) const {
    LlvmIrTextEmitter emitter(options);
    return emitter.emit(module);
}

std::unique_ptr<IBackend> createLlvmIrTextBackend() {
    return std::make_unique<LlvmIrTextBackend>();
}
