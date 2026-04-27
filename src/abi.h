#pragma once

#include "target.h"
#include <optional>
#include <string>
#include <vector>

struct CallingConventionDescriptor {
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

std::optional<CallingConventionDescriptor> callingConventionForTarget(const TargetDescription& target);

