#pragma once

#include "vir.h"

#include <cstdint>
#include <vector>

class CodeGeneratorBase {
public:
    virtual ~CodeGeneratorBase() = default;
    virtual std::vector<std::uint8_t> generateText(const vir::Module& module) = 0;
};
