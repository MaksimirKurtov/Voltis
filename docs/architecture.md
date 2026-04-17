# Voltis Architecture Status (code-truth, whitepaper-aligned)

## 1) Current implemented architecture

`voltisc` currently executes this flow:

```text
Voltis source (.vlt)
  -> Lexer
  -> Parser
  -> AST
  -> Semantic analysis (subset)
  -> Typed VIR lowering
  -> Backend abstraction (IBackend)
  -> LLVM IR text
  -> Native object emission via clang (`--emit-obj` or default path)
  -> Native executable link stage via clang + voltis_runtime (default path)
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
- lexical block scopes for locals
- assignment/return compatibility checks
- expression typing for literals, unary/binary ops, calls
- conversion-member validation (`ToString/ToInt32/ToFloat32/ToFloat64/ToBool/Round/Floor/Ceil`)

**Not implemented yet (whitepaper-scale semantics):**

- class/struct/member semantics
- module/import semantics
- nullability/dataflow/control-flow completeness checks
- ownership/region semantics
- interop declaration semantics (`extern` DLL/calling conventions)
- full modifier semantics (modifiers are parsed but effectively ignored in this subset)

### VIR status

**Implemented (subset VIR):**

- VIR data model (`src/vir.*`) with typed values, locals, basic blocks, instructions, terminators
- lowering from semantic info into VIR (`src/lowering.*`)
- debug text dump via `--emit-vir`

**Not implemented yet:**

- optimization pass pipeline
- broader lowering coverage for full language surface
- ABI-aware lowering for interop/advanced runtime features

### Backend abstraction + LLVM IR emission status

**Implemented:**

- backend boundary (`IBackend`, backend options/results/artifacts)
- LLVM backend module that emits **LLVM IR text** (`BackendOutputKind::LlvmIrText`)
- CLI production path uses semantic -> VIR -> backend flow and can emit:
  - LLVM IR text (`--emit-llvm`)
  - native object (`--emit-obj`)
  - native executable (default mode)
- runtime library target `voltis_runtime` for backend helper symbols

**Not implemented yet:**

- direct object emission from backend internals (today object generation is delegated to clang from emitted LLVM IR text)
- explicit lld-link/link.exe orchestration owned independently from clang driver
- DLL workflow (`.dll/.lib`) on production path
- optimization pipeline between VIR and LLVM lowering

## 3) What remains for full native toolchain maturity

Current repository can produce native objects/executables from Voltis without C++ transpilation. Remaining milestones are:

1. Move from clang-driven object generation to dedicated LLVM backend integration APIs (or custom COFF backend milestone path).
2. Add explicit linker strategy controls (`lld-link`/`link.exe`) and import-library/DLL support.
3. Harden runtime ABI and memory ownership for string-producing helpers.
4. Add optimization passes and richer backend validation coverage.

Bootstrap C++ mode remains temporary scaffolding and is not required for the default production-directed compile path.

## 4) Whitepaper alignment and scope guard

Whitepaper target pipeline remains:

```text
Lexer -> Parser -> AST -> Semantic analysis -> Typed IR -> Optimization -> Backend lowering -> Object generation -> Link -> PE output
```

Current branch is aligned directionally and now includes first native object/executable generation through the production-directed pipeline, with backend/linker hardening still in progress.

## 5) Implemented syntax subset guard (docs/examples)

The active parser subset in this repo is function-centric and semicolon-based:

- top-level `fn` declarations only
- braces for blocks
- semicolons required for statements
- primitives: `int32`, `float32`, `float64`, `string`, `bool`, `void`
- `if/else`, `return`, local declarations/assignment, direct calls, conversion members

This document intentionally avoids claiming support for classes, modules, extern interop syntax, or full backend maturity (DLL/import-lib, optimizer, custom COFF backend), even though native exe generation is now available.
