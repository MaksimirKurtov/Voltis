# Voltis

> A Windows-first, native-compiled language project with a real compiler pipeline.

[![Build](https://img.shields.io/badge/build-ctest%2025%2F25%20passing-brightgreen)](tests/CMakeLists.txt)
[![Repository Status](https://img.shields.io/badge/status-public%20alpha-blue)](ROADMAP.md)
[![Language Stage](https://img.shields.io/badge/language%20stage-early%20compiler%20maturity-orange)](docs/whitepaper.md)
[![License](https://img.shields.io/badge/license-VCL%20%2B%20CC--BY%204.0-6f42c1)](LICENSE)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-0078D6)](docs/spec/backend.md)
[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B)](compiler/README.md)

📖 [Whitepaper](docs/whitepaper.md) · 🧭 [Spec Index](docs/spec/README.md) · 🧪 [Examples Guide](docs/examples.md) · 🤝 [Contributing](CONTRIBUTING.md) · 🏛️ [Governance](GOVERNANCE.md)

Voltis is an early-stage but real language toolchain.  
Current default path:

`source -> lexer -> parser -> AST -> semantic analysis -> VIR -> VIR optimize/verify -> backend -> Windows x64 PE .exe`

Voltis is **not** an interpreter and is **not** a transpile-first architecture.

## Table of contents

- [Hello world](#hello-world)
- [What works today](#what-works-today)
- [What is still missing](#what-is-still-missing)
- [Architecture overview](#architecture-overview)
- [Quick start](#quick-start)
- [Compile commands](#compile-commands)
- [Project layout](#project-layout)
- [Documentation and authority](#documentation-and-authority)

## Hello world

```voltis
public fn main() -> int32 {
    print("Hello from Voltis");
    return 0;
}
```

## What works today

| Area | Current status |
|---|---|
| Frontend | Lexer, parser, AST, semantic analysis for the implemented subset |
| Type system | `int32`, `float32`, `float64`, `string`, `bool`, `void` |
| Control flow | `if/else`, `while`, `break`, `continue`, `return`/`return;` |
| Conversions | `ToString`, `ToInt32`, `ToFloat32`, `ToFloat64`, `ToBool`, `Round`, `Floor`, `Ceil` |
| Interop | `import` + `extern fn ... from ...;` declarations with PE IAT-backed DLL calls |
| IR | Typed VIR model/lowering plus optimization + structural verification passes (`src/vir.*`, `src/lowering.*`, `src/vir_passes.*`) |
| Backends | Native PE x64 executable output (default) + LLVM IR text (`--emit-llvm`) |
| Validation | CMake/CTest suite with parser, sema, VIR, and runtime-oriented cases |

## What is still missing

| Category | Not complete yet |
|---|---|
| Language surface | User-defined type system, generics, module/package system beyond direct DLL imports |
| Backend maturity | Full DLL/import-lib workflow, wider ABI coverage, richer target controls |
| Optimization | VIR pass pipeline and optimization stages |
| Tooling | Formatter, LSP, package manager, debugger workflow |

## Architecture overview

```text
.vlt source
  -> lexer
  -> parser
  -> AST
  -> semantic analysis
  -> VIR lowering
  -> VIR optimization + verification
  -> backend abstraction
       -> PE x64 backend (default native executable)
       -> LLVM IR text backend (--emit-llvm)
```

More detail: [docs/architecture.md](docs/architecture.md) and [docs/spec/backend.md](docs/spec/backend.md).

## Quick start

### 1) Build

```bash
# Windows (MinGW)
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j

# Default generator (including Linux/macOS)
cmake -S . -B build
cmake --build build -j
```

### 2) Run tests

```bash
ctest --test-dir build --output-on-failure
```

### 3) Compile and run

```bash
build/voltisc examples/hello.vlt -o hello.exe
./hello.exe
```

## Compile commands

```bash
# Native executable (default)
build/voltisc examples/hello.vlt -o hello.exe

# Emit VIR text
build/voltisc examples/hello.vlt --emit-vir -o hello.vir

# Emit LLVM IR text
build/voltisc examples/hello.vlt --emit-llvm -o hello.ll

# Temporary bootstrap C++ path (non-production)
build/voltisc examples/hello.vlt --bootstrap-cpp --no-link
```

## Project layout

```text
.
├── compiler/
├── docs/
│   ├── whitepaper.md
│   ├── examples.md
│   └── spec/
├── examples/
├── veps/
├── src/
└── tests/
```

`src/` and `tests/` are the active implementation roots used by CMake.

## Documentation and authority

| Document | Purpose |
|---|---|
| [docs/whitepaper.md](docs/whitepaper.md) | Vision, architecture rationale, and project direction |
| [docs/spec/README.md](docs/spec/README.md) | Normative spec entrypoint |
| [docs/examples.md](docs/examples.md) | Quick reference with runnable feature examples |
| [CONTRIBUTING.md](CONTRIBUTING.md) | PR workflow and contribution rules |
| [GOVERNANCE.md](GOVERNANCE.md) | Language authority and decision policy |
| [ROADMAP.md](ROADMAP.md) | Short/mid/long-term execution plan |
| [LICENSE](LICENSE) | Compiler code license (VCL) |
| [SPEC_LICENSE.md](SPEC_LICENSE.md) | Spec/docs license (CC BY 4.0) |

Language authority is defined by this repository and its spec documents. See [GOVERNANCE.md](GOVERNANCE.md).
