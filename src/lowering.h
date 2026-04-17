#pragma once

#include "ast.h"
#include "diagnostics.h"
#include "sema.h"
#include "vir.h"
#include <unordered_map>

struct LoweringResult {
    vir::Module module;
    DiagnosticBag diagnostics;

    bool ok() const { return !diagnostics.hasErrors(); }
};

class VIRLowerer {
public:
    LoweringResult lower(
        const Program& program,
        const std::unordered_map<const Expr*, std::string>& expressionTypes,
        const std::unordered_map<const Expr*, SemanticAnalyzer::ConversionInfo>& conversionInfos) const;

    LoweringResult lower(const Program& program, const SemanticAnalyzer& sema) const;
};
