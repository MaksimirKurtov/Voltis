#include "backend_pe_x64.h"
#include "backend_llvm_ir.h"
#include "codegen.h"
#include "lexer.h"
#include "lowering.h"
#include "parser.h"
#include "sema.h"
#include "vir.h"
#include "vir_passes.h"
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
    bool noLink = false;
    bool bootstrapCpp = false;
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

static void runCommandOrThrow(const std::string& command, const std::string& failureMessage) {
    std::cout << "Invoking: " << command << "\n";
    int code = std::system(command.c_str());
    if (code != 0) {
        throw std::runtime_error(failureMessage + " (exit code " + std::to_string(code) + ")");
    }
}

static void printUsage() {
    std::cout << "voltisc <input.vlt> [options]\n"
              << "Default: native compile to a Windows x64 PE executable.\n"
              << "Options:\n"
              << "  -o <path>          Output artifact path (single artifact mode or default executable)\n"
              << "  --emit-vir         Emit VIR text (.vir)\n"
              << "  --emit-llvm        Emit LLVM IR text (.ll)\n"
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
    if (options.bootstrapCpp && (options.emitVir || options.emitLlvm)) {
        throw std::runtime_error("--bootstrap-cpp cannot be combined with --emit-vir/--emit-llvm");
    }

    const int explicitEmitCount =
        static_cast<int>(options.emitVir) +
        static_cast<int>(options.emitLlvm);

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

        DiagnosticBag preOptimizationVerify = vir::verifyModule(lowered.module);
        if (preOptimizationVerify.hasErrors()) {
            preOptimizationVerify.print(std::cerr);
            return 1;
        }

        vir::optimizeModule(lowered.module);

        DiagnosticBag postOptimizationVerify = vir::verifyModule(lowered.module);
        if (postOptimizationVerify.hasErrors()) {
            postOptimizationVerify.print(std::cerr);
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

        const bool defaultNativeExe = !options.emitVir && !options.emitLlvm;
        if (defaultNativeExe) {
            auto nativeBackend = createPeX64Backend();
            BackendOptions nativeOptions;
            nativeOptions.output = BackendOutputKind::Executable;
            nativeOptions.track = BackendTrack::ProductionDirected;
            nativeOptions.moduleName = options.inputPath.stem().string();

            BackendResult nativeResult = nativeBackend->compile(lowered.module, nativeOptions);
            if (nativeResult.ok()) {
                const BackendArtifact* exeArtifact = nullptr;
                for (const auto& artifact : nativeResult.artifacts) {
                    if (artifact.kind == BackendOutputKind::Executable) {
                        exeArtifact = &artifact;
                        break;
                    }
                }
                if (!exeArtifact) {
                    throw std::runtime_error("Direct PE backend produced no executable artifact");
                }

                const fs::path executablePath = options.outputPath.value_or(defaultExecutablePath(options.inputPath));
                writeFile(executablePath, exeArtifact->payload);
                std::cout << "Built executable: " << executablePath.string() << "\n";
                return 0;
            }
            nativeResult.diagnostics.print(std::cerr);
            return 1;
        }

        if (options.emitVir) {
            const fs::path virPath = options.outputPath.value_or(defaultArtifactPath(options.inputPath, ".vir"));
            writeFile(virPath, vir::dump(lowered.module));
            std::cout << "Emitted VIR: " << virPath.string() << "\n";
        }

        if (options.emitLlvm) {
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

            const fs::path llvmPath = options.outputPath.value_or(defaultArtifactPath(options.inputPath, ".ll"));
            writeFile(llvmPath, llvmArtifact->payload);
            std::cout << "Emitted LLVM IR: " << llvmPath.string() << "\n";
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "voltisc error: " << ex.what() << "\n";
        return 1;
    }
}
