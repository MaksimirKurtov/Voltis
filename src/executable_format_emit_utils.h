#pragma once

#include "target.h"

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

struct NativeProgramImage {
    std::vector<std::uint8_t> textBytes;
    std::uint64_t entryOffset = 0;
};

std::optional<NativeProgramImage> extractTextImageFromPe(const std::vector<std::uint8_t>& peBytes);
std::string emitExecutableForFormat(const NativeProgramImage& image,
                                    BinaryFormat format,
                                    TargetArch arch);
