#pragma once

#include <algorithm>
#include <cctype>
#include <filesystem>
#include <string>

enum class ImportPathKind {
    SourceModule,
    DynamicLibrary,
    ImportLibrary,
    ArchiveLibrary,
    SharedObject,
    Dylib,
    Unknown
};

inline std::string toLowerAsciiCopy(std::string value) {
    std::transform(value.begin(), value.end(), value.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    return value;
}

inline std::string fileNameFromImportPath(const std::string& path) {
    const std::size_t slashPos = path.find_last_of("/\\");
    return slashPos == std::string::npos ? path : path.substr(slashPos + 1);
}

inline std::string extensionLowerFromPath(const std::string& path) {
    return toLowerAsciiCopy(std::filesystem::path(path).extension().string());
}

inline ImportPathKind classifyImportPath(const std::string& path) {
    const std::string extension = extensionLowerFromPath(path);
    if (extension == ".vlt") {
        return ImportPathKind::SourceModule;
    }
    if (extension == ".dll") {
        return ImportPathKind::DynamicLibrary;
    }
    if (extension == ".lib") {
        return ImportPathKind::ImportLibrary;
    }
    if (extension == ".a") {
        return ImportPathKind::ArchiveLibrary;
    }
    if (extension == ".so") {
        return ImportPathKind::SharedObject;
    }
    if (extension == ".dylib") {
        return ImportPathKind::Dylib;
    }
    return ImportPathKind::Unknown;
}

inline bool isSourceModuleImportPath(const std::string& path) {
    return classifyImportPath(path) == ImportPathKind::SourceModule;
}

inline bool isNativeLibraryImportPath(const std::string& path) {
    const ImportPathKind kind = classifyImportPath(path);
    return kind == ImportPathKind::DynamicLibrary ||
           kind == ImportPathKind::ImportLibrary ||
           kind == ImportPathKind::ArchiveLibrary ||
           kind == ImportPathKind::SharedObject ||
           kind == ImportPathKind::Dylib;
}

inline std::string normalizeNativeLibraryNameForPe(const std::string& path) {
    const std::string lowerFileName = toLowerAsciiCopy(fileNameFromImportPath(path));
    if (lowerFileName.empty()) {
        return {};
    }

    const std::filesystem::path filePath(lowerFileName);
    const std::string extension = filePath.extension().string();
    std::string stem = filePath.stem().string();

    const auto mapSharedLibraryStemToDll = [](std::string baseName) -> std::string {
        if (baseName.rfind("lib", 0) == 0 && baseName.size() > 3) {
            baseName = baseName.substr(3);
        }
        if (baseName.empty()) {
            return {};
        }
        if (baseName.size() > 4 && baseName.substr(baseName.size() - 4) == ".dll") {
            return baseName;
        }
        return baseName + ".dll";
    };

    if (extension == ".dll") {
        return lowerFileName;
    }
    if (extension == ".lib") {
        return stem.empty() ? std::string{} : stem + ".dll";
    }
    if (extension == ".a" || extension == ".so" || extension == ".dylib") {
        return mapSharedLibraryStemToDll(stem);
    }

    return lowerFileName;
}
