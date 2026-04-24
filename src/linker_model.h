#pragma once

#include <cstdint>
#include <string>
#include <unordered_map>
#include <vector>

namespace linker {

enum class LinkSectionKind {
    Text,
    Rdata,
    Data,
    Idata,
    Reloc
};

enum class LinkSymbolVisibility {
    Local,
    Global,
    ExternalImport
};

enum class RelocationKind {
    Rel32,
    RipDisp32,
    Dir64
};

struct Relocation {
    LinkSectionKind section = LinkSectionKind::Text;
    std::size_t offset = 0;
    std::string target;
    std::size_t instructionSize = 0;
    RelocationKind kind = RelocationKind::Rel32;

    std::size_t sourceObjectIndex = 0;
    std::string sourceObjectName;
    std::int64_t addend = 0;
};

struct ImportSymbol {
    std::string dll;
    std::string name;
    std::string symbolName;
};

struct LinkSymbol {
    std::string name;
    LinkSectionKind section = LinkSectionKind::Text;
    std::size_t offset = 0;
    LinkSymbolVisibility visibility = LinkSymbolVisibility::Local;
    bool isDefined = false;
    bool isImport = false;
    std::string objectName;
    std::string importDll;
    std::string importName;
};

struct LinkSection {
    std::string name;
    LinkSectionKind kind = LinkSectionKind::Text;
    std::uint32_t characteristics = 0;
    std::uint32_t alignment = 0;
    std::vector<std::uint8_t> bytes;

    std::uint32_t rva = 0;
    std::uint32_t rawOffset = 0;
    std::uint32_t virtualSize = 0;
    std::uint32_t rawSize = 0;
};

struct LinkObject {
    std::string name;
    std::vector<LinkSection> sections;
    std::vector<LinkSymbol> symbols;
    std::vector<Relocation> relocations;
    std::vector<ImportSymbol> imports;
};

struct LinkedImage {
    std::vector<LinkSection> sections;
    std::unordered_map<std::string, std::uint32_t> symbolRvas;

    std::uint64_t imageBase = 0;
    std::uint32_t sectionAlignment = 0;
    std::uint32_t fileAlignment = 0;
    std::uint32_t headersSize = 0;
    std::uint32_t sizeOfImage = 0;

    std::uint32_t entryPointRva = 0;
    std::uint32_t importDirectoryRva = 0;
    std::uint32_t importDirectorySize = 0;
    std::uint32_t iatDirectoryRva = 0;
    std::uint32_t iatDirectorySize = 0;
    std::uint32_t baseRelocDirectoryRva = 0;
    std::uint32_t baseRelocDirectorySize = 0;
};

} // namespace linker

