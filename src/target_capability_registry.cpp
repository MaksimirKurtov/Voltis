#include "target_capability_registry.h"

#include <algorithm>

namespace TargetCapabilityRegistry {

bool supportsNativeEmit(const TargetTriple& triple) {
    return readinessFor(triple) != BackendReadiness::Planned;
}

bool supportsFormat(const TargetTriple& triple, BinaryFormat format) {
    const auto formats = supportedFormatsFor(triple);
    return std::find(formats.begin(), formats.end(), format) != formats.end();
}

BackendReadiness readinessFor(const TargetTriple& triple) {
    const auto desc = describeTarget(triple);
    if (!desc.has_value()) {
        return BackendReadiness::Planned;
    }
    return desc->readiness;
}

std::vector<BinaryFormat> supportedFormatsFor(const TargetTriple& triple) {
    const auto desc = describeTarget(triple);
    if (!desc.has_value()) {
        return {};
    }
    return desc->supportedFormats;
}

} // namespace TargetCapabilityRegistry
