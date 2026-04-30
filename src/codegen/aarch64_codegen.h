#pragma once

#include "codegen/code_generator_base.h"

class AArch64CodeGenerator final : public CodeGeneratorBase {
public:
    std::vector<std::uint8_t> generateText(const vir::Module& module) override;
};
