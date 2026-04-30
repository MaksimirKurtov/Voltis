#pragma once

#include "codegen/code_generator_base.h"

class X64CodeGenerator final : public CodeGeneratorBase {
public:
    std::vector<std::uint8_t> generateText(const vir::Module& module) override;
};
