#include "backend_pe_x64.h"
#include "backend_llvm_ir.h"
#include "codegen.h"
#include "import_utils.h"
#include "lexer.h"
#include "lowering.h"
#include "parser.h"
#include "sema.h"
#include "vir.h"
#include "vir_passes.h"
#include <algorithm>
#include <array>
#include <cctype>
#include <chrono>
#include <cmath>
#include <cstdlib>
#include <ctime>
#include <filesystem>
#include <fstream>
#include <future>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <limits>
#include <optional>
#include <sstream>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <unordered_set>
#include <system_error>
#include <thread>
#include <vector>

namespace fs = std::filesystem;
using Clock = std::chrono::steady_clock;

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
    std::vector<fs::path> inputPaths;
    std::optional<fs::path> outputPath;
    std::optional<fs::path> emitCppPath;
    bool emitVir = false;
    bool emitLlvm = false;
    bool noLink = false;
    bool bootstrapCpp = false;
    bool benchmark = false;
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

static const fs::path& primaryInputPath(const CliOptions& options) {
    if (options.inputPaths.empty()) {
        throw std::runtime_error("No input .vlt file supplied");
    }
    return options.inputPaths.front();
}

static void runCommandOrThrow(const std::string& command, const std::string& failureMessage) {
    std::cout << "Invoking: " << command << "\n";
    int code = std::system(command.c_str());
    if (code != 0) {
        throw std::runtime_error(failureMessage + " (exit code " + std::to_string(code) + ")");
    }
}

static void printUsage() {
    std::cout << "voltisc <input.vlt> [more_inputs.vlt ...] [options]\n"
              << "voltisc --benchmark [-o <benchmark.exe>]\n"
              << "Default: native compile to a Windows x64 PE executable.\n"
              << "Options:\n"
              << "  -o <path>          Output artifact path (single artifact mode or default executable)\n"
              << "  --benchmark        Run embedded benchmark compile+execute mode\n"
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
        if (arg == "--benchmark") {
            options.benchmark = true;
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
            options.inputPaths.push_back(fs::path(arg));
            continue;
        }
        throw std::runtime_error("Unknown argument: " + arg);
    }

    if (options.benchmark) {
        if (!options.inputPaths.empty()) {
            throw std::runtime_error("--benchmark does not accept an input .vlt file");
        }
        if (options.bootstrapCpp || options.emitVir || options.emitLlvm ||
            options.emitCppPath.has_value() || options.noLink) {
            throw std::runtime_error("--benchmark cannot be combined with emit/bootstrap options");
        }
        return options;
    }

    if (options.inputPaths.empty()) {
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

static std::string diagnosticsToString(const DiagnosticBag& diagnostics) {
    std::ostringstream out;
    diagnostics.print(out);
    return out.str();
}

struct FrontendPipelineResult {
    Program program;
    vir::Module module;
};

static Program parseProgramFromSource(const std::string& source, const std::string& contextLabel) {
    Lexer lexer(source);
    Parser parser(lexer.tokenize());
    try {
        return parser.parseProgram();
    } catch (const std::exception& ex) {
        throw std::runtime_error("Parse failed for " + contextLabel + ": " + ex.what());
    }
}

struct ModuleBuildState {
    std::unordered_set<std::string> loaded;
    std::unordered_set<std::string> inProgress;
    std::vector<std::string> stack;
    std::vector<Program> programsInDependencyOrder;
};

static std::string normalizedKey(const fs::path& path) {
    std::string key = path.lexically_normal().string();
#ifdef _WIN32
    std::transform(key.begin(), key.end(), key.begin(), [](unsigned char c) {
        if (c == '\\') {
            return '/';
        }
        return static_cast<char>(std::tolower(c));
    });
#endif
    return key;
}

static fs::path resolveCanonical(const fs::path& path) {
    std::error_code ec;
    const fs::path canonical = fs::weakly_canonical(path, ec);
    if (!ec) {
        return canonical;
    }
    return path.lexically_normal();
}

static Program mergePrograms(std::vector<Program>&& programs) {
    Program merged;
    std::unordered_set<std::string> seenImports;
    for (auto& program : programs) {
        std::unordered_set<std::string> seenInCurrentProgram;
        merged.structs.insert(
            merged.structs.end(),
            std::make_move_iterator(program.structs.begin()),
            std::make_move_iterator(program.structs.end()));
        merged.externFunctions.insert(
            merged.externFunctions.end(),
            std::make_move_iterator(program.externFunctions.begin()),
            std::make_move_iterator(program.externFunctions.end()));
        merged.functions.insert(
            merged.functions.end(),
            std::make_move_iterator(program.functions.begin()),
            std::make_move_iterator(program.functions.end()));
        for (const auto& importDecl : program.imports) {
            const std::string key = normalizedKey(importDecl.path);
            const bool duplicateInsideCurrent = !seenInCurrentProgram.insert(key).second;
            if (duplicateInsideCurrent || seenImports.insert(key).second) {
                merged.imports.push_back(importDecl);
            }
        }
    }
    return merged;
}

static void loadProgramRecursive(const fs::path& modulePath, ModuleBuildState& state) {
    const fs::path canonicalPath = resolveCanonical(modulePath);
    const std::string key = normalizedKey(canonicalPath);
    if (state.loaded.find(key) != state.loaded.end()) {
        return;
    }
    if (state.inProgress.find(key) != state.inProgress.end()) {
        std::ostringstream cycle;
        for (const auto& item : state.stack) {
            cycle << item << " -> ";
        }
        cycle << canonicalPath.string();
        throw std::runtime_error("Source module import cycle detected: " + cycle.str());
    }

    if (!fs::exists(canonicalPath)) {
        throw std::runtime_error("Source module not found: " + canonicalPath.string());
    }

    state.inProgress.insert(key);
    state.stack.push_back(canonicalPath.string());

    Program parsed = parseProgramFromSource(readFile(canonicalPath), canonicalPath.string());
    std::vector<ImportDecl> runtimeImports;
    runtimeImports.reserve(parsed.imports.size());

    for (const auto& importDecl : parsed.imports) {
        if (isSourceModuleImportPath(importDecl.path)) {
            const fs::path childPath = resolveCanonical(canonicalPath.parent_path() / fs::path(importDecl.path));
            loadProgramRecursive(childPath, state);
        } else {
            runtimeImports.push_back(importDecl);
        }
    }
    parsed.imports = std::move(runtimeImports);
    state.programsInDependencyOrder.push_back(std::move(parsed));

    state.stack.pop_back();
    state.inProgress.erase(key);
    state.loaded.insert(key);
}

static Program buildProgramFromEntryFiles(const std::vector<fs::path>& entryFilePaths) {
    if (entryFilePaths.empty()) {
        throw std::runtime_error("No input .vlt file supplied");
    }

    ModuleBuildState state;
    for (const auto& entryFilePath : entryFilePaths) {
        loadProgramRecursive(entryFilePath, state);
    }
    return mergePrograms(std::move(state.programsInDependencyOrder));
}

static FrontendPipelineResult runFrontendPipeline(Program program) {

    SemanticAnalyzer sema;
    if (!sema.analyze(program)) {
        throw std::runtime_error("Semantic analysis failed:\n" + diagnosticsToString(sema.diagnostics()));
    }

    VIRLowerer lowerer;
    LoweringResult lowered = lowerer.lower(program, sema);
    if (!lowered.ok()) {
        throw std::runtime_error("VIR lowering failed:\n" + diagnosticsToString(lowered.diagnostics));
    }

    DiagnosticBag preOptimizationVerify = vir::verifyModule(lowered.module);
    if (preOptimizationVerify.hasErrors()) {
        throw std::runtime_error("VIR verification failed before optimization:\n" +
            diagnosticsToString(preOptimizationVerify));
    }

    vir::optimizeModule(lowered.module);

    DiagnosticBag postOptimizationVerify = vir::verifyModule(lowered.module);
    if (postOptimizationVerify.hasErrors()) {
        throw std::runtime_error("VIR verification failed after optimization:\n" +
            diagnosticsToString(postOptimizationVerify));
    }

    return FrontendPipelineResult{std::move(program), std::move(lowered.module)};
}

static BackendArtifact compileNativeExecutableArtifact(const vir::Module& module, const std::string& moduleName) {
    auto nativeBackend = createPeX64Backend();
    BackendOptions nativeOptions;
    nativeOptions.output = BackendOutputKind::Executable;
    nativeOptions.track = BackendTrack::ProductionDirected;
    nativeOptions.moduleName = moduleName;

    BackendResult nativeResult = nativeBackend->compile(module, nativeOptions);
    if (!nativeResult.ok()) {
        throw std::runtime_error("Native backend failed:\n" + diagnosticsToString(nativeResult.diagnostics));
    }

    for (const auto& artifact : nativeResult.artifacts) {
        if (artifact.kind == BackendOutputKind::Executable) {
            return artifact;
        }
    }

    throw std::runtime_error("Direct PE backend produced no executable artifact");
}

static std::string formatDuration(double seconds) {
    const double safeSeconds = seconds < 0.0 ? 0.0 : seconds;
    const long long totalTenThousandths = static_cast<long long>(safeSeconds * 10000.0 + 0.5);
    const long long wholeSeconds = totalTenThousandths / 10000;
    const long long fractional = totalTenThousandths % 10000;

    std::ostringstream out;
    out << std::setw(2) << std::setfill('0') << wholeSeconds
        << ":" << std::setw(4) << std::setfill('0') << fractional;
    return out.str();
}

static std::string formatPercent(double percent) {
    std::ostringstream out;
    out << std::fixed << std::setprecision(2) << percent;
    return out.str();
}

static void renderStatusLine(const std::string& text) {
    constexpr std::size_t kStatusWidth = 96;
    std::cout << "\r" << text;
    if (text.size() < kStatusWidth) {
        std::cout << std::string(kStatusWidth - text.size(), ' ');
    }
    std::cout << std::flush;
}

template <typename Operation>
auto runWithSpinner(const std::string& label, Operation&& operation)
    -> std::pair<decltype(operation()), double> {
    const std::array<std::string, 4> spinnerFrames = {"*---", "-*--", "--*-", "---*"};

    const auto start = Clock::now();
    auto future = std::async(std::launch::async, std::forward<Operation>(operation));
    std::size_t frameIndex = 0;

    while (future.wait_for(std::chrono::milliseconds(80)) != std::future_status::ready) {
        const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
        renderStatusLine(label + ": " + spinnerFrames[frameIndex % spinnerFrames.size()] + " :" + formatDuration(elapsed));
        ++frameIndex;
    }

    auto result = future.get();
    const double elapsed = std::chrono::duration<double>(Clock::now() - start).count();
    return {std::move(result), elapsed};
}

struct BenchmarkRecord {
    std::string timestamp;
    double compileSeconds = 0.0;
    double benchmarkSeconds = 0.0;
    long long totalInstructions = 0;
};

static std::string currentTimestamp() {
    const auto now = std::chrono::system_clock::now();
    const std::time_t raw = std::chrono::system_clock::to_time_t(now);
    std::tm localTime{};
#ifdef _WIN32
    localtime_s(&localTime, &raw);
#else
    localtime_r(&raw, &localTime);
#endif

    char buffer[32]{};
    std::strftime(buffer, sizeof(buffer), "%Y-%m-%d %H:%M:%S", &localTime);
    return buffer;
}

static fs::path benchmarkTempDirectory(std::string& warning) {
    try {
        const fs::path tempDir = fs::temp_directory_path();
        return tempDir;
    } catch (const std::exception& ex) {
        warning = std::string("benchmark history directory fallback in use: ") + ex.what();
        return fs::current_path();
    }
}

static std::vector<BenchmarkRecord> readBenchmarkHistory(const fs::path& csvPath, std::string& warning) {
    std::vector<BenchmarkRecord> records;
    std::ifstream input(csvPath);
    if (!input.good()) {
        return records;
    }

    std::string line;
    std::size_t lineNumber = 0;
    while (std::getline(input, line)) {
        ++lineNumber;
        if (line.empty()) {
            continue;
        }
        if (lineNumber == 1 && line.rfind("timestamp,", 0) == 0) {
            continue;
        }

        std::istringstream lineStream(line);
        std::string timestamp;
        std::string compileToken;
        std::string benchmarkToken;
        std::string instructionToken;
        if (!std::getline(lineStream, timestamp, ',')) {
            continue;
        }
        if (!std::getline(lineStream, compileToken, ',')) {
            continue;
        }
        if (!std::getline(lineStream, benchmarkToken, ',')) {
            continue;
        }
        if (!std::getline(lineStream, instructionToken, ',')) {
            continue;
        }

        try {
            BenchmarkRecord record;
            record.timestamp = timestamp;
            record.compileSeconds = std::stod(compileToken);
            record.benchmarkSeconds = std::stod(benchmarkToken);
            record.totalInstructions = std::stoll(instructionToken);
            records.push_back(record);
        } catch (const std::exception&) {
            warning = "benchmark history contains malformed rows that were skipped";
        }
    }

    return records;
}

static bool appendBenchmarkHistory(const fs::path& csvPath, const BenchmarkRecord& record, std::string& warning) {
    try {
        if (!csvPath.parent_path().empty()) {
            fs::create_directories(csvPath.parent_path());
        }
    } catch (const std::exception& ex) {
        warning = std::string("failed to create benchmark history directory: ") + ex.what();
        return false;
    }

    const bool needsHeader = !fs::exists(csvPath);
    std::ofstream out(csvPath, std::ios::app);
    if (!out) {
        warning = "failed to open benchmark history CSV for append";
        return false;
    }

    out << std::fixed << std::setprecision(6);
    if (needsHeader) {
        out << "timestamp,compile_time,benchmark_time,total_instructions\n";
    }
    out << record.timestamp << ","
        << record.compileSeconds << ","
        << record.benchmarkSeconds << ","
        << record.totalInstructions << "\n";
    return true;
}

static double bestCompileTime(const std::vector<BenchmarkRecord>& records) {
    double best = std::numeric_limits<double>::infinity();
    for (const auto& record : records) {
        best = std::min(best, record.compileSeconds);
    }
    return best;
}

static double bestBenchmarkTime(const std::vector<BenchmarkRecord>& records) {
    double best = std::numeric_limits<double>::infinity();
    for (const auto& record : records) {
        best = std::min(best, record.benchmarkSeconds);
    }
    return best;
}

static double improvementPercent(double baseline, double current) {
    if (!std::isfinite(baseline) || baseline <= 0.0) {
        return 0.0;
    }
    const double improvement = ((baseline - current) / baseline) * 100.0;
    return improvement > 0.0 ? improvement : 0.0;
}

static constexpr int kDefaultBenchmarkIterations = 1000000000;
static constexpr long long kBenchmarkOpsPerIteration = 7;

static std::string formatIntegerWithCommas(long long value) {
    std::string text = std::to_string(value);
    for (long long i = static_cast<long long>(text.size()) - 3; i > 0; i -= 3) {
        text.insert(static_cast<std::size_t>(i), ",");
    }
    return text;
}

static int resolveBenchmarkIterations(std::vector<std::string>& warnings) {
    const char* envValue = std::getenv("VOLTIS_BENCHMARK_ITERATIONS");
    if (envValue == nullptr || *envValue == '\0') {
        return kDefaultBenchmarkIterations;
    }

    try {
        const long long parsed = std::stoll(envValue);
        if (parsed <= 0 || parsed > std::numeric_limits<int>::max()) {
            throw std::out_of_range("out of range");
        }
        return static_cast<int>(parsed);
    } catch (const std::exception&) {
        warnings.push_back(
            "invalid VOLTIS_BENCHMARK_ITERATIONS value '" + std::string(envValue) + "', using default " +
            std::to_string(kDefaultBenchmarkIterations));
        return kDefaultBenchmarkIterations;
    }
}

static long long benchmarkInstructionCount(int iterations) {
    return static_cast<long long>(iterations) * kBenchmarkOpsPerIteration;
}

static std::string benchmarkSource(int iterations) {
    std::ostringstream source;
    source << "public fn benchmark_leibniz_pi(int32 iterations) -> float64 {\n"
           << "    float64 sum = 0.0;\n"
           << "    int32 i = 0;\n"
           << "    int32 sign = 1;\n"
           << "    while (i < iterations) {\n"
           << "        int32 denominator_int = (i * 2) + 1;\n"
           << "        float64 denominator = denominator_int.ToFloat64();\n"
           << "        float64 term = sign.ToFloat64() / denominator;\n"
           << "        sum = sum + term;\n"
           << "        sign = 0 - sign;\n"
           << "        i = i + 1;\n"
           << "    }\n"
           << "    return sum * 4.0;\n"
           << "}\n\n"
           << "public fn main() -> int32 {\n"
           << "    float64 pi = benchmark_leibniz_pi(" << iterations << ");\n"
           << "    if (pi < 0.0) {\n"
           << "        print(\"unreachable\");\n"
           << "    }\n"
           << "    return 0;\n"
           << "}\n";
    return source.str();
}

static int runBenchmarkMode(const CliOptions& options) {
    std::vector<std::string> warnings;
    const int benchmarkIterations = resolveBenchmarkIterations(warnings);
    std::string tempWarning;
    const fs::path tempDir = benchmarkTempDirectory(tempWarning);
    if (!tempWarning.empty()) {
        warnings.push_back(tempWarning);
    }

    const fs::path historyCsv = tempDir / "voltis_benchmark_history.csv";
    std::string readWarning;
    const std::vector<BenchmarkRecord> historyBefore = readBenchmarkHistory(historyCsv, readWarning);
    if (!readWarning.empty()) {
        warnings.push_back(readWarning);
    }

    const double priorBestCompile = bestCompileTime(historyBefore);
    const double priorBestBenchmark = bestBenchmarkTime(historyBefore);

    const auto runSuffix =
        std::chrono::duration_cast<std::chrono::milliseconds>(
            std::chrono::system_clock::now().time_since_epoch())
            .count();
    const fs::path benchmarkExePath = options.outputPath.value_or(tempDir / ("voltis_benchmark_" + std::to_string(runSuffix) + ".exe"));
    const bool keepExecutable = options.outputPath.has_value();

    std::cout << "*** Voltis Benchmark ***\n";
    std::cout << "Speed Comparison - Leibniz pi, "
              << formatIntegerWithCommas(benchmarkIterations)
              << " iterations\n";

    const auto compileOp = [&]() -> bool {
        Program benchmarkProgram = parseProgramFromSource(benchmarkSource(benchmarkIterations), "<embedded benchmark>");
        FrontendPipelineResult pipeline = runFrontendPipeline(std::move(benchmarkProgram));
        BackendArtifact artifact = compileNativeExecutableArtifact(pipeline.module, "voltis_benchmark");
        writeFile(benchmarkExePath, artifact.payload);
        return true;
    };
    auto [compiled, compileSeconds] = runWithSpinner("Compiling", compileOp);
    (void)compiled;
    renderStatusLine("Compiled Successfully: " + formatDuration(compileSeconds) + " Seconds");
    std::cout << "\n";

    const auto benchmarkOp = [&]() -> int {
        return std::system(shellQuote(benchmarkExePath.string()).c_str());
    };
    auto [benchmarkExitCode, benchmarkSeconds] = runWithSpinner("Benchmarking", benchmarkOp);
    if (benchmarkExitCode != 0) {
        throw std::runtime_error("benchmark executable failed (exit code " + std::to_string(benchmarkExitCode) + ")");
    }
    renderStatusLine("Benchmarked Successfully: " + formatDuration(benchmarkSeconds) + " Seconds");
    std::cout << "\n";

    const BenchmarkRecord currentRecord{
        currentTimestamp(),
        compileSeconds,
        benchmarkSeconds,
        benchmarkInstructionCount(benchmarkIterations)
    };

    std::string appendWarning;
    if (!appendBenchmarkHistory(historyCsv, currentRecord, appendWarning)) {
        warnings.push_back(appendWarning.empty() ? "failed to append benchmark CSV history" : appendWarning);
    }

    const double bestCompile = std::isfinite(priorBestCompile)
        ? std::min(priorBestCompile, compileSeconds)
        : compileSeconds;
    const double bestBenchmark = std::isfinite(priorBestBenchmark)
        ? std::min(priorBestBenchmark, benchmarkSeconds)
        : benchmarkSeconds;

    std::cout << "---------------------------\n";
    std::cout << "Total Instructions: " << currentRecord.totalInstructions << "\n";
    std::cout << "Benchmark Time: " << formatDuration(benchmarkSeconds) << "\n";
    std::cout << "Benchmark Time Improvement: " << formatPercent(improvementPercent(priorBestBenchmark, benchmarkSeconds)) << "%\n";
    std::cout << "Compile Time Improvement: " << formatPercent(improvementPercent(priorBestCompile, compileSeconds)) << "%\n";
    std::cout << "Best Benchmark Time: " << formatDuration(bestBenchmark) << "\n";
    std::cout << "Best Compile Time: " << formatDuration(bestCompile) << "\n";

    if (!keepExecutable) {
        std::error_code removeError;
        fs::remove(benchmarkExePath, removeError);
        if (removeError) {
            warnings.push_back("failed to clean benchmark executable: " + removeError.message());
        }
    }

    for (const auto& warning : warnings) {
        if (!warning.empty()) {
            std::cerr << "benchmark warning: " << warning << "\n";
        }
    }

    return 0;
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            printUsage();
            return 1;
        }

        const CliOptions options = parseCliOptions(argc, argv);

        if (options.benchmark) {
            return runBenchmarkMode(options);
        }

        const fs::path& primaryInput = primaryInputPath(options);
        Program program = buildProgramFromEntryFiles(options.inputPaths);
        FrontendPipelineResult pipeline = runFrontendPipeline(std::move(program));

        if (options.bootstrapCpp) {
            std::cout << "Using bootstrap C++ backend (temporary scaffolding, not production direction).\n";
            CodeGenerator generator;
            const std::string cppSource = generator.generate(pipeline.program);
            const fs::path generatedCpp = options.emitCppPath.has_value()
                ? *options.emitCppPath
                : defaultArtifactPath(primaryInput, ".generated.cpp");
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
            const std::string moduleName =
                options.inputPaths.size() == 1 ? primaryInput.stem().string() : "multi_module";
            BackendArtifact executableArtifact =
                compileNativeExecutableArtifact(pipeline.module, moduleName);
            const fs::path executablePath = options.outputPath.value_or(defaultExecutablePath(primaryInput));
            writeFile(executablePath, executableArtifact.payload);
            std::cout << "Built executable: " << executablePath.string() << "\n";
            return 0;
        }

        if (options.emitVir) {
            const fs::path virPath = options.outputPath.value_or(defaultArtifactPath(primaryInput, ".vir"));
            writeFile(virPath, vir::dump(pipeline.module));
            std::cout << "Emitted VIR: " << virPath.string() << "\n";
        }

        if (options.emitLlvm) {
            auto backend = createLlvmIrTextBackend();
            BackendOptions backendOptions;
            backendOptions.output = BackendOutputKind::LlvmIrText;
            backendOptions.track = BackendTrack::ProductionDirected;
            backendOptions.moduleName =
                options.inputPaths.size() == 1 ? primaryInput.stem().string() : "multi_module";

            BackendResult backendResult = backend->compile(pipeline.module, backendOptions);
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

            const fs::path llvmPath = options.outputPath.value_or(defaultArtifactPath(primaryInput, ".ll"));
            writeFile(llvmPath, llvmArtifact->payload);
            std::cout << "Emitted LLVM IR: " << llvmPath.string() << "\n";
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "voltisc error: " << ex.what() << "\n";
        return 1;
    }
}
