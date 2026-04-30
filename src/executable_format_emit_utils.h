#pragma once

#include "native_image.h"
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
std::optional<NativeProgramImage> extractTextImageFromNativeImage(const NativeImage& image);
std::string emitExecutableForFormat(const NativeProgramImage& image,
                                    BinaryFormat format,
                                    TargetArch arch);
