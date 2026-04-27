#pragma once

#include <cstdint>
#include <optional>
#include <string>
#include <vector>

enum class TargetArch {
    X86,
    X64,
    Arm64,
    Armv7,
    Armv6,
    Riscv32,
    Riscv64,
    Mips,
    Mips64,
    PowerPc64,
    S390x,
    Xtensa,
    Avr,
    Unknown
};

enum class TargetVendor {
    Pc,
    Apple,
    Unknown,
    None
};

enum class TargetOs {
    Windows,
    MacOS,
    Linux,
    BareMetal,
    Unknown
};

enum class TargetAbi {
    Msvc,
    Gnu,
    Musl,
    Eabi,
    Eabihf,
    Unknown,
    None
};

enum class Endianness {
    Little,
    Big
};

enum class BinaryFormat {
    Pe32Plus,
    Elf,
    MachO,
    RawBin,
    IntelHex,
    SRecord
};

struct TargetTriple {
    TargetArch arch = TargetArch::Unknown;
    TargetVendor vendor = TargetVendor::Unknown;
    TargetOs os = TargetOs::Unknown;
    TargetAbi abi = TargetAbi::Unknown;
    std::string canonical;
};

struct TargetDescription {
    TargetTriple triple;
    Endianness endianness = Endianness::Little;
    std::uint32_t pointerWidth = 64;
    std::uint32_t stackAlignment = 16;
    std::string callingConvention;
    std::vector<std::string> isaExtensions;
    std::vector<BinaryFormat> supportedFormats;
};

std::optional<TargetTriple> parseTargetTriple(const std::string& tripleText);
std::optional<TargetDescription> describeTarget(const TargetTriple& triple);
std::string toString(BinaryFormat format);
std::optional<BinaryFormat> parseBinaryFormat(const std::string& text);
std::vector<TargetDescription> listSupportedTargets();
std::string canonicalTargetTripleForHost();

