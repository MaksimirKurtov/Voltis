#pragma once

#include "target.h"

#include <vector>

namespace TargetCapabilityRegistry {

bool supportsNativeEmit(const TargetTriple& triple);
bool supportsFormat(const TargetTriple& triple, BinaryFormat format);
BackendReadiness readinessFor(const TargetTriple& triple);
std::vector<BinaryFormat> supportedFormatsFor(const TargetTriple& triple);

} // namespace TargetCapabilityRegistry
