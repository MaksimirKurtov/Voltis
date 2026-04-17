#include "codegen.h"
#include "lexer.h"
#include "parser.h"
#include <filesystem>
#include <fstream>
#include <iostream>
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
#ifdef _WIN32
    return "g++ -std=c++17 " + shellQuote(cppFile.string()) + " -o " + shellQuote(outputFile.string());
#else
    return "g++ -std=c++17 " + shellQuote(cppFile.string()) + " -o " + shellQuote(outputFile.string());
#endif
}

static void printUsage() {
    std::cout << "voltisc <input.vlt> [-o output.exe] [--emit-cpp only.cpp] [--no-link]\n";
}

int main(int argc, char** argv) {
    try {
        if (argc < 2) {
            printUsage();
            return 1;
        }

        fs::path inputPath;
        fs::path outputPath = "a.exe";
        fs::path emitCppPath;
        bool noLink = false;

        for (int i = 1; i < argc; ++i) {
            std::string arg = argv[i];
            if (arg == "-o" && i + 1 < argc) {
                outputPath = argv[++i];
            } else if (arg == "--emit-cpp" && i + 1 < argc) {
                emitCppPath = argv[++i];
            } else if (arg == "--no-link") {
                noLink = true;
            } else if (!arg.empty() && arg[0] != '-') {
                inputPath = arg;
            } else {
                throw std::runtime_error("Unknown argument: " + arg);
            }
        }

        if (inputPath.empty()) {
            throw std::runtime_error("No input .vlt file supplied");
        }

        const std::string source = readFile(inputPath);
        Lexer lexer(source);
        auto tokens = lexer.tokenize();
        Parser parser(std::move(tokens));
        Program program = parser.parseProgram();

        CodeGenerator generator;
        std::string cppSource = generator.generate(program);

        fs::path generatedCpp = emitCppPath.empty()
            ? inputPath.parent_path() / (inputPath.stem().string() + ".generated.cpp")
            : emitCppPath;
        writeFile(generatedCpp, cppSource);

        std::cout << "Generated C++: " << generatedCpp.string() << "\n";

        if (!noLink) {
            std::string command = compilerCommand(generatedCpp, outputPath);
            std::cout << "Invoking: " << command << "\n";
            int code = std::system(command.c_str());
            if (code != 0) {
                throw std::runtime_error("Native compiler failed with exit code " + std::to_string(code));
            }
            std::cout << "Built executable: " << outputPath.string() << "\n";
        }

        return 0;
    } catch (const std::exception& ex) {
        std::cerr << "voltisc error: " << ex.what() << "\n";
        return 1;
    }
}
