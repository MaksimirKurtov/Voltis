#pragma once

#include "target.h"

#include <cstdint>
#include <string>
#include <vector>

enum class NativeSectionKind {
    Text,
    Rodata,
    Data,
    Bss,
    Tls,
    Import,
    Reloc,
    Custom
};

enum class NativeRelocationKind {
    Abs32,
    Abs64,
    Rel32,
    Rel64,
    PcRel32,
    PcRel64
};

enum class NativeSymbolVisibility {
    Local,
    Global,
    External
};

struct NativeSymbol {
    std::string name;
    NativeSectionKind section = NativeSectionKind::Text;
    std::uint64_t offset = 0;
    NativeSymbolVisibility visibility = NativeSymbolVisibility::Local;
    bool isDefined = false;
};

struct NativeRelocation {
    NativeSectionKind section = NativeSectionKind::Text;
    std::uint64_t offset = 0;
    std::string target;
    NativeRelocationKind kind = NativeRelocationKind::Rel32;
    std::int64_t addend = 0;
};

struct NativeImport {
    std::string library;
    std::string symbol;
};

struct NativeSection {
    std::string name;
    NativeSectionKind kind = NativeSectionKind::Text;
    std::uint32_t alignment = 0;
    std::vector<std::uint8_t> bytes;
};

struct NativeImage {
    TargetTriple target;
    std::vector<NativeSection> sections;
    std::vector<NativeSymbol> symbols;
    std::vector<NativeRelocation> relocations;
    std::vector<NativeImport> imports;
    std::string entrySymbol;
    bool positionIndependent = true;
};
