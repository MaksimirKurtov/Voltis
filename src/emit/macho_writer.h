#pragma once

#include <cstdint>
#include <vector>

struct NativeProgramImage;

class MachOWriter {
public:
    explicit MachOWriter(bool pie = true) : pie_(pie) {}
    std::vector<std::uint8_t> writeExecutable(const NativeProgramImage& image, bool arm64) const;

private:
    bool pie_ = true;
};
