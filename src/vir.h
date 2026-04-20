#pragma once

#include "source_location.h"
#include <cstdint>
#include <optional>
#include <string>
#include <variant>
#include <vector>

namespace vir {

enum class TypeKind {
    Void,
    Int32,
    Float32,
    Float64,
    Bool,
    String
};

struct Type {
    TypeKind kind = TypeKind::Void;
};

inline bool operator==(Type lhs, Type rhs) { return lhs.kind == rhs.kind; }
inline bool operator!=(Type lhs, Type rhs) { return !(lhs == rhs); }

inline bool isVoid(Type type) { return type.kind == TypeKind::Void; }

struct ValueId {
    std::uint32_t index = 0;
};

struct LocalId {
    std::uint32_t index = 0;
};

struct BlockId {
    std::uint32_t index = 0;
};

struct Constant {
    Type type;
    std::variant<std::int32_t, float, double, bool, std::string> value;
};

enum class UnaryOp {
    Negate,
    LogicalNot
};

enum class BinaryOp {
    Add,
    Subtract,
    Multiply,
    Divide,
    Equal,
    NotEqual,
    Less,
    LessEqual,
    Greater,
    GreaterEqual,
    LogicalAnd,
    LogicalOr
};

enum class ConversionKind {
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
    Ceil,
    ImplicitInt32ToFloat32,
    ImplicitInt32ToFloat64,
    ImplicitFloat32ToFloat64
};

struct ConstantInst {
    ValueId result;
    Constant constant;
};

struct LoadLocalInst {
    ValueId result;
    LocalId local;
    Type type;
};

struct StoreLocalInst {
    LocalId local;
    ValueId value;
    Type valueType;
};

struct UnaryInst {
    ValueId result;
    UnaryOp op;
    ValueId operand;
    Type type;
};

struct BinaryInst {
    ValueId result;
    BinaryOp op;
    ValueId left;
    ValueId right;
    Type type;
};

struct ConvertInst {
    ValueId result;
    ValueId input;
    Type fromType;
    Type toType;
    ConversionKind kind;
};

struct CallInst {
    std::optional<ValueId> result;
    std::string callee;
    std::vector<ValueId> args;
    std::vector<Type> argTypes;
    Type returnType;
    bool isBuiltin = false;
    bool isExtern = false;
    std::string importPath;
};

struct Instruction {
    enum class Kind {
        Constant,
        LoadLocal,
        StoreLocal,
        Unary,
        Binary,
        Convert,
        Call
    };

    Kind kind = Kind::Constant;
    SourceLocation location;
    std::variant<ConstantInst, LoadLocalInst, StoreLocalInst, UnaryInst, BinaryInst, ConvertInst, CallInst> data;
};

struct ReturnTerminator {
    std::optional<ValueId> value;
    Type valueType;
};

struct BranchTerminator {
    BlockId target;
};

struct CondBranchTerminator {
    ValueId condition;
    BlockId trueBlock;
    BlockId falseBlock;
};

struct UnreachableTerminator {};

struct Terminator {
    enum class Kind {
        Return,
        Branch,
        CondBranch,
        Unreachable
    };

    Kind kind = Kind::Unreachable;
    SourceLocation location;
    std::variant<ReturnTerminator, BranchTerminator, CondBranchTerminator, UnreachableTerminator> data;
};

struct BasicBlock {
    BlockId id;
    std::string name;
    std::vector<Instruction> instructions;
    std::optional<Terminator> terminator;
};

struct Parameter {
    std::string name;
    Type type;
    SourceLocation location;
};

struct Local {
    LocalId id;
    std::string name;
    Type type;
    bool isParameter = false;
    std::optional<std::size_t> parameterIndex;
    SourceLocation location;
};

struct Function {
    std::string name;
    Type returnType;
    std::vector<Parameter> params;
    std::vector<Local> locals;
    std::vector<BasicBlock> blocks;
};

struct ImportDecl {
    std::string path;
};

struct ExternFunctionDecl {
    std::string name;
    std::string importPath;
    Type returnType;
    std::vector<Parameter> params;
    SourceLocation location;
};

struct Module {
    std::vector<ImportDecl> imports;
    std::vector<ExternFunctionDecl> externFunctions;
    std::vector<Function> functions;
};

std::string toString(Type type);
std::string dump(const Module& module);

} // namespace vir
