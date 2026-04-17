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
  -> LLVM IR text artifact (.ll)
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
- CLI default path uses semantic -> VIR -> backend flow

**Not implemented yet:**

- object-file emission from backend flow (`.obj`)
- executable emission from backend flow (`.exe/.dll`)
- integrated linker stage in production-directed path
- packaged runtime implementation for helper symbols referenced by emitted IR

## 3) What remains for true native `.obj/.exe` generation

To satisfy the whitepaper’s native backend goal in this repository:

1. Add backend output mode for object emission (LLVM object emission or direct COFF writer).
2. Provide/ship runtime implementation for helper calls used by generated IR (print/string conversion helpers).
3. Integrate linker orchestration (`lld-link`/`link.exe`) into compiler driver.
4. Add stable CLI artifact modes for `.obj` and final `.exe/.dll` on the production-directed path.

Until then, native executable generation is only available through the **temporary bootstrap C++ mode**, not through the primary VIR backend path.

## 4) Whitepaper alignment and scope guard

Whitepaper target pipeline remains:

```text
Lexer -> Parser -> AST -> Semantic analysis -> Typed IR -> Optimization -> Backend lowering -> Object generation -> Link -> PE output
```

Current branch is aligned directionally (frontend + typed VIR + backend boundary exist), but still pre-native-output.

## 5) Implemented syntax subset guard (docs/examples)

The active parser subset in this repo is function-centric and semicolon-based:

- top-level `fn` declarations only
- braces for blocks
- semicolons required for statements
- primitives: `int32`, `float32`, `float64`, `string`, `bool`, `void`
- `if/else`, `return`, local declarations/assignment, direct calls, conversion members

This document intentionally avoids claiming support for classes, modules, extern interop syntax, or native binary emission from the primary backend path.
