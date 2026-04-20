#pragma once

#include "diagnostics.h"
#include "vir.h"
#include <cstddef>

namespace vir {

struct OptimizationSummary {
    std::size_t foldedConstantBranches = 0;
    std::size_t removedUnreachableBlocks = 0;
};

DiagnosticBag verifyModule(const Module& module);
OptimizationSummary optimizeModule(Module& module);

} // namespace vir
