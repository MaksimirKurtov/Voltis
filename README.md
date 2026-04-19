# Voltis

![Build](https://img.shields.io/badge/build-cmake%20%2B%20ctest-blue)
![Project status](https://img.shields.io/badge/status-active%20development-orange)
![Language stage](https://img.shields.io/badge/stage-early%20alpha-purple)

**Voltis** is an early-stage, native compiled language project with a working compiler pipeline from `.vlt` source to Windows x64 PE executables.

It is not an interpreter and not a transpiler-first project. The production path is source -> AST -> semantic analysis -> VIR -> backend -> native binary.

⚙️ Native compiler path is active. 🧪 CMake/CTest coverage is included.

## Contents

- [Why Voltis](#why-voltis)
- [Hello world](#hello-world)
- [What works today](#what-works-today)
- [What's missing](#whats-missing)
- [Architecture overview](#architecture-overview)
- [Quick start](#quick-start)
- [Compile commands](#compile-commands)
- [Repository layout](#repository-layout)
- [Documentation](#documentation)
- [Governance and licensing](#governance-and-licensing)

## Why Voltis

Voltis aims to provide:

- native compilation for Windows-first systems work
- readable syntax with explicit typing
- a custom intermediate representation (VIR)
- a clear path toward broader language and backend maturity

## Hello world

```voltis
public fn main() -> int32 {
    print("Hello, Voltis!");
    return 0;
}
```

## What works today

| Area | Current state |
|---|---|
| Frontend pipeline | Lexer, parser, AST, semantic checks for implemented subset |
| IR | Typed VIR generation (`src/vir.*`, `src/lowering.*`) |
| Native backend | Windows x64 PE executable emission (default mode) |
| LLVM path | LLVM IR text emission with `--emit-llvm` |
| Diagnostics | Parser and semantic diagnostics for symbol/type/control-flow issues |
| Tests | CMake/CTest suite with parser, sema, VIR, and native compile/runtime cases |

## What's missing

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

## Quick start

### Build (Windows MinGW)

```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j
```

### Build (Linux/macOS or default generator)

```bash
cmake -S . -B build
cmake --build build -j
```

### Run tests

```bash
ctest --test-dir build --output-on-failure
```

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

## Repository layout

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

## Documentation

| Document | Purpose |
|---|---|
| [docs/whitepaper.md](docs/whitepaper.md) | Design rationale and direction |
| [docs/spec/syntax.md](docs/spec/syntax.md) | Syntax specification (implemented subset + direction) |
| [docs/spec/types.md](docs/spec/types.md) | Type system specification |
| [docs/spec/conversions.md](docs/spec/conversions.md) | Conversion model and rules |
| [docs/spec/control_flow.md](docs/spec/control_flow.md) | Control-flow semantics |
| [docs/spec/backend.md](docs/spec/backend.md) | Compiler/backend architecture and outputs |
| [docs/examples.md](docs/examples.md) | Quick reference for implemented language usage |
| [CONTRIBUTING.md](CONTRIBUTING.md) | Contribution workflow and review policy |

## Governance and licensing

- Compiler code: [LICENSE](LICENSE)
- Language specification/docs: [SPEC_LICENSE.md](SPEC_LICENSE.md) (CC BY 4.0)
- Project governance and language authority: [GOVERNANCE.md](GOVERNANCE.md)
- Community standards: [CODE_OF_CONDUCT.md](CODE_OF_CONDUCT.md)
- Direction and milestones: [ROADMAP.md](ROADMAP.md)
