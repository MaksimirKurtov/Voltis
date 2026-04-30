#include "abi.h"
#include "file_utils.h"
#include "target.h"
#include "target_capability_registry.h"

#include <cassert>
#include <array>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <regex>

int main() {
    const std::string host = canonicalTargetTripleForHost();
    assert(!host.empty());
    assert(std::regex_match(host, std::regex("^[a-z0-9_]+-[a-z0-9_]+-[a-z0-9_]+(-[a-z0-9_]+)?$")));

    const auto hostParsed = parseTargetTriple(host);
    assert(hostParsed.has_value());

    assert(parseCallingConvention("ms_x64_abi") == CallingConvention::MicrosoftX64);
    assert(parseCallingConvention("sysv_amd64") == CallingConvention::SystemVAMD64);
    assert(parseCallingConvention("cdecl") == CallingConvention::Win32Cdecl);
    assert(parseCallingConvention("__stdcall") == CallingConvention::Win32Stdcall);
    assert(parseCallingConvention("__fastcall") == CallingConvention::Win32Fastcall);
    assert(parseCallingConvention("aapcs64") == CallingConvention::AArch64AAPCS);
    assert(parseCallingConvention("MS_X64_ABI") == CallingConvention::MicrosoftX64);
    assert(parseCallingConvention("__CDECL") == CallingConvention::Win32Cdecl);

    assert(callingConventionName(CallingConvention::MicrosoftX64) == std::string_view("microsoft_x64"));
    assert(callingConventionName(CallingConvention::SystemVAMD64) == std::string_view("systemv_amd64"));
    assert(callingConventionName(CallingConvention::Win32Cdecl) == std::string_view("win32_cdecl"));
    assert(callingConventionName(CallingConvention::Win32Stdcall) == std::string_view("win32_stdcall"));
    assert(callingConventionName(CallingConvention::Win32Fastcall) == std::string_view("win32_fastcall"));
    assert(callingConventionName(CallingConvention::AArch64AAPCS) == std::string_view("aarch64_aapcs"));
    assert(callingConventionName(CallingConvention::Unknown) == std::string_view("unknown"));

    const auto x64 = parseTargetTriple("x86_64-pc-linux-gnu");
    const auto a64 = parseTargetTriple("aarch64-pc-linux-gnu");
    const auto planned = parseTargetTriple("riscv64-pc-linux-gnu");
    assert(x64.has_value() && a64.has_value() && planned.has_value());

    assert(TargetCapabilityRegistry::readinessFor(*x64) == BackendReadiness::Production);
    assert(TargetCapabilityRegistry::readinessFor(*a64) == BackendReadiness::Experimental);
    assert(TargetCapabilityRegistry::readinessFor(*planned) == BackendReadiness::Planned);

    assert(TargetCapabilityRegistry::supportsNativeEmit(*x64));
    assert(TargetCapabilityRegistry::supportsNativeEmit(*a64));
    assert(!TargetCapabilityRegistry::supportsNativeEmit(*planned));

    assert(TargetCapabilityRegistry::supportsFormat(*x64, BinaryFormat::Elf));
    assert(!TargetCapabilityRegistry::supportsFormat(*a64, BinaryFormat::Pe32Plus));

    {
        const std::array<CallingConvention, 7> all = {
            CallingConvention::MicrosoftX64,
            CallingConvention::SystemVAMD64,
            CallingConvention::Win32Cdecl,
            CallingConvention::Win32Stdcall,
            CallingConvention::Win32Fastcall,
            CallingConvention::AArch64AAPCS,
            CallingConvention::Unknown
        };
        for (const CallingConvention cc : all) {
            const std::string_view name = callingConventionName(cc);
            const CallingConvention reparsed = parseCallingConvention(name);
            if (cc == CallingConvention::Unknown) {
                assert(reparsed == CallingConvention::Unknown);
            } else {
                assert(reparsed == cc);
            }
        }
    }

    {
        namespace fs = std::filesystem;
        const fs::path dir = fs::temp_directory_path() / "voltis_unit_file_utils";
        fs::create_directories(dir);
        const fs::path out = dir / "safe-write.bin";
        safeWriteFile(out, std::vector<std::uint8_t>{'o', 'n', 'e'});
        safeWriteFile(out, std::vector<std::uint8_t>{'t', 'w', 'o'});
        std::ifstream in(out, std::ios::binary);
        std::string contents((std::istreambuf_iterator<char>(in)), std::istreambuf_iterator<char>());
        assert(contents == "two");
        fs::remove(out);
    }

    std::cout << "unit tests passed\n";
    return 0;
}
