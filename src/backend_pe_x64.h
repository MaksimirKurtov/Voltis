#pragma once

#include "backend.h"
#include <memory>

class PeX64Backend final : public IBackend {
public:
    const char* id() const override;
    BackendFlavor flavor() const override;
    bool supportsOutput(BackendOutputKind output) const override;
    BackendResult compile(const vir::Module& module, const BackendOptions& options) const override;
};

std::unique_ptr<IBackend> createPeX64Backend();
