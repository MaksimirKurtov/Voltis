#include "file_utils.h"

#include <algorithm>
#include <chrono>
#include <stdexcept>
#include <system_error>

#ifdef _WIN32
#define NOMINMAX
#include <Windows.h>
#else
#include <cerrno>
#include <fcntl.h>
#include <unistd.h>
#endif

namespace fs = std::filesystem;

namespace {

fs::path makeTempPathNear(const fs::path& outPath) {
    const fs::path dir = outPath.has_parent_path() ? outPath.parent_path() : fs::current_path();
    const std::string base = outPath.filename().string();
    const auto nonce = std::to_string(static_cast<unsigned long long>(std::chrono::steady_clock::now().time_since_epoch().count()));
    return dir / ("." + base + ".tmp." + nonce);
}

#ifndef _WIN32
void writePosixFile(const fs::path& path, const std::vector<std::uint8_t>& data) {
    constexpr mode_t kMode = 0644;
    // Temp file names are unique (timestamp nonce), so O_TRUNC is safe and
    // avoids the TOCTOU race of the O_EXCL + remove + retry pattern.
    const int flags = O_WRONLY | O_CREAT | O_TRUNC | O_NOFOLLOW;

    const int fd = ::open(path.c_str(), flags, kMode);
    if (fd < 0) {
        throw std::runtime_error("Could not open temp output file: " + path.string());
    }

    std::size_t written = 0;
    while (written < data.size()) {
        const ssize_t rc = ::write(fd, data.data() + written, data.size() - written);
        if (rc <= 0) {
            ::close(fd);
            throw std::runtime_error("Failed writing output file: " + path.string());
        }
        written += static_cast<std::size_t>(rc);
    }

    if (::fsync(fd) != 0) {
        ::close(fd);
        throw std::runtime_error("Failed to fsync output file: " + path.string());
    }
    if (::close(fd) != 0) {
        throw std::runtime_error("Failed to close output file: " + path.string());
    }
}
#else
std::wstring toWide(const fs::path& path) {
    return path.wstring();
}

void writeWindowsFile(const fs::path& path, const std::vector<std::uint8_t>& data) {
    const std::wstring widePath = toWide(path);
    const DWORD attrs = GetFileAttributesW(widePath.c_str());
    if (attrs != INVALID_FILE_ATTRIBUTES && (attrs & FILE_ATTRIBUTE_REPARSE_POINT) != 0) {
        throw std::runtime_error("Refusing to write symlink/reparse output path: " + path.string());
    }

    HANDLE handle = CreateFileW(
        widePath.c_str(),
        GENERIC_WRITE,
        0,
        nullptr,
        CREATE_ALWAYS,
        FILE_ATTRIBUTE_NORMAL | FILE_FLAG_WRITE_THROUGH,
        nullptr);
    if (handle == INVALID_HANDLE_VALUE) {
        throw std::runtime_error("Could not safely open output file: " + path.string());
    }

    std::size_t writtenTotal = 0;
    while (writtenTotal < data.size()) {
        const DWORD chunk = static_cast<DWORD>(std::min<std::size_t>(data.size() - writtenTotal, 1u << 20));
        DWORD wrote = 0;
        const BOOL ok = WriteFile(handle, data.data() + writtenTotal, chunk, &wrote, nullptr);
        if (!ok || wrote == 0) {
            CloseHandle(handle);
            throw std::runtime_error("Failed writing output file: " + path.string());
        }
        writtenTotal += wrote;
    }

    FlushFileBuffers(handle);
    CloseHandle(handle);
}
#endif

} // namespace

void safeWriteFile(const fs::path& outPath, const std::vector<std::uint8_t>& data) {
    const fs::path dir = outPath.has_parent_path() ? outPath.parent_path() : fs::current_path();
    std::error_code ec;
    fs::create_directories(dir, ec);

    const fs::path tmpPath = makeTempPathNear(outPath);
    try {
#ifdef _WIN32
        writeWindowsFile(tmpPath, data);
#else
        writePosixFile(tmpPath, data);
#endif
    } catch (...) {
        std::error_code ignored;
        fs::remove(tmpPath, ignored);
        throw;
    }

    fs::rename(tmpPath, outPath, ec);
    if (ec) {
        std::error_code ignored;
        fs::remove(tmpPath, ignored);
        throw std::runtime_error("Atomic rename failed for output file: " + outPath.string() + " (" + ec.message() + ")");
    }
}
