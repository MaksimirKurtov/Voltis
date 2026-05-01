# Getting Started with Voltis

Welcome to Voltis! This guide will help you set up your environment, build the compiler, and create your first program.

**Navigation:** [README](README.md) · [Compiler Flags](COMPILER_FLAGS.md) · [Platforms](PLATFORMS.md) · [Build System](BUILD_SYSTEM.md) · [Examples](docs/examples.md)

## Table of Contents

- [Prerequisites](#prerequisites)
- [Installation](#installation)
- [Your First Program](#your-first-program)
- [Running Examples](#running-examples)
- [Common Tasks](#common-tasks)
- [Troubleshooting](#troubleshooting)

## Prerequisites

Voltis compiler is written in **C++17** and uses **CMake** for the build system. You'll need:

### Required Tools

- **CMake** 3.16 or later
- **C++17 compiler**: 
  - Windows: MSVC (Visual Studio), Clang, or GCC (MinGW)
  - Linux: GCC 9+, Clang 10+
  - macOS: Clang 12+ (part of Xcode Command Line Tools)

### Platform-Specific Setup

**Windows:**
```powershell
# Install Visual Studio Build Tools (MSVC) or MinGW
# Verify installations:
cmake --version
cl /version          # For MSVC
g++ --version        # For MinGW
```

**Linux (Ubuntu/Debian):**
```bash
sudo apt-get update
sudo apt-get install build-essential cmake git
# Verify:
cmake --version
g++ --version
```

**macOS:**
```bash
xcode-select --install  # Install Xcode Command Line Tools
brew install cmake
# Verify:
cmake --version
clang++ --version
```

## Installation

### Step 1: Clone the Repository

```bash
git clone https://github.com/voltis-lang/voltis.git
cd voltis
```

### Step 2: Build the Compiler

**Windows (MinGW):**
```powershell
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j
```

**Windows (MSVC):**
```powershell
cmake -S . -B build -G "Visual Studio 17 2022"
cmake --build build -j
```

**Linux/macOS:**
```bash
cmake -S . -B build
cmake --build build -j
```

The build process will create the `voltisc` compiler executable in the `build` directory.

### Step 3: Verify the Installation

```bash
# Run the test suite
cd build
ctest --output-on-failure

# Check compiler version/help
./voltisc --list-targets
```

All 53/53 tests should pass. ✅

## Your First Program

### 1. Create a File

Create `hello.vlt`:

```voltis
public fn main() -> int32 {
    print("Hello from Voltis!");
    return 0;
}
```

### 2. Compile It

```bash
./build/voltisc hello.vlt -o hello.exe
```

### 3. Run It

**Windows:**
```powershell
./hello.exe
```

**Linux/macOS:**
```bash
./hello.exe
```

**Expected output:**
```
Hello from Voltis!
```

### 4. Try With Types and Control Flow

Create `calculator.vlt`:

```voltis
public fn add(int32 a, int32 b) -> int32 {
    return a + b;
}

public fn main() -> int32 {
    var result = add(10, 20);
    print("10 + 20 = ");
    print(result.ToString());
    return 0;
}
```

Compile and run:

```bash
./build/voltisc calculator.vlt -o calculator.exe
./calculator.exe
```

**Output:**
```
10 + 20 = 30
```

## Running Examples

Voltis includes several complete examples demonstrating language features:

### Available Examples

| File | Description | Features |
|------|---|---|
| `examples/hello.vlt` | Minimal hello world | `main`, `print` |
| `examples/control_flow.vlt` | Conditionals and loops | `if/else`, `while`, `break`, `continue` |
| `examples/conversions.vlt` | Type conversions | `ToString()`, `ToInt32()`, `ToFloat32()`, `ToBool()`, `Round()`, `Floor()`, `Ceil()` |
| `examples/advanced_control_flow.vlt` | Complex control flow | Nested loops, function calls, accumulation |
| `examples/windows_api.vlt` | Windows API interop | DLL imports, `extern fn`, Windows API calls |

### Run an Example

```bash
./build/voltisc examples/hello.vlt -o hello_example.exe
./hello_example.exe
```

### Run All Examples with Tests

```bash
cd build
ctest --output-on-failure
```

## Common Tasks

### Compile Multiple Files

Voltis supports source file imports:

**main.vlt:**
```voltis
import "utils.vlt";

public fn main() -> int32 {
    print(helper());
    return 0;
}
```

**utils.vlt:**
```voltis
public fn helper() -> string {
    return "Hello from utils!";
}
```

**Compile:**
```bash
./build/voltisc main.vlt -o app.exe
```

### Output Different Formats

**Native Executable (default):**
```bash
./build/voltisc hello.vlt -o hello.exe
```

**Voltis Intermediate Representation (VIR):**
```bash
./build/voltisc hello.vlt --emit-vir -o hello.vir
```

**LLVM Intermediate Representation:**
```bash
./build/voltisc hello.vlt --emit-llvm -o hello.ll
```

**ELF Binary (Linux):**
```bash
./build/voltisc hello.vlt --emit elf -o hello.elf
```

**Mach-O Binary (macOS):**
```bash
./build/voltisc hello.vlt --emit macho -o hello.macho
```

See [COMPILER_FLAGS.md](COMPILER_FLAGS.md) for complete flag reference.

### Use DLL/SO Imports (Windows)

**app.vlt:**
```voltis
import "kernel32.dll";

extern fn GetCurrentProcessId() -> int32 from "kernel32.dll";
extern fn GetTickCount() -> int32 from "kernel32.dll";

public fn main() -> int32 {
    var pid = GetCurrentProcessId();
    var ticks = GetTickCount();
    
    print("Process ID: ");
    print(pid.ToString());
    print(", Ticks: ");
    print(ticks.ToString());
    return 0;
}
```

**Compile and run:**
```bash
./build/voltisc app.vlt -o app.exe
./app.exe
```

See [LINKER_USAGE.md](LINKER_USAGE.md) for more linking examples.

### Cross-Compile to a Different Target

```bash
# List available targets
./build/voltisc --list-all-targets

# Cross-compile to x86_64 Linux
./build/voltisc hello.vlt --target x86_64-unknown-linux-gnu --emit elf -o hello.elf

# Cross-compile to ARM64 (AArch64)
./build/voltisc hello.vlt --target aarch64-unknown-linux-gnu --emit elf -o hello.elf
```

See [PLATFORMS.md](PLATFORMS.md) for target platform details.

### Run Benchmark Mode

Voltis includes an embedded benchmarking tool that measures compilation and execution time:

```bash
./build/voltisc --benchmark
```

This will run a live terminal dashboard showing compilation metrics and save results to a CSV history file.

## Troubleshooting

### "CMake not found"

**Solution:** Install CMake from https://cmake.org/download/ or use your package manager.

**Linux:**
```bash
sudo apt-get install cmake
```

**macOS:**
```bash
brew install cmake
```

### "C++17 compiler not found"

**Windows (Visual Studio):**
- Install Visual Studio 2019 or later with C++ workload

**Linux:**
```bash
sudo apt-get install g++-9
# or
sudo apt-get install clang-10
```

**macOS:**
```bash
xcode-select --install
```

### Build fails with "Permission denied"

**Linux/macOS:**
```bash
chmod +x ./build/voltisc
```

### "voltisc: command not found"

Use the full path to the executable:

```bash
# Windows
./build/voltisc.exe hello.vlt -o hello.exe

# Linux/macOS
./build/voltisc hello.vlt -o hello.exe
```

Or add the build directory to your PATH:

**Linux/macOS:**
```bash
export PATH="$(pwd)/build:$PATH"
voltisc hello.vlt -o hello.exe
```

**Windows (PowerShell):**
```powershell
$env:PATH = "$(pwd)\build;$env:PATH"
./voltisc hello.vlt -o hello.exe
```

### Tests fail after clean build

Ensure CMake completed successfully:

```bash
# Clean rebuild
rm -rf build
cmake -S . -B build
cmake --build build -j
cd build
ctest --output-on-failure
```

### "input.vlt: No such file or directory"

Check the file path. Voltis looks for `.vlt` files relative to the current directory:

```bash
# These are equivalent:
./build/voltisc hello.vlt -o hello.exe
./build/voltisc ./hello.vlt -o hello.exe

# If in a subdirectory:
./build/voltisc examples/hello.vlt -o hello.exe
```

### Execution crashes or produces wrong output

1. **Verify the `.vlt` file is syntactically correct:**
   ```bash
   ./build/voltisc hello.vlt --emit-vir
   ```

2. **Check VIR output for issues:**
   - Review the `.vir` file for unexpected instructions

3. **Try a simpler example first:**
   ```bash
   ./build/voltisc examples/hello.vlt -o test.exe
   ./test.exe
   ```

## Next Steps

- 📖 Read the [Architecture Overview](docs/architecture.md)
- 🔧 Explore [Compiler Flags](COMPILER_FLAGS.md)
- 🌍 Check [Platform Support](PLATFORMS.md)
- 📚 Browse [Examples](docs/examples.md)
- 🔗 Learn about [Linker Usage](LINKER_USAGE.md)
- 🛠️ Understand [Build System](BUILD_SYSTEM.md)
- 🤝 See how to [Contribute](CONTRIBUTING.md)

## Support

- **Issues:** [GitHub Issues](https://github.com/voltis-lang/voltis/issues)
- **Discussions:** [GitHub Discussions](https://github.com/voltis-lang/voltis/discussions)
- **Documentation:** [docs/](docs/)
- **Governance:** [GOVERNANCE.md](GOVERNANCE.md)
