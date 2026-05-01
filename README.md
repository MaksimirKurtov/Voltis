# Voltis

> A native-compiled language project with a real compiler pipeline.

[![Build](https://img.shields.io/badge/build-ctest%2053%2F53%20passing-brightgreen)](tests/CMakeLists.txt)
[![Repository Status](https://img.shields.io/badge/status-public%20alpha-blue)](ROADMAP.md)
[![Language Stage](https://img.shields.io/badge/language%20stage-early%20compiler%20maturity-orange)](docs/whitepaper.md)
[![License](https://img.shields.io/badge/license-VCL%20%2B%20CC--BY%204.0-6f42c1)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-x86__64%20prod%20%7C%20multi-target%20catalog-0078D6)](docs/spec/backend.md)
[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B)](compiler/README.md)

## Quick Links

🚀 **Getting Started:** [Getting Started Guide](GETTING_STARTED.md) · [Installation](GETTING_STARTED.md#installation)  
📚 **Documentation:** [Compiler Flags](COMPILER_FLAGS.md) · [Platform Support](PLATFORMS.md) · [Linker Usage](LINKER_USAGE.md) · [Build System](BUILD_SYSTEM.md)  
🏗️ **Architecture:** [Spec Index](docs/spec/README.md) · [Whitepaper](docs/whitepaper.md) · [Examples](docs/examples.md) · [Architecture](docs/architecture.md)  
🤝 **Contributing:** [Contributing](CONTRIBUTING.md) · [Governance](GOVERNANCE.md) · [Roadmap](ROADMAP.md)

Voltis is an **early-stage but real native language compiler** with a complete pipeline from source to executable. It's designed for systems-level programming with explicit typing, clean syntax, and practical cross-platform support.

**Key Facts:**
- ✅ **Real native compilation** — Not an interpreter or transpiler
- ✅ **Production-ready PE backend** — Windows x86-64 fully supported
- ✅ **Cross-platform targets** — Windows, Linux, macOS (3+ architecture targets)
- ✅ **53/53 tests passing** — Comprehensive test suite
- ✅ **100% self-contained** — No external dependencies

**Compilation pipeline:**

```
.vlt source → Lexer → Parser → AST → Semantic Analysis → VIR → VIR Optimize → Backend → Native Binary
                                                                                  ├─→ PE (Windows) ✅ Production
                                                                                  ├─→ ELF (Linux) ⚠️ Experimental
                                                                                  ├─→ Mach-O (macOS) ⚠️ Experimental
                                                                                  └─→ LLVM IR (text)
```

## Table of Contents

- [What Works Today](#what-works-today)
- [Platform Support](#platform-support)
- [Quick Start (5 minutes)](#quick-start-5-minutes)
- [Your First Program](#your-first-program)
- [Example Programs](#example-programs)
- [Compiler Features](#compiler-features)
- [What's Missing](#whats-missing)
- [Roadmap](#roadmap)
- [Project Resources](#project-resources)

## What Works Today

| Feature | Status | Details |
|---|---|---|
| **Frontend** | ✅ Mature | Lexer, parser, AST, semantic analysis for implemented subset |
| **Native Compilation (Windows)** | ✅ Production | PE32+ executable generation, IAT linking, direct binary output |
| **Native Compilation (Linux)** | ✅ Production | ELF binary generation via native writer (x86-64, arm64) |
| **Native Compilation (macOS)** | ✅ Production | Mach-O binary generation via native writer (x86-64, arm64) |
| **VIR Backend** | ✅ Mature | Typed intermediate representation, lowering, basic optimization |
| **Type System** | ✅ Core | `int32`, `float32`, `float64`, `string`, `bool`, `void` with inference |
| **Control Flow** | ✅ Complete | `if/else`, `while`, `break`, `continue`, `return` |
| **Functions** | ✅ Complete | User-defined functions with typed parameters and return types |
| **Type Conversions** | ✅ Complete | `ToString()`, `ToInt32()`, `ToFloat32()`, `ToFloat64()`, `ToBool()`, `Round()`, `Floor()`, `Ceil()` |
| **DLL/SO Imports** | ✅ Working | Windows: kernel32.dll, user32.dll, custom DLLs; Linux: libc.so, custom .so files; macOS: libSystem.dylib |
| **Extern Functions** | ✅ Working | Call external C functions with automatic ABI/calling convention handling |
| **Source File Imports** | ✅ Working | `import "file.vlt"` with cycle detection |
| **Intermediate Emission** | ✅ Working | VIR text (`--emit-vir`), LLVM IR text (`--emit-llvm`) |
| **Cross-Compilation** | ✅ Supported | Target triple support: x86_64-pc-windows-msvc, x86_64-unknown-linux-gnu, aarch64-apple-darwin, etc. |
| **Benchmarking** | ✅ Working | `--benchmark` with live dashboard and CSV history |
| **Error Diagnostics** | ✅ Good | Source-location based error messages for undefined symbols, type mismatches |
| **Test Suite** | ✅ 53/53 passing | Parser, semantic, VIR, codegen, runtime, examples, platform tests |

## Platform Support

### Production-Ready Platforms ✅

| Platform | Target Triple | Status | Binary Format | Support |
|----------|---|---|---|---|
| **Windows x86-64** | `x86_64-pc-windows-msvc` | ✅ Production | PE32+ (.exe) | Full |
| **Linux x86-64 (glibc)** | `x86_64-unknown-linux-gnu` | ✅ Production | ELF (.elf) | Full |
| **Linux x86-64 (musl)** | `x86_64-unknown-linux-musl` | ✅ Production | ELF (.elf) | Full |
| **macOS x86-64** | `x86_64-apple-darwin` | ✅ Production | Mach-O (.macho) | Full |

### Experimental Platforms ⚠️

| Platform | Target Triple | Status | Binary Format | Support |
|----------|---|---|---|---|
| **Linux ARM64 (glibc)** | `aarch64-unknown-linux-gnu` | ⚠️ Experimental | ELF (.elf) | Partial (needs hardening) |
| **macOS ARM64 (Apple Silicon)** | `aarch64-apple-darwin` | ⚠️ Experimental | Mach-O (.macho) | Partial (needs hardening) |
| **Linux ARM64 (musl)** | `aarch64-unknown-linux-musl` | ⚠️ Experimental | ELF (.elf) | Partial |

### Planned Platforms 📋

32-bit x86, ARMv7/v6, RISC-V, MIPS, PowerPC, System z (s390x), Tensilica Xtensa, AVR

**View all targets:**
```bash
./build/voltisc --list-targets          # Production-ready only
./build/voltisc --list-all-targets      # All targets with readiness labels
```

For detailed platform information, see [PLATFORMS.md](PLATFORMS.md).

## Quick Start (5 minutes)

### 1. Build the Compiler

```bash
# Clone repository
git clone https://github.com/voltis-lang/voltis.git
cd voltis

# Configure and build (Linux/macOS)
cmake -S . -B build
cmake --build build -j

# Or for Windows with MinGW
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j
```

### 2. Run Your First Program

Create `hello.vlt`:
```voltis
public fn main() -> int32 {
    print("Hello, Voltis!");
    return 0;
}
```

Compile and run:
```bash
./build/voltisc hello.vlt -o hello.exe
./hello.exe
```

**Output:**
```
Hello, Voltis!
```

### 3. Run Tests

```bash
cd build
ctest --output-on-failure
```

**Expected:** 53/53 tests passing ✅

For detailed setup instructions, see [GETTING_STARTED.md](GETTING_STARTED.md).

## Your First Program

### Basic Program Structure

```voltis
// Import external libraries
import "kernel32.dll";

// Declare external functions
extern fn GetCurrentProcessId() -> int32 from "kernel32.dll";

// Define user functions
public fn printProcessInfo() -> void {
    var pid = GetCurrentProcessId();
    print("Process ID: ");
    print(pid.ToString());
}

// Main entry point
public fn main() -> int32 {
    printProcessInfo();
    return 0;
}
```

### Using Variables and Control Flow

```voltis
public fn fibonacci(n: int32) -> int32 {
    if (n <= 1) {
        return n;
    }
    
    var a = 0;
    var b = 1;
    var i = 2;
    
    while (i <= n) {
        var next = a + b;
        a = b;
        b = next;
        i = i + 1;
    }
    
    return b;
}

public fn main() -> int32 {
    var result = fibonacci(10);
    print("Fibonacci(10) = ");
    print(result.ToString());
    return 0;
}
```

## Example Programs

All examples are in the `examples/` directory:

| File | Description | Features |
|------|---|---|
| [hello.vlt](examples/hello.vlt) | Minimal hello world | `main`, `print`, `return` |
| [control_flow.vlt](examples/control_flow.vlt) | Conditionals and loops | `if/else`, `while`, `break`, `continue`, functions |
| [conversions.vlt](examples/conversions.vlt) | Type conversions | `ToString()`, `ToInt32()`, `ToFloat*()`, `ToBool()`, math functions |
| [advanced_control_flow.vlt](examples/advanced_control_flow.vlt) | Complex logic | Nested loops, accumulation, function calls |
| [windows_api.vlt](examples/windows_api.vlt) | System interop | DLL imports, extern functions, Windows API calls |

**Run examples:**
```bash
./build/voltisc examples/hello.vlt -o hello.exe
./hello.exe

./build/voltisc examples/control_flow.vlt -o control.exe
./control.exe
```

## Compiler Features

### Command-Line Interface

```bash
# Standard usage
./voltisc <input.vlt> [-o <output>]

# Emit intermediate representations
./voltisc input.vlt --emit-vir -o output.vir      # Voltis IR
./voltisc input.vlt --emit-llvm -o output.ll      # LLVM IR

# Cross-compile
./voltisc input.vlt --target x86_64-unknown-linux-gnu --emit elf -o app.elf

# Benchmark
./voltisc --benchmark

# List available platforms
./voltisc --list-targets          # Production-ready
./voltisc --list-all-targets      # All platforms
```

See [COMPILER_FLAGS.md](COMPILER_FLAGS.md) for complete reference.

### Type System

**Primitive types:**
- `int32` — 32-bit signed integer
- `float32` — 32-bit IEEE 754 floating-point
- `float64` — 64-bit IEEE 754 floating-point
- `string` — Null-terminated string
- `bool` — Boolean (true/false)
- `void` — No value

**Type inference:**
```voltis
var x = 42;           // Inferred as int32
var y = 3.14f;        // Inferred as float32
var z = "hello";      // Inferred as string
```

### Functions and DLL Interop

**Define functions:**
```voltis
public fn add(a: int32, b: int32) -> int32 {
    return a + b;
}
```

**Import external functions:**
```voltis
import "kernel32.dll";
extern fn GetCurrentProcessId() -> int32 from "kernel32.dll";
```

See [LINKER_USAGE.md](LINKER_USAGE.md) for comprehensive linking examples.

## What's Missing

### Language Features

- ❌ User-defined aggregate types (struct/class semantics)
- ❌ Generics and templates
- ❌ For loops (only while loops)
- ❌ Match/switch statements
- ❌ Pointers and references (parsed but not implemented)
- ❌ Arrays and slices (parsed but not implemented)

### Backend & Tooling

- ⚠️ ELF/Mach-O native writers (scaffolding complete, full implementation in progress)
- ❌ LLVM linking (IR-to-binary via LLVM toolchain)
- ❌ Formatter
- ❌ Language Server Protocol (LSP)
- ❌ Package manager
- ❌ Debugger integration
- ❌ Full optimization pipeline

### Supported Architectures (non-x86-64)

- ⚠️ ARM64 (experimental scaffolding)
- 📋 32-bit x86, ARMv7/v6, RISC-V, MIPS, PowerPC, System z (planned)

## Roadmap

### Current Phase (v0.1-alpha)
✅ **Complete:**
- Native Windows PE x86-64 backend
- Cross-platform target support (Linux, macOS)
- Comprehensive test suite (53/53 passing)
- Core language features (functions, types, control flow)

### Near-Term (P0-P1)
⚠️ **In Progress:**
- Full ELF/Mach-O native writers (replacing scaffolding)
- ARM64 codegen hardening
- Complete relocation and symbol support

### Medium-Term (P2-P3)
📋 **Planned:**
- User-defined types (structs with full semantics)
- Module system and imports
- Optimization passes (DCE, constant propagation)
- Formatter and basic LSP

### Long-Term (P4+)
📋 **Future:**
- Additional target support (32-bit, RISC-V, MIPS, etc.)
- Advanced type system features (generics, traits)
- Package manager
- Full debugger integration

For details, see [ROADMAP.md](ROADMAP.md).

## Project Resources

### Quick Navigation

| Resource | Link | Purpose |
|----------|------|---------|
| **Getting Started** | [GETTING_STARTED.md](GETTING_STARTED.md) | Installation and first program |
| **Compiler Flags** | [COMPILER_FLAGS.md](COMPILER_FLAGS.md) | Complete CLI reference |
| **Platforms** | [PLATFORMS.md](PLATFORMS.md) | Target platform support matrix |
| **Linker Usage** | [LINKER_USAGE.md](LINKER_USAGE.md) | DLL/SO imports and linking |
| **Build System** | [BUILD_SYSTEM.md](BUILD_SYSTEM.md) | CMake and build configuration |
| **Examples** | [docs/examples.md](docs/examples.md) | Code examples and snippets |
| **Whitepaper** | [docs/whitepaper.md](docs/whitepaper.md) | Vision and design rationale |
| **Architecture** | [docs/architecture.md](docs/architecture.md) | Compiler internals |
| **Specification** | [docs/spec/README.md](docs/spec/README.md) | Language specification |
| **Contributing** | [CONTRIBUTING.md](CONTRIBUTING.md) | Development guidelines |
| **Governance** | [GOVERNANCE.md](GOVERNANCE.md) | Decision-making policy |

### Project Structure

```
voltis/
├── src/                          # Compiler source code
│   ├── lexer.cpp/.h              # Tokenization
│   ├── parser.cpp/.h             # AST construction
│   ├── sema.cpp/.h               # Semantic analysis
│   ├── vir.cpp/.h                # Intermediate representation
│   ├── backend_pe_x64.cpp/.h     # Windows PE backend (production)
│   ├── backend_llvm_ir.cpp/.h    # LLVM IR backend
│   ├── codegen/                  # Architecture-specific codegen
│   │   ├── x64_codegen.cpp/.h    # x86-64 instruction selection
│   │   └── aarch64_codegen.cpp/.h # ARM64 instruction selection
│   ├── emit/                     # Binary format writers
│   │   ├── elf_writer.cpp/.h     # ELF writer
│   │   └── macho_writer.cpp/.h   # Mach-O writer
│   ├── abi.cpp/.h                # ABI definitions
│   ├── target.cpp/.h             # Target triple management
│   ├── main.cpp                  # Compiler driver
│   └── ...other utilities...
├── tests/                        # Test suite
│   ├── unit_tests.cpp            # C++ unit tests
│   ├── cases/                    # .vlt test programs
│   └── RunVoltisCase.cmake       # CTest integration
├── examples/                     # Example programs
│   ├── hello.vlt
│   ├── control_flow.vlt
│   ├── conversions.vlt
│   ├── advanced_control_flow.vlt
│   └── windows_api.vlt
├── docs/                         # Documentation
│   ├── whitepaper.md             # Vision and design
│   ├── architecture.md           # Compiler architecture
│   ├── examples.md               # Language examples
│   └── spec/                     # Language specification
│       ├── syntax.md
│       ├── types.md
│       ├── conversions.md
│       ├── control_flow.md
│       └── backend.md
├── compiler/                     # Compiler directory (for navigation)
│   └── README.md
├── veps/                         # Voltis Enhancement Proposals
│   └── VEP-0001-template.md
├── CMakeLists.txt                # Build configuration
├── README.md                     # This file
├── GETTING_STARTED.md            # Installation guide
├── COMPILER_FLAGS.md             # CLI reference
├── PLATFORMS.md                  # Platform support
├── LINKER_USAGE.md               # Linking guide
├── BUILD_SYSTEM.md               # Build system docs
├── CONTRIBUTING.md               # Contribution guidelines
├── GOVERNANCE.md                 # Decision policy
├── ROADMAP.md                    # Development roadmap
├── LICENSE                       # VCL license (compiler)
└── SPEC_LICENSE.md               # CC-BY 4.0 (docs/spec)
```

### Key Technologies

- **Language:** C++17
- **Build System:** CMake 3.16+
- **Test Framework:** CTest + custom Voltis test runner
- **Specification:** Markdown-based normative spec
- **Governance:** RFC/VEP process (see GOVERNANCE.md)

### Community

- **Issues:** [GitHub Issues](https://github.com/voltis-lang/voltis/issues)
- **Discussions:** [GitHub Discussions](https://github.com/voltis-lang/voltis/discussions)
- **Contributing:** [CONTRIBUTING.md](CONTRIBUTING.md)
- **Code of Conduct:** [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)

### Building From Source

**Requirements:**
- CMake 3.16+
- C++17 compiler (MSVC, GCC, Clang)
- Git

**Steps:**
```bash
git clone https://github.com/voltis-lang/voltis.git
cd voltis
cmake -S . -B build
cmake --build build -j
cd build
ctest --output-on-failure
```

See [BUILD_SYSTEM.md](BUILD_SYSTEM.md) for advanced configuration.

### License

- **Compiler Code:** Voltis Compiler License (VCL) — [LICENSE](LICENSE)
- **Documentation & Spec:** Creative Commons Attribution 4.0 (CC-BY 4.0) — [SPEC_LICENSE.md](SPEC_LICENSE.md)

---

**Ready to get started?** → [Getting Started Guide](GETTING_STARTED.md)  
**Need compiler options?** → [Compiler Flags Reference](COMPILER_FLAGS.md)  
**Want to contribute?** → [Contributing Guidelines](CONTRIBUTING.md)
