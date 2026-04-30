#include "emit/elf_writer.h"

#include "executable_format_emit_utils.h"

#include <cstring>

namespace {

template <typename T>
void appendStruct(std::vector<std::uint8_t>& out, const T& value) {
    const auto* raw = reinterpret_cast<const std::uint8_t*>(&value);
    out.insert(out.end(), raw, raw + sizeof(T));
}

std::size_t alignUp(std::size_t value, std::size_t align) {
    const std::size_t rem = value % align;
    return rem == 0 ? value : value + (align - rem);
}

} // namespace

std::vector<std::uint8_t> ELFWriter::writeExecutable(const NativeProgramImage& image,
                                                     const std::vector<std::uint8_t>& data,
                                                     const std::vector<std::uint8_t>& rodata,
                                                     const std::vector<Relocation>&) const {
    struct Elf64_EhdrLocal {
        unsigned char e_ident[16];
        std::uint16_t e_type;
        std::uint16_t e_machine;
        std::uint32_t e_version;
        std::uint64_t e_entry;
        std::uint64_t e_phoff;
        std::uint64_t e_shoff;
        std::uint32_t e_flags;
        std::uint16_t e_ehsize;
        std::uint16_t e_phentsize;
        std::uint16_t e_phnum;
        std::uint16_t e_shentsize;
        std::uint16_t e_shnum;
        std::uint16_t e_shstrndx;
    };

    struct Elf64_PhdrLocal {
        std::uint32_t p_type;
        std::uint32_t p_flags;
        std::uint64_t p_offset;
        std::uint64_t p_vaddr;
        std::uint64_t p_paddr;
        std::uint64_t p_filesz;
        std::uint64_t p_memsz;
        std::uint64_t p_align;
    };

    const std::size_t headerSize = sizeof(Elf64_EhdrLocal) + sizeof(Elf64_PhdrLocal);
    const std::size_t textOffset = alignUp(headerSize, 0x1000);
    const std::size_t dataOffset = alignUp(textOffset + image.textBytes.size(), 0x10);
    const std::size_t roOffset = alignUp(dataOffset + data.size(), 0x10);
    const std::uint64_t baseVaddr = pie_ ? 0 : 0x400000;

    std::vector<std::uint8_t> out;
    out.resize(textOffset, 0);

    Elf64_EhdrLocal eh{};
    eh.e_ident[0] = 0x7F; eh.e_ident[1] = 'E'; eh.e_ident[2] = 'L'; eh.e_ident[3] = 'F';
    eh.e_ident[4] = 2; eh.e_ident[5] = 1; eh.e_ident[6] = 1;
    eh.e_type = pie_ ? 3 : 2; // ET_DYN / ET_EXEC
    eh.e_machine = 0x3E;
    eh.e_version = 1;
    eh.e_entry = baseVaddr + textOffset + image.entryOffset;
    eh.e_phoff = sizeof(Elf64_EhdrLocal);
    eh.e_ehsize = sizeof(Elf64_EhdrLocal);
    eh.e_phentsize = sizeof(Elf64_PhdrLocal);
    eh.e_phnum = 1;

    Elf64_PhdrLocal ph{};
    ph.p_type = 1;
    ph.p_flags = 0x5;
    ph.p_offset = textOffset;
    ph.p_vaddr = baseVaddr + textOffset;
    ph.p_paddr = ph.p_vaddr;
    ph.p_filesz = image.textBytes.size();
    ph.p_memsz = image.textBytes.size();
    ph.p_align = 0x1000;

    std::memcpy(out.data(), &eh, sizeof(eh));
    std::memcpy(out.data() + sizeof(eh), &ph, sizeof(ph));

    out.insert(out.end(), image.textBytes.begin(), image.textBytes.end());
    out.resize(dataOffset, 0);
    out.insert(out.end(), data.begin(), data.end());
    out.resize(roOffset, 0);
    out.insert(out.end(), rodata.begin(), rodata.end());

    return out;
}
