#pragma once

#include <cstdint>
#include <filesystem>
#include <string>
#include <vector>

void safeWriteFile(const std::filesystem::path& outPath, const std::vector<std::uint8_t>& data);
inline void safeWriteFile(const std::filesystem::path& outPath, const std::string& data) {
    safeWriteFile(outPath, std::vector<std::uint8_t>(data.begin(), data.end()));
}
