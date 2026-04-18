#include "backend_pe_x64.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <cstring>
#include <memory>
#include <optional>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

enum class SectionId {
    Text,
    Rdata,
    Idata
};

enum class FixupKind {
    Rel32,
    RipDisp32
};

struct Symbol {
    SectionId section = SectionId::Text;
    std::size_t offset = 0;
};

struct Fixup {
    SectionId section = SectionId::Text;
    std::size_t offset = 0;
    std::string target;
    std::size_t instructionSize = 0;
    FixupKind kind = FixupKind::Rel32;
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
    PeImage() : imports_(idata_) {}

    SectionBuilder& text() { return text_; }
    SectionBuilder& rdata() { return rdata_; }
    SectionBuilder& idata() { return idata_; }
    ImportTableBuilder& imports() { return imports_; }

    void defineSymbol(const std::string& name, SectionId section, std::size_t offset) {
        symbols_[name] = Symbol{section, offset};
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
        defineSymbol(label, SectionId::Rdata, offset);
        stringLabels_[value] = label;
        return label;
    }

    void addFixup(SectionId section, std::size_t offset, const std::string& target, std::size_t instructionSize, FixupKind kind) {
        fixups_.push_back(Fixup{section, offset, target, instructionSize, kind});
    }

    void finalizeImports() {
        imports_.emit();
        for (const auto& [label, offset] : imports_.iatOffsets()) {
            defineSymbol(label, SectionId::Idata, offset);
        }
    }

    std::string writeBinary(std::uint32_t entryOffset) {
        finalizeImports();

        const std::size_t fileAlignment = 0x200;
        const std::size_t sectionAlignment = 0x1000;
        const std::size_t headersSize = alignTo(0x80 + 4 + 20 + 240 + 3 * 40, fileAlignment);

        const std::size_t textRawSize = alignTo(text_.size(), fileAlignment);
        const std::size_t rdataRawSize = alignTo(rdata_.size(), fileAlignment);
        const std::size_t idataRawSize = alignTo(idata_.size(), fileAlignment);

        const std::uint32_t textRva = static_cast<std::uint32_t>(alignTo(headersSize, sectionAlignment));
        const std::uint32_t rdataRva = static_cast<std::uint32_t>(alignTo(textRva + static_cast<std::uint32_t>(alignTo(text_.size(), sectionAlignment)), sectionAlignment));
        const std::uint32_t idataRva = static_cast<std::uint32_t>(alignTo(rdataRva + static_cast<std::uint32_t>(alignTo(rdata_.size(), sectionAlignment)), sectionAlignment));

        patchImports(idataRva);
        patchCodeFixups(textRva, rdataRva, idataRva);

        const std::uint32_t textRaw = static_cast<std::uint32_t>(headersSize);
        const std::uint32_t rdataRaw = textRaw + static_cast<std::uint32_t>(textRawSize);
        const std::uint32_t idataRaw = rdataRaw + static_cast<std::uint32_t>(rdataRawSize);
        const std::size_t imageSize = alignTo(idataRva + alignTo(idata_.size(), sectionAlignment), sectionAlignment);
        const std::uint32_t importDirectoryRva = idataRva;
        const std::uint32_t importDirectorySize = static_cast<std::uint32_t>(idata_.size());

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
        appendU16(header, 3);
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
        appendU32(header, textRva + entryOffset);
        appendU32(header, textRva);
        appendU64(header, 0x0000000140000000ULL);
        appendU32(header, 0x1000);
        appendU32(header, 0x200);
        appendU16(header, 6);
        appendU16(header, 0);
        appendU16(header, 6);
        appendU16(header, 0);
        appendU16(header, 6);
        appendU16(header, 0);
        appendU32(header, 0);
        appendU32(header, static_cast<std::uint32_t>(imageSize));
        appendU32(header, static_cast<std::uint32_t>(headersSize));
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
        appendU32(header, importDirectoryRva);
        appendU32(header, importDirectorySize);
        for (int i = 2; i < 16; ++i) {
            appendU32(header, 0);
            appendU32(header, 0);
        }

        auto appendSectionHeader = [&](const char* name, std::uint32_t virtualSize, std::uint32_t virtualAddress,
                                       std::uint32_t rawSize, std::uint32_t rawPointer, std::uint32_t characteristics) {
            char secName[8]{};
            std::memcpy(secName, name, std::min<std::size_t>(7, std::strlen(name)));
            header.insert(header.end(), secName, secName + 8);
            appendU32(header, virtualSize);
            appendU32(header, virtualAddress);
            appendU32(header, rawSize);
            appendU32(header, rawPointer);
            appendU32(header, 0);
            appendU32(header, 0);
            appendU16(header, 0);
            appendU16(header, 0);
            appendU32(header, characteristics);
        };

        appendSectionHeader(".text", static_cast<std::uint32_t>(text_.size()), textRva, static_cast<std::uint32_t>(textRawSize), textRaw, 0x60000020);
        appendSectionHeader(".rdata", static_cast<std::uint32_t>(rdata_.size()), rdataRva, static_cast<std::uint32_t>(rdataRawSize), rdataRaw, 0x40000040);
        appendSectionHeader(".idata", static_cast<std::uint32_t>(idata_.size()), idataRva, static_cast<std::uint32_t>(idataRawSize), idataRaw, 0xC0000040);
        header.resize(headersSize, 0);

        std::vector<std::uint8_t> binary;
        binary.reserve(headersSize + textRawSize + rdataRawSize + idataRawSize);
        binary.insert(binary.end(), header.begin(), header.end());
        appendPadded(binary, text_.bytes(), textRawSize);
        appendPadded(binary, rdata_.bytes(), rdataRawSize);
        appendPadded(binary, idata_.bytes(), idataRawSize);
        return std::string(reinterpret_cast<const char*>(binary.data()), binary.size());
    }

    std::vector<Fixup>& fixups() { return fixups_; }

private:
    static void appendPadded(std::vector<std::uint8_t>& out, const std::vector<std::uint8_t>& bytes, std::size_t rawSize) {
        out.insert(out.end(), bytes.begin(), bytes.end());
        out.resize(out.size() + (rawSize - bytes.size()), 0);
    }

    void patchImports(std::uint32_t idataRva) {
        imports_.patch(idataRva);
    }

    void patchCodeFixups(std::uint32_t textRva, std::uint32_t rdataRva, std::uint32_t idataRva) {
        for (const auto& fixup : fixups_) {
            const Symbol& sym = symbol(fixup.target);
            const std::uint32_t targetRva = (sym.section == SectionId::Text ? textRva : sym.section == SectionId::Rdata ? rdataRva : idataRva) +
                                            static_cast<std::uint32_t>(sym.offset);
            const std::uint32_t sectionRva = (fixup.section == SectionId::Text ? textRva : fixup.section == SectionId::Rdata ? rdataRva : idataRva);
            const std::uint32_t siteRva = sectionRva + static_cast<std::uint32_t>(fixup.offset);
            const std::uint32_t nextRva = siteRva + 4;
            const std::uint32_t disp = static_cast<std::uint32_t>(static_cast<std::int64_t>(targetRva) - static_cast<std::int64_t>(nextRva));
            if (fixup.section == SectionId::Text) {
                patchU32(text_.bytes(), fixup.offset, disp);
            }
        }
    }

    SectionBuilder text_{SectionId::Text};
    SectionBuilder rdata_{SectionId::Rdata};
    SectionBuilder idata_{SectionId::Idata};
    ImportTableBuilder imports_;
    std::unordered_map<std::string, Symbol> symbols_;
    std::unordered_map<std::string, std::string> stringLabels_;
    std::vector<Fixup> fixups_;
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
        : options_(options), asm_(image_.text(), image_.fixups()) {
        registerImports();
    }

    BackendResult emit(const vir::Module& module) {
        BackendResult result;
        if (!emitModule(module, result.diagnostics)) {
            return result;
        }
        const std::string entryLabel = "__voltis_entry";
        const std::uint32_t entryOffset = static_cast<std::uint32_t>(image_.symbol(entryLabel).offset);
        const std::string payload = image_.writeBinary(entryOffset);
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

    bool emitEntryStub(const vir::Function& mainFn, DiagnosticBag& diagnostics) {
        (void)diagnostics;
        resetTemps();
        const std::string label = "__voltis_entry";
        image_.defineSymbol(label, SectionId::Text, asm_.currentOffset());
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
        image_.defineSymbol(fnLabel, SectionId::Text, asm_.currentOffset());
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

        const std::string label = functionLabel(inst.callee);
        asm_.emitCallRel32(label);

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
