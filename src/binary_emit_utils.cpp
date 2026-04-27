#include "binary_emit_utils.h"

#include <algorithm>
#include <iomanip>
#include <sstream>

namespace {

std::string toHexByte(std::uint8_t value) {
    std::ostringstream out;
    out << std::uppercase << std::hex << std::setw(2) << std::setfill('0') << static_cast<int>(value);
    return out.str();
}

std::uint8_t checksumIntelHex(std::uint8_t length, std::uint16_t addr, std::uint8_t recordType, const std::vector<std::uint8_t>& data) {
    std::uint32_t sum = length + static_cast<std::uint8_t>((addr >> 8) & 0xFF) + static_cast<std::uint8_t>(addr & 0xFF) + recordType;
    for (std::uint8_t byte : data) {
        sum += byte;
    }
    return static_cast<std::uint8_t>((~sum + 1) & 0xFF);
}

std::uint8_t checksumSrec(const std::vector<std::uint8_t>& bytes) {
    std::uint32_t sum = 0;
    for (std::uint8_t value : bytes) {
        sum += value;
    }
    return static_cast<std::uint8_t>(~sum & 0xFF);
}

} // namespace

std::string bytesToRawBinaryString(const std::vector<std::uint8_t>& bytes) {
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}

std::string bytesToIntelHex(const std::vector<std::uint8_t>& bytes, std::uint32_t baseAddress) {
    std::ostringstream out;
    constexpr std::size_t kRecordSize = 16;
    std::uint32_t address = baseAddress;

    std::uint16_t currentHigh = static_cast<std::uint16_t>((address >> 16) & 0xFFFF);
    {
        const std::vector<std::uint8_t> extData = {
            static_cast<std::uint8_t>((currentHigh >> 8) & 0xFF),
            static_cast<std::uint8_t>(currentHigh & 0xFF)
        };
        const std::uint8_t chk = checksumIntelHex(2, 0, 0x04, extData);
        out << ":02000004" << toHexByte(extData[0]) << toHexByte(extData[1]) << toHexByte(chk) << "\n";
    }

    for (std::size_t offset = 0; offset < bytes.size(); offset += kRecordSize) {
        const std::size_t count = std::min(kRecordSize, bytes.size() - offset);
        const std::uint32_t absoluteAddr = address + static_cast<std::uint32_t>(offset);
        const std::uint16_t high = static_cast<std::uint16_t>((absoluteAddr >> 16) & 0xFFFF);
        if (high != currentHigh) {
            currentHigh = high;
            const std::vector<std::uint8_t> extData = {
                static_cast<std::uint8_t>((currentHigh >> 8) & 0xFF),
                static_cast<std::uint8_t>(currentHigh & 0xFF)
            };
            const std::uint8_t chk = checksumIntelHex(2, 0, 0x04, extData);
            out << ":02000004" << toHexByte(extData[0]) << toHexByte(extData[1]) << toHexByte(chk) << "\n";
        }

        std::vector<std::uint8_t> recordData(bytes.begin() + static_cast<std::ptrdiff_t>(offset),
                                             bytes.begin() + static_cast<std::ptrdiff_t>(offset + count));
        const std::uint16_t lowAddr = static_cast<std::uint16_t>(absoluteAddr & 0xFFFF);
        const std::uint8_t chk = checksumIntelHex(static_cast<std::uint8_t>(count), lowAddr, 0x00, recordData);
        out << ":" << toHexByte(static_cast<std::uint8_t>(count))
            << toHexByte(static_cast<std::uint8_t>((lowAddr >> 8) & 0xFF))
            << toHexByte(static_cast<std::uint8_t>(lowAddr & 0xFF))
            << "00";
        for (const auto byte : recordData) {
            out << toHexByte(byte);
        }
        out << toHexByte(chk) << "\n";
    }

    out << ":00000001FF\n";
    return out.str();
}

std::string bytesToSRecord(const std::vector<std::uint8_t>& bytes, std::uint32_t baseAddress) {
    std::ostringstream out;
    constexpr std::size_t kRecordSize = 16;

    for (std::size_t offset = 0; offset < bytes.size(); offset += kRecordSize) {
        const std::size_t count = std::min(kRecordSize, bytes.size() - offset);
        const std::uint32_t address = baseAddress + static_cast<std::uint32_t>(offset);
        std::vector<std::uint8_t> body;
        body.reserve(1 + 4 + count + 1);
        const std::uint8_t byteCount = static_cast<std::uint8_t>(4 + count + 1);
        body.push_back(byteCount);
        body.push_back(static_cast<std::uint8_t>((address >> 24) & 0xFF));
        body.push_back(static_cast<std::uint8_t>((address >> 16) & 0xFF));
        body.push_back(static_cast<std::uint8_t>((address >> 8) & 0xFF));
        body.push_back(static_cast<std::uint8_t>(address & 0xFF));
        for (std::size_t i = 0; i < count; ++i) {
            body.push_back(bytes[offset + i]);
        }
        body.push_back(checksumSrec(body));

        out << "S3";
        for (std::uint8_t value : body) {
            out << toHexByte(value);
        }
        out << "\n";
    }

    out << "S70500000000FA\n";
    return out.str();
}

