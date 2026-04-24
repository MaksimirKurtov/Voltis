#include "backend_pe_x64.h"
#include "import_utils.h"
#include "linker_model.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <memory>
#include <limits>
#include <optional>
#include <set>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using SectionId = linker::LinkSectionKind;
using FixupKind = linker::RelocationKind;
using Fixup = linker::Relocation;
using LinkSymbolVisibility = linker::LinkSymbolVisibility;

struct Symbol {
    SectionId section = SectionId::Text;
    std::size_t offset = 0;
};

static std::size_t alignTo(std::size_t value, std::size_t alignment) {
    return (value + alignment - 1) / alignment * alignment;
}

static void appendU16(std::vector<std::uint8_t>& out, std::uint16_t value) {
    out.push_back(static_cast<std::uint8_t>(value & 0xFF));
    out.push_back(static_cast<std::uint8_t>((value >> 8) & 0xFF));
}

static void appendU32(std::vector<std::uint8_t>& out, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF));
    }
}

static void appendU64(std::vector<std::uint8_t>& out, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        out.push_back(static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF));
    }
}

static void patchU32(std::vector<std::uint8_t>& out, std::size_t offset, std::uint32_t value) {
    for (int i = 0; i < 4; ++i) {
        out[offset + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF);
    }
}

static void patchU64(std::vector<std::uint8_t>& out, std::size_t offset, std::uint64_t value) {
    for (int i = 0; i < 8; ++i) {
        out[offset + static_cast<std::size_t>(i)] = static_cast<std::uint8_t>((value >> (8 * i)) & 0xFF);
    }
}

static bool tryReadU16(const std::string& bytes, std::size_t offset, std::uint16_t& out) {
    if (offset + 2 > bytes.size()) {
        return false;
    }
    out = static_cast<std::uint16_t>(
        static_cast<std::uint8_t>(bytes[offset]) |
        (static_cast<std::uint16_t>(static_cast<std::uint8_t>(bytes[offset + 1])) << 8));
    return true;
}

static bool tryReadU32(const std::string& bytes, std::size_t offset, std::uint32_t& out) {
    if (offset + 4 > bytes.size()) {
        return false;
    }
    out = static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[offset])) |
          (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[offset + 1])) << 8) |
          (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[offset + 2])) << 16) |
          (static_cast<std::uint32_t>(static_cast<std::uint8_t>(bytes[offset + 3])) << 24);
    return true;
}

struct PeSectionHeaderView {
    std::string name;
    std::uint32_t virtualSize = 0;
    std::uint32_t virtualAddress = 0;
    std::uint32_t rawSize = 0;
    std::uint32_t rawPointer = 0;
};

static bool rvaInSection(std::uint32_t rva, const PeSectionHeaderView& section) {
    const std::uint64_t sectionStart = section.virtualAddress;
    const std::uint32_t sectionSpan = std::max(section.virtualSize, section.rawSize);
    const std::uint64_t sectionEnd = sectionStart + static_cast<std::uint64_t>(sectionSpan == 0 ? 1 : sectionSpan);
    return static_cast<std::uint64_t>(rva) >= sectionStart && static_cast<std::uint64_t>(rva) < sectionEnd;
}

static std::optional<std::string> validatePeExecutablePayload(const std::string& payload) {
    if (payload.size() < 0x40) {
        return std::string("payload is too small for DOS header");
    }
    if (payload[0] != 'M' || payload[1] != 'Z') {
        return std::string("missing MZ signature");
    }

    std::uint32_t peOffset = 0;
    if (!tryReadU32(payload, 0x3C, peOffset)) {
        return std::string("missing e_lfanew");
    }
    if (static_cast<std::uint64_t>(peOffset) + 24 > payload.size()) {
        return std::string("PE header offset is outside payload bounds");
    }
    if (payload[peOffset] != 'P' || payload[peOffset + 1] != 'E' || payload[peOffset + 2] != 0 || payload[peOffset + 3] != 0) {
        return std::string("missing PE signature");
    }

    std::uint16_t machine = 0;
    std::uint16_t sectionCount = 0;
    std::uint16_t optionalSize = 0;
    if (!tryReadU16(payload, static_cast<std::size_t>(peOffset) + 4, machine) ||
        !tryReadU16(payload, static_cast<std::size_t>(peOffset) + 6, sectionCount) ||
        !tryReadU16(payload, static_cast<std::size_t>(peOffset) + 20, optionalSize)) {
        return std::string("truncated COFF header");
    }
    if (machine != 0x8664) {
        return std::string("unexpected machine type (expected x64)");
    }
    if (sectionCount < 3) {
        return std::string("expected at least .text/.rdata/.idata sections");
    }

    const std::size_t optionalOffset = static_cast<std::size_t>(peOffset) + 24;
    if (optionalOffset + optionalSize > payload.size()) {
        return std::string("optional header extends past payload bounds");
    }

    std::uint16_t optionalMagic = 0;
    std::uint32_t entryRva = 0;
    std::uint32_t numberOfRvaAndSizes = 0;
    if (!tryReadU16(payload, optionalOffset + 0, optionalMagic) ||
        !tryReadU32(payload, optionalOffset + 16, entryRva)) {
        return std::string("truncated optional header");
    }
    if (optionalMagic != 0x20B) {
        return std::string("unexpected optional header magic (expected PE32+)");
    }
    if (optionalSize < 128 ||
        !tryReadU32(payload, optionalOffset + 108, numberOfRvaAndSizes)) {
        return std::string("optional header missing data directory table");
    }
    if (numberOfRvaAndSizes < 2) {
        return std::string("optional header has no import directory entry");
    }

    std::uint32_t importRva = 0;
    std::uint32_t importSize = 0;
    if (!tryReadU32(payload, optionalOffset + 120, importRva) ||
        !tryReadU32(payload, optionalOffset + 124, importSize)) {
        return std::string("truncated import directory entry");
    }
    if (importRva == 0 || importSize == 0) {
        return std::string("import directory is missing");
    }

    const std::size_t sectionHeadersOffset = optionalOffset + optionalSize;
    const std::size_t sectionHeadersSize = static_cast<std::size_t>(sectionCount) * 40;
    if (sectionHeadersOffset + sectionHeadersSize > payload.size()) {
        return std::string("section table extends past payload bounds");
    }

    std::vector<PeSectionHeaderView> sections;
    sections.reserve(sectionCount);
    for (std::size_t i = 0; i < sectionCount; ++i) {
        const std::size_t headerOffset = sectionHeadersOffset + i * 40;
        PeSectionHeaderView section;
        for (std::size_t nameIndex = 0; nameIndex < 8; ++nameIndex) {
            const char ch = payload[headerOffset + nameIndex];
            if (ch == '\0') {
                break;
            }
            section.name.push_back(ch);
        }
        if (!tryReadU32(payload, headerOffset + 8, section.virtualSize) ||
            !tryReadU32(payload, headerOffset + 12, section.virtualAddress) ||
            !tryReadU32(payload, headerOffset + 16, section.rawSize) ||
            !tryReadU32(payload, headerOffset + 20, section.rawPointer)) {
            return std::string("truncated section header");
        }

        if (section.rawSize > 0) {
            const std::uint64_t rawStart = section.rawPointer;
            const std::uint64_t rawEnd = rawStart + section.rawSize;
            if (rawEnd > payload.size()) {
                return "section '" + section.name + "' raw data lies outside payload bounds";
            }
        }
        sections.push_back(std::move(section));
    }

    const auto findSection = [&](const std::string& name) -> const PeSectionHeaderView* {
        for (const auto& section : sections) {
            if (section.name == name) {
                return &section;
            }
        }
        return nullptr;
    };

    const PeSectionHeaderView* textSection = findSection(".text");
    const PeSectionHeaderView* idataSection = findSection(".idata");
    if (textSection == nullptr) {
        return std::string("missing .text section");
    }
    if (idataSection == nullptr) {
        return std::string("missing .idata section");
    }
    if (!rvaInSection(entryRva, *textSection)) {
        return std::string("entrypoint RVA is not inside .text");
    }
    if (!rvaInSection(importRva, *idataSection)) {
        return std::string("import directory RVA is not inside .idata");
    }

    return std::nullopt;
}

static std::string sanitizeIdentifier(const std::string& name) {
    if (name.empty()) {
        return "unnamed";
    }
    std::string out;
    out.reserve(name.size());
    for (char ch : name) {
        const unsigned char value = static_cast<unsigned char>(ch);
        if (std::isalnum(value) || ch == '_') {
            out.push_back(ch);
        } else {
            out.push_back('_');
        }
    }
    if (!out.empty() && std::isdigit(static_cast<unsigned char>(out.front()))) {
        out.insert(out.begin(), '_');
    }
    return out;
}

class SectionBuilder {
public:
    explicit SectionBuilder(SectionId id) : id_(id) {}

    std::size_t size() const { return bytes_.size(); }
    const std::vector<std::uint8_t>& bytes() const { return bytes_; }
    std::vector<std::uint8_t>& bytes() { return bytes_; }

    std::size_t defineLabel(const std::string& name) {
        auto [it, inserted] = labels_.emplace(name, bytes_.size());
        if (!inserted) {
            throw std::runtime_error("duplicate label: " + name);
        }
        return it->second;
    }

    std::size_t labelOffset(const std::string& name) const { return labels_.at(name); }

    void appendByte(std::uint8_t value) { bytes_.push_back(value); }
    void appendBytes(const void* data, std::size_t count) {
        const auto* ptr = static_cast<const std::uint8_t*>(data);
        bytes_.insert(bytes_.end(), ptr, ptr + count);
    }
    void appendU16(std::uint16_t value) { ::appendU16(bytes_, value); }
    void appendU32(std::uint32_t value) { ::appendU32(bytes_, value); }
    void appendU64(std::uint64_t value) { ::appendU64(bytes_, value); }

    std::size_t appendCString(const std::string& value) {
        const std::size_t offset = bytes_.size();
        bytes_.insert(bytes_.end(), value.begin(), value.end());
        bytes_.push_back(0);
        return offset;
    }

    std::size_t reserve(std::size_t count) {
        const std::size_t offset = bytes_.size();
        bytes_.resize(bytes_.size() + count, 0);
        return offset;
    }

private:
    SectionId id_;
    std::vector<std::uint8_t> bytes_;
    std::unordered_map<std::string, std::size_t> labels_;
};

class ImportTableBuilder {
public:
    explicit ImportTableBuilder(SectionBuilder& idata) : idata_(idata) {}

    std::string addImport(const std::string& dll, const std::string& name) {
        const std::string key = dll + "!" + name;
        const auto existing = labels_.find(key);
        if (existing != labels_.end()) {
            return existing->second;
        }

        const std::string label = "__iat_" + sanitizeIdentifier(dll) + "_" + sanitizeIdentifier(name);
        labels_[key] = label;
        labelImports_[label] = linker::ImportSymbol{dll, name, label};
        if (dllIndex_.find(dll) == dllIndex_.end()) {
            dllIndex_[dll] = dlls_.size();
            dlls_.push_back(DllLayout{dll});
        }
        dlls_[dllIndex_[dll]].functions.push_back(FunctionLayout{name, label});
        return label;
    }

    void emit() {
        const std::size_t descriptorCount = dlls_.size() + 1;
        descriptorBase_ = idata_.size();
        idata_.reserve(descriptorCount * 20);
        for (std::size_t i = 0; i < descriptorCount; ++i) {
            idata_.appendU32(0);
            idata_.appendU32(0);
            idata_.appendU32(0);
            idata_.appendU32(0);
            idata_.appendU32(0);
        }

        for (auto& dll : dlls_) {
            dll.descriptorOffset = descriptorBase_ + dllIndexPosition(dll.name) * 20;
            dll.intOffset = idata_.size();
            for (std::size_t i = 0; i < dll.functions.size() + 1; ++i) {
                idata_.appendU64(0);
            }
            dll.iatOffset = idata_.size();
            for (const auto& fn : dll.functions) {
                dll.iatSlots.push_back(idata_.size());
                iatOffsets_[fn.iatLabel] = idata_.size();
                idata_.defineLabel(fn.iatLabel);
                idata_.appendU64(0);
            }
            idata_.appendU64(0);

            dll.nameOffset = idata_.size();
            idata_.appendCString(dll.name);

            for (auto& fn : dll.functions) {
                fn.hintNameOffset = idata_.size();
                idata_.appendU16(0);
                idata_.appendCString(fn.name);
            }
        }
    }

    const std::unordered_map<std::string, std::size_t>& iatOffsets() const {
        return iatOffsets_;
    }

    const std::unordered_map<std::string, linker::ImportSymbol>& importsByLabel() const {
        return labelImports_;
    }

    void patch(std::uint32_t idataRva) {
        for (const auto& dll : dlls_) {
            const std::uint32_t originalThunkRva = idataRva + static_cast<std::uint32_t>(dll.intOffset);
            const std::uint32_t firstThunkRva = idataRva + static_cast<std::uint32_t>(dll.iatOffset);
            const std::uint32_t nameRva = idataRva + static_cast<std::uint32_t>(dll.nameOffset);
            patchU32(idata_.bytes(), dll.descriptorOffset + 0, originalThunkRva);
            patchU32(idata_.bytes(), dll.descriptorOffset + 12, nameRva);
            patchU32(idata_.bytes(), dll.descriptorOffset + 16, firstThunkRva);

            for (std::size_t i = 0; i < dll.functions.size(); ++i) {
                const std::uint32_t hintRva = idataRva + static_cast<std::uint32_t>(dll.functions[i].hintNameOffset);
                patchU64(idata_.bytes(), dll.intOffset + i * 8, hintRva);
                patchU64(idata_.bytes(), dll.iatOffset + i * 8, hintRva);
            }
        }
    }

private:
    struct FunctionLayout {
        std::string name;
        std::string iatLabel;
        std::size_t hintNameOffset = 0;
    };

    struct DllLayout {
        explicit DllLayout(std::string dllName) : name(std::move(dllName)) {}
        std::string name;
        std::size_t descriptorOffset = 0;
        std::size_t intOffset = 0;
        std::size_t iatOffset = 0;
        std::size_t nameOffset = 0;
        std::vector<std::size_t> iatSlots;
        std::vector<FunctionLayout> functions;
    };

    std::size_t dllIndexPosition(const std::string& name) const {
        return static_cast<std::size_t>(dllIndex_.at(name));
    }

    SectionBuilder& idata_;
    std::unordered_map<std::string, std::string> labels_;
    std::unordered_map<std::string, linker::ImportSymbol> labelImports_;
    std::unordered_map<std::string, std::size_t> dllIndex_;
    std::unordered_map<std::string, std::size_t> iatOffsets_;
    std::vector<DllLayout> dlls_;
    std::size_t descriptorBase_ = 0;
};

enum class GpReg : std::uint8_t {
    RAX = 0,
    RCX = 1,
    RDX = 2,
    RBX = 3,
    RSP = 4,
    RBP = 5,
    RSI = 6,
    RDI = 7,
    R8 = 8,
    R9 = 9
};

enum class CondCode {
    E,
    NE,
    A,
    AE,
    B,
    BE,
    L,
    LE,
    G,
    GE
};

class Assembler {
public:
    explicit Assembler(SectionBuilder& text, std::vector<Fixup>& fixups) : text_(text), fixups_(fixups) {}

    std::size_t currentOffset() const { return text_.size(); }

    void markLabel(const std::string& name, std::unordered_map<std::string, Symbol>& symbols, SectionId section = SectionId::Text) {
        symbols[name] = Symbol{section, text_.size()};
        text_.defineLabel(name);
    }

    void emitByte(std::uint8_t value) { text_.appendByte(value); }
    void emitU32(std::uint32_t value) { text_.appendU32(value); }
    void emitU64(std::uint64_t value) { text_.appendU64(value); }
    void emitBytes(std::initializer_list<std::uint8_t> bytes) {
        for (std::uint8_t byte : bytes) {
            text_.appendByte(byte);
        }
    }

    void emitPushRbp() { emitBytes({0x55}); }
    void emitMovRbpRsp() { emitBytes({0x48, 0x89, 0xE5}); }
    void emitSubRsp(std::uint32_t amount) { emitBytes({0x48, 0x81, 0xEC}); emitU32(amount); }
    void emitAddRsp(std::uint32_t amount) { emitBytes({0x48, 0x81, 0xC4}); emitU32(amount); }
    void emitLeave() { emitBytes({0xC9}); }
    void emitRet() { emitBytes({0xC3}); }

    void emitXorEaxEax() { emitBytes({0x31, 0xC0}); }
    void emitMovRegImm32(GpReg reg, std::uint32_t value) {
        emitRex(false, std::nullopt, reg);
        emitByte(static_cast<std::uint8_t>(0xB8 + (static_cast<std::uint8_t>(reg) & 7)));
        emitU32(value);
    }
    void emitMovRaxImm64(std::uint64_t value) {
        emitBytes({0x48, 0xB8});
        emitU64(value);
    }
    void emitMovEaxFromMem32(GpReg base, std::int32_t disp) {
        emitRex(false, std::nullopt, base);
        emitBytes({0x8B, 0x85});
        emitU32(static_cast<std::uint32_t>(disp));
    }
    void emitMovMemFromEax32(GpReg base, std::int32_t disp) {
        emitRex(false, std::nullopt, base);
        emitBytes({0x89, 0x85});
        emitU32(static_cast<std::uint32_t>(disp));
    }
    void emitMovRaxFromMem64(GpReg base, std::int32_t disp) {
        emitRex(true, std::nullopt, base);
        emitBytes({0x8B, 0x85});
        emitU32(static_cast<std::uint32_t>(disp));
    }
    void emitMovMemFromRax64(GpReg base, std::int32_t disp) {
        emitRex(true, std::nullopt, base);
        emitBytes({0x89, 0x85});
        emitU32(static_cast<std::uint32_t>(disp));
    }
    void emitMovsdXmm0FromMem64(GpReg base, std::int32_t disp) {
        emitBytes({0xF2});
        emitRex(false, std::nullopt, base);
        emitBytes({0x0F, 0x10, 0x85});
        emitU32(static_cast<std::uint32_t>(disp));
    }
    void emitMovsdXmm1FromMem64(GpReg base, std::int32_t disp) {
        emitBytes({0xF2});
        emitRex(false, std::nullopt, base);
        emitBytes({0x0F, 0x10, 0x8D});
        emitU32(static_cast<std::uint32_t>(disp));
    }
    void emitMovsdXmm2FromMem64(GpReg base, std::int32_t disp) {
        emitBytes({0xF2});
        emitRex(false, std::nullopt, base);
        emitBytes({0x0F, 0x10, 0x95});
        emitU32(static_cast<std::uint32_t>(disp));
    }
    void emitMovsdXmm3FromMem64(GpReg base, std::int32_t disp) {
        emitBytes({0xF2});
        emitRex(false, std::nullopt, base);
        emitBytes({0x0F, 0x10, 0x9D});
        emitU32(static_cast<std::uint32_t>(disp));
    }
    void emitMovsdMemFromXmm0(GpReg base, std::int32_t disp) {
        emitBytes({0xF2});
        emitRex(false, std::nullopt, base);
        emitBytes({0x0F, 0x11, 0x85});
        emitU32(static_cast<std::uint32_t>(disp));
    }
    void emitMovsdMemFromXmm1(GpReg base, std::int32_t disp) {
        emitBytes({0xF2});
        emitRex(false, std::nullopt, base);
        emitBytes({0x0F, 0x11, 0x8D});
        emitU32(static_cast<std::uint32_t>(disp));
    }
    void emitMovsdMemFromXmm2(GpReg base, std::int32_t disp) {
        emitBytes({0xF2});
        emitRex(false, std::nullopt, base);
        emitBytes({0x0F, 0x11, 0x95});
        emitU32(static_cast<std::uint32_t>(disp));
    }
    void emitMovsdMemFromXmm3(GpReg base, std::int32_t disp) {
        emitBytes({0xF2});
        emitRex(false, std::nullopt, base);
        emitBytes({0x0F, 0x11, 0x9D});
        emitU32(static_cast<std::uint32_t>(disp));
    }
    void emitMovRcxFromMem64(GpReg base, std::int32_t disp) { emitMovFromMem64(GpReg::RCX, base, disp); }
    void emitMovRdxFromMem64(GpReg base, std::int32_t disp) { emitMovFromMem64(GpReg::RDX, base, disp); }
    void emitMovR8FromMem64(GpReg base, std::int32_t disp) { emitMovFromMem64(GpReg::R8, base, disp); }
    void emitMovRcxFromRax() { emitBytes({0x48, 0x89, 0xC1}); }
    void emitMovRdxFromRax() { emitBytes({0x48, 0x89, 0xC2}); }
    void emitMovR8FromRax() { emitBytes({0x49, 0x89, 0xC0}); }
    void emitMovEdxFromEax() { emitBytes({0x89, 0xC2}); }
    void emitMovR8dFromEax() { emitBytes({0x41, 0x89, 0xC0}); }
    void emitMovRcxFromRax64() { emitBytes({0x48, 0x89, 0xC1}); }
    void emitMovRdxFromRax64() { emitMovRdxFromRax(); }
    void emitMovR8FromRax64() { emitMovR8FromRax(); }

    void emitLeaRegRip(GpReg reg, const std::string& label) {
        const std::size_t offset = text_.size();
        const std::uint8_t regBits = static_cast<std::uint8_t>(reg) & 7;
        emitRex(true, reg, std::nullopt);
        emitBytes({0x8D, static_cast<std::uint8_t>(0x05 | (regBits << 3))});
        emitU32(0);
        fixups_.push_back(Fixup{SectionId::Text, offset + 3, label, 7, FixupKind::RipDisp32});
    }

    void emitLeaRegRbpDisp(GpReg reg, std::int32_t disp) {
        emitRex(true, reg, GpReg::RBP);
        emitBytes({0x8D, static_cast<std::uint8_t>(0x85 | ((static_cast<std::uint8_t>(reg) & 7) << 3))});
        emitU32(static_cast<std::uint32_t>(disp));
    }

    void emitCmpEaxEcx() { emitBytes({0x39, 0xC8}); }
    void emitTestEaxEax() { emitBytes({0x85, 0xC0}); }
    void emitTestRaxRax() { emitBytes({0x48, 0x85, 0xC0}); }
    void emitTestRcxRcx() { emitBytes({0x48, 0x85, 0xC9}); }
    void emitTestRdxRdx() { emitBytes({0x48, 0x85, 0xD2}); }
    void emitAddRaxRcx() { emitBytes({0x48, 0x01, 0xC8}); }
    void emitAddEaxEcx() { emitBytes({0x01, 0xC8}); }
    void emitSubEaxEcx() { emitBytes({0x29, 0xC8}); }
    void emitImulEaxEcx() { emitBytes({0x0F, 0xAF, 0xC1}); }
    void emitCdq() { emitBytes({0x99}); }
    void emitIdivEcx() { emitBytes({0xF7, 0xF9}); }
    void emitAndEaxEcx() { emitBytes({0x21, 0xC8}); }
    void emitOrEaxEcx() { emitBytes({0x09, 0xC8}); }
    void emitXorEaxImm32(std::uint32_t value) { emitBytes({0x35}); emitU32(value); }
    void emitNegEax() { emitBytes({0xF7, 0xD8}); }
    void emitCvtsi2sdXmm0Eax() { emitBytes({0xF2, 0x0F, 0x2A, 0xC0}); }
    void emitCvttsd2siEaxXmm0() { emitBytes({0xF2, 0x0F, 0x2C, 0xC0}); }
    void emitUcomisdXmm0Xmm1() { emitBytes({0x66, 0x0F, 0x2E, 0xC1}); }
    void emitAddsdXmm0Xmm1() { emitBytes({0xF2, 0x0F, 0x58, 0xC1}); }
    void emitSubsdXmm0Xmm1() { emitBytes({0xF2, 0x0F, 0x5C, 0xC1}); }
    void emitMulsdXmm0Xmm1() { emitBytes({0xF2, 0x0F, 0x59, 0xC1}); }
    void emitDivsdXmm0Xmm1() { emitBytes({0xF2, 0x0F, 0x5E, 0xC1}); }
    void emitXorpdXmm1Xmm1() { emitBytes({0x66, 0x0F, 0x57, 0xC9}); }
    void emitRoundsdXmm0(std::uint8_t mode) { emitBytes({0x66, 0x0F, 0x3A, 0x0B, 0xC0}); emitByte(mode); }
    void emitSetccAl(CondCode cc) {
        switch (cc) {
            case CondCode::E: emitBytes({0x0F, 0x94, 0xC0}); break;
            case CondCode::NE: emitBytes({0x0F, 0x95, 0xC0}); break;
            case CondCode::A: emitBytes({0x0F, 0x97, 0xC0}); break;
            case CondCode::AE: emitBytes({0x0F, 0x93, 0xC0}); break;
            case CondCode::B: emitBytes({0x0F, 0x92, 0xC0}); break;
            case CondCode::BE: emitBytes({0x0F, 0x96, 0xC0}); break;
            case CondCode::L: emitBytes({0x0F, 0x9C, 0xC0}); break;
            case CondCode::LE: emitBytes({0x0F, 0x9E, 0xC0}); break;
            case CondCode::G: emitBytes({0x0F, 0x9F, 0xC0}); break;
            case CondCode::GE: emitBytes({0x0F, 0x9D, 0xC0}); break;
        }
    }
    void emitMovzxEaxAl() { emitBytes({0x0F, 0xB6, 0xC0}); }

    void emitCallRel32(const std::string& label) {
        emitSubRsp(32);
        const std::size_t offset = text_.size();
        emitByte(0xE8);
        emitU32(0);
        fixups_.push_back(Fixup{SectionId::Text, offset + 1, label, 5, FixupKind::Rel32});
        emitAddRsp(32);
    }
    void emitCallIat(const std::string& label) {
        emitSubRsp(32);
        const std::size_t offset = text_.size();
        emitBytes({0xFF, 0x15});
        emitU32(0);
        fixups_.push_back(Fixup{SectionId::Text, offset + 2, label, 6, FixupKind::RipDisp32});
        emitAddRsp(32);
    }
    void emitJmpRel32(const std::string& label) {
        const std::size_t offset = text_.size();
        emitByte(0xE9);
        emitU32(0);
        fixups_.push_back(Fixup{SectionId::Text, offset + 1, label, 5, FixupKind::Rel32});
    }
    void emitJccRel32(CondCode cc, const std::string& label) {
        const std::size_t offset = text_.size();
        emitBytes({0x0F});
        switch (cc) {
            case CondCode::E: emitByte(0x84); break;
            case CondCode::NE: emitByte(0x85); break;
            case CondCode::L: emitByte(0x8C); break;
            case CondCode::LE: emitByte(0x8E); break;
            case CondCode::G: emitByte(0x8F); break;
            case CondCode::GE: emitByte(0x8D); break;
        }
        emitU32(0);
        fixups_.push_back(Fixup{SectionId::Text, offset + 2, label, 6, FixupKind::Rel32});
    }

private:
    void emitMovFromMem64(GpReg reg, GpReg base, std::int32_t disp) {
        const std::uint8_t regBits = static_cast<std::uint8_t>(reg) & 7;
        emitRex(true, reg, base);
        emitByte(0x8B);
        emitByte(static_cast<std::uint8_t>(0x80 | (regBits << 3) | 0x05));
        emitU32(static_cast<std::uint32_t>(disp));
    }

    void emitRex(bool w, std::optional<GpReg> reg, std::optional<GpReg> rm) {
        std::uint8_t rex = 0x40;
        if (w) rex |= 0x08;
        if (reg.has_value() && static_cast<std::uint8_t>(*reg) >= 8) rex |= 0x04;
        if (rm.has_value() && static_cast<std::uint8_t>(*rm) >= 8) rex |= 0x01;
        if (rex != 0x40) {
            emitByte(rex);
        }
    }

    SectionBuilder& text_;
    std::vector<Fixup>& fixups_;
};

struct ValueInfo {
    vir::Type type{};
    std::size_t offset = 0;
};

struct FunctionFrame {
    std::size_t callArea = 32;
    std::size_t scratchArea = 96;
    std::size_t localBase = 0;
    std::size_t valueBase = 0;
    std::size_t frameSize = 0;
    std::unordered_map<std::uint32_t, ValueInfo> locals;
    std::unordered_map<std::uint32_t, ValueInfo> values;
};

class PeImage {
public:
    explicit PeImage(std::string objectName) : imports_(idata_) {
        linkObject_.name = std::move(objectName);
    }

    SectionBuilder& text() { return text_; }
    SectionBuilder& rdata() { return rdata_; }
    SectionBuilder& idata() { return idata_; }
    ImportTableBuilder& imports() { return imports_; }

    void defineSymbol(const std::string& name,
                      SectionId section,
                      std::size_t offset,
                      LinkSymbolVisibility visibility = LinkSymbolVisibility::Local,
                      bool isImport = false,
                      const std::string& importDll = {},
                      const std::string& importName = {}) {
        symbols_[name] = Symbol{section, offset};
        auto existing = linkSymbolIndexByName_.find(name);
        if (existing == linkSymbolIndexByName_.end()) {
            linker::LinkSymbol symbol;
            symbol.name = name;
            symbol.section = section;
            symbol.offset = offset;
            symbol.visibility = visibility;
            symbol.isDefined = true;
            symbol.isImport = isImport;
            symbol.objectName = linkObject_.name;
            symbol.importDll = importDll;
            symbol.importName = importName;
            linkObject_.symbols.push_back(std::move(symbol));
            linkSymbolIndexByName_[name] = linkObject_.symbols.size() - 1;
        } else {
            linker::LinkSymbol& symbol = linkObject_.symbols[existing->second];
            symbol.section = section;
            symbol.offset = offset;
            symbol.visibility = visibility;
            symbol.isDefined = true;
            symbol.isImport = isImport;
            symbol.objectName = linkObject_.name;
            if (isImport) {
                symbol.importDll = importDll;
                symbol.importName = importName;
            }
        }
    }

    void defineGlobalSymbol(const std::string& name, SectionId section, std::size_t offset) {
        defineSymbol(name, section, offset, LinkSymbolVisibility::Global);
    }

    const Symbol& symbol(const std::string& name) const {
        return symbols_.at(name);
    }

    std::string addStringLiteral(const std::string& value) {
        const auto existing = stringLabels_.find(value);
        if (existing != stringLabels_.end()) {
            return existing->second;
        }
        const std::string label = "__lit_" + std::to_string(stringLabels_.size());
        const std::size_t offset = rdata_.appendCString(value);
        rdata_.defineLabel(label);
        defineSymbol(label, SectionId::Rdata, offset, LinkSymbolVisibility::Local);
        stringLabels_[value] = label;
        return label;
    }

    void addFixup(SectionId section, std::size_t offset, const std::string& target, std::size_t instructionSize, FixupKind kind) {
        linker::Relocation relocation;
        relocation.section = section;
        relocation.offset = offset;
        relocation.target = target;
        relocation.instructionSize = instructionSize;
        relocation.kind = kind;
        relocation.sourceObjectName = linkObject_.name;
        relocation.sourceObjectIndex = 0;
        relocation.addend = 0;
        linkObject_.relocations.push_back(std::move(relocation));
    }

    std::vector<Fixup>& fixups() { return linkObject_.relocations; }
    const linker::LinkObject& linkObject() const { return linkObject_; }
    const linker::LinkedImage& linkedImage() const { return linkedImage_; }

    bool buildImage(DiagnosticBag& diagnostics, std::string& payload, std::uint32_t& entryOffsetOut) {
        if (!finalizeImports(diagnostics)) {
            return false;
        }

        std::vector<linker::LinkObject> objects = splitIntoLinkObjects();
        std::vector<std::uint8_t> textBytes;
        std::vector<std::uint8_t> rdataBytes;
        std::vector<std::uint8_t> idataBytes;
        std::vector<std::uint8_t> relocBytes;
        if (!linkObjects(objects, diagnostics, textBytes, rdataBytes, idataBytes, relocBytes)) {
            return false;
        }

        const std::size_t sectionCount = linkedImage_.sections.size();
        const std::size_t headersSize = alignTo(0x80 + 4 + 20 + 240 + sectionCount * 40, linkedImage_.fileAlignment);
        linkedImage_.headersSize = static_cast<std::uint32_t>(headersSize);

        std::vector<std::uint8_t> header;
        header.reserve(headersSize);
        header.resize(0x80, 0);
        header[0] = 'M';
        header[1] = 'Z';
        patchU32(header, 0x3C, 0x80);
        header.push_back('P');
        header.push_back('E');
        header.push_back(0);
        header.push_back(0);
        appendU16(header, 0x8664);
        appendU16(header, static_cast<std::uint16_t>(sectionCount));
        appendU32(header, 0);
        appendU32(header, 0);
        appendU32(header, 0);
        appendU16(header, 240);
        appendU16(header, 0x0022);

        appendU16(header, 0x20B);
        header.push_back(0);
        header.push_back(0);
        appendU32(header, 0);
        appendU32(header, 0);
        appendU32(header, 0);
        appendU32(header, linkedImage_.entryPointRva);
        appendU32(header, linkedImage_.sections.empty() ? 0 : linkedImage_.sections.front().rva);
        appendU64(header, linkedImage_.imageBase);
        appendU32(header, linkedImage_.sectionAlignment);
        appendU32(header, linkedImage_.fileAlignment);
        appendU16(header, 6);
        appendU16(header, 0);
        appendU16(header, 6);
        appendU16(header, 0);
        appendU16(header, 6);
        appendU16(header, 0);
        appendU32(header, 0);
        appendU32(header, linkedImage_.sizeOfImage);
        appendU32(header, linkedImage_.headersSize);
        appendU32(header, 0);
        appendU16(header, 3);
        appendU16(header, 0x0100);
        appendU64(header, 0x100000);
        appendU64(header, 0x1000);
        appendU64(header, 0x100000);
        appendU64(header, 0x1000);
        appendU32(header, 0);
        appendU32(header, 16);
        appendU32(header, 0);
        appendU32(header, 0);
        appendU32(header, linkedImage_.importDirectoryRva);
        appendU32(header, linkedImage_.importDirectorySize);
        appendU32(header, 0);
        appendU32(header, 0);
        appendU32(header, 0);
        appendU32(header, 0);
        appendU32(header, 0);
        appendU32(header, 0);
        appendU32(header, linkedImage_.baseRelocDirectoryRva);
        appendU32(header, linkedImage_.baseRelocDirectorySize);
        for (int i = 6; i < 12; ++i) {
            appendU32(header, 0);
            appendU32(header, 0);
        }
        appendU32(header, linkedImage_.iatDirectoryRva);
        appendU32(header, linkedImage_.iatDirectorySize);
        for (int i = 13; i < 16; ++i) {
            appendU32(header, 0);
            appendU32(header, 0);
        }

        auto appendSectionHeader = [&](const linker::LinkSection& section) {
            char secName[8]{};
            std::memcpy(secName, section.name.c_str(), std::min<std::size_t>(7, section.name.size()));
            header.insert(header.end(), secName, secName + 8);
            appendU32(header, section.virtualSize);
            appendU32(header, section.rva);
            appendU32(header, section.rawSize);
            appendU32(header, section.rawOffset);
            appendU32(header, 0);
            appendU32(header, 0);
            appendU16(header, 0);
            appendU16(header, 0);
            appendU32(header, section.characteristics);
        };
        for (const auto& section : linkedImage_.sections) {
            appendSectionHeader(section);
        }
        header.resize(headersSize, 0);

        std::vector<std::uint8_t> binary;
        binary.reserve(headersSize +
                       alignTo(textBytes.size(), linkedImage_.fileAlignment) +
                       alignTo(rdataBytes.size(), linkedImage_.fileAlignment) +
                       alignTo(idataBytes.size(), linkedImage_.fileAlignment) +
                       alignTo(relocBytes.size(), linkedImage_.fileAlignment));
        binary.insert(binary.end(), header.begin(), header.end());
        appendPadded(binary, textBytes, alignTo(textBytes.size(), linkedImage_.fileAlignment));
        appendPadded(binary, rdataBytes, alignTo(rdataBytes.size(), linkedImage_.fileAlignment));
        appendPadded(binary, idataBytes, alignTo(idataBytes.size(), linkedImage_.fileAlignment));
        if (!relocBytes.empty()) {
            appendPadded(binary, relocBytes, alignTo(relocBytes.size(), linkedImage_.fileAlignment));
        }

        auto entryIt = linkedImage_.symbolRvas.find("__voltis_entry");
        if (entryIt == linkedImage_.symbolRvas.end()) {
            diagnostics.error({}, "direct pe backend linker: missing resolved entry symbol '__voltis_entry'");
            return false;
        }
        entryOffsetOut = entryIt->second - linkedImage_.sections.front().rva;
        payload.assign(reinterpret_cast<const char*>(binary.data()), binary.size());
        return true;
    }

private:
    struct SectionContribution {
        std::size_t objectIndex = 0;
        SectionId kind = SectionId::Text;
        std::size_t baseOffset = 0;
        std::size_t size = 0;
    };

    struct ResolvedSymbol {
        std::string name;
        SectionId section = SectionId::Text;
        std::size_t offset = 0;
        std::string objectName;
        bool isImport = false;
        std::string importDll;
        std::string importName;
    };

    struct RelocationJob {
        SectionId sourceSection = SectionId::Text;
        std::size_t sourceOffset = 0;
        FixupKind kind = FixupKind::Rel32;
        std::int64_t addend = 0;
        std::string sourceObjectName;
        ResolvedSymbol target;
    };

    static std::string sectionName(SectionId kind) {
        switch (kind) {
            case SectionId::Text: return ".text";
            case SectionId::Rdata: return ".rdata";
            case SectionId::Data: return ".data";
            case SectionId::Idata: return ".idata";
            case SectionId::Reloc: return ".reloc";
        }
        return ".unknown";
    }

    static std::string relocationName(FixupKind kind) {
        switch (kind) {
            case FixupKind::Rel32: return "REL32";
            case FixupKind::RipDisp32: return "RIP_DISP32";
            case FixupKind::Dir64: return "DIR64";
        }
        return "UNKNOWN";
    }

    static void appendPadded(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& bytes, std::size_t rawSize) {
        out.insert(out.end(), bytes.begin(), bytes.end());
        out.resize(out.size() + (rawSize - bytes.size()), 0);
    }

    bool finalizeImports(DiagnosticBag&) {
        if (importsFinalized_) {
            return true;
        }
        imports_.emit();
        iatOffsetsSnapshot_ = imports_.iatOffsets();
        linkObject_.imports.clear();
        for (const auto& [label, importSymbol] : imports_.importsByLabel()) {
            linkObject_.imports.push_back(importSymbol);
        }
        for (const auto& [label, offset] : imports_.iatOffsets()) {
            const auto importIt = imports_.importsByLabel().find(label);
            if (importIt != imports_.importsByLabel().end()) {
                defineSymbol(label, SectionId::Idata, offset, LinkSymbolVisibility::ExternalImport, true,
                             importIt->second.dll, importIt->second.name);
            } else {
                defineSymbol(label, SectionId::Idata, offset, LinkSymbolVisibility::ExternalImport, true);
            }
        }
        importsFinalized_ = true;
        return true;
    }

    std::vector<linker::LinkObject> splitIntoLinkObjects() const {
        std::vector<linker::LinkObject> objects;
        const auto& textBytes = text_.bytes();
        const auto& rdataBytes = rdata_.bytes();
        const auto& idataBytes = idata_.bytes();

        struct RangeStart {
            std::string name;
            std::size_t offset = 0;
        };
        std::vector<RangeStart> globalTextStarts;
        for (const auto& symbol : linkObject_.symbols) {
            if (symbol.section == SectionId::Text &&
                symbol.isDefined &&
                symbol.visibility == LinkSymbolVisibility::Global) {
                globalTextStarts.push_back(RangeStart{symbol.name, symbol.offset});
            }
        }
        std::sort(globalTextStarts.begin(), globalTextStarts.end(), [](const RangeStart& a, const RangeStart& b) {
            return a.offset < b.offset;
        });
        globalTextStarts.erase(
            std::unique(globalTextStarts.begin(), globalTextStarts.end(), [](const RangeStart& a, const RangeStart& b) {
                return a.offset == b.offset && a.name == b.name;
            }),
            globalTextStarts.end());

        auto emitTextObject = [&](const std::string& objectName, std::size_t start, std::size_t end) {
            linker::LinkObject object;
            object.name = objectName;
            if (end < start) {
                end = start;
            }
            end = std::min(end, textBytes.size());
            start = std::min(start, end);
            object.sections.push_back(linker::LinkSection{
                ".text",
                SectionId::Text,
                0x60000020,
                0x1000,
                std::vector<std::uint8_t>(textBytes.begin() + static_cast<std::ptrdiff_t>(start),
                                          textBytes.begin() + static_cast<std::ptrdiff_t>(end)),
                0,
                0,
                static_cast<std::uint32_t>(end - start),
                0
            });

            for (const auto& symbol : linkObject_.symbols) {
                if (symbol.section != SectionId::Text || symbol.offset < start || symbol.offset >= end) {
                    continue;
                }
                linker::LinkSymbol adjusted = symbol;
                adjusted.offset -= start;
                adjusted.objectName = object.name;
                object.symbols.push_back(std::move(adjusted));
            }
            for (const auto& relocation : linkObject_.relocations) {
                if (relocation.section != SectionId::Text || relocation.offset < start || relocation.offset >= end) {
                    continue;
                }
                linker::Relocation adjusted = relocation;
                adjusted.offset -= start;
                adjusted.sourceObjectName = object.name;
                object.relocations.push_back(std::move(adjusted));
            }
            objects.push_back(std::move(object));
        };

        if (!globalTextStarts.empty()) {
            for (std::size_t i = 0; i < globalTextStarts.size(); ++i) {
                const std::size_t start = globalTextStarts[i].offset;
                const std::size_t end = (i + 1 < globalTextStarts.size()) ? globalTextStarts[i + 1].offset : textBytes.size();
                emitTextObject(globalTextStarts[i].name + ".obj", start, end);
            }
        } else {
            emitTextObject(linkObject_.name + ".text.obj", 0, textBytes.size());
        }

        linker::LinkObject rdataObject;
        rdataObject.name = linkObject_.name + ".rdata.obj";
        rdataObject.sections.push_back(linker::LinkSection{
            ".rdata", SectionId::Rdata, 0x40000040, 0x1000, rdataBytes, 0, 0, static_cast<std::uint32_t>(rdataBytes.size()), 0
        });
        for (const auto& symbol : linkObject_.symbols) {
            if (symbol.section == SectionId::Rdata) {
                linker::LinkSymbol adjusted = symbol;
                adjusted.objectName = rdataObject.name;
                rdataObject.symbols.push_back(std::move(adjusted));
            }
        }
        for (const auto& relocation : linkObject_.relocations) {
            if (relocation.section == SectionId::Rdata) {
                linker::Relocation adjusted = relocation;
                adjusted.sourceObjectName = rdataObject.name;
                rdataObject.relocations.push_back(std::move(adjusted));
            }
        }
        objects.push_back(std::move(rdataObject));

        linker::LinkObject idataObject;
        idataObject.name = linkObject_.name + ".idata.obj";
        idataObject.sections.push_back(linker::LinkSection{
            ".idata", SectionId::Idata, 0xC0000040, 0x1000, idataBytes, 0, 0, static_cast<std::uint32_t>(idataBytes.size()), 0
        });
        for (const auto& symbol : linkObject_.symbols) {
            if (symbol.section == SectionId::Idata) {
                linker::LinkSymbol adjusted = symbol;
                adjusted.objectName = idataObject.name;
                idataObject.symbols.push_back(std::move(adjusted));
            }
        }
        for (const auto& relocation : linkObject_.relocations) {
            if (relocation.section == SectionId::Idata) {
                linker::Relocation adjusted = relocation;
                adjusted.sourceObjectName = idataObject.name;
                idataObject.relocations.push_back(std::move(adjusted));
            }
        }
        idataObject.imports = linkObject_.imports;
        objects.push_back(std::move(idataObject));
        return objects;
    }

    bool linkObjects(const std::vector<linker::LinkObject>& objects,
                     DiagnosticBag& diagnostics,
                     std::vector<std::uint8_t>& textBytes,
                     std::vector<std::uint8_t>& rdataBytes,
                     std::vector<std::uint8_t>& idataBytes,
                     std::vector<std::uint8_t>& relocBytes) {
        textBytes.clear();
        rdataBytes.clear();
        idataBytes.clear();
        relocBytes.clear();

        std::vector<SectionContribution> contributions;
        contributions.reserve(objects.size() * 2);
        auto appendSectionBytes = [&](SectionId kind, const std::vector<std::uint8_t>& bytes) -> std::size_t {
            std::vector<std::uint8_t>* out = nullptr;
            switch (kind) {
                case SectionId::Text: out = &textBytes; break;
                case SectionId::Rdata: out = &rdataBytes; break;
                case SectionId::Idata: out = &idataBytes; break;
                case SectionId::Data:
                case SectionId::Reloc:
                    out = &rdataBytes; // stage-2/3 model: data-like sections share rdata backing for now
                    break;
            }
            const std::size_t base = out->size();
            out->insert(out->end(), bytes.begin(), bytes.end());
            return base;
        };

        for (std::size_t objectIndex = 0; objectIndex < objects.size(); ++objectIndex) {
            const auto& object = objects[objectIndex];
            for (const auto& section : object.sections) {
                const std::size_t baseOffset = appendSectionBytes(section.kind, section.bytes);
                contributions.push_back(SectionContribution{objectIndex, section.kind, baseOffset, section.bytes.size()});
            }
        }

        auto findContribution = [&](std::size_t objectIndex, SectionId kind) -> const SectionContribution* {
            for (const auto& contribution : contributions) {
                if (contribution.objectIndex == objectIndex && contribution.kind == kind) {
                    return &contribution;
                }
            }
            return nullptr;
        };

        std::unordered_map<std::string, ResolvedSymbol> globals;
        std::unordered_map<std::string, ResolvedSymbol> locals;
        std::unordered_map<std::string, std::vector<ResolvedSymbol>> allSymbolsByName;
        auto localKey = [](std::size_t objectIndex, const std::string& symbolName) {
            return std::to_string(objectIndex) + "|" + symbolName;
        };

        for (std::size_t objectIndex = 0; objectIndex < objects.size(); ++objectIndex) {
            const auto& object = objects[objectIndex];
            for (const auto& symbol : object.symbols) {
                if (!symbol.isDefined) {
                    continue;
                }
                const SectionContribution* contribution = findContribution(objectIndex, symbol.section);
                if (contribution == nullptr) {
                    diagnostics.error({}, "direct pe backend linker: symbol '" + symbol.name + "' in object '" + object.name +
                        "' references missing section " + sectionName(symbol.section));
                    return false;
                }
                if (symbol.offset > contribution->size) {
                    diagnostics.error({}, "direct pe backend linker: symbol '" + symbol.name + "' in object '" + object.name +
                        "' has out-of-range section offset");
                    return false;
                }
                ResolvedSymbol resolved;
                resolved.name = symbol.name;
                resolved.section = symbol.section;
                resolved.offset = contribution->baseOffset + symbol.offset;
                resolved.objectName = object.name;
                resolved.isImport = symbol.isImport;
                resolved.importDll = symbol.importDll;
                resolved.importName = symbol.importName;
                allSymbolsByName[symbol.name].push_back(resolved);

                if (symbol.visibility == LinkSymbolVisibility::Local) {
                    const std::string key = localKey(objectIndex, symbol.name);
                    if (locals.find(key) != locals.end()) {
                        diagnostics.error({}, "direct pe backend linker: duplicate local symbol '" + symbol.name +
                            "' in object '" + object.name + "'");
                        return false;
                    }
                    locals.emplace(key, std::move(resolved));
                    continue;
                }

                const auto existing = globals.find(symbol.name);
                if (existing != globals.end()) {
                    const bool bothImports = existing->second.isImport && resolved.isImport &&
                                             existing->second.importDll == resolved.importDll &&
                                             existing->second.importName == resolved.importName;
                    if (!bothImports) {
                        diagnostics.error({}, "direct pe backend linker: duplicate symbol '" + symbol.name +
                            "' defined in objects '" + existing->second.objectName + "' and '" + object.name + "'");
                        return false;
                    }
                    continue;
                }
                globals.emplace(symbol.name, std::move(resolved));
            }
        }

        std::vector<RelocationJob> relocationJobs;
        relocationJobs.reserve(linkObject_.relocations.size());
        for (std::size_t objectIndex = 0; objectIndex < objects.size(); ++objectIndex) {
            const auto& object = objects[objectIndex];
            for (const auto& relocation : object.relocations) {
                const SectionContribution* sourceContribution = findContribution(objectIndex, relocation.section);
                if (sourceContribution == nullptr) {
                    diagnostics.error({}, "direct pe backend linker: relocation in object '" + object.name +
                        "' references missing source section " + sectionName(relocation.section));
                    return false;
                }
                if (relocation.offset + (relocation.kind == FixupKind::Dir64 ? 8u : 4u) > sourceContribution->size) {
                    diagnostics.error({}, "direct pe backend linker: relocation in object '" + object.name +
                        "' at " + sectionName(relocation.section) + "+" + std::to_string(relocation.offset) +
                        " exceeds section bounds");
                    return false;
                }

                ResolvedSymbol target;
                const auto localIt = locals.find(localKey(objectIndex, relocation.target));
                if (localIt != locals.end()) {
                    target = localIt->second;
                } else {
                    const auto globalIt = globals.find(relocation.target);
                    if (globalIt != globals.end()) {
                        target = globalIt->second;
                    } else {
                        const auto anyIt = allSymbolsByName.find(relocation.target);
                        if (anyIt == allSymbolsByName.end() || anyIt->second.empty()) {
                            diagnostics.error({}, "direct pe backend linker: undefined symbol '" + relocation.target +
                                "' referenced from object '" + object.name + "' at " +
                                sectionName(relocation.section) + "+" + std::to_string(relocation.offset));
                            return false;
                        }
                        if (anyIt->second.size() > 1) {
                            diagnostics.error({}, "direct pe backend linker: ambiguous non-exported symbol '" + relocation.target +
                                "' referenced from object '" + object.name + "'");
                            return false;
                        }
                        target = anyIt->second.front();
                    }
                }

                RelocationJob job;
                job.sourceSection = relocation.section;
                job.sourceOffset = sourceContribution->baseOffset + relocation.offset;
                job.kind = relocation.kind;
                job.addend = relocation.addend;
                job.sourceObjectName = object.name;
                job.target = std::move(target);
                relocationJobs.push_back(std::move(job));
            }
        }
        
        const bool needsRelocSection = std::any_of(relocationJobs.begin(), relocationJobs.end(), [](const RelocationJob& job) {
            return job.kind == FixupKind::Dir64;
        });

        linkedImage_.imageBase = 0x0000000140000000ULL;
        linkedImage_.sectionAlignment = 0x1000;
        linkedImage_.fileAlignment = 0x200;
        const std::size_t sectionCount = needsRelocSection ? 4 : 3;
        linkedImage_.headersSize = static_cast<std::uint32_t>(
            alignTo(0x80 + 4 + 20 + 240 + sectionCount * 40, linkedImage_.fileAlignment));

        const std::uint32_t textRva = static_cast<std::uint32_t>(alignTo(linkedImage_.headersSize, linkedImage_.sectionAlignment));
        const std::uint32_t rdataRva = static_cast<std::uint32_t>(alignTo(
            static_cast<std::size_t>(textRva) + alignTo(textBytes.size(), linkedImage_.sectionAlignment),
            linkedImage_.sectionAlignment));
        const std::uint32_t idataRva = static_cast<std::uint32_t>(alignTo(
            static_cast<std::size_t>(rdataRva) + alignTo(rdataBytes.size(), linkedImage_.sectionAlignment),
            linkedImage_.sectionAlignment));
        const std::uint32_t relocRva = needsRelocSection
            ? static_cast<std::uint32_t>(alignTo(
                static_cast<std::size_t>(idataRva) + alignTo(idataBytes.size(), linkedImage_.sectionAlignment),
                linkedImage_.sectionAlignment))
            : 0;

        imports_.patch(idataRva);
        idataBytes = idata_.bytes();

        auto sectionRva = [&](SectionId section) -> std::uint32_t {
            switch (section) {
                case SectionId::Text: return textRva;
                case SectionId::Rdata: return rdataRva;
                case SectionId::Idata: return idataRva;
                case SectionId::Reloc: return relocRva;
                case SectionId::Data: return rdataRva;
            }
            return 0;
        };

        auto sectionBytes = [&](SectionId section) -> std::vector<std::uint8_t>& {
            switch (section) {
                case SectionId::Text: return textBytes;
                case SectionId::Rdata: return rdataBytes;
                case SectionId::Idata: return idataBytes;
                case SectionId::Reloc: return relocBytes;
                case SectionId::Data: return rdataBytes;
            }
            return textBytes;
        };

        std::vector<std::uint32_t> baseRelocRvas;
        for (const auto& relocation : relocationJobs) {
            std::vector<std::uint8_t>& sourceBytes = sectionBytes(relocation.sourceSection);
            if (relocation.sourceOffset + (relocation.kind == FixupKind::Dir64 ? 8u : 4u) > sourceBytes.size()) {
                diagnostics.error({}, "direct pe backend linker: relocation source out of range for object '" +
                    relocation.sourceObjectName + "' at " + sectionName(relocation.sourceSection) + "+" +
                    std::to_string(relocation.sourceOffset));
                return false;
            }

            const std::int64_t rawTargetRva =
                static_cast<std::int64_t>(sectionRva(relocation.target.section)) +
                static_cast<std::int64_t>(relocation.target.offset) +
                relocation.addend;
            if (rawTargetRva < 0) {
                diagnostics.error({}, "direct pe backend linker: relocation produced negative target RVA for symbol '" +
                    relocation.target.name + "'");
                return false;
            }
            const std::uint32_t targetRva = static_cast<std::uint32_t>(rawTargetRva);

            if (relocation.kind == FixupKind::Dir64) {
                const std::uint64_t absoluteValue = linkedImage_.imageBase + static_cast<std::uint64_t>(targetRva);
                patchU64(sourceBytes, relocation.sourceOffset, absoluteValue);
                baseRelocRvas.push_back(sectionRva(relocation.sourceSection) + static_cast<std::uint32_t>(relocation.sourceOffset));
                continue;
            }

            const std::uint32_t sourceRva = sectionRva(relocation.sourceSection) + static_cast<std::uint32_t>(relocation.sourceOffset);
            const std::int64_t disp = static_cast<std::int64_t>(targetRva) - static_cast<std::int64_t>(sourceRva + 4u);
            if (disp < std::numeric_limits<std::int32_t>::min() || disp > std::numeric_limits<std::int32_t>::max()) {
                diagnostics.error({}, "direct pe backend linker: relocation overflow (" + relocationName(relocation.kind) +
                    ") in object '" + relocation.sourceObjectName + "' at " +
                    sectionName(relocation.sourceSection) + "+" + std::to_string(relocation.sourceOffset) +
                    " targeting '" + relocation.target.name + "'");
                return false;
            }
            patchU32(sourceBytes, relocation.sourceOffset, static_cast<std::uint32_t>(static_cast<std::int32_t>(disp)));
        }

        if (!baseRelocRvas.empty()) {
            std::sort(baseRelocRvas.begin(), baseRelocRvas.end());
            baseRelocRvas.erase(std::unique(baseRelocRvas.begin(), baseRelocRvas.end()), baseRelocRvas.end());

            std::size_t i = 0;
            while (i < baseRelocRvas.size()) {
                const std::uint32_t pageRva = baseRelocRvas[i] & 0xFFFFF000u;
                std::vector<std::uint16_t> entries;
                while (i < baseRelocRvas.size() && (baseRelocRvas[i] & 0xFFFFF000u) == pageRva) {
                    const std::uint16_t offset = static_cast<std::uint16_t>(baseRelocRvas[i] & 0x0FFFu);
                    entries.push_back(static_cast<std::uint16_t>((10u << 12) | offset)); // IMAGE_REL_BASED_DIR64
                    ++i;
                }

                std::size_t blockSize = 8 + entries.size() * 2;
                if ((blockSize % 4) != 0) {
                    entries.push_back(0); // IMAGE_REL_BASED_ABSOLUTE padding
                    blockSize += 2;
                }

                appendU32(relocBytes, pageRva);
                appendU32(relocBytes, static_cast<std::uint32_t>(blockSize));
                for (std::uint16_t entry : entries) {
                    appendU16(relocBytes, entry);
                }
            }
        }

        const std::uint32_t textRaw = linkedImage_.headersSize;
        const std::uint32_t textRawSize = static_cast<std::uint32_t>(alignTo(textBytes.size(), linkedImage_.fileAlignment));
        const std::uint32_t rdataRaw = textRaw + textRawSize;
        const std::uint32_t rdataRawSize = static_cast<std::uint32_t>(alignTo(rdataBytes.size(), linkedImage_.fileAlignment));
        const std::uint32_t idataRaw = rdataRaw + rdataRawSize;
        const std::uint32_t idataRawSize = static_cast<std::uint32_t>(alignTo(idataBytes.size(), linkedImage_.fileAlignment));
        const std::uint32_t relocRaw = idataRaw + idataRawSize;
        const std::uint32_t relocRawSize = static_cast<std::uint32_t>(alignTo(relocBytes.size(), linkedImage_.fileAlignment));

        linkedImage_.sections.clear();
        linkedImage_.sections.push_back(linker::LinkSection{
            ".text", SectionId::Text, 0x60000020, linkedImage_.sectionAlignment, textBytes,
            textRva, textRaw, static_cast<std::uint32_t>(textBytes.size()), textRawSize});
        linkedImage_.sections.push_back(linker::LinkSection{
            ".rdata", SectionId::Rdata, 0x40000040, linkedImage_.sectionAlignment, rdataBytes,
            rdataRva, rdataRaw, static_cast<std::uint32_t>(rdataBytes.size()), rdataRawSize});
        linkedImage_.sections.push_back(linker::LinkSection{
            ".idata", SectionId::Idata, 0xC0000040, linkedImage_.sectionAlignment, idataBytes,
            idataRva, idataRaw, static_cast<std::uint32_t>(idataBytes.size()), idataRawSize});
        if (!relocBytes.empty()) {
            linkedImage_.sections.push_back(linker::LinkSection{
                ".reloc", SectionId::Reloc, 0x42000040, linkedImage_.sectionAlignment, relocBytes,
                relocRva, relocRaw, static_cast<std::uint32_t>(relocBytes.size()), relocRawSize});
        }

        const std::uint32_t imageTailRva = relocBytes.empty()
            ? idataRva + static_cast<std::uint32_t>(alignTo(idataBytes.size(), linkedImage_.sectionAlignment))
            : relocRva + static_cast<std::uint32_t>(alignTo(relocBytes.size(), linkedImage_.sectionAlignment));
        linkedImage_.sizeOfImage = static_cast<std::uint32_t>(alignTo(imageTailRva, linkedImage_.sectionAlignment));

        linkedImage_.symbolRvas.clear();
        for (const auto& [name, resolved] : globals) {
            linkedImage_.symbolRvas[name] = sectionRva(resolved.section) + static_cast<std::uint32_t>(resolved.offset);
        }

        const auto entryIt = linkedImage_.symbolRvas.find("__voltis_entry");
        if (entryIt == linkedImage_.symbolRvas.end()) {
            diagnostics.error({}, "direct pe backend linker: missing entry symbol '__voltis_entry'");
            return false;
        }
        linkedImage_.entryPointRva = entryIt->second;
        linkedImage_.importDirectoryRva = idataRva;
        linkedImage_.importDirectorySize = static_cast<std::uint32_t>(idataBytes.size());
        if (!iatOffsetsSnapshot_.empty()) {
            std::size_t minIat = std::numeric_limits<std::size_t>::max();
            std::size_t maxIat = 0;
            for (const auto& [_, offset] : iatOffsetsSnapshot_) {
                minIat = std::min(minIat, offset);
                maxIat = std::max(maxIat, offset + 8);
            }
            linkedImage_.iatDirectoryRva = idataRva + static_cast<std::uint32_t>(minIat);
            linkedImage_.iatDirectorySize = static_cast<std::uint32_t>(maxIat - minIat);
        } else {
            linkedImage_.iatDirectoryRva = 0;
            linkedImage_.iatDirectorySize = 0;
        }
        if (!relocBytes.empty()) {
            linkedImage_.baseRelocDirectoryRva = relocRva;
            linkedImage_.baseRelocDirectorySize = static_cast<std::uint32_t>(relocBytes.size());
        } else {
            linkedImage_.baseRelocDirectoryRva = 0;
            linkedImage_.baseRelocDirectorySize = 0;
        }

        return true;
    }

    SectionBuilder text_{SectionId::Text};
    SectionBuilder rdata_{SectionId::Rdata};
    SectionBuilder idata_{SectionId::Idata};
    ImportTableBuilder imports_;
    std::unordered_map<std::string, Symbol> symbols_;
    std::unordered_map<std::string, std::size_t> linkSymbolIndexByName_;
    std::unordered_map<std::string, std::string> stringLabels_;
    linker::LinkObject linkObject_{};
    linker::LinkedImage linkedImage_{};
    std::unordered_map<std::string, std::size_t> iatOffsetsSnapshot_;
    bool importsFinalized_ = false;
};

static bool isSupportedType(const vir::Type& type) {
    return type.kind == vir::TypeKind::Int32 ||
           type.kind == vir::TypeKind::Float32 ||
           type.kind == vir::TypeKind::Float64 ||
           type.kind == vir::TypeKind::Bool ||
           type.kind == vir::TypeKind::String ||
           type.kind == vir::TypeKind::Void;
}

static std::uint64_t doubleToBits(double value) {
    std::uint64_t bits = 0;
    std::memcpy(&bits, &value, sizeof(bits));
    return bits;
}

class WindowsPeX64Emitter {
public:
    explicit WindowsPeX64Emitter(const BackendOptions& options)
        : options_(options),
          image_(options.moduleName + ".obj"),
          asm_(image_.text(), image_.fixups()) {
        registerImports();
    }

    BackendResult emit(const vir::Module& module) {
        BackendResult result;
        if (!emitModule(module, result.diagnostics)) {
            return result;
        }
        std::uint32_t entryOffset = 0;
        std::string payload;
        if (!image_.buildImage(result.diagnostics, payload, entryOffset)) {
            return result;
        }
        (void)entryOffset;
        if (const auto validationError = validatePeExecutablePayload(payload); validationError.has_value()) {
            result.diagnostics.error({}, "direct pe backend linker self-check failed: " + *validationError);
            return result;
        }
        result.artifacts.push_back(BackendArtifact{
            BackendOutputKind::Executable,
            options_.moduleName + ".exe",
            payload,
            false
        });
        return result;
    }

private:
    struct FunctionLayout {
        const vir::Function* function = nullptr;
        FunctionFrame frame;
    };

    void registerImports() {
        image_.imports().addImport("kernel32.dll", "ExitProcess");
        image_.imports().addImport("msvcrt.dll", "puts");
        image_.imports().addImport("msvcrt.dll", "printf");
        image_.imports().addImport("msvcrt.dll", "sprintf");
        image_.imports().addImport("msvcrt.dll", "malloc");
        image_.imports().addImport("msvcrt.dll", "memcpy");
        image_.imports().addImport("msvcrt.dll", "strlen");
        image_.imports().addImport("msvcrt.dll", "strcmp");
        image_.imports().addImport("msvcrt.dll", "strtol");
        image_.imports().addImport("msvcrt.dll", "strtod");
    }

    bool emitModule(const vir::Module& module, DiagnosticBag& diagnostics) {
        if (!registerExternImports(module, diagnostics)) {
            return false;
        }

        const auto mainIt = std::find_if(module.functions.begin(), module.functions.end(), [](const vir::Function& fn) {
            return fn.name == "main";
        });
        if (mainIt == module.functions.end()) {
            diagnostics.error({}, "direct pe backend: missing entry function 'main'");
            return false;
        }
        if (!mainIt->params.empty()) {
            diagnostics.error({}, "direct pe backend: entry function 'main' must not take parameters");
            return false;
        }
        if (mainIt->returnType.kind != vir::TypeKind::Int32 && mainIt->returnType.kind != vir::TypeKind::Void) {
            diagnostics.error({}, "direct pe backend: entry function 'main' must return int32 or void");
            return false;
        }

        if (!emitEntryStub(*mainIt, diagnostics)) {
            return false;
        }

        for (const auto& function : module.functions) {
            if (!emitFunction(function, diagnostics)) {
                return false;
            }
        }

        return true;
    }

    bool registerExternImports(const vir::Module& module, DiagnosticBag& diagnostics) {
        externImports_.clear();
        std::unordered_map<std::string, std::string> resolvedByImportPath;
        for (const auto& importDecl : module.imports) {
            const ImportPathKind kind = classifyImportPath(importDecl.path);
            if (kind == ImportPathKind::SourceModule) {
                continue;
            }

            const std::string resolvedDll = normalizeNativeLibraryNameForPe(importDecl.path);
            if (resolvedDll.empty()) {
                diagnostics.error({}, "direct pe backend: cannot resolve native library import '" + importDecl.path + "'");
                return false;
            }
            resolvedByImportPath[importDecl.path] = resolvedDll;
        }

        for (const auto& externFunction : module.externFunctions) {
            auto resolvedIt = resolvedByImportPath.find(externFunction.importPath);
            std::string dll = resolvedIt == resolvedByImportPath.end()
                ? normalizeNativeLibraryNameForPe(externFunction.importPath)
                : resolvedIt->second;
            if (dll.empty()) {
                diagnostics.error(externFunction.location, "direct pe backend: extern function '" + externFunction.name + "' has an empty import path");
                return false;
            }

            externImports_[externFunction.name] = dll;
            image_.imports().addImport(dll, externFunction.name);
        }
        return true;
    }

    bool emitEntryStub(const vir::Function& mainFn, DiagnosticBag& diagnostics) {
        (void)diagnostics;
        resetTemps();
        const std::string label = "__voltis_entry";
        image_.defineGlobalSymbol(label, SectionId::Text, asm_.currentOffset());
        asm_.markLabel(label, symbols_);

        asm_.emitPushRbp();
        asm_.emitMovRbpRsp();
        asm_.emitSubRsp(static_cast<std::uint32_t>(alignTo(32 + 96, 16)));
        asm_.emitCallRel32(functionLabel(mainFn.name));
        if (mainFn.returnType.kind == vir::TypeKind::Int32 || mainFn.returnType.kind == vir::TypeKind::Bool) {
            asm_.emitBytes({0x89, 0xC1});
        } else {
            asm_.emitXorEaxEax();
            asm_.emitBytes({0x89, 0xC1});
        }
        asm_.emitCallIat(importLabel("kernel32.dll", "ExitProcess"));
        asm_.emitXorEaxEax();
        asm_.emitLeave();
        asm_.emitRet();
        return true;
    }

    bool emitFunction(const vir::Function& function, DiagnosticBag& diagnostics) {
        if (!isSupportedType(function.returnType)) {
            diagnostics.error({}, "direct pe backend: unsupported return type in function '" + function.name + "'");
            return false;
        }
        if (function.params.size() > 4) {
            diagnostics.error({}, "direct pe backend: functions with more than 4 parameters are not supported");
            return false;
        }
        for (const auto& param : function.params) {
            if (!isSupportedType(param.type)) {
                diagnostics.error(param.location, "direct pe backend: unsupported parameter type in function '" + function.name + "'");
                return false;
            }
        }
        for (const auto& local : function.locals) {
            if (!isSupportedType(local.type)) {
                diagnostics.error(local.location, "direct pe backend: unsupported local type in function '" + function.name + "'");
                return false;
            }
        }

        FunctionLayout layout;
        layout.function = &function;
        if (!buildFrame(layout, diagnostics)) {
            return false;
        }

        const std::string fnLabel = functionLabel(function.name);
        image_.defineGlobalSymbol(fnLabel, SectionId::Text, asm_.currentOffset());
        asm_.markLabel(fnLabel, symbols_);
        asm_.emitPushRbp();
        asm_.emitMovRbpRsp();
        asm_.emitSubRsp(static_cast<std::uint32_t>(layout.frame.frameSize));
        storeParameters(function, layout);

        for (const auto& block : function.blocks) {
            const std::string blkLabel = blockLabel(function.name, block.id.index);
            image_.defineSymbol(blkLabel, SectionId::Text, asm_.currentOffset());
            asm_.markLabel(blkLabel, symbols_);
            for (const auto& instruction : block.instructions) {
                if (!emitInstruction(instruction, layout, diagnostics)) {
                    return false;
                }
            }
            if (block.terminator.has_value()) {
                if (!emitTerminator(*block.terminator, layout, diagnostics)) {
                    return false;
                }
            } else {
                diagnostics.error({}, "direct pe backend: block '" + block.name + "' missing terminator");
                return false;
            }
        }

        return true;
    }

    bool buildFrame(FunctionLayout& layout, DiagnosticBag& diagnostics) {
        std::unordered_map<std::uint32_t, vir::Type> valueTypes;
        std::size_t maxValue = 0;

        for (const auto& block : layout.function->blocks) {
            for (const auto& instruction : block.instructions) {
                switch (instruction.kind) {
                    case vir::Instruction::Kind::Constant: {
                        const auto& inst = std::get<vir::ConstantInst>(instruction.data);
                        valueTypes[inst.result.index] = inst.constant.type;
                        maxValue = std::max(maxValue, static_cast<std::size_t>(inst.result.index + 1));
                        break;
                    }
                    case vir::Instruction::Kind::LoadLocal: {
                        const auto& inst = std::get<vir::LoadLocalInst>(instruction.data);
                        valueTypes[inst.result.index] = inst.type;
                        maxValue = std::max(maxValue, static_cast<std::size_t>(inst.result.index + 1));
                        break;
                    }
                    case vir::Instruction::Kind::Unary: {
                        const auto& inst = std::get<vir::UnaryInst>(instruction.data);
                        valueTypes[inst.result.index] = inst.type;
                        maxValue = std::max(maxValue, static_cast<std::size_t>(inst.result.index + 1));
                        break;
                    }
                    case vir::Instruction::Kind::Binary: {
                        const auto& inst = std::get<vir::BinaryInst>(instruction.data);
                        valueTypes[inst.result.index] = inst.type;
                        maxValue = std::max(maxValue, static_cast<std::size_t>(inst.result.index + 1));
                        break;
                    }
                    case vir::Instruction::Kind::Convert: {
                        const auto& inst = std::get<vir::ConvertInst>(instruction.data);
                        valueTypes[inst.result.index] = inst.toType;
                        maxValue = std::max(maxValue, static_cast<std::size_t>(inst.result.index + 1));
                        break;
                    }
                    case vir::Instruction::Kind::Call: {
                        const auto& inst = std::get<vir::CallInst>(instruction.data);
                        if (inst.result.has_value()) {
                            valueTypes[inst.result->index] = inst.returnType;
                            maxValue = std::max(maxValue, static_cast<std::size_t>(inst.result->index + 1));
                        }
                        break;
                    }
                    case vir::Instruction::Kind::StoreLocal:
                        break;
                }
            }
        }

        layout.frame.callArea = 32;
        layout.frame.scratchArea = 256;
        layout.frame.localBase = layout.frame.callArea + layout.frame.scratchArea;
        layout.frame.valueBase = layout.frame.localBase + layout.function->locals.size() * 8;
        layout.frame.frameSize = alignTo(layout.frame.valueBase + maxValue * 8, 16);

        for (const auto& local : layout.function->locals) {
            layout.frame.locals[local.id.index] = ValueInfo{local.type, layout.frame.localBase + local.id.index * 8};
        }
        for (std::size_t i = 0; i < maxValue; ++i) {
            const auto it = valueTypes.find(static_cast<std::uint32_t>(i));
            if (it == valueTypes.end()) {
                diagnostics.error({}, "direct pe backend: missing type info for SSA value");
                return false;
            }
            layout.frame.values[static_cast<std::uint32_t>(i)] = ValueInfo{it->second, layout.frame.valueBase + i * 8};
        }
        return true;
    }

    void storeParameters(const vir::Function& function, const FunctionLayout& layout) {
        for (std::size_t i = 0; i < function.params.size(); ++i) {
            const auto& param = function.params[i];
            const auto local = layout.frame.locals.at(function.locals[i].id.index);
            const std::int32_t disp = -static_cast<std::int32_t>(local.offset);
            switch (param.type.kind) {
                case vir::TypeKind::Int32:
                case vir::TypeKind::Bool:
                    if (i == 0) {
                        asm_.emitBytes({0x89, 0x8D});
                        asm_.emitU32(static_cast<std::uint32_t>(disp));
                    } else if (i == 1) {
                        asm_.emitBytes({0x89, 0x95});
                        asm_.emitU32(static_cast<std::uint32_t>(disp));
                    } else if (i == 2) {
                        asm_.emitBytes({0x44, 0x89, 0x85});
                        asm_.emitU32(static_cast<std::uint32_t>(disp));
                    } else {
                        asm_.emitBytes({0x44, 0x89, 0x8D});
                        asm_.emitU32(static_cast<std::uint32_t>(disp));
                    }
                    break;
                case vir::TypeKind::String:
                    if (i == 0) {
                        asm_.emitBytes({0x48, 0x89, 0x8D});
                        asm_.emitU32(static_cast<std::uint32_t>(disp));
                    } else if (i == 1) {
                        asm_.emitBytes({0x48, 0x89, 0x95});
                        asm_.emitU32(static_cast<std::uint32_t>(disp));
                    } else if (i == 2) {
                        asm_.emitBytes({0x4C, 0x89, 0x85});
                        asm_.emitU32(static_cast<std::uint32_t>(disp));
                    } else {
                        asm_.emitBytes({0x4C, 0x89, 0x8D});
                        asm_.emitU32(static_cast<std::uint32_t>(disp));
                    }
                    break;
                case vir::TypeKind::Float32:
                case vir::TypeKind::Float64:
                    if (i == 0) {
                        asm_.emitMovsdMemFromXmm0(GpReg::RBP, disp);
                    } else if (i == 1) {
                        asm_.emitMovsdMemFromXmm1(GpReg::RBP, disp);
                    } else if (i == 2) {
                        asm_.emitMovsdMemFromXmm2(GpReg::RBP, disp);
                    } else {
                        asm_.emitMovsdMemFromXmm3(GpReg::RBP, disp);
                    }
                    break;
                default:
                    break;
            }
        }
    }

    bool emitInstruction(const vir::Instruction& instruction, const FunctionLayout& layout, DiagnosticBag& diagnostics) {
        switch (instruction.kind) {
            case vir::Instruction::Kind::Constant:
                return emitConstant(std::get<vir::ConstantInst>(instruction.data), layout, diagnostics);
            case vir::Instruction::Kind::LoadLocal:
                return emitLoadLocal(std::get<vir::LoadLocalInst>(instruction.data), layout, diagnostics);
            case vir::Instruction::Kind::StoreLocal:
                return emitStoreLocal(std::get<vir::StoreLocalInst>(instruction.data), layout, diagnostics);
            case vir::Instruction::Kind::Unary:
                return emitUnary(std::get<vir::UnaryInst>(instruction.data), layout, diagnostics);
            case vir::Instruction::Kind::Binary:
                return emitBinary(std::get<vir::BinaryInst>(instruction.data), layout, diagnostics);
            case vir::Instruction::Kind::Convert:
                return emitConvert(std::get<vir::ConvertInst>(instruction.data), layout, diagnostics);
            case vir::Instruction::Kind::Call:
                return emitCall(std::get<vir::CallInst>(instruction.data), layout, diagnostics);
        }
        return false;
    }

    bool emitConstant(const vir::ConstantInst& inst, const FunctionLayout& layout, DiagnosticBag& diagnostics) {
        const auto value = layout.frame.values.find(inst.result.index);
        if (value == layout.frame.values.end()) {
            diagnostics.error({}, "direct pe backend: missing slot for constant");
            return false;
        }
        const std::int32_t disp = -static_cast<std::int32_t>(value->second.offset);

        switch (inst.constant.type.kind) {
            case vir::TypeKind::Int32:
                if (!std::holds_alternative<std::int32_t>(inst.constant.value)) {
                    diagnostics.error({}, "direct pe backend: invalid int32 constant");
                    return false;
                }
                asm_.emitMovRegImm32(GpReg::RAX, static_cast<std::uint32_t>(std::get<std::int32_t>(inst.constant.value)));
                asm_.emitMovMemFromEax32(GpReg::RBP, disp);
                return true;
            case vir::TypeKind::Bool:
                if (!std::holds_alternative<bool>(inst.constant.value)) {
                    diagnostics.error({}, "direct pe backend: invalid bool constant");
                    return false;
                }
                asm_.emitMovRegImm32(GpReg::RAX, std::get<bool>(inst.constant.value) ? 1u : 0u);
                asm_.emitMovMemFromEax32(GpReg::RBP, disp);
                return true;
            case vir::TypeKind::String: {
                if (!std::holds_alternative<std::string>(inst.constant.value)) {
                    diagnostics.error({}, "direct pe backend: invalid string constant");
                    return false;
                }
                asm_.emitLeaRegRip(GpReg::RAX, ensureLiteral(std::get<std::string>(inst.constant.value)));
                asm_.emitMovMemFromRax64(GpReg::RBP, disp);
                return true;
            }
            case vir::TypeKind::Float32:
                if (!std::holds_alternative<float>(inst.constant.value)) {
                    diagnostics.error({}, "direct pe backend: invalid float32 constant");
                    return false;
                }
                asm_.emitMovRaxImm64(doubleToBits(static_cast<double>(std::get<float>(inst.constant.value))));
                asm_.emitMovMemFromRax64(GpReg::RBP, disp);
                return true;
            case vir::TypeKind::Float64:
                if (!std::holds_alternative<double>(inst.constant.value)) {
                    diagnostics.error({}, "direct pe backend: invalid float64 constant");
                    return false;
                }
                asm_.emitMovRaxImm64(doubleToBits(std::get<double>(inst.constant.value)));
                asm_.emitMovMemFromRax64(GpReg::RBP, disp);
                return true;
            default:
                diagnostics.error({}, "direct pe backend: unsupported constant type");
                return false;
        }
    }

    bool emitLoadLocal(const vir::LoadLocalInst& inst, const FunctionLayout& layout, DiagnosticBag& diagnostics) {
        const auto local = layout.frame.locals.find(inst.local.index);
        const auto value = layout.frame.values.find(inst.result.index);
        if (local == layout.frame.locals.end() || value == layout.frame.values.end()) {
            diagnostics.error({}, "direct pe backend: missing slot for load");
            return false;
        }
        const std::int32_t localDisp = -static_cast<std::int32_t>(local->second.offset);
        const std::int32_t valueDisp = -static_cast<std::int32_t>(value->second.offset);
        switch (inst.type.kind) {
            case vir::TypeKind::Int32:
            case vir::TypeKind::Bool:
                asm_.emitMovEaxFromMem32(GpReg::RBP, localDisp);
                asm_.emitMovMemFromEax32(GpReg::RBP, valueDisp);
                return true;
            case vir::TypeKind::Float32:
            case vir::TypeKind::Float64:
                asm_.emitMovRaxFromMem64(GpReg::RBP, localDisp);
                asm_.emitMovMemFromRax64(GpReg::RBP, valueDisp);
                return true;
            case vir::TypeKind::String:
                asm_.emitMovRaxFromMem64(GpReg::RBP, localDisp);
                asm_.emitMovMemFromRax64(GpReg::RBP, valueDisp);
                return true;
            default:
                diagnostics.error({}, "direct pe backend: unsupported load type");
                return false;
        }
    }

    bool emitStoreLocal(const vir::StoreLocalInst& inst, const FunctionLayout& layout, DiagnosticBag& diagnostics) {
        const auto local = layout.frame.locals.find(inst.local.index);
        const auto value = layout.frame.values.find(inst.value.index);
        if (local == layout.frame.locals.end() || value == layout.frame.values.end()) {
            diagnostics.error({}, "direct pe backend: missing slot for store");
            return false;
        }
        const std::int32_t localDisp = -static_cast<std::int32_t>(local->second.offset);
        const std::int32_t valueDisp = -static_cast<std::int32_t>(value->second.offset);
        switch (inst.valueType.kind) {
            case vir::TypeKind::Int32:
            case vir::TypeKind::Bool:
                asm_.emitMovEaxFromMem32(GpReg::RBP, valueDisp);
                asm_.emitMovMemFromEax32(GpReg::RBP, localDisp);
                return true;
            case vir::TypeKind::Float32:
            case vir::TypeKind::Float64:
                asm_.emitMovRaxFromMem64(GpReg::RBP, valueDisp);
                asm_.emitMovMemFromRax64(GpReg::RBP, localDisp);
                return true;
            case vir::TypeKind::String:
                asm_.emitMovRaxFromMem64(GpReg::RBP, valueDisp);
                asm_.emitMovMemFromRax64(GpReg::RBP, localDisp);
                return true;
            default:
                diagnostics.error({}, "direct pe backend: unsupported store type");
                return false;
        }
    }

    bool emitUnary(const vir::UnaryInst& inst, const FunctionLayout& layout, DiagnosticBag& diagnostics) {
        const auto operand = layout.frame.values.find(inst.operand.index);
        const auto result = layout.frame.values.find(inst.result.index);
        if (operand == layout.frame.values.end() || result == layout.frame.values.end()) {
            diagnostics.error({}, "direct pe backend: missing slot for unary");
            return false;
        }
        const std::int32_t operandDisp = -static_cast<std::int32_t>(operand->second.offset);
        const std::int32_t resultDisp = -static_cast<std::int32_t>(result->second.offset);
        switch (inst.op) {
            case vir::UnaryOp::Negate:
                if (inst.type.kind != vir::TypeKind::Int32) {
                    diagnostics.error({}, "direct pe backend: negate only supports int32");
                    return false;
                }
                asm_.emitMovEaxFromMem32(GpReg::RBP, operandDisp);
                asm_.emitNegEax();
                asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                return true;
            case vir::UnaryOp::LogicalNot:
                if (inst.type.kind != vir::TypeKind::Bool) {
                    diagnostics.error({}, "direct pe backend: logical not only supports bool");
                    return false;
                }
                asm_.emitMovEaxFromMem32(GpReg::RBP, operandDisp);
                asm_.emitXorEaxImm32(1);
                asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                return true;
        }
        return false;
    }

    bool emitBinary(const vir::BinaryInst& inst, const FunctionLayout& layout, DiagnosticBag& diagnostics) {
        const auto left = layout.frame.values.find(inst.left.index);
        const auto right = layout.frame.values.find(inst.right.index);
        const auto result = layout.frame.values.find(inst.result.index);
        if (left == layout.frame.values.end() || right == layout.frame.values.end() || result == layout.frame.values.end()) {
            diagnostics.error({}, "direct pe backend: missing slot for binary");
            return false;
        }
        const std::int32_t leftDisp = -static_cast<std::int32_t>(left->second.offset);
        const std::int32_t rightDisp = -static_cast<std::int32_t>(right->second.offset);
        const std::int32_t resultDisp = -static_cast<std::int32_t>(result->second.offset);

        switch (inst.op) {
            case vir::BinaryOp::Add:
                if (inst.type.kind == vir::TypeKind::Float32 || inst.type.kind == vir::TypeKind::Float64) {
                    asm_.emitMovsdXmm0FromMem64(GpReg::RBP, leftDisp);
                    asm_.emitMovsdXmm1FromMem64(GpReg::RBP, rightDisp);
                    asm_.emitAddsdXmm0Xmm1();
                    asm_.emitMovsdMemFromXmm0(GpReg::RBP, resultDisp);
                    return true;
                }
                if (inst.type.kind == vir::TypeKind::Int32) {
                    asm_.emitMovEaxFromMem32(GpReg::RBP, leftDisp);
                    asm_.emitBytes({0x8B, 0x8D}); asm_.emitU32(static_cast<std::uint32_t>(rightDisp));
                    asm_.emitAddEaxEcx();
                    asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                    return true;
                }
                if (inst.type.kind == vir::TypeKind::String) {
                    return emitStringConcat(leftDisp, rightDisp, resultDisp, diagnostics);
                }
                diagnostics.error({}, "direct pe backend: add only supports int32 and string");
                return false;
            case vir::BinaryOp::Subtract:
                if (inst.type.kind == vir::TypeKind::Float32 || inst.type.kind == vir::TypeKind::Float64) {
                    asm_.emitMovsdXmm0FromMem64(GpReg::RBP, leftDisp);
                    asm_.emitMovsdXmm1FromMem64(GpReg::RBP, rightDisp);
                    asm_.emitSubsdXmm0Xmm1();
                    asm_.emitMovsdMemFromXmm0(GpReg::RBP, resultDisp);
                    return true;
                }
                if (inst.type.kind != vir::TypeKind::Int32) {
                    diagnostics.error({}, "direct pe backend: subtract only supports int32");
                    return false;
                }
                asm_.emitMovEaxFromMem32(GpReg::RBP, leftDisp);
                asm_.emitBytes({0x8B, 0x8D}); asm_.emitU32(static_cast<std::uint32_t>(rightDisp));
                asm_.emitSubEaxEcx();
                asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                return true;
            case vir::BinaryOp::Multiply:
                if (inst.type.kind == vir::TypeKind::Float32 || inst.type.kind == vir::TypeKind::Float64) {
                    asm_.emitMovsdXmm0FromMem64(GpReg::RBP, leftDisp);
                    asm_.emitMovsdXmm1FromMem64(GpReg::RBP, rightDisp);
                    asm_.emitMulsdXmm0Xmm1();
                    asm_.emitMovsdMemFromXmm0(GpReg::RBP, resultDisp);
                    return true;
                }
                if (inst.type.kind != vir::TypeKind::Int32) {
                    diagnostics.error({}, "direct pe backend: multiply only supports int32");
                    return false;
                }
                asm_.emitMovEaxFromMem32(GpReg::RBP, leftDisp);
                asm_.emitBytes({0x8B, 0x8D}); asm_.emitU32(static_cast<std::uint32_t>(rightDisp));
                asm_.emitImulEaxEcx();
                asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                return true;
            case vir::BinaryOp::Divide:
                if (inst.type.kind == vir::TypeKind::Float32 || inst.type.kind == vir::TypeKind::Float64) {
                    asm_.emitMovsdXmm0FromMem64(GpReg::RBP, leftDisp);
                    asm_.emitMovsdXmm1FromMem64(GpReg::RBP, rightDisp);
                    asm_.emitDivsdXmm0Xmm1();
                    asm_.emitMovsdMemFromXmm0(GpReg::RBP, resultDisp);
                    return true;
                }
                if (inst.type.kind != vir::TypeKind::Int32) {
                    diagnostics.error({}, "direct pe backend: divide only supports int32");
                    return false;
                }
                asm_.emitMovEaxFromMem32(GpReg::RBP, leftDisp);
                asm_.emitBytes({0x8B, 0x8D}); asm_.emitU32(static_cast<std::uint32_t>(rightDisp));
                asm_.emitCdq();
                asm_.emitIdivEcx();
                asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                return true;
            case vir::BinaryOp::Equal:
            case vir::BinaryOp::NotEqual:
                if (inst.type.kind == vir::TypeKind::String) {
                    return emitStringCompare(inst.op, leftDisp, rightDisp, resultDisp, diagnostics);
                }
                if (inst.type.kind == vir::TypeKind::Float32 || inst.type.kind == vir::TypeKind::Float64) {
                    asm_.emitMovsdXmm0FromMem64(GpReg::RBP, leftDisp);
                    asm_.emitMovsdXmm1FromMem64(GpReg::RBP, rightDisp);
                    asm_.emitUcomisdXmm0Xmm1();
                    asm_.emitSetccAl(inst.op == vir::BinaryOp::Equal ? CondCode::E : CondCode::NE);
                    asm_.emitMovzxEaxAl();
                    asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                    return true;
                }
                if (inst.type.kind == vir::TypeKind::Int32 || inst.type.kind == vir::TypeKind::Bool) {
                    asm_.emitMovEaxFromMem32(GpReg::RBP, leftDisp);
                    asm_.emitBytes({0x8B, 0x8D}); asm_.emitU32(static_cast<std::uint32_t>(rightDisp));
                    asm_.emitCmpEaxEcx();
                    asm_.emitSetccAl(inst.op == vir::BinaryOp::Equal ? CondCode::E : CondCode::NE);
                    asm_.emitMovzxEaxAl();
                    asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                    return true;
                }
                diagnostics.error({}, "direct pe backend: equality only supports int32/bool/string/float64");
                return false;
            case vir::BinaryOp::Less:
            case vir::BinaryOp::LessEqual:
            case vir::BinaryOp::Greater:
            case vir::BinaryOp::GreaterEqual:
                if (inst.type.kind != vir::TypeKind::Bool) {
                    diagnostics.error({}, "direct pe backend: comparison must return bool");
                    return false;
                }
                if (left->second.type.kind == vir::TypeKind::Float32 || left->second.type.kind == vir::TypeKind::Float64 ||
                    right->second.type.kind == vir::TypeKind::Float32 || right->second.type.kind == vir::TypeKind::Float64) {
                    asm_.emitMovsdXmm0FromMem64(GpReg::RBP, leftDisp);
                    asm_.emitMovsdXmm1FromMem64(GpReg::RBP, rightDisp);
                    asm_.emitUcomisdXmm0Xmm1();
                    asm_.emitSetccAl(inst.op == vir::BinaryOp::Less ? CondCode::B :
                                     inst.op == vir::BinaryOp::LessEqual ? CondCode::BE :
                                     inst.op == vir::BinaryOp::Greater ? CondCode::A : CondCode::AE);
                    asm_.emitMovzxEaxAl();
                    asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                    return true;
                }
                asm_.emitMovEaxFromMem32(GpReg::RBP, leftDisp);
                asm_.emitBytes({0x8B, 0x8D}); asm_.emitU32(static_cast<std::uint32_t>(rightDisp));
                asm_.emitCmpEaxEcx();
                asm_.emitSetccAl(inst.op == vir::BinaryOp::Less ? CondCode::L :
                                 inst.op == vir::BinaryOp::LessEqual ? CondCode::LE :
                                 inst.op == vir::BinaryOp::Greater ? CondCode::G : CondCode::GE);
                asm_.emitMovzxEaxAl();
                asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                return true;
            case vir::BinaryOp::LogicalAnd:
                if (inst.type.kind != vir::TypeKind::Bool) {
                    diagnostics.error({}, "direct pe backend: logical and only supports bool");
                    return false;
                }
                asm_.emitMovEaxFromMem32(GpReg::RBP, leftDisp);
                asm_.emitBytes({0x8B, 0x8D}); asm_.emitU32(static_cast<std::uint32_t>(rightDisp));
                asm_.emitAndEaxEcx();
                asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                return true;
            case vir::BinaryOp::LogicalOr:
                if (inst.type.kind != vir::TypeKind::Bool) {
                    diagnostics.error({}, "direct pe backend: logical or only supports bool");
                    return false;
                }
                asm_.emitMovEaxFromMem32(GpReg::RBP, leftDisp);
                asm_.emitBytes({0x8B, 0x8D}); asm_.emitU32(static_cast<std::uint32_t>(rightDisp));
                asm_.emitOrEaxEcx();
                asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                return true;
        }
        return false;
    }

    bool emitConvert(const vir::ConvertInst& inst, const FunctionLayout& layout, DiagnosticBag& diagnostics) {
        const auto input = layout.frame.values.find(inst.input.index);
        const auto result = layout.frame.values.find(inst.result.index);
        if (input == layout.frame.values.end() || result == layout.frame.values.end()) {
            diagnostics.error({}, "direct pe backend: missing slot for conversion");
            return false;
        }
        const std::int32_t inputDisp = -static_cast<std::int32_t>(input->second.offset);
        const std::int32_t resultDisp = -static_cast<std::int32_t>(result->second.offset);

        switch (inst.kind) {
            case vir::ConversionKind::ToInt32Identity:
                asm_.emitMovEaxFromMem32(GpReg::RBP, inputDisp);
                asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                return true;
            case vir::ConversionKind::ToInt32FromFloatTruncateTowardZero:
                asm_.emitMovsdXmm0FromMem64(GpReg::RBP, inputDisp);
                asm_.emitCvttsd2siEaxXmm0();
                asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                return true;
            case vir::ConversionKind::ToInt32FromStringParse:
                return emitStringToInt(inputDisp, resultDisp, diagnostics);
            case vir::ConversionKind::ToInt32FromBool:
                asm_.emitMovEaxFromMem32(GpReg::RBP, inputDisp);
                asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                return true;
            case vir::ConversionKind::ToFloat32:
                if (inst.fromType.kind == vir::TypeKind::String) {
                    return emitStringToFloat(inputDisp, resultDisp, diagnostics);
                }
                if (inst.fromType.kind == vir::TypeKind::Int32 || inst.fromType.kind == vir::TypeKind::Bool) {
                    asm_.emitMovEaxFromMem32(GpReg::RBP, inputDisp);
                    asm_.emitCvtsi2sdXmm0Eax();
                    asm_.emitMovsdMemFromXmm0(GpReg::RBP, resultDisp);
                    return true;
                }
                if (inst.fromType.kind == vir::TypeKind::Float32 || inst.fromType.kind == vir::TypeKind::Float64) {
                    asm_.emitMovRaxFromMem64(GpReg::RBP, inputDisp);
                    asm_.emitMovMemFromRax64(GpReg::RBP, resultDisp);
                    return true;
                }
                diagnostics.error({}, "direct pe backend: float conversion only supports int32/float32/float64/bool/string sources");
                return false;
            case vir::ConversionKind::ImplicitInt32ToFloat32:
            case vir::ConversionKind::ImplicitInt32ToFloat64:
                asm_.emitMovEaxFromMem32(GpReg::RBP, inputDisp);
                asm_.emitCvtsi2sdXmm0Eax();
                asm_.emitMovsdMemFromXmm0(GpReg::RBP, resultDisp);
                return true;
            case vir::ConversionKind::ImplicitFloat32ToFloat64:
                asm_.emitMovRaxFromMem64(GpReg::RBP, inputDisp);
                asm_.emitMovMemFromRax64(GpReg::RBP, resultDisp);
                return true;
            case vir::ConversionKind::ToFloat64:
                if (inst.fromType.kind == vir::TypeKind::String) {
                    return emitStringToFloat(inputDisp, resultDisp, diagnostics);
                }
                if (inst.fromType.kind == vir::TypeKind::Int32 || inst.fromType.kind == vir::TypeKind::Bool) {
                    asm_.emitMovEaxFromMem32(GpReg::RBP, inputDisp);
                    asm_.emitCvtsi2sdXmm0Eax();
                    asm_.emitMovsdMemFromXmm0(GpReg::RBP, resultDisp);
                    return true;
                }
                if (inst.fromType.kind == vir::TypeKind::Float32 || inst.fromType.kind == vir::TypeKind::Float64) {
                    asm_.emitMovRaxFromMem64(GpReg::RBP, inputDisp);
                    asm_.emitMovMemFromRax64(GpReg::RBP, resultDisp);
                    return true;
                }
                diagnostics.error({}, "direct pe backend: float conversion only supports int32/float32/float64/bool/string sources");
                return false;
            case vir::ConversionKind::Round:
            case vir::ConversionKind::Floor:
            case vir::ConversionKind::Ceil: {
                if (inst.fromType.kind != vir::TypeKind::Float32 && inst.fromType.kind != vir::TypeKind::Float64) {
                    diagnostics.error({}, "direct pe backend: round/floor/ceil only supports float32/float64");
                    return false;
                }
                asm_.emitMovsdXmm0FromMem64(GpReg::RBP, inputDisp);
                asm_.emitRoundsdXmm0(inst.kind == vir::ConversionKind::Round ? 0 : inst.kind == vir::ConversionKind::Floor ? 1 : 2);
                asm_.emitMovsdMemFromXmm0(GpReg::RBP, resultDisp);
                return true;
            }
            case vir::ConversionKind::ToString:
                return emitToString(inst.fromType, inputDisp, resultDisp, diagnostics);
            case vir::ConversionKind::ToBool:
                return emitToBool(inst.fromType, inputDisp, resultDisp, diagnostics);
            default:
                diagnostics.error({}, "direct pe backend: unsupported conversion kind");
                return false;
        }
    }

    bool emitCall(const vir::CallInst& inst, const FunctionLayout& layout, DiagnosticBag& diagnostics) {
        if (inst.isBuiltin && inst.callee == "print") {
            return emitBuiltinPrint(inst, layout, diagnostics);
        }

        if (inst.args.size() > 4) {
            diagnostics.error({}, "direct pe backend: calls with more than 4 arguments are not supported");
            return false;
        }
        for (std::size_t i = 0; i < inst.args.size(); ++i) {
            const auto arg = layout.frame.values.find(inst.args[i].index);
            if (arg == layout.frame.values.end()) {
                diagnostics.error({}, "direct pe backend: missing call argument slot");
                return false;
            }
            const std::int32_t disp = -static_cast<std::int32_t>(arg->second.offset);
            switch (arg->second.type.kind) {
                case vir::TypeKind::Int32:
                case vir::TypeKind::Bool:
                    asm_.emitMovEaxFromMem32(GpReg::RBP, disp);
                    moveIntegerArg(i);
                    break;
                case vir::TypeKind::Float32:
                case vir::TypeKind::Float64:
                    moveFloatArg(i, disp);
                    break;
                case vir::TypeKind::String:
                    asm_.emitMovRaxFromMem64(GpReg::RBP, disp);
                    moveStringArg(i);
                    break;
                default:
                    diagnostics.error({}, "direct pe backend: unsupported call argument type");
                    return false;
            }
        }

        if (inst.isExtern) {
            std::string dll;
            const auto externIt = externImports_.find(inst.callee);
            if (externIt != externImports_.end()) {
                dll = externIt->second;
            } else {
                dll = normalizeNativeLibraryNameForPe(inst.importPath);
            }

            if (dll.empty()) {
                diagnostics.error({}, "direct pe backend: extern call '" + inst.callee + "' is missing an import library");
                return false;
            }
            asm_.emitCallIat(importLabel(dll, inst.callee));
        } else {
            const std::string label = functionLabel(inst.callee);
            asm_.emitCallRel32(label);
        }

        if (inst.result.has_value()) {
            const auto result = layout.frame.values.find(inst.result->index);
            if (result == layout.frame.values.end()) {
                diagnostics.error({}, "direct pe backend: missing call result slot");
                return false;
            }
            const std::int32_t resultDisp = -static_cast<std::int32_t>(result->second.offset);
            switch (inst.returnType.kind) {
                case vir::TypeKind::Int32:
                case vir::TypeKind::Bool:
                    asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                    return true;
                case vir::TypeKind::Float32:
                case vir::TypeKind::Float64:
                    asm_.emitMovsdMemFromXmm0(GpReg::RBP, resultDisp);
                    return true;
                case vir::TypeKind::String:
                    asm_.emitMovMemFromRax64(GpReg::RBP, resultDisp);
                    return true;
                default:
                    diagnostics.error({}, "direct pe backend: unsupported call return type");
                    return false;
            }
        }
        return true;
    }

    bool emitBuiltinPrint(const vir::CallInst& inst, const FunctionLayout& layout, DiagnosticBag& diagnostics) {
        resetTemps();
        for (std::size_t i = 0; i < inst.args.size(); ++i) {
            const auto arg = layout.frame.values.find(inst.args[i].index);
            if (arg == layout.frame.values.end()) {
                diagnostics.error({}, "direct pe backend: missing print argument slot");
                return false;
            }
            const std::int32_t disp = -static_cast<std::int32_t>(arg->second.offset);
            switch (arg->second.type.kind) {
                case vir::TypeKind::Int32:
                    asm_.emitLeaRegRip(GpReg::RCX, ensureLiteral("%d\n"));
                    asm_.emitMovEaxFromMem32(GpReg::RBP, disp);
                    asm_.emitMovEdxFromEax();
                    asm_.emitCallIat(importLabel("msvcrt.dll", "printf"));
                    break;
                case vir::TypeKind::Float32:
                case vir::TypeKind::Float64:
                    asm_.emitLeaRegRip(GpReg::RCX, ensureLiteral("%.15g\n"));
                    asm_.emitMovRdxFromMem64(GpReg::RBP, disp);
                    asm_.emitMovsdXmm1FromMem64(GpReg::RBP, disp);
                    asm_.emitCallIat(importLabel("msvcrt.dll", "printf"));
                    break;
                case vir::TypeKind::Bool:
                {
                    const std::string boolTrueLabel = tempLabel("print_bool_true_" + std::to_string(i));
                    const std::string boolDoneLabel = tempLabel("print_bool_done_" + std::to_string(i));
                    asm_.emitMovEaxFromMem32(GpReg::RBP, disp);
                    asm_.emitTestEaxEax();
                    asm_.emitJccRel32(CondCode::NE, boolTrueLabel);
                    asm_.emitLeaRegRip(GpReg::RCX, ensureLiteral("false"));
                    asm_.emitCallIat(importLabel("msvcrt.dll", "puts"));
                    asm_.emitJmpRel32(boolDoneLabel);
                    markTemp(boolTrueLabel);
                    asm_.emitLeaRegRip(GpReg::RCX, ensureLiteral("true"));
                    asm_.emitCallIat(importLabel("msvcrt.dll", "puts"));
                    markTemp(boolDoneLabel);
                    break;
                }
                case vir::TypeKind::String:
                {
                    const std::string strNonNullLabel = tempLabel("print_str_nonnull_" + std::to_string(i));
                    const std::string strDoneLabel = tempLabel("print_str_done_" + std::to_string(i));
                    asm_.emitMovRcxFromMem64(GpReg::RBP, disp);
                    asm_.emitTestRcxRcx();
                    asm_.emitJccRel32(CondCode::NE, strNonNullLabel);
                    asm_.emitLeaRegRip(GpReg::RCX, ensureLiteral(""));
                    asm_.emitCallIat(importLabel("msvcrt.dll", "puts"));
                    asm_.emitJmpRel32(strDoneLabel);
                    markTemp(strNonNullLabel);
                    asm_.emitMovRcxFromMem64(GpReg::RBP, disp);
                    asm_.emitCallIat(importLabel("msvcrt.dll", "puts"));
                    markTemp(strDoneLabel);
                    break;
                }
                default:
                    diagnostics.error({}, "direct pe backend: unsupported print argument type");
                    return false;
            }
        }
        return true;
    }

    bool emitStringConcat(std::int32_t leftDisp, std::int32_t rightDisp, std::int32_t resultDisp, DiagnosticBag& diagnostics) {
        (void)diagnostics;
        resetTemps();
        const std::string lhsOkLabel = tempLabel("concat_lhs_ok");
        const std::string rhsOkLabel = tempLabel("concat_rhs_ok");
        const std::size_t lhsPtrOff = layoutScratch(0);
        const std::size_t rhsPtrOff = layoutScratch(8);
        const std::size_t lhsLenOff = layoutScratch(16);
        const std::size_t rhsLenOff = layoutScratch(24);
        const std::size_t destOff = layoutScratch(32);

        asm_.emitMovRaxFromMem64(GpReg::RBP, leftDisp);
        asm_.emitTestRaxRax();
        asm_.emitJccRel32(CondCode::NE, lhsOkLabel);
        asm_.emitLeaRegRip(GpReg::RAX, ensureLiteral(""));
        markTemp(lhsOkLabel);
        asm_.emitMovMemFromRax64(GpReg::RBP, -static_cast<std::int32_t>(lhsPtrOff));

        asm_.emitMovRaxFromMem64(GpReg::RBP, rightDisp);
        asm_.emitTestRaxRax();
        asm_.emitJccRel32(CondCode::NE, rhsOkLabel);
        asm_.emitLeaRegRip(GpReg::RAX, ensureLiteral(""));
        markTemp(rhsOkLabel);
        asm_.emitMovMemFromRax64(GpReg::RBP, -static_cast<std::int32_t>(rhsPtrOff));

        asm_.emitMovRcxFromMem64(GpReg::RBP, -static_cast<std::int32_t>(lhsPtrOff));
        asm_.emitCallIat(importLabel("msvcrt.dll", "strlen"));
        asm_.emitMovMemFromRax64(GpReg::RBP, -static_cast<std::int32_t>(lhsLenOff));

        asm_.emitMovRcxFromMem64(GpReg::RBP, -static_cast<std::int32_t>(rhsPtrOff));
        asm_.emitCallIat(importLabel("msvcrt.dll", "strlen"));
        asm_.emitMovMemFromRax64(GpReg::RBP, -static_cast<std::int32_t>(rhsLenOff));

        asm_.emitMovRaxFromMem64(GpReg::RBP, -static_cast<std::int32_t>(lhsLenOff));
        asm_.emitMovRcxFromRax64();
        asm_.emitMovRaxFromMem64(GpReg::RBP, -static_cast<std::int32_t>(rhsLenOff));
        asm_.emitAddRaxRcx();
        asm_.emitBytes({0x48, 0x83, 0xC0, 0x01});
        asm_.emitMovRcxFromRax64();
        asm_.emitCallIat(importLabel("msvcrt.dll", "malloc"));
        asm_.emitMovMemFromRax64(GpReg::RBP, -static_cast<std::int32_t>(destOff));

        asm_.emitMovRcxFromMem64(GpReg::RBP, -static_cast<std::int32_t>(destOff));
        asm_.emitMovRdxFromMem64(GpReg::RBP, -static_cast<std::int32_t>(lhsPtrOff));
        asm_.emitMovR8FromMem64(GpReg::RBP, -static_cast<std::int32_t>(lhsLenOff));
        asm_.emitCallIat(importLabel("msvcrt.dll", "memcpy"));

        asm_.emitMovRaxFromMem64(GpReg::RBP, -static_cast<std::int32_t>(destOff));
        asm_.emitMovRcxFromRax64();
        asm_.emitMovRaxFromMem64(GpReg::RBP, -static_cast<std::int32_t>(lhsLenOff));
        asm_.emitAddRaxRcx();
        asm_.emitMovRcxFromRax64();
        asm_.emitMovRdxFromMem64(GpReg::RBP, -static_cast<std::int32_t>(rhsPtrOff));
        asm_.emitMovRaxFromMem64(GpReg::RBP, -static_cast<std::int32_t>(rhsLenOff));
        asm_.emitBytes({0x48, 0x83, 0xC0, 0x01});
        asm_.emitMovR8FromRax();
        asm_.emitCallIat(importLabel("msvcrt.dll", "memcpy"));

        asm_.emitMovRaxFromMem64(GpReg::RBP, -static_cast<std::int32_t>(destOff));
        asm_.emitMovMemFromRax64(GpReg::RBP, resultDisp);
        return true;
    }

    bool emitStringCompare(vir::BinaryOp op, std::int32_t leftDisp, std::int32_t rightDisp, std::int32_t resultDisp, DiagnosticBag& diagnostics) {
        (void)diagnostics;
        resetTemps();
        const std::string lhsOkLabel = tempLabel("strcmp_lhs_ok");
        const std::string rhsOkLabel = tempLabel("strcmp_rhs_ok");
        asm_.emitMovRcxFromMem64(GpReg::RBP, leftDisp);
        asm_.emitTestRcxRcx();
        asm_.emitJccRel32(CondCode::NE, lhsOkLabel);
        asm_.emitLeaRegRip(GpReg::RCX, ensureLiteral(""));
        markTemp(lhsOkLabel);
        asm_.emitMovRdxFromMem64(GpReg::RBP, rightDisp);
        asm_.emitTestRdxRdx();
        asm_.emitJccRel32(CondCode::NE, rhsOkLabel);
        asm_.emitLeaRegRip(GpReg::RDX, ensureLiteral(""));
        markTemp(rhsOkLabel);
        asm_.emitCallIat(importLabel("msvcrt.dll", "strcmp"));
        asm_.emitTestEaxEax();
        asm_.emitSetccAl(op == vir::BinaryOp::Equal ? CondCode::E : CondCode::NE);
        asm_.emitMovzxEaxAl();
        asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
        return true;
    }

    bool emitStringToInt(std::int32_t inputDisp, std::int32_t resultDisp, DiagnosticBag& diagnostics) {
        (void)diagnostics;
        resetTemps();
        const std::string okLabel = tempLabel("strtol_ok");
        asm_.emitMovRcxFromMem64(GpReg::RBP, inputDisp);
        asm_.emitTestRcxRcx();
        asm_.emitJccRel32(CondCode::NE, okLabel);
        asm_.emitLeaRegRip(GpReg::RCX, ensureLiteral(""));
        markTemp(okLabel);
        asm_.emitXorEaxEax();
        asm_.emitMovRdxFromRax64();
        asm_.emitMovRegImm32(GpReg::R8, 10);
        asm_.emitCallIat(importLabel("msvcrt.dll", "strtol"));
        asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
        return true;
    }

    bool emitStringToFloat(std::int32_t inputDisp, std::int32_t resultDisp, DiagnosticBag& diagnostics) {
        (void)diagnostics;
        resetTemps();
        const std::string okLabel = tempLabel("strtod_ok");
        asm_.emitMovRcxFromMem64(GpReg::RBP, inputDisp);
        asm_.emitTestRcxRcx();
        asm_.emitJccRel32(CondCode::NE, okLabel);
        asm_.emitLeaRegRip(GpReg::RCX, ensureLiteral(""));
        markTemp(okLabel);
        asm_.emitXorEaxEax();
        asm_.emitMovRdxFromRax64();
        asm_.emitCallIat(importLabel("msvcrt.dll", "strtod"));
        asm_.emitMovsdMemFromXmm0(GpReg::RBP, resultDisp);
        return true;
    }

    bool emitToString(vir::Type fromType, std::int32_t inputDisp, std::int32_t resultDisp, DiagnosticBag& diagnostics) {
        resetTemps();
        if (fromType.kind == vir::TypeKind::String) {
            asm_.emitMovRaxFromMem64(GpReg::RBP, inputDisp);
            asm_.emitMovMemFromRax64(GpReg::RBP, resultDisp);
            return true;
        }
        if (fromType.kind == vir::TypeKind::Bool) {
            const std::string trueLabel = tempLabel("bool_to_string_true");
            const std::string doneLabel = tempLabel("bool_to_string_done");
            asm_.emitMovEaxFromMem32(GpReg::RBP, inputDisp);
            asm_.emitTestEaxEax();
            asm_.emitJccRel32(CondCode::NE, trueLabel);
            asm_.emitLeaRegRip(GpReg::RAX, ensureLiteral("false"));
            asm_.emitMovMemFromRax64(GpReg::RBP, resultDisp);
            asm_.emitJmpRel32(doneLabel);
            markTemp(trueLabel);
            asm_.emitLeaRegRip(GpReg::RAX, ensureLiteral("true"));
            asm_.emitMovMemFromRax64(GpReg::RBP, resultDisp);
            markTemp(doneLabel);
            return true;
        }
        const std::size_t bufferOff = layoutScratch(64);
        const std::size_t lenOff = layoutScratch(0);
        const std::size_t heapOff = layoutScratch(8);

        asm_.emitLeaRegRbpDisp(GpReg::RCX, -static_cast<std::int32_t>(bufferOff));
        if (fromType.kind == vir::TypeKind::Int32) {
            asm_.emitLeaRegRip(GpReg::RDX, ensureLiteral("%d"));
            asm_.emitMovEaxFromMem32(GpReg::RBP, inputDisp);
            asm_.emitMovR8dFromEax();
        } else if (fromType.kind == vir::TypeKind::Float32 || fromType.kind == vir::TypeKind::Float64) {
            asm_.emitLeaRegRip(GpReg::RDX, ensureLiteral("%.15g"));
            asm_.emitMovR8FromMem64(GpReg::RBP, inputDisp);
            asm_.emitMovsdXmm2FromMem64(GpReg::RBP, inputDisp);
        } else {
            diagnostics.error({}, "direct pe backend: ToString only supports int32, float32, float64, bool, and string");
            return false;
        }
        asm_.emitCallIat(importLabel("msvcrt.dll", "sprintf"));
        asm_.emitMovMemFromEax32(GpReg::RBP, -static_cast<std::int32_t>(lenOff));

        asm_.emitMovEaxFromMem32(GpReg::RBP, -static_cast<std::int32_t>(lenOff));
        asm_.emitBytes({0x48, 0x83, 0xC0, 0x01});
        asm_.emitMovRcxFromRax64();
        asm_.emitCallIat(importLabel("msvcrt.dll", "malloc"));
        asm_.emitMovMemFromRax64(GpReg::RBP, -static_cast<std::int32_t>(heapOff));

        asm_.emitMovRcxFromMem64(GpReg::RBP, -static_cast<std::int32_t>(heapOff));
        asm_.emitLeaRegRbpDisp(GpReg::RDX, -static_cast<std::int32_t>(bufferOff));
        asm_.emitMovEaxFromMem32(GpReg::RBP, -static_cast<std::int32_t>(lenOff));
        asm_.emitBytes({0x48, 0x83, 0xC0, 0x01}); // len+1 in rax
        asm_.emitMovR8FromRax();
        asm_.emitCallIat(importLabel("msvcrt.dll", "memcpy"));

        asm_.emitMovRaxFromMem64(GpReg::RBP, -static_cast<std::int32_t>(heapOff));
        asm_.emitMovMemFromRax64(GpReg::RBP, resultDisp);
        return true;
    }

    bool emitToBool(vir::Type fromType, std::int32_t inputDisp, std::int32_t resultDisp, DiagnosticBag& diagnostics) {
        resetTemps();
        switch (fromType.kind) {
            case vir::TypeKind::Int32:
                asm_.emitMovEaxFromMem32(GpReg::RBP, inputDisp);
                asm_.emitTestEaxEax();
                asm_.emitSetccAl(CondCode::NE);
                asm_.emitMovzxEaxAl();
                asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                return true;
            case vir::TypeKind::Float32:
            case vir::TypeKind::Float64:
                asm_.emitMovsdXmm0FromMem64(GpReg::RBP, inputDisp);
                asm_.emitXorpdXmm1Xmm1();
                asm_.emitUcomisdXmm0Xmm1();
                asm_.emitSetccAl(CondCode::NE);
                asm_.emitMovzxEaxAl();
                asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                return true;
            case vir::TypeKind::Bool:
                asm_.emitMovEaxFromMem32(GpReg::RBP, inputDisp);
                asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                return true;
            case vir::TypeKind::String: {
                const std::string nonNullLabel = tempLabel("bool_parse_nonnull");
                const std::string trueLabel = tempLabel("bool_true");
                const std::string falseLabel = tempLabel("bool_false");
                const std::string doneLabel = tempLabel("bool_done");
                const std::size_t canonicalOff = layoutScratch(40);
                asm_.emitMovRcxFromMem64(GpReg::RBP, inputDisp);
                asm_.emitTestRcxRcx();
                asm_.emitJccRel32(CondCode::NE, nonNullLabel);
                asm_.emitLeaRegRip(GpReg::RCX, ensureLiteral(""));
                markTemp(nonNullLabel);
                asm_.emitBytes({0x48, 0x89, 0x8D});
                asm_.emitU32(static_cast<std::uint32_t>(-static_cast<std::int32_t>(canonicalOff)));
                asm_.emitMovRcxFromMem64(GpReg::RBP, -static_cast<std::int32_t>(canonicalOff));
                asm_.emitLeaRegRip(GpReg::RDX, ensureLiteral("true"));
                asm_.emitCallIat(importLabel("msvcrt.dll", "strcmp"));
                asm_.emitTestEaxEax();
                asm_.emitJccRel32(CondCode::E, trueLabel);
                asm_.emitMovRcxFromMem64(GpReg::RBP, -static_cast<std::int32_t>(canonicalOff));
                asm_.emitLeaRegRip(GpReg::RDX, ensureLiteral("1"));
                asm_.emitCallIat(importLabel("msvcrt.dll", "strcmp"));
                asm_.emitTestEaxEax();
                asm_.emitJccRel32(CondCode::E, trueLabel);
                asm_.emitMovRcxFromMem64(GpReg::RBP, -static_cast<std::int32_t>(canonicalOff));
                asm_.emitLeaRegRip(GpReg::RDX, ensureLiteral("false"));
                asm_.emitCallIat(importLabel("msvcrt.dll", "strcmp"));
                asm_.emitTestEaxEax();
                asm_.emitJccRel32(CondCode::E, falseLabel);
                asm_.emitMovRcxFromMem64(GpReg::RBP, -static_cast<std::int32_t>(canonicalOff));
                asm_.emitLeaRegRip(GpReg::RDX, ensureLiteral("0"));
                asm_.emitCallIat(importLabel("msvcrt.dll", "strcmp"));
                asm_.emitTestEaxEax();
                asm_.emitJccRel32(CondCode::E, falseLabel);
                asm_.emitMovRegImm32(GpReg::RAX, 0);
                asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                asm_.emitJmpRel32(doneLabel);
                markTemp(trueLabel);
                asm_.emitMovRegImm32(GpReg::RAX, 1);
                asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                asm_.emitJmpRel32(doneLabel);
                markTemp(falseLabel);
                asm_.emitMovRegImm32(GpReg::RAX, 0);
                asm_.emitMovMemFromEax32(GpReg::RBP, resultDisp);
                markTemp(doneLabel);
                return true;
            }
            default:
                diagnostics.error({}, "direct pe backend: unsupported ToBool source type");
                return false;
        }
    }

    bool emitTerminator(const vir::Terminator& terminator, const FunctionLayout& layout, DiagnosticBag& diagnostics) {
        switch (terminator.kind) {
            case vir::Terminator::Kind::Return: {
                const auto& term = std::get<vir::ReturnTerminator>(terminator.data);
                if (term.value.has_value()) {
                    const auto value = layout.frame.values.find(term.value->index);
                    if (value == layout.frame.values.end()) {
                        diagnostics.error({}, "direct pe backend: missing return value slot");
                        return false;
                    }
                    const std::int32_t disp = -static_cast<std::int32_t>(value->second.offset);
                    if (term.valueType.kind == vir::TypeKind::String) {
                        asm_.emitMovRaxFromMem64(GpReg::RBP, disp);
                    } else if (term.valueType.kind == vir::TypeKind::Float32 || term.valueType.kind == vir::TypeKind::Float64) {
                        asm_.emitMovsdXmm0FromMem64(GpReg::RBP, disp);
                    } else {
                        asm_.emitMovEaxFromMem32(GpReg::RBP, disp);
                    }
                } else {
                    asm_.emitXorEaxEax();
                }
                asm_.emitLeave();
                asm_.emitRet();
                return true;
            }
            case vir::Terminator::Kind::Branch: {
                const auto& term = std::get<vir::BranchTerminator>(terminator.data);
                asm_.emitJmpRel32(blockLabel(layout.function->name, term.target.index));
                return true;
            }
            case vir::Terminator::Kind::CondBranch: {
                const auto& term = std::get<vir::CondBranchTerminator>(terminator.data);
                const auto cond = layout.frame.values.find(term.condition.index);
                if (cond == layout.frame.values.end()) {
                    diagnostics.error({}, "direct pe backend: missing condition slot");
                    return false;
                }
                const std::int32_t disp = -static_cast<std::int32_t>(cond->second.offset);
                asm_.emitMovEaxFromMem32(GpReg::RBP, disp);
                asm_.emitTestEaxEax();
                asm_.emitJccRel32(CondCode::NE, blockLabel(layout.function->name, term.trueBlock.index));
                asm_.emitJmpRel32(blockLabel(layout.function->name, term.falseBlock.index));
                return true;
            }
            case vir::Terminator::Kind::Unreachable:
                asm_.emitXorEaxEax();
                asm_.emitLeave();
                asm_.emitRet();
                return true;
        }
        return false;
    }

    std::string functionLabel(const std::string& name) const {
        return "__fn_" + sanitizeIdentifier(name);
    }

    std::string blockLabel(const std::string& function, std::uint32_t blockIndex) const {
        return "__blk_" + sanitizeIdentifier(function) + "_" + std::to_string(blockIndex);
    }

    std::string importLabel(const std::string& dll, const std::string& name) const {
        return "__iat_" + sanitizeIdentifier(dll) + "_" + sanitizeIdentifier(name);
    }

    std::string ensureLiteral(const std::string& value) {
        return image_.addStringLiteral(value);
    }

    void resetTemps() {
        tempLabels_.clear();
    }

    std::string scratchLabel(const std::string& name) {
        const auto existing = scratch_.find(name);
        if (existing != scratch_.end()) {
            return existing->second;
        }
        const std::string label = "__scratch_" + sanitizeIdentifier(name);
        const std::size_t offset = image_.rdata().size();
        if (name == "buffer") {
            image_.rdata().reserve(128);
        } else {
            image_.rdata().reserve(8);
        }
        image_.rdata().defineLabel(label);
        image_.defineSymbol(label, SectionId::Rdata, offset);
        scratch_[name] = label;
        return label;
    }

    std::size_t layoutScratch(std::size_t offset) const {
        return layoutScratchBase() + offset;
    }

    std::size_t layoutScratchBase() const {
        return 32;
    }

    void moveIntegerArg(std::size_t index) {
        switch (index) {
            case 0: asm_.emitMovRcxFromRax64(); break;
            case 1: asm_.emitMovRdxFromRax(); break;
            case 2: asm_.emitMovR8FromRax(); break;
            case 3: asm_.emitBytes({0x49, 0x89, 0xC1}); break;
        }
    }

    void moveFloatArg(std::size_t index, std::int32_t disp) {
        switch (index) {
            case 0: asm_.emitMovsdXmm0FromMem64(GpReg::RBP, disp); break;
            case 1: asm_.emitMovsdXmm1FromMem64(GpReg::RBP, disp); break;
            case 2: asm_.emitMovsdXmm2FromMem64(GpReg::RBP, disp); break;
            case 3: asm_.emitMovsdXmm3FromMem64(GpReg::RBP, disp); break;
        }
    }

    void moveStringArg(std::size_t index) {
        moveIntegerArg(index);
    }

    void markTemp(const std::string& label) {
        image_.defineSymbol(label, SectionId::Text, asm_.currentOffset());
        asm_.markLabel(label, symbols_);
    }

    std::string tempLabel(const std::string& prefix) {
        auto& label = tempLabels_[prefix];
        if (label.empty()) {
            label = "__tmp_" + prefix + "_" + std::to_string(tempCounter_++);
        }
        return label;
    }

    BackendOptions options_;
    PeImage image_;
    Assembler asm_;
    std::unordered_map<std::string, Symbol> symbols_;
    std::unordered_map<std::string, std::string> externImports_;
    std::unordered_map<std::string, std::string> scratch_;
    std::unordered_map<std::string, std::string> tempLabels_;
    std::size_t tempCounter_ = 0;
};

} // namespace

const char* PeX64Backend::id() const {
    return "pe-x64";
}

BackendFlavor PeX64Backend::flavor() const {
    return BackendFlavor::X64Coff;
}

bool PeX64Backend::supportsOutput(BackendOutputKind output) const {
    return output == BackendOutputKind::Executable;
}

BackendResult PeX64Backend::compile(const vir::Module& module, const BackendOptions& options) const {
#ifdef _WIN32
    if (options.output != BackendOutputKind::Executable) {
        BackendResult result;
        result.diagnostics.error({}, "direct pe backend: only executable output is supported");
        return result;
    }
    WindowsPeX64Emitter emitter(options);
    return emitter.emit(module);
#else
    BackendResult result;
    result.diagnostics.error({}, "direct pe backend: Windows-only backend");
    return result;
#endif
}

std::unique_ptr<IBackend> createPeX64Backend() {
    return std::make_unique<PeX64Backend>();
}
