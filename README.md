# Voltis

[![License](https://img.shields.io/badge/license-VCL%20%2B%20CC--BY%204.0-blue)](LICENSE)
[![C++](https://img.shields.io/badge/C%2B%2B-17-00599C?logo=c%2B%2B)](compiler/README.md)
[![Voltis](https://img.shields.io/badge/language-Voltis-6f42c1)](docs/spec/syntax.md)
[![Build](https://img.shields.io/badge/build-cmake%20%2B%20ctest-blue)](README.md#quick-start)
[![Stage](https://img.shields.io/badge/stage-early%20alpha-orange)](ROADMAP.md)
[![Stars](https://img.shields.io/github/stars/MaksimirKurtov/Voltis?style=flat)](https://github.com/MaksimirKurtov/Voltis/stargazers)
[![Platform](https://img.shields.io/badge/platform-Windows%20x64-0078D6)](docs/spec/backend.md)

📓 [Whitepaper](docs/whitepaper.md) · 💬 [Discussions](https://github.com/MaksimirKurtov/Voltis/discussions) · 📚 [Wiki](https://github.com/MaksimirKurtov/Voltis/wiki)

**Voltis** is an early-stage native compiled language with a real working pipeline:

`source -> lexer -> parser -> AST -> semantic analysis -> VIR -> backend -> Windows x64 .exe`

It is not an interpreter and not a transpiler-first project.

## Contents

- [Quick start](#quick-start)
- [Examples](#examples)
- [What works today](#what-works-today)
- [Current limits](#current-limits)
- [Architecture overview](#architecture-overview)
- [Compile commands](#compile-commands)
- [Project layout](#project-layout)
- [Docs and governance](#docs-and-governance)

## Quick start

### 1. Build

```bash
# Windows (MinGW)
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j

# Linux/macOS or default generator
cmake -S . -B build
cmake --build build -j
```

### 2. Run tests

```bash
ctest --test-dir build --output-on-failure
```

### 3. Compile a sample

```bash
build/voltisc examples/hello.vlt -o hello.exe
```

## Examples

Want runnable sample programs first? Start here:

- **Examples index:** [Examples](docs/examples.md)
- Includes a **downloadable file table** and quick usage notes.

## What works today

| Area | Status |
|---|---|
| Frontend pipeline | Lexer, parser, AST, semantic checks for implemented subset |
| IR | Typed VIR generation (`src/vir.*`, `src/lowering.*`) |
| Native backend | Windows x64 PE executable emission (default mode) |
| LLVM path | LLVM IR text emission with `--emit-llvm` |
| Diagnostics | Parser and semantic diagnostics for symbol/type/control-flow issues |
| Tests | CMake/CTest suite with parser, sema, VIR, and native compile/runtime cases |

## Current limits

| Category | Not yet implemented |
|---|---|
| Language surface | Modules, user-defined types, generics, full interop model |
| Backend maturity | Full DLL workflow, broader ABI coverage, optimization pipeline |
| Tooling | Formatter, language server, package manager, debugger integration |

## Architecture overview

```text
.vlt source
  -> lexer
  -> parser
  -> AST
  -> semantic analysis
  -> VIR lowering
  -> backend abstraction
     -> PE x64 backend (default, native .exe)
     -> LLVM IR text backend (--emit-llvm)
```

See [docs/architecture.md](docs/architecture.md) and [docs/spec/backend.md](docs/spec/backend.md).

## Compile commands

```bash
# Native executable (default path)
build/voltisc examples/hello.vlt -o hello.exe

# Emit VIR text
build/voltisc examples/hello.vlt --emit-vir -o hello.vir

# Emit LLVM IR text
build/voltisc examples/hello.vlt --emit-llvm -o hello.ll

# Optional bootstrap C++ path (temporary)
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

`src/` and `tests/` are the current compiler implementation roots used by the active CMake build.

## Docs and governance

| Document | Purpose |
|---|---|
| [docs/whitepaper.md](docs/whitepaper.md) | Design rationale and direction |
| [Examples](docs/examples.md) | Example file index + quick language reference |
| [docs/spec/syntax.md](docs/spec/syntax.md) | Syntax specification (implemented subset + direction) |
| [docs/spec/types.md](docs/spec/types.md) | Type system specification |
| [docs/spec/conversions.md](docs/spec/conversions.md) | Conversion model and rules |
| [docs/spec/control_flow.md](docs/spec/control_flow.md) | Control-flow semantics |
| [docs/spec/backend.md](docs/spec/backend.md) | Compiler/backend architecture and outputs |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Contribution workflow and review policy |
| [GOVERNANCE.md](GOVERNANCE.md) | Language authority and project governance |
| [LICENSE](LICENSE) | Compiler code license |
| [SPEC_LICENSE.md](SPEC_LICENSE.md) | Documentation/spec license (CC BY 4.0) |
| [ROADMAP.md](ROADMAP.md) | Short/mid/long-term direction |
