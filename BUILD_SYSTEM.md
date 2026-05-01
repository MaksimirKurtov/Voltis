# Voltis Build System Documentation

Complete reference for building, configuring, and extending the Voltis compiler project.

**Navigation:** [README](README.md) · [Getting Started](GETTING_STARTED.md) · [Compiler Flags](COMPILER_FLAGS.md) · [Contributing](CONTRIBUTING.md)

## Table of Contents

- [Build System Overview](#build-system-overview)
- [Build Prerequisites](#build-prerequisites)
- [Build Procedures](#build-procedures)
- [Build Targets](#build-targets)
- [Testing](#testing)
- [Build Configuration](#build-configuration)
- [Customization](#customization)
- [CI/CD Integration](#cicd-integration)
- [Troubleshooting](#troubleshooting)

---

## Build System Overview

### Architecture

Voltis uses **CMake 3.16+** as its meta-build system:

```
CMakeLists.txt (root configuration)
├── src/CMakeLists.txt (compiler sources)
├── tests/CMakeLists.txt (test suite)
└── docs/ (documentation)
```

### Build Output

| Artifact | Location | Purpose |
|----------|----------|---------|
| **voltisc** | `build/voltisc` (Unix) or `build/voltisc.exe` (Windows) | Main compiler executable |
| **Test executable** | `build/unit_tests` | Unit test suite |
| **Build cache** | `build/CMakeCache.txt` | CMake configuration |

### Key Design Principles

1. **Single executable:** One `voltisc` binary; no runtime dependencies
2. **Platform detection:** Automatic host platform discovery
3. **CTest integration:** Standard CMake testing framework
4. **Dependency-free:** No external C++ libraries (only standard library)

---

## Build Prerequisites

### Minimum Requirements

| Component | Version | Platform |
|-----------|---------|----------|
| **CMake** | 3.16+ | All |
| **C++ Compiler** | C++17 compatible | All |

### Compiler Support

| Compiler | Version | Platform | Status |
|----------|---------|----------|--------|
| **MSVC** | 2019+ | Windows | ✅ Full |
| **GCC** | 9.0+ | Linux, MinGW | ✅ Full |
| **Clang** | 10.0+ | Linux, macOS, Windows | ✅ Full |
| **Apple Clang** | 12.0+ | macOS | ✅ Full |

### Installation

**Windows (MSVC):**
```powershell
# Install Visual Studio 2019+ with C++ workload
# Or install Visual Studio Build Tools

cmake --version    # Verify CMake
cl /version        # Verify MSVC
```

**Windows (MinGW):**
```powershell
# Install MinGW via https://www.mingw-w64.org/ or package manager
cmake --version
g++ --version
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get update
sudo apt-get install build-essential cmake git
cmake --version
g++ --version
```

**Linux (Fedora/RHEL):**
```bash
sudo dnf install cmake gcc-c++ make git
cmake --version
g++ --version
```

**macOS:**
```bash
xcode-select --install  # Xcode Command Line Tools
brew install cmake      # CMake via Homebrew
cmake --version
clang++ --version
```

---

## Build Procedures

### Standard Build (Default Generator)

**Linux/macOS:**
```bash
# Configure
cmake -S . -B build

# Build
cmake --build build -j

# Verify
./build/voltisc --list-targets
```

**Windows (with Visual Studio):**
```powershell
# Configure (generates Visual Studio project)
cmake -S . -B build -G "Visual Studio 17 2022"

# Build
cmake --build build -j

# Verify
./build/Release/voltisc.exe --list-targets
```

### Explicit Generator Selection

**Linux/macOS (Unix Makefiles):**
```bash
cmake -S . -B build -G "Unix Makefiles"
cmake --build build -j
```

**Linux (Ninja):**
```bash
cmake -S . -B build -G "Ninja"
cmake --build build -j
```

**Windows (MinGW):**
```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j
```

**Windows (MSVC):**
```powershell
cmake -S . -B build -G "Visual Studio 17 2022" -A x64
cmake --build build --config Release -j
```

### Multi-Configuration Build (Visual Studio)

```powershell
# Build Debug and Release variants
cmake --build build --config Debug -j
cmake --build build --config Release -j

# Default (typically Release)
./build/Release/voltisc.exe -h
```

---

## Build Targets

### Main Compiler Target

```bash
# Build just the compiler
cmake --build build --target voltisc
```

### Test Targets

```bash
# Build and run all tests
cmake --build build --target test

# Run only tests (after building)
cd build
ctest --output-on-failure
```

### Verbose Build

```bash
# Show full compiler commands
cmake --build build --verbose
```

### Clean Build

```bash
# Remove build artifacts
rm -rf build          # Unix/macOS
rmdir /s build        # Windows

# Clean reconfigure and rebuild
cmake -S . -B build
cmake --build build -j
```

---

## Testing

### Run All Tests

```bash
cd build
ctest --output-on-failure
```

### Run Specific Test

```bash
cd build
ctest -R "test_name_pattern" --output-on-failure
```

### Run with Output

```bash
cd build
ctest --verbose
```

### Test Suite Organization

| Test Type | Location | Count | Coverage |
|-----------|----------|-------|----------|
| **Parser tests** | `tests/unit_tests.cpp` | ~8 | Syntax parsing |
| **Semantic tests** | `tests/unit_tests.cpp` | ~4 | Type checking |
| **VIR tests** | `tests/cases/*.vlt` | ~3 | IR generation |
| **Backend tests** | `tests/cases/*.vlt` | ~5 | Code generation |
| **Runtime tests** | `tests/cases/*.vlt` | ~3 | Execution |
| **Tool tests** | `tests/cases/*.vlt` | ~2 | CLI commands |
| **Example tests** | `tests/cases/*.vlt` | ~3 | Documented examples |

**Total:** 25/25 passing ✅

### Custom Test Case

Add a `.vlt` file to `tests/cases/`:

```voltis
// tests/cases/my_test.vlt
public fn main() -> int32 {
    print("Test output");
    return 0;
}
```

CTest will automatically discover and run it.

---

## Build Configuration

### CMake Variables

Key CMake variables you can configure:

| Variable | Type | Default | Description |
|----------|------|---------|---|
| `CMAKE_BUILD_TYPE` | String | `Release` | Build type (Debug, Release, RelWithDebInfo, MinSizeRel) |
| `CMAKE_CXX_COMPILER` | Path | Auto | C++ compiler path |
| `CMAKE_CXX_FLAGS` | String | `-std=c++17` | C++ compiler flags |
| `ENABLE_TESTING` | Bool | `ON` | Enable CTest |
| `VOLTIS_PLATFORM_*` | Bool | Auto-detected | Platform defines (WINDOWS, LINUX, MACOS) |
| `VOLTIS_ARCH_*` | Bool | Auto-detected | Architecture (X86_64, AARCH64) |

### Debug Build

```bash
# Configure with debug symbols
cmake -S . -B build -DCMAKE_BUILD_TYPE=Debug
cmake --build build -j

# Build is now debuggable:
gdb ./build/voltisc
```

### Release Build (Optimized)

```bash
cmake -S . -B build -DCMAKE_BUILD_TYPE=Release
cmake --build build -j
```

### Custom Compiler

```bash
# Use a specific C++ compiler
cmake -S . -B build -DCMAKE_CXX_COMPILER=/usr/bin/clang++
cmake --build build -j
```

### Platform-Specific Configuration

```bash
# Detect platform at configure time (already automatic)
cmake -S . -B build
cat build/CMakeCache.txt | grep VOLTIS_PLATFORM  # View detected platform
```

---

## Customization

### Adding Source Files

1. **Create source file** in `src/`:
   ```cpp
   // src/my_module.cpp
   #include "my_module.h"
   void myFunction() { /* ... */ }
   ```

2. **Create header** in `src/`:
   ```cpp
   // src/my_module.h
   #pragma once
   void myFunction();
   ```

3. **Register in CMakeLists.txt:**
   ```cmake
   # In src/CMakeLists.txt
   add_executable(voltisc
       # ... existing files ...
       my_module.cpp
       my_module.h
   )
   ```

4. **Rebuild:**
   ```bash
   cmake --build build
   ```

### Adding Tests

1. **Create test file** in `tests/cases/`:
   ```voltis
   // tests/cases/my_feature.vlt
   public fn main() -> int32 {
       // Test code
       return 0;
   }
   ```

2. **Rebuild to auto-discover:**
   ```bash
   cmake --build build
   ctest --test-dir build
   ```

### Modifying Compiler Flags

Edit `src/CMakeLists.txt`:

```cmake
# Add custom compiler flags
add_executable(voltisc ...)
target_compile_options(voltisc PRIVATE -Wall -Wextra -pedantic)
```

Then rebuild:

```bash
cmake --build build
```

---

## CI/CD Integration

### GitHub Actions Example

```yaml
# .github/workflows/build.yml
name: Build and Test

on: [push, pull_request]

jobs:
  build:
    runs-on: ${{ matrix.os }}
    strategy:
      matrix:
        os: [ubuntu-latest, windows-latest, macos-latest]
        compiler: [gcc, clang]
    steps:
      - uses: actions/checkout@v2
      
      - name: Configure
        run: cmake -S . -B build
      
      - name: Build
        run: cmake --build build -j
      
      - name: Test
        run: |
          cd build
          ctest --output-on-failure
```

### Local CI Simulation

```bash
# Build on all supported configurations
for config in Debug Release; do
    rm -rf build
    cmake -S . -B build -DCMAKE_BUILD_TYPE=$config
    cmake --build build -j
    cd build
    ctest --output-on-failure || exit 1
    cd ..
done
```

---

## Troubleshooting

### CMake Configuration Fails

**Error:** "CMake not found"

**Solution:**
```bash
# Install CMake
# Windows: Download from https://cmake.org/download/
# Linux: sudo apt-get install cmake
# macOS: brew install cmake

which cmake
cmake --version
```

---

### C++ Compiler Not Found

**Error:** "Could not find C++ compiler"

**Solution:**
```bash
# Check compiler availability
which g++          # Linux/macOS
where cl.exe       # Windows (MSVC)

# Install compiler
# Windows: Visual Studio or MinGW
# Linux: sudo apt-get install build-essential
# macOS: xcode-select --install
```

---

### Out-of-Tree Build Fails

**Error:** "Source and build directories are the same"

**Solution:** Always use a separate build directory:
```bash
# ✅ Correct
cmake -S . -B build
cmake --build build

# ❌ Wrong
cmake .
cmake --build .
```

---

### Stale CMake Cache

**Error:** Build fails after significant changes

**Solution:** Clean and reconfigure:
```bash
rm -rf build
cmake -S . -B build
cmake --build build -j
```

---

### Build Directory Permission Issues

**Error:** "Permission denied" when building

**Solution (Linux/macOS):**
```bash
chmod -R u+w build
cmake --build build -j
```

---

### Ninja Build Fails

**Error:** "Ninja command not found"

**Solution:** Use default generator instead:
```bash
rm -rf build
cmake -S . -B build   # Uses default generator
cmake --build build -j
```

Or install Ninja:
```bash
sudo apt-get install ninja-build  # Linux
brew install ninja               # macOS
```

---

### CTest Doesn't Find Tests

**Error:** "No tests were found"

**Solution:**
1. Ensure `enable_testing()` is in CMakeLists.txt
2. Rebuild CMake configuration:
   ```bash
   rm -rf build
   cmake -S . -B build
   cmake --build build
   cd build
   ctest --verbose  # Lists discovered tests
   ```

---

### Compiler Warnings or Errors

**Solution:** Use verbose build output:
```bash
cmake --build build --verbose

# Then address warnings/errors in source code
# Common fixes:
# - Use #include <header> instead of #include "header"
# - Initialize all variables
# - Use const for non-mutable references
```

---

## Advanced Topics

### Static Analysis

```bash
# Clang static analyzer
scan-build cmake --build build

# Or use compiler-integrated analysis
cmake -S . -B build -DCMAKE_CXX_FLAGS="-Wall -Wextra -Wconversion"
cmake --build build
```

### Code Coverage

```bash
# Build with coverage flags (GCC/Clang)
cmake -S . -B build -DCMAKE_CXX_FLAGS="--coverage"
cmake --build build
cd build
ctest
lcov --capture --directory . --output-file coverage.info
genhtml coverage.info --output-directory coverage_report
open coverage_report/index.html  # View HTML report
```

### LTO (Link Time Optimization)

```bash
cmake -S . -B build \
  -DCMAKE_CXX_FLAGS_RELEASE="-O3 -flto" \
  -DCMAKE_EXE_LINKER_FLAGS_RELEASE="-flto"
cmake --build build -j
```

### Cross-Compilation

```bash
# Create toolchain file (cross-compile-arm64.cmake)
set(CMAKE_SYSTEM_NAME Linux)
set(CMAKE_SYSTEM_PROCESSOR aarch64)
set(CMAKE_CXX_COMPILER aarch64-linux-gnu-g++)

# Use toolchain
cmake -S . -B build -DCMAKE_TOOLCHAIN_FILE=cross-compile-arm64.cmake
cmake --build build -j
```

---

## See Also

- [CMake Documentation](https://cmake.org/documentation/)
- [CONTRIBUTING.md](CONTRIBUTING.md) — Development guidelines
- [GETTING_STARTED.md](GETTING_STARTED.md) — Quick start
- [Compiler Flags](COMPILER_FLAGS.md) — Usage reference
