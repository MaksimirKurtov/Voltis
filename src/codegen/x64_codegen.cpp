#include "codegen/x64_codegen.h"

#include <stdexcept>

std::vector<std::uint8_t> X64CodeGenerator::generateText(const vir::Module&) {
    throw std::runtime_error("X64CodeGenerator is not yet wired into backend dispatch");
}
