#pragma once

#include "diagnostics.h"
#include "vir.h"
#include <optional>
#include <string>
#include <vector>

enum class BackendFlavor {
    Llvm,
    X64Coff
};

enum class BackendOutputKind {
    LlvmIrText,
    ObjectFile,
    Executable
};

enum class BackendTrack {
    ProductionDirected,
    TemporaryScaffolding
};

struct BackendOptions {
    BackendFlavor flavor = BackendFlavor::Llvm;
    BackendOutputKind output = BackendOutputKind::LlvmIrText;
    BackendTrack track = BackendTrack::ProductionDirected;
    std::string moduleName = "voltis_module";
    std::string targetTriple = "x86_64-pc-windows-msvc";
    std::optional<std::string> targetDataLayout;
};

struct BackendArtifact {
    BackendOutputKind kind = BackendOutputKind::LlvmIrText;
    std::string name;
    std::string payload;
    bool temporaryScaffolding = false;
};

struct BackendResult {
    std::vector<BackendArtifact> artifacts;
    DiagnosticBag diagnostics;

    bool ok() const { return !diagnostics.hasErrors(); }
};

class IBackend {
public:
    virtual ~IBackend() = default;

    virtual const char* id() const = 0;
    virtual BackendFlavor flavor() const = 0;
    virtual bool supportsOutput(BackendOutputKind output) const = 0;
    virtual BackendResult compile(const vir::Module& module, const BackendOptions& options) const = 0;
};
