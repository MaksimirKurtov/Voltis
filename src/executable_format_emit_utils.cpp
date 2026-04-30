#include "executable_format_emit_utils.h"
#include "emit/elf_writer.h"
#include "emit/macho_writer.h"

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

} // namespace

/*
 * NATIVE FORMAT EMISSION — CURRENT APPROACH AND KNOWN LIMITATIONS
 * ================================================================
 *
 * CURRENT APPROACH:
 * ELF and Mach-O outputs are produced by extracting the text (.text) section
 * from the PE binary produced by the primary x86_64 backend, then wrapping it
 * in a minimal ELF or Mach-O container header. This is a scaffolding approach
 * adopted to bootstrap non-Windows output paths quickly.
 *
 * KNOWN LIMITATIONS (do not paper over these):
 * 1. Single-section only. Programs requiring .data, .rodata, .bss, or any
 *    section beyond .text will produce non-runnable output. The extracted
 *    payload will be incomplete.
 * 2. No relocation resolution. Absolute addresses embedded in the PE are not
 *    rebased. Position-dependent code will crash at load time on systems that
 *    apply ASLR or load at a non-default base address.
 * 3. No import resolution. External symbol references (libc, platform libs)
 *    are not linked. Output is suitable only for position-independent,
 *    fully self-contained programs.
 * 4. No debug info. DWARF/CodeView sections are not translated.
 *
 * INTENDED UPGRADE PATH:
 * Replace this entire file with dedicated ELFWriter and MachOWriter classes
 * that implement proper section tables, relocation resolution (Elf64_Rela /
 * MachO relocation_info chains), and symbol export. Until that work is complete,
 * ELF and Mach-O output must be treated as experimental and must not be
 * marketed as production-ready native linkers.
 *
 * DO NOT add new features to the PE-extraction path. New work goes into the
 * dedicated writer pipeline.
 */

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
            bytes = ELFWriter(arch, true).writeExecutable(image);
            break;
        case BinaryFormat::MachO:
            if (arch != TargetArch::X64 && arch != TargetArch::Arm64) {
                throw std::runtime_error("Mach-O emission currently supports x86_64 and arm64 targets only");
            }
            bytes = MachOWriter(true).writeExecutable(image, arch == TargetArch::Arm64);
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
