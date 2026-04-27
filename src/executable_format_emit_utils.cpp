#include "executable_format_emit_utils.h"

#include <algorithm>
#include <array>
#include <cstring>
#include <stdexcept>

namespace {

std::uint16_t readU16(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (offset + 2 > bytes.size()) {
        throw std::runtime_error("Truncated binary while reading u16");
    }
    return static_cast<std::uint16_t>(bytes[offset]) |
           (static_cast<std::uint16_t>(bytes[offset + 1]) << 8);
}

std::uint32_t readU32(const std::vector<std::uint8_t>& bytes, std::size_t offset) {
    if (offset + 4 > bytes.size()) {
        throw std::runtime_error("Truncated binary while reading u32");
    }
    return static_cast<std::uint32_t>(bytes[offset]) |
           (static_cast<std::uint32_t>(bytes[offset + 1]) << 8) |
           (static_cast<std::uint32_t>(bytes[offset + 2]) << 16) |
           (static_cast<std::uint32_t>(bytes[offset + 3]) << 24);
}

void appendU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

void appendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 16) & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 24) & 0xFF));
}

void appendU64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    appendU32(out, static_cast<std::uint32_t>(value & 0xFFFFFFFFULL));
    appendU32(out, static_cast<std::uint32_t>((value >> 32) & 0xFFFFFFFFULL));
}

std::size_t alignUp(std::size_t value, std::size_t alignment) {
    if (alignment == 0) {
        return value;
    }
    const std::size_t remainder = value % alignment;
    return remainder == 0 ? value : value + (alignment - remainder);
}

std::vector<std::uint8_t> emitElf64Executable(const NativeProgramImage& image, TargetArch arch) {
    const std::uint16_t machine = (arch == TargetArch::X64) ? 0x3E : 0xB7; // EM_X86_64 / EM_AARCH64
    const std::uint64_t baseVaddr = 0x400000;
    const std::size_t headerSize = 64 + 56;
    const std::size_t codeOffset = alignUp(headerSize, 0x1000);
    const std::uint64_t entry = baseVaddr + static_cast<std::uint64_t>(codeOffset) + image.entryOffset;

    std::vector<std::uint8_t> out;
    out.reserve(codeOffset + image.textBytes.size());

    out.push_back(0x7F);
    out.push_back('E');
    out.push_back('L');
    out.push_back('F');
    out.push_back(2); // 64-bit
    out.push_back(1); // little-endian
    out.push_back(1); // version
    out.push_back(0); // sysv
    out.insert(out.end(), 8, 0);

    appendU16(out, 2); // ET_EXEC
    appendU16(out, machine);
    appendU32(out, 1);
    appendU64(out, entry);
    appendU64(out, 64); // phoff
    appendU64(out, 0);  // shoff
    appendU32(out, 0);
    appendU16(out, 64);
    appendU16(out, 56);
    appendU16(out, 1);
    appendU16(out, 0);
    appendU16(out, 0);
    appendU16(out, 0);

    appendU32(out, 1); // PT_LOAD
    appendU32(out, 5); // PF_R|PF_X
    appendU64(out, codeOffset);
    appendU64(out, baseVaddr + static_cast<std::uint64_t>(codeOffset));
    appendU64(out, baseVaddr + static_cast<std::uint64_t>(codeOffset));
    appendU64(out, image.textBytes.size());
    appendU64(out, image.textBytes.size());
    appendU64(out, 0x1000);

    if (out.size() < codeOffset) {
        out.resize(codeOffset, 0);
    }
    out.insert(out.end(), image.textBytes.begin(), image.textBytes.end());
    return out;
}

std::vector<std::uint8_t> emitMachO64Executable(const NativeProgramImage& image, TargetArch arch) {
    const bool isArm64 = arch == TargetArch::Arm64;
    if (!isArm64 && arch != TargetArch::X64) {
        throw std::runtime_error("Mach-O emission currently supports x86_64 and arm64 targets only");
    }

    constexpr std::uint32_t MH_MAGIC_64 = 0xFEEDFACF;
    constexpr std::uint32_t MH_EXECUTE = 2;
    constexpr std::uint32_t LC_SEGMENT_64 = 0x19;
    constexpr std::uint32_t LC_MAIN = 0x80000028;

    const std::uint32_t cpuType = isArm64 ? 0x0100000C : 0x01000007;
    const std::uint32_t cpuSubtype = isArm64 ? 0x00000000 : 0x00000003;
    const std::size_t segmentCmdSize = 72 + 80;
    const std::size_t entryCmdSize = 24;
    const std::size_t sizeofcmds = segmentCmdSize + entryCmdSize;
    const std::size_t headerSize = 32 + sizeofcmds;
    const std::size_t codeOffset = alignUp(headerSize, 0x1000);
    const std::uint64_t vmaddr = 0x100000000ULL;

    std::vector<std::uint8_t> out;
    out.reserve(codeOffset + image.textBytes.size());

    appendU32(out, MH_MAGIC_64);
    appendU32(out, cpuType);
    appendU32(out, cpuSubtype);
    appendU32(out, MH_EXECUTE);
    appendU32(out, 2); // ncmds
    appendU32(out, static_cast<std::uint32_t>(sizeofcmds));
    appendU32(out, 0);
    appendU32(out, 0);

    appendU32(out, LC_SEGMENT_64);
    appendU32(out, static_cast<std::uint32_t>(segmentCmdSize));
    std::array<char, 16> segname{};
    std::memcpy(segname.data(), "__TEXT", 6);
    out.insert(out.end(), segname.begin(), segname.end());
    appendU64(out, vmaddr);
    appendU64(out, alignUp(image.textBytes.size(), 0x1000));
    appendU64(out, 0);
    appendU64(out, alignUp(image.textBytes.size(), 0x1000));
    appendU32(out, 7);
    appendU32(out, 5);
    appendU32(out, 1);
    appendU32(out, 0);

    std::array<char, 16> sectname{};
    std::memcpy(sectname.data(), "__text", 6);
    out.insert(out.end(), sectname.begin(), sectname.end());
    out.insert(out.end(), segname.begin(), segname.end());
    appendU64(out, vmaddr);
    appendU64(out, image.textBytes.size());
    appendU32(out, 0);
    appendU32(out, 4);
    appendU32(out, 0);
    appendU32(out, 0);
    appendU32(out, 0x80000400); // pure instructions + some instructions
    appendU32(out, 0);
    appendU32(out, 0);
    appendU32(out, 0);

    appendU32(out, LC_MAIN);
    appendU32(out, static_cast<std::uint32_t>(entryCmdSize));
    appendU64(out, image.entryOffset);
    appendU64(out, 0);

    if (out.size() < codeOffset) {
        out.resize(codeOffset, 0);
    }
    out.insert(out.end(), image.textBytes.begin(), image.textBytes.end());
    return out;
}

} // namespace

std::optional<NativeProgramImage> extractTextImageFromPe(const std::vector<std::uint8_t>& peBytes) {
    if (peBytes.size() < 0x100 || peBytes[0] != 'M' || peBytes[1] != 'Z') {
        return std::nullopt;
    }

    const std::size_t peOffset = readU32(peBytes, 0x3C);
    if (peOffset + 24 > peBytes.size()) {
        return std::nullopt;
    }
    if (peBytes[peOffset] != 'P' || peBytes[peOffset + 1] != 'E' || peBytes[peOffset + 2] != 0 || peBytes[peOffset + 3] != 0) {
        return std::nullopt;
    }

    const std::size_t fileHeader = peOffset + 4;
    const std::uint16_t sectionCount = readU16(peBytes, fileHeader + 2);
    const std::uint16_t optionalHeaderSize = readU16(peBytes, fileHeader + 16);
    const std::size_t optionalHeader = fileHeader + 20;
    if (optionalHeader + optionalHeaderSize > peBytes.size()) {
        return std::nullopt;
    }

    const std::uint32_t entryRva = readU32(peBytes, optionalHeader + 16);

    const std::size_t sectionsOffset = optionalHeader + optionalHeaderSize;
    for (std::uint16_t i = 0; i < sectionCount; ++i) {
        const std::size_t sh = sectionsOffset + static_cast<std::size_t>(i) * 40;
        if (sh + 40 > peBytes.size()) {
            return std::nullopt;
        }

        const std::uint32_t virtualSize = readU32(peBytes, sh + 8);
        const std::uint32_t virtualAddress = readU32(peBytes, sh + 12);
        const std::uint32_t rawSize = readU32(peBytes, sh + 16);
        const std::uint32_t rawPtr = readU32(peBytes, sh + 20);
        const std::uint32_t sizeBound = std::max(virtualSize, rawSize);
        if (sizeBound == 0) {
            continue;
        }

        const bool containsEntry = entryRva >= virtualAddress && entryRva < (virtualAddress + sizeBound);
        if (!containsEntry) {
            continue;
        }
        if (rawPtr + rawSize > peBytes.size()) {
            return std::nullopt;
        }

        NativeProgramImage image;
        image.textBytes.assign(peBytes.begin() + rawPtr, peBytes.begin() + rawPtr + rawSize);
        image.entryOffset = static_cast<std::uint64_t>(entryRva - virtualAddress);
        if (image.entryOffset >= image.textBytes.size()) {
            return std::nullopt;
        }
        return image;
    }

    return std::nullopt;
}

std::string emitExecutableForFormat(const NativeProgramImage& image,
                                    BinaryFormat format,
                                    TargetArch arch) {
    std::vector<std::uint8_t> bytes;
    switch (format) {
        case BinaryFormat::Pe32Plus:
            throw std::runtime_error("Internal error: PE emission must use direct backend payload");
        case BinaryFormat::Elf:
            bytes = emitElf64Executable(image, arch);
            break;
        case BinaryFormat::MachO:
            bytes = emitMachO64Executable(image, arch);
            break;
        case BinaryFormat::RawBin:
            bytes = image.textBytes;
            break;
        case BinaryFormat::IntelHex:
        case BinaryFormat::SRecord:
            throw std::runtime_error("Internal error: textual hex formats handled by binary emit utilities");
    }
    return std::string(reinterpret_cast<const char*>(bytes.data()), bytes.size());
}
