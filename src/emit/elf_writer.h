#pragma once

#include <cstdint>
#include <string>
#include <vector>

struct NativeProgramImage;

class ELFWriter {
public:
    struct Relocation {
        std::uint64_t offset = 0;
        std::uint64_t symbolIndex = 0;
        std::uint32_t type = 0;
        std::int64_t addend = 0;
    };

    explicit ELFWriter(bool pie = true) : pie_(pie) {}
    std::vector<std::uint8_t> writeExecutable(const NativeProgramImage& image,
                                              const std::vector<std::uint8_t>& data = {},
                                              const std::vector<std::uint8_t>& rodata = {},
                                              const std::vector<Relocation>& relaText = {}) const;

private:
    bool pie_ = true;
};
