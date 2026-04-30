#pragma once

#include "target.h"
#include <optional>
#include <string>
#include <string_view>
#include <vector>

enum class CallingConvention {
    MicrosoftX64,
    SystemVAMD64,
    Win32Cdecl,
    Win32Stdcall,
    Win32Fastcall,
    AArch64AAPCS,
    Unknown
};

struct CallingConventionDescriptor {
    CallingConvention convention = CallingConvention::Unknown;
    std::string name;
    std::vector<std::string> integerArgRegisters;
    std::vector<std::string> floatArgRegisters;
    std::string integerReturnRegister;
    std::string floatReturnRegister;
    std::vector<std::string> calleeSavedRegisters;
    std::vector<std::string> callerSavedRegisters;
    std::uint32_t stackAlignment = 16;
    bool shadowSpaceRequired = false;
};

CallingConvention parseCallingConvention(std::string_view name);
std::string_view callingConventionName(CallingConvention cc);
std::optional<CallingConventionDescriptor> callingConventionForTarget(const TargetDescription& target);
