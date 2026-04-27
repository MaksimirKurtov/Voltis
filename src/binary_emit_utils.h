#pragma once

#include <cstdint>
#include <string>
#include <vector>

std::string bytesToRawBinaryString(const std::vector<std::uint8_t>& bytes);
std::string bytesToIntelHex(const std::vector<std::uint8_t>& bytes, std::uint32_t baseAddress = 0);
std::string bytesToSRecord(const std::vector<std::uint8_t>& bytes, std::uint32_t baseAddress = 0);

