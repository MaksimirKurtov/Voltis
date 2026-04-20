# Voltis Architecture Status (code-truth, whitepaper-aligned)

Navigation: [README](../README.md) · [Whitepaper](whitepaper.md) · [Backend spec](spec/backend.md) · [Examples guide](examples.md)

## 1) Current implemented architecture

`voltisc` currently executes this flow:

```text
Voltis source (.vlt)
  -> Lexer
  -> Parser
  -> AST
  -> Semantic analysis (subset)
  -> Typed VIR lowering
  -> VIR optimization + verification
  -> Backend abstraction (IBackend)
  -> Direct Windows x64 PE executable output (default path)
  -> LLVM IR text (`--emit-llvm`)
```

Temporary bootstrap mode (explicit only):

```text
Voltis source (.vlt)
  -> Lexer -> Parser -> AST
  -> C++17 generator
  -> Host C++ compiler (optional)
```

## 2) Implemented vs scaffolded

### Semantic analysis status

**Implemented (subset):**

- function symbol registration/checks
- import declaration registration/checks
- extern function declaration registration/checks
- lexical block scopes for locals
- assignment/return compatibility checks
- expression typing for literals, unary/binary ops, calls
- conversion-member validation (`ToString/ToInt32/ToFloat32/ToFloat64/ToBool/Round/Floor/Ceil`)
- loop semantics for `while`, `break`, and `continue`
- non-void return-path checks (diagnose missing returns on some paths)

**Not implemented yet (whitepaper-scale semantics):**

- class/struct/member semantics
- module/package semantics beyond DLL imports
- nullability/dataflow/control-flow completeness checks
- ownership/region semantics
- advanced interop semantics (calling conventions, import-library ownership, richer ABI contracts)
- full modifier semantics (modifiers are parsed but effectively ignored in this subset)

### VIR status

**Implemented (subset VIR):**

- VIR data model (`src/vir.*`) with typed values, locals, basic blocks, instructions, terminators
- lowering from semantic info into VIR (`src/lowering.*`)
- hardening passes (`src/vir_passes.*`) for constant-condition branch folding, unreachable block removal, and structural verification
- debug text dump via `--emit-vir`

**Not implemented yet:**

- richer optimization pass pipeline beyond current baseline simplifications
- broader lowering coverage for full language surface
- advanced ABI-aware lowering for richer interop/runtime features

### Backend abstraction + LLVM IR emission status

**Implemented:**

- backend boundary (`IBackend`, backend options/results/artifacts)
- LLVM backend module that emits **LLVM IR text** (`BackendOutputKind::LlvmIrText`)
- CLI production path uses semantic -> VIR -> backend flow and can emit:
  - LLVM IR text (`--emit-llvm`)
  - native executable (default mode)
- direct Windows PE backend for self-contained native output
- default native path no longer depends on a project runtime static library target

Direct PE backend coverage currently includes `int32`, `float32`, `float64`, `bool`, and `string` for the supported example and test paths.

**Not implemented yet:**

- explicit lld-link/link.exe orchestration owned independently from the direct backend
- full `.dll/.lib` workflow on production path
- optimization pipeline between VIR and LLVM lowering

## 3) What remains for full native toolchain maturity

Current repository can produce native executables from Voltis without C++ transpilation. Remaining milestones are:

1. Harden the direct PE backend coverage for more types and operations.
2. Add explicit linker strategy controls (`lld-link`/`link.exe`) and import-library/DLL workflow controls.
3. Add optimization passes and richer backend validation coverage.

Bootstrap C++ mode remains temporary scaffolding and is not required for the default production-directed compile path.

## 4) Whitepaper alignment and scope guard

Whitepaper target pipeline remains:

```text
Lexer -> Parser -> AST -> Semantic analysis -> Typed IR -> Optimization -> Backend lowering -> Object generation -> Link -> PE output
```

Current branch is aligned directionally and now includes first native object/executable generation through the production-directed pipeline, with backend/linker hardening still in progress.

## 5) Implemented syntax subset guard (docs/examples)

The active parser subset in this repo is declaration/function-centric and semicolon-based:

- top-level `import`, `extern fn ... from ...;`, and `fn` declarations
- braces for blocks
- semicolons required for statements
- primitives: `int32`, `float32`, `float64`, `string`, `bool`, `void`
- `if/else`, `while`, `break`, `continue`, `return`, local declarations/assignment, direct calls, conversion members

This document intentionally avoids claiming support for classes, module/package systems, advanced interop ABI surface, or full backend maturity (DLL/import-lib workflow controls, optimizer, custom COFF backend), even though native exe generation is now available.
