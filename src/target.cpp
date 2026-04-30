#include "target.h"

#include <algorithm>
#include <array>
#include <cctype>
#include <sstream>
#ifdef __APPLE__
#include <TargetConditionals.h>
#endif

namespace {

std::string lower(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

std::vector<std::string> split(const std::string& text, char delim) {
    std::vector<std::string> out;
    std::stringstream ss(text);
    std::string part;
    while (std::getline(ss, part, delim)) {
        out.push_back(part);
    }
    return out;
}

std::optional<TargetArch> parseArch(const std::string& text) {
    const std::string t = lower(text);
    if (t == "x86_64" || t == "amd64") return TargetArch::X64;
    if (t == "x86" || t == "i386" || t == "i686") return TargetArch::X86;
    if (t == "aarch64" || t == "arm64") return TargetArch::Arm64;
    if (t == "armv7" || t == "armv7a") return TargetArch::Armv7;
    if (t == "armv6") return TargetArch::Armv6;
    if (t == "riscv32") return TargetArch::Riscv32;
    if (t == "riscv64") return TargetArch::Riscv64;
    if (t == "mips") return TargetArch::Mips;
    if (t == "mips64") return TargetArch::Mips64;
    if (t == "powerpc64" || t == "ppc64") return TargetArch::PowerPc64;
    if (t == "s390x") return TargetArch::S390x;
    if (t == "xtensa") return TargetArch::Xtensa;
    if (t == "avr") return TargetArch::Avr;
    return std::nullopt;
}

TargetVendor parseVendor(const std::string& text) {
    const std::string t = lower(text);
    if (t == "pc") return TargetVendor::Pc;
    if (t == "apple") return TargetVendor::Apple;
    if (t == "none" || t == "unknown") return TargetVendor::None;
    return TargetVendor::Unknown;
}

std::optional<TargetOs> parseOs(const std::string& text) {
    const std::string t = lower(text);
    if (t == "windows" || t == "win32") return TargetOs::Windows;
    if (t == "linux") return TargetOs::Linux;
    if (t == "macos" || t == "darwin" || t.rfind("macos", 0) == 0) return TargetOs::MacOS;
    if (t == "none" || t == "baremetal") return TargetOs::BareMetal;
    return std::nullopt;
}

TargetAbi parseAbi(const std::string& text) {
    const std::string t = lower(text);
    if (t == "msvc") return TargetAbi::Msvc;
    if (t == "gnu") return TargetAbi::Gnu;
    if (t == "musl") return TargetAbi::Musl;
    if (t == "eabi") return TargetAbi::Eabi;
    if (t == "eabihf") return TargetAbi::Eabihf;
    if (t == "none" || t == "unknown") return TargetAbi::None;
    return TargetAbi::Unknown;
}

std::string archText(TargetArch arch) {
    switch (arch) {
        case TargetArch::X86: return "x86";
        case TargetArch::X64: return "x86_64";
        case TargetArch::Arm64: return "aarch64";
        case TargetArch::Armv7: return "armv7";
        case TargetArch::Armv6: return "armv6";
        case TargetArch::Riscv32: return "riscv32";
        case TargetArch::Riscv64: return "riscv64";
        case TargetArch::Mips: return "mips";
        case TargetArch::Mips64: return "mips64";
        case TargetArch::PowerPc64: return "powerpc64";
        case TargetArch::S390x: return "s390x";
        case TargetArch::Xtensa: return "xtensa";
        case TargetArch::Avr: return "avr";
        case TargetArch::Unknown: return "unknown";
    }
    return "unknown";
}

std::string vendorText(TargetVendor vendor) {
    switch (vendor) {
        case TargetVendor::Pc: return "pc";
        case TargetVendor::Apple: return "apple";
        case TargetVendor::None: return "none";
        case TargetVendor::Unknown: return "unknown";
    }
    return "unknown";
}

std::string osText(TargetOs os) {
    switch (os) {
        case TargetOs::Windows: return "windows";
        case TargetOs::MacOS: return "macos";
        case TargetOs::Linux: return "linux";
        case TargetOs::BareMetal: return "none";
        case TargetOs::Unknown: return "unknown";
    }
    return "unknown";
}

std::string abiText(TargetAbi abi) {
    switch (abi) {
        case TargetAbi::Msvc: return "msvc";
        case TargetAbi::Gnu: return "gnu";
        case TargetAbi::Musl: return "musl";
        case TargetAbi::Eabi: return "eabi";
        case TargetAbi::Eabihf: return "eabihf";
        case TargetAbi::None: return "none";
        case TargetAbi::Unknown: return "unknown";
    }
    return "unknown";
}

TargetDescription makeTarget(TargetArch arch,
                             TargetVendor vendor,
                             TargetOs os,
                             TargetAbi abi,
                             Endianness endianness,
                             std::uint32_t ptrWidth,
                             std::uint32_t stackAlign,
                             std::string cc,
                             std::vector<std::string> ext,
                             std::vector<BinaryFormat> formats,
                             BackendReadiness readiness) {
    TargetDescription desc;
    desc.triple.arch = arch;
    desc.triple.vendor = vendor;
    desc.triple.os = os;
    desc.triple.abi = abi;
    desc.triple.canonical = archText(arch) + "-" + vendorText(vendor) + "-" + osText(os) + "-" + abiText(abi);
    desc.endianness = endianness;
    desc.pointerWidth = ptrWidth;
    desc.stackAlignment = stackAlign;
    desc.callingConvention = std::move(cc);
    desc.isaExtensions = std::move(ext);
    desc.supportedFormats = std::move(formats);
    desc.readiness = readiness;
    return desc;
}

const std::vector<TargetDescription>& builtinTargets() {
    static const std::vector<TargetDescription> targets = {
        makeTarget(TargetArch::X64, TargetVendor::Pc, TargetOs::Windows, TargetAbi::Msvc,
                   Endianness::Little, 64, 16, "ms_x64_abi",
                   {"sse2", "avx"}, {BinaryFormat::Pe32Plus}, BackendReadiness::Production),
        makeTarget(TargetArch::X86, TargetVendor::Pc, TargetOs::Windows, TargetAbi::Msvc,
                   Endianness::Little, 32, 16, "__stdcall/__fastcall/cdecl",
                   {"sse2"}, {BinaryFormat::Pe32Plus}, BackendReadiness::Planned),
        makeTarget(TargetArch::Arm64, TargetVendor::Pc, TargetOs::Windows, TargetAbi::Msvc,
                   Endianness::Little, 64, 16, "aapcs64_windows",
                   {}, {BinaryFormat::Pe32Plus}, BackendReadiness::Experimental),
        makeTarget(TargetArch::X64, TargetVendor::Apple, TargetOs::MacOS, TargetAbi::None,
                   Endianness::Little, 64, 16, "sysv_amd64",
                   {}, {BinaryFormat::MachO}, BackendReadiness::Production),
        makeTarget(TargetArch::Arm64, TargetVendor::Apple, TargetOs::MacOS, TargetAbi::None,
                   Endianness::Little, 64, 16, "aapcs64",
                   {}, {BinaryFormat::MachO}, BackendReadiness::Experimental),
        makeTarget(TargetArch::X64, TargetVendor::Pc, TargetOs::Linux, TargetAbi::Gnu,
                   Endianness::Little, 64, 16, "sysv_amd64",
                   {}, {BinaryFormat::Elf}, BackendReadiness::Production),
        makeTarget(TargetArch::X86, TargetVendor::Pc, TargetOs::Linux, TargetAbi::Gnu,
                   Endianness::Little, 32, 16, "sysv_i386",
                   {}, {BinaryFormat::Elf}, BackendReadiness::Planned),
        makeTarget(TargetArch::Arm64, TargetVendor::Pc, TargetOs::Linux, TargetAbi::Gnu,
                   Endianness::Little, 64, 16, "aapcs64",
                   {}, {BinaryFormat::Elf}, BackendReadiness::Experimental),
        makeTarget(TargetArch::Armv7, TargetVendor::Pc, TargetOs::Linux, TargetAbi::Eabihf,
                   Endianness::Little, 32, 8, "aapcs",
                   {"thumb2"}, {BinaryFormat::Elf}, BackendReadiness::Planned),
        makeTarget(TargetArch::Riscv64, TargetVendor::Pc, TargetOs::Linux, TargetAbi::Gnu,
                   Endianness::Little, 64, 16, "riscv_lp64",
                   {}, {BinaryFormat::Elf}, BackendReadiness::Planned),
        makeTarget(TargetArch::Riscv32, TargetVendor::None, TargetOs::BareMetal, TargetAbi::Eabi,
                   Endianness::Little, 32, 16, "riscv_ilp32",
                   {"m", "c"}, {BinaryFormat::Elf, BinaryFormat::RawBin, BinaryFormat::IntelHex, BinaryFormat::SRecord}, BackendReadiness::Planned),
        makeTarget(TargetArch::Armv7, TargetVendor::None, TargetOs::BareMetal, TargetAbi::Eabi,
                   Endianness::Little, 32, 8, "aapcs",
                   {"thumb2"}, {BinaryFormat::Elf, BinaryFormat::RawBin, BinaryFormat::IntelHex, BinaryFormat::SRecord}, BackendReadiness::Planned),
        makeTarget(TargetArch::Xtensa, TargetVendor::None, TargetOs::BareMetal, TargetAbi::Eabi,
                   Endianness::Little, 32, 16, "xtensa_call0",
                   {}, {BinaryFormat::Elf, BinaryFormat::RawBin}, BackendReadiness::Planned),
        makeTarget(TargetArch::Avr, TargetVendor::None, TargetOs::BareMetal, TargetAbi::Eabi,
                   Endianness::Little, 16, 2, "avr_abi",
                   {}, {BinaryFormat::Elf, BinaryFormat::IntelHex}, BackendReadiness::Planned)
    };
    return targets;
}

} // namespace

std::optional<TargetTriple> parseTargetTriple(const std::string& tripleText) {
    const auto parts = split(tripleText, '-');
    if (parts.size() < 3 || parts.size() > 4) {
        return std::nullopt;
    }
    const auto arch = parseArch(parts[0]);
    const auto os = parseOs(parts.size() >= 3 ? parts[2] : "");
    if (!arch.has_value() || !os.has_value()) {
        return std::nullopt;
    }

    TargetTriple triple;
    triple.arch = *arch;
    triple.vendor = parseVendor(parts[1]);
    triple.os = *os;
    triple.abi = parts.size() == 4 ? parseAbi(parts[3]) : TargetAbi::None;
    triple.canonical = archText(triple.arch) + "-" + vendorText(triple.vendor) + "-" + osText(triple.os) + "-" + abiText(triple.abi);
    return triple;
}

std::optional<TargetDescription> describeTarget(const TargetTriple& triple) {
    for (const auto& target : builtinTargets()) {
        if (target.triple.arch == triple.arch &&
            target.triple.vendor == triple.vendor &&
            target.triple.os == triple.os &&
            target.triple.abi == triple.abi) {
            return target;
        }
    }
    return std::nullopt;
}

std::string toString(BinaryFormat format) {
    switch (format) {
        case BinaryFormat::Pe32Plus: return "pe";
        case BinaryFormat::Elf: return "elf";
        case BinaryFormat::MachO: return "macho";
        case BinaryFormat::RawBin: return "bin";
        case BinaryFormat::IntelHex: return "hex";
        case BinaryFormat::SRecord: return "srec";
    }
    return "unknown";
}

std::optional<BinaryFormat> parseBinaryFormat(const std::string& text) {
    const std::string value = lower(text);
    if (value == "pe" || value == "exe" || value == "dll") return BinaryFormat::Pe32Plus;
    if (value == "elf" || value == "so" || value == "o") return BinaryFormat::Elf;
    if (value == "macho" || value == "dylib") return BinaryFormat::MachO;
    if (value == "bin" || value == "raw") return BinaryFormat::RawBin;
    if (value == "hex" || value == "ihex") return BinaryFormat::IntelHex;
    if (value == "srec" || value == "s19") return BinaryFormat::SRecord;
    return std::nullopt;
}

std::vector<TargetDescription> listSupportedTargets() {
    return builtinTargets();
}

std::string canonicalTargetTripleForHost() {
#if defined(_WIN32)
#if defined(__aarch64__) || defined(_M_ARM64)
    return "aarch64-pc-windows-msvc";
#elif defined(__x86_64__) || defined(_M_X64) || defined(_M_AMD64)
    return "x86_64-pc-windows-msvc";
#else
    return "x86_64-pc-windows-msvc";
#endif
#elif defined(__APPLE__) && defined(TARGET_OS_MAC) && TARGET_OS_MAC
#if defined(__aarch64__) || defined(__arm64__)
    return "aarch64-apple-macos12";
#elif defined(__x86_64__)
    return "x86_64-apple-macos12";
#else
    return "x86_64-apple-macos12";
#endif
#elif defined(__linux__)
#if defined(__aarch64__)
    return "aarch64-pc-linux-gnu";
#elif defined(__x86_64__)
    return "x86_64-pc-linux-gnu";
#else
    return "x86_64-pc-linux-gnu";
#endif
#else
    return "x86_64-pc-linux-gnu";
#endif
}
