#include "codegen/aarch64_codegen.h"

#include <stdexcept>

std::vector<std::uint8_t> AArch64CodeGenerator::generateText(const vir::Module&) {
    throw std::runtime_error("AArch64CodeGenerator is experimental scaffold only");
}
