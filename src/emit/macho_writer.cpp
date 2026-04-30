#include "emit/macho_writer.h"

#include "executable_format_emit_utils.h"

#include <array>
#include <cstring>

namespace {

void appendU32(std::vector<std::uint8_t>& out, std::uint32_t v) {
    out.push_back(static_cast<std::uint8_t>(v));
    out.push_back(static_cast<std::uint8_t>(v >> 8));
    out.push_back(static_cast<std::uint8_t>(v >> 16));
    out.push_back(static_cast<std::uint8_t>(v >> 24));
}

void appendU64(std::vector<std::uint8_t>& out, std::uint64_t v) {
    appendU32(out, static_cast<std::uint32_t>(v & 0xFFFFFFFFu));
    appendU32(out, static_cast<std::uint32_t>(v >> 32));
}

std::size_t alignUp(std::size_t value, std::size_t align) {
    const std::size_t rem = value % align;
    return rem == 0 ? value : value + (align - rem);
}

} // namespace

std::vector<std::uint8_t> MachOWriter::writeExecutable(const NativeProgramImage& image, bool arm64) const {
    constexpr std::uint32_t MH_MAGIC_64 = 0xFEEDFACF;
    constexpr std::uint32_t MH_EXECUTE = 2;
    constexpr std::uint32_t MH_PIE = 0x00200000;
    constexpr std::uint32_t LC_SEGMENT_64 = 0x19;
    constexpr std::uint32_t LC_MAIN = 0x80000028;

    const std::uint32_t cpuType = arm64 ? 0x0100000C : 0x01000007;
    const std::uint32_t cpuSubtype = arm64 ? 0 : 3;
    const std::size_t segmentCmdSize = 72 + 80;
    const std::size_t entryCmdSize = 24;
    const std::size_t sizeofcmds = segmentCmdSize + entryCmdSize;
    const std::size_t headerSize = 32 + sizeofcmds;
    const std::size_t codeOffset = alignUp(headerSize, 0x1000);

    std::vector<std::uint8_t> out;
    out.reserve(codeOffset + image.textBytes.size());

    appendU32(out, MH_MAGIC_64);
    appendU32(out, cpuType);
    appendU32(out, cpuSubtype);
    appendU32(out, MH_EXECUTE);
    appendU32(out, 2);
    appendU32(out, static_cast<std::uint32_t>(sizeofcmds));
    appendU32(out, pie_ ? MH_PIE : 0);
    appendU32(out, 0);

    appendU32(out, LC_SEGMENT_64);
    appendU32(out, static_cast<std::uint32_t>(segmentCmdSize));
    std::array<char, 16> seg{};
    std::memcpy(seg.data(), "__TEXT", 6);
    out.insert(out.end(), seg.begin(), seg.end());
    appendU64(out, 0x100000000ULL);
    appendU64(out, alignUp(image.textBytes.size(), 0x1000));
    appendU64(out, 0);
    appendU64(out, alignUp(image.textBytes.size(), 0x1000));
    appendU32(out, 7);
    appendU32(out, 5);
    appendU32(out, 1);
    appendU32(out, 0);

    std::array<char, 16> sec{};
    std::memcpy(sec.data(), "__text", 6);
    out.insert(out.end(), sec.begin(), sec.end());
    out.insert(out.end(), seg.begin(), seg.end());
    appendU64(out, 0x100000000ULL);
    appendU64(out, image.textBytes.size());
    appendU32(out, static_cast<std::uint32_t>(codeOffset));
    appendU32(out, 4);
    appendU32(out, 0);
    appendU32(out, 0);
    appendU32(out, 0x80000400);
    appendU32(out, 0);
    appendU32(out, 0);
    appendU32(out, 0);

    appendU32(out, LC_MAIN);
    appendU32(out, static_cast<std::uint32_t>(entryCmdSize));
    appendU64(out, image.entryOffset);
    appendU64(out, 0);

    out.resize(codeOffset, 0);
    out.insert(out.end(), image.textBytes.begin(), image.textBytes.end());
    return out;
}
