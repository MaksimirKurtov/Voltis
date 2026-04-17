#include "backend_llvm_ir.h"
#include "codegen.h"
#include "lexer.h"
#include "lowering.h"
#include "parser.h"
#include "sema.h"
#include "vir.h"
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace fs = std::filesystem;

static std::string readFile(const fs::path& path) {
    std::ifstream input(path, std::ios::binary);
    if (!input) {
        throw std::runtime_error("Could not open input file: " + path.string());
    }
    std::ostringstream buffer;
    buffer << input.rdbuf();
    return buffer.str();
}

static void writeFile(const fs::path& path, const std::string& data) {
    std::ofstream output(path, std::ios::binary);
    if (!output) {
        throw std::runtime_error("Could not write output file: " + path.string());
    }
    output << data;
}

static std::string shellQuote(const std::string& value) {
#ifdef _WIN32
    return "\"" + value + "\"";
#else
    return "'" + value + "'";
#endif
}

static std::string compilerCommand(const fs::path& cppFile, const fs::path& outputFile) {
    const char* envCompiler = std::getenv("VOLTIS_CXX");
    if (envCompiler && *envCompiler) {
        return std::string(envCompiler) + " -std=c++17 " + shellQuote(cppFile.string()) + " -o " + shellQuote(outputFile.string());
    }
    return "g++ -std=c++17 " + shellQuote(cppFile.string()) + " -o " + shellQuote(outputFile.string());
}

struct CliOptions {
    fs::path inputPath;
    std::optional<fs::path> outputPath;
    std::optional<fs::path> emitCppPath;
    bool emitVir = false;
    bool emitLlvm = false;
    bool emitObj = false;
    bool noLink = false;
    bool bootstrapCpp = false;
};

struct NativeToolchain {
    std::string clangExecutable = "clang";
    std::optional<fs::path> runtimeLibraryPath;
};

static fs::path defaultArtifactPath(const fs::path& inputPath, const std::string& extension) {
    return inputPath.parent_path() / (inputPath.stem().string() + extension);
}

static fs::path defaultExecutablePath(const fs::path& inputPath) {
#ifdef _WIN32
    return defaultArtifactPath(inputPath, ".exe");
#else
    return inputPath.parent_path() / inputPath.stem();
#endif
}

static std::string objectExtension() {
#ifdef _WIN32
    return ".obj";
#else
    return ".o";
#endif
}

static bool runProbeCommand(const std::string& command) {
    return std::system(command.c_str()) == 0;
}

static bool commandAvailable(const std::string& executable) {
#ifdef _WIN32
    const std::string command = shellQuote(executable) + " --version >nul 2>&1";
#else
    const std::string command = shellQuote(executable) + " --version >/dev/null 2>&1";
#endif
    return runProbeCommand(command);
}

static void runCommandOrThrow(const std::string& command, const std::string& failureMessage) {
    std::cout << "Invoking: " << command << "\n";
    int code = std::system(command.c_str());
    if (code != 0) {
        throw std::runtime_error(failureMessage + " (exit code " + std::to_string(code) + ")");
    }
}

static std::optional<fs::path> resolveRuntimeLibrary(const fs::path& executablePath) {
    if (const char* envRuntime = std::getenv("VOLTIS_RUNTIME_LIB")) {
        fs::path candidate(envRuntime);
        if (fs::exists(candidate)) {
            return candidate;
        }
    }

    const fs::path exeDir = fs::absolute(executablePath).parent_path();
    const std::vector<fs::path> candidates = {
        exeDir / "voltis_runtime.lib",
        exeDir / "libvoltis_runtime.a",
        exeDir / "voltis_runtime.a",
        exeDir.parent_path() / "voltis_runtime.lib",
        exeDir.parent_path() / "libvoltis_runtime.a",
        exeDir.parent_path() / "voltis_runtime.a"
    };

    for (const auto& candidate : candidates) {
        if (fs::exists(candidate)) {
            return candidate;
        }
    }

    return std::nullopt;
}

static NativeToolchain resolveNativeToolchain(const fs::path& executablePath, bool requireRuntimeLibrary) {
    NativeToolchain toolchain;
    if (const char* envClang = std::getenv("VOLTIS_CLANG")) {
        if (*envClang) {
            toolchain.clangExecutable = envClang;
        }
    }

    if (!commandAvailable(toolchain.clangExecutable)) {
        throw std::runtime_error(
            "Native toolchain unavailable: clang was not found. Install clang/LLVM (or set VOLTIS_CLANG), "
            "or use --emit-llvm/--emit-vir.");
    }

    if (requireRuntimeLibrary) {
        const auto runtimeLibrary = resolveRuntimeLibrary(executablePath);
        if (!runtimeLibrary.has_value()) {
            throw std::runtime_error(
                "Native link stage unavailable: voltis_runtime library not found next to voltisc. "
                "Build the voltis_runtime CMake target or set VOLTIS_RUNTIME_LIB.");
        }
        toolchain.runtimeLibraryPath = *runtimeLibrary;
    }
    return toolchain;
}

static void printUsage() {
    std::cout << "voltisc <input.vlt> [options]\n"
              << "Default: native compile (LLVM IR -> object -> executable) when clang/LLVM toolchain is available.\n"
              << "Options:\n"
              << "  -o <path>          Output artifact path (single artifact mode or default executable)\n"
              << "  --emit-vir         Emit VIR text (.vir)\n"
              << "  --emit-llvm        Emit LLVM IR text (.ll)\n"
              << "  --emit-obj         Emit native object (.obj/.o) via clang from LLVM IR\n"
              << "  --bootstrap-cpp    Use temporary C++ bootstrap backend (explicit only)\n"
              << "  --emit-cpp <path>  C++ output path (requires --bootstrap-cpp)\n"
              << "  --no-link          Skip host C++ compile (requires --bootstrap-cpp)\n";
}

static CliOptions parseCliOptions(int argc, char** argv) {
    CliOptions options;
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-o") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for -o");
            }
            options.outputPath = fs::path(argv[++i]);
            continue;
        }
        if (arg == "--emit-cpp") {
            if (i + 1 >= argc) {
                throw std::runtime_error("Missing value for --emit-cpp");
            }
            options.emitCppPath = fs::path(argv[++i]);
            continue;
        }
        if (arg == "--emit-vir") {
            options.emitVir = true;
            continue;
        }
        if (arg == "--emit-llvm") {
            options.emitLlvm = true;
            continue;
        }
        if (arg == "--emit-obj") {
            options.emitObj = true;
            continue;
        }
        if (arg == "--bootstrap-cpp") {
            options.bootstrapCpp = true;
            continue;
        }
        if (arg == "--no-link") {
            options.noLink = true;
            continue;
        }
        if (!arg.empty() && arg[0] != '-') {
            if (!options.inputPath.empty()) {
                throw std::runtime_error("Only one input file is supported");
            }
            options.inputPath = fs::path(arg);
            continue;
        }
        throw std::runtime_error("Unknown argument: " + arg);
    }

    if (options.inputPath.empty()) {
        throw std::runtime_error("No input .vlt file supplied");
    }
    if (options.noLink && !options.bootstrapCpp) {
        throw std::runtime_error("--no-link is only valid with --bootstrap-cpp");
    }
    if (options.emitCppPath.has_value() && !options.bootstrapCpp) {
        throw std::runtime_error("--emit-cpp requires --bootstrap-cpp");
    }
    if (options.bootstrapCpp && (options.emitVir || options.emitLlvm || options.emitObj)) {
        throw std::runtime_error("--bootstrap-cpp cannot be combined with --emit-vir/--emit-llvm/--emit-obj");
    }

    const int explicitEmitCount =
        static_cast<int>(options.emitVir) +
        static_cast<int>(options.emitLlvm) +
        static_cast<int>(options.emitObj);

    if (options.outputPath.has_value() && explicitEmitCount > 1) {
        throw std::runtime_error("-o cannot be used when emitting multiple artifacts");
    }

    return options;
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            printUsage();
            return 1;
        }

        const CliOptions options = parseCliOptions(argc, argv);

        const std::string source = readFile(options.inputPath);
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        Program program = parser.parseProgram();

        SemanticAnalyzer sema;
        if (!sema.analyze(program)) {
            sema.diagnostics().print(std::cerr);
            return 1;
        }

        VIRLowerer lowerer;
        LoweringResult lowered = lowerer.lower(program, sema);
        if (!lowered.ok()) {
            lowered.diagnostics.print(std::cerr);
            return 1;
        }

        if (options.bootstrapCpp) {
            std::cout << "Using bootstrap C++ backend (temporary scaffolding, not production direction).\n";
            CodeGenerator generator;
            const std::string cppSource = generator.generate(program);
            const fs::path generatedCpp = options.emitCppPath.has_value()
                ? *options.emitCppPath
                : defaultArtifactPath(options.inputPath, ".generated.cpp");
            writeFile(generatedCpp, cppSource);
            std::cout << "Generated C++: " << generatedCpp.string() << "\n";

            if (!options.noLink) {
                const fs::path executablePath = options.outputPath.value_or(fs::path("a.exe"));
                const std::string command = compilerCommand(generatedCpp, executablePath);
                runCommandOrThrow(command, "Native C++ compiler failed");
                std::cout << "Built executable: " << executablePath.string() << "\n";
            }

            return 0;
        }

        std::cout << "Production-directed pipeline: source -> lexer -> parser -> semantic analysis -> VIR lowering -> backend abstraction.\n";

        const bool defaultNativeExe = !options.emitVir && !options.emitLlvm && !options.emitObj;

        if (options.emitVir) {
            const fs::path virPath = options.outputPath.value_or(defaultArtifactPath(options.inputPath, ".vir"));
            writeFile(virPath, vir::dump(lowered.module));
            std::cout << "Emitted VIR: " << virPath.string() << "\n";
        }

        const bool needsLlvmIr = options.emitLlvm || options.emitObj || defaultNativeExe;
        std::optional<std::string> llvmIrText;
        std::optional<fs::path> emittedLlvmPath;

        if (needsLlvmIr) {
            auto backend = createLlvmIrTextBackend();
            BackendOptions backendOptions;
            backendOptions.output = BackendOutputKind::LlvmIrText;
            backendOptions.track = BackendTrack::ProductionDirected;
            backendOptions.moduleName = options.inputPath.stem().string();

            BackendResult backendResult = backend->compile(lowered.module, backendOptions);
            if (!backendResult.ok()) {
                backendResult.diagnostics.print(std::cerr);
                return 1;
            }

            const BackendArtifact* llvmArtifact = nullptr;
            for (const auto& artifact : backendResult.artifacts) {
                if (artifact.kind == BackendOutputKind::LlvmIrText) {
                    llvmArtifact = &artifact;
                    break;
                }
            }

            if (!llvmArtifact) {
                throw std::runtime_error("LLVM backend produced no LLVM IR artifact");
            }
            llvmIrText = llvmArtifact->payload;
        }

        if (options.emitLlvm) {
            const fs::path llvmPath = options.outputPath.value_or(defaultArtifactPath(options.inputPath, ".ll"));
            writeFile(llvmPath, *llvmIrText);
            emittedLlvmPath = llvmPath;
            std::cout << "Emitted LLVM IR: " << llvmPath.string() << "\n";
        }

        if (options.emitObj || defaultNativeExe) {
            const NativeToolchain toolchain = resolveNativeToolchain(argv[0], defaultNativeExe);

            fs::path llvmInputPath;
            bool temporaryLlvmFile = false;
            if (emittedLlvmPath.has_value()) {
                llvmInputPath = *emittedLlvmPath;
            } else {
                llvmInputPath = defaultArtifactPath(options.inputPath, ".native.ll");
                writeFile(llvmInputPath, *llvmIrText);
                temporaryLlvmFile = true;
            }

            fs::path objectPath;
            bool temporaryObjectFile = false;
            if (options.emitObj) {
                objectPath = options.outputPath.value_or(defaultArtifactPath(options.inputPath, objectExtension()));
            } else {
                objectPath = defaultArtifactPath(options.inputPath, ".native" + objectExtension());
                temporaryObjectFile = true;
            }

            const std::string compileObjCommand =
                shellQuote(toolchain.clangExecutable) +
                " -x ir -c " + shellQuote(llvmInputPath.string()) +
                " -o " + shellQuote(objectPath.string());
            runCommandOrThrow(compileObjCommand, "LLVM IR -> object compilation failed");

            if (options.emitObj) {
                std::cout << "Emitted object: " << objectPath.string() << "\n";
            }

            if (defaultNativeExe) {
                const fs::path executablePath = options.outputPath.value_or(defaultExecutablePath(options.inputPath));
                const std::string linkCommand =
                    shellQuote(toolchain.clangExecutable) +
                    " " + shellQuote(objectPath.string()) +
                    " " + shellQuote(toolchain.runtimeLibraryPath->string()) +
                    " -o " + shellQuote(executablePath.string());
                runCommandOrThrow(linkCommand, "Native link stage failed");
                std::cout << "Built executable: " << executablePath.string() << "\n";
            }

            if (temporaryLlvmFile) {
                std::error_code removeError;
                fs::remove(llvmInputPath, removeError);
            }
            if (temporaryObjectFile) {
                std::error_code removeError;
                fs::remove(objectPath, removeError);
            }
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "voltisc error: " << ex.what() << "\n";
        return 1;
    }
}
