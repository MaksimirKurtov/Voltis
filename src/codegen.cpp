#include "codegen.h"
#include <sstream>
#include <stdexcept>

std::string CodeGenerator::indent(int level) const {
    return std::string(static_cast<std::size_t>(level) * 4, ' ');
}

std::string CodeGenerator::mapType(const std::string& type) const {
    if (type == "int32") return "std::int32_t";
    if (type == "float32") return "float";
    if (type == "float64") return "double";
    if (type == "string") return "std::string";
    if (type == "bool") return "bool";
    if (type == "void") return "void";
    throw std::runtime_error("Unsupported type: " + type);
}

std::string CodeGenerator::escapeString(const std::string& value) const {
    std::string out;
    for (char c : value) {
        switch (c) {
            case '\\': out += "\\\\"; break;
            case '"': out += "\\\""; break;
            case '\n': out += "\\n"; break;
            case '\r': out += "\\r"; break;
            case '\t': out += "\\t"; break;
            default: out += c; break;
        }
    }
    return out;
}

std::string CodeGenerator::inferCppType(const Expr* expr) {
    if (auto lit = dynamic_cast<const LiteralExpr*>(expr)) {
        switch (lit->kind) {
            case LiteralExpr::Kind::String: return "std::string";
            case LiteralExpr::Kind::Bool: return "bool";
            case LiteralExpr::Kind::Number:
                return (lit->value.find('.') != std::string::npos || (!lit->value.empty() && lit->value.back() == 'f'))
                    ? (lit->value.back() == 'f' ? "float" : "double")
                    : "std::int32_t";
        }
    }
    return "auto";
}

std::string CodeGenerator::genExpr(const Expr* expr) {
    if (auto lit = dynamic_cast<const LiteralExpr*>(expr)) {
        switch (lit->kind) {
            case LiteralExpr::Kind::Number:
                return lit->value;
            case LiteralExpr::Kind::String:
                return "std::string(\"" + escapeString(lit->value) + "\")";
            case LiteralExpr::Kind::Bool:
                return lit->value;
        }
    }
    if (auto var = dynamic_cast<const VariableExpr*>(expr)) {
        return var->name;
    }
    if (auto unary = dynamic_cast<const UnaryExpr*>(expr)) {
        std::string op = unary->op;
        if (op == "not") op = "!";
        return "(" + op + genExpr(unary->right.get()) + ")";
    }
    if (auto bin = dynamic_cast<const BinaryExpr*>(expr)) {
        std::string op = bin->op;
        if (op == "and") op = "&&";
        if (op == "or") op = "||";
        return "(" + genExpr(bin->left.get()) + " " + op + " " + genExpr(bin->right.get()) + ")";
    }
    if (auto call = dynamic_cast<const CallExpr*>(expr)) {
        std::ostringstream out;
        out << genExpr(call->callee.get()) << "(";
        for (std::size_t i = 0; i < call->args.size(); ++i) {
            if (i > 0) out << ", ";
            out << genExpr(call->args[i].get());
        }
        out << ")";
        return out.str();
    }
    if (auto member = dynamic_cast<const MemberCallExpr*>(expr)) {
        std::string object = genExpr(member->object.get());
        if (member->method == "ToString") return "vt::ToString(" + object + ")";
        if (member->method == "ToInt32") return "vt::ToInt32(" + object + ")";
        if (member->method == "ToFloat32") return "vt::ToFloat32(" + object + ")";
        if (member->method == "ToFloat64") return "vt::ToFloat64(" + object + ")";
        if (member->method == "ToBool") return "vt::ToBool(" + object + ")";
        if (member->method == "Round") return "vt::Round(" + object + ")";
        if (member->method == "Floor") return "vt::Floor(" + object + ")";
        if (member->method == "Ceil") return "vt::Ceil(" + object + ")";
        throw std::runtime_error("Unsupported member call: ." + member->method + "()");
    }
    throw std::runtime_error("Unsupported expression node");
}

std::string CodeGenerator::genStmt(const Stmt* stmt, int level) {
    std::ostringstream out;
    if (auto exprStmt = dynamic_cast<const ExprStmt*>(stmt)) {
        if (auto call = dynamic_cast<const CallExpr*>(exprStmt->expr.get())) {
            if (auto callee = dynamic_cast<const VariableExpr*>(call->callee.get())) {
                if (callee->name == "print") {
                    if (call->args.size() != 1) {
                        throw std::runtime_error("print() expects exactly one argument");
                    }
                    out << indent(level) << "vt::Print(" << genExpr(call->args[0].get()) << ");\n";
                    return out.str();
                }
            }
        }
        out << indent(level) << genExpr(exprStmt->expr.get()) << ";\n";
        return out.str();
    }
    if (auto ret = dynamic_cast<const ReturnStmt*>(stmt)) {
        out << indent(level) << "return " << genExpr(ret->expr.get()) << ";\n";
        return out.str();
    }
    if (auto decl = dynamic_cast<const VarDeclStmt*>(stmt)) {
        std::string type = decl->isVarInference ? inferCppType(decl->init.get()) : mapType(decl->type);
        out << indent(level) << type << " " << decl->name << " = " << genExpr(decl->init.get()) << ";\n";
        return out.str();
    }
    if (auto assign = dynamic_cast<const AssignStmt*>(stmt)) {
        out << indent(level) << assign->name << " = " << genExpr(assign->value.get()) << ";\n";
        return out.str();
    }
    if (auto block = dynamic_cast<const BlockStmt*>(stmt)) {
        out << indent(level) << "{\n";
        for (const auto& child : block->statements) {
            out << genStmt(child.get(), level + 1);
        }
        out << indent(level) << "}\n";
        return out.str();
    }
    if (auto ifStmt = dynamic_cast<const IfStmt*>(stmt)) {
        out << indent(level) << "if (" << genExpr(ifStmt->condition.get()) << ") ";
        out << genStmt(ifStmt->thenBlock.get(), level);
        if (ifStmt->elseBlock) {
            out << indent(level) << "else ";
            out << genStmt(ifStmt->elseBlock.get(), level);
        }
        return out.str();
    }
    throw std::runtime_error("Unsupported statement node");
}

std::string CodeGenerator::generate(const Program& program) {
    std::ostringstream out;
    out << "#include <cstdint>\n";
    out << "#include <cmath>\n";
    out << "#include <iostream>\n";
    out << "#include <stdexcept>\n";
    out << "#include <string>\n\n";
    out << "namespace vt {\n";
    out << "    inline void Print(const std::string& value) { std::cout << value << std::endl; }\n";
    out << "    inline void Print(const char* value) { std::cout << value << std::endl; }\n";
    out << "    inline void Print(std::int32_t value) { std::cout << value << std::endl; }\n";
    out << "    inline void Print(float value) { std::cout << value << std::endl; }\n";
    out << "    inline void Print(double value) { std::cout << value << std::endl; }\n";
    out << "    inline void Print(bool value) { std::cout << (value ? \"true\" : \"false\") << std::endl; }\n";
    out << "    inline std::string ToString(const std::string& value) { return value; }\n";
    out << "    inline std::string ToString(const char* value) { return std::string(value); }\n";
    out << "    inline std::string ToString(std::int32_t value) { return std::to_string(value); }\n";
    out << "    inline std::string ToString(float value) { return std::to_string(value); }\n";
    out << "    inline std::string ToString(double value) { return std::to_string(value); }\n";
    out << "    inline std::string ToString(bool value) { return value ? \"true\" : \"false\"; }\n";
    out << "    inline std::int32_t ToInt32(const std::string& value) { return std::stoi(value); }\n";
    out << "    inline std::int32_t ToInt32(float value) { return static_cast<std::int32_t>(value); }\n";
    out << "    inline std::int32_t ToInt32(double value) { return static_cast<std::int32_t>(value); }\n";
    out << "    inline std::int32_t ToInt32(bool value) { return value ? 1 : 0; }\n";
    out << "    inline float ToFloat32(const std::string& value) { return std::stof(value); }\n";
    out << "    inline float ToFloat32(std::int32_t value) { return static_cast<float>(value); }\n";
    out << "    inline float ToFloat32(double value) { return static_cast<float>(value); }\n";
    out << "    inline double ToFloat64(const std::string& value) { return std::stod(value); }\n";
    out << "    inline double ToFloat64(std::int32_t value) { return static_cast<double>(value); }\n";
    out << "    inline double ToFloat64(float value) { return static_cast<double>(value); }\n";
    out << "    inline bool ToBool(const std::string& value) { if (value == \"true\") return true; if (value == \"false\") return false; throw std::runtime_error(\"Invalid bool conversion\"); }\n";
    out << "    inline bool ToBool(std::int32_t value) { return value != 0; }\n";
    out << "    inline bool ToBool(float value) { return value != 0.0f; }\n";
    out << "    inline bool ToBool(double value) { return value != 0.0; }\n";
    out << "    inline double Round(double value) { return std::round(value); }\n";
    out << "    inline double Floor(double value) { return std::floor(value); }\n";
    out << "    inline double Ceil(double value) { return std::ceil(value); }\n";
    out << "    inline float Round(float value) { return std::round(value); }\n";
    out << "    inline float Floor(float value) { return std::floor(value); }\n";
    out << "    inline float Ceil(float value) { return std::ceil(value); }\n";
    out << "}\n\n";

    for (const auto& fn : program.functions) {
        out << mapType(fn.returnType) << " " << fn.name << "(";
        for (std::size_t i = 0; i < fn.params.size(); ++i) {
            if (i > 0) out << ", ";
            out << mapType(fn.params[i].type) << " " << fn.params[i].name;
        }
        out << ")\n";
        out << genStmt(fn.body.get(), 0) << "\n";
    }

    return out.str();
}
