# Voltis Whitepaper

Navigation: [README](../README.md) · [Examples guide](examples.md) · [Spec index](spec/README.md) · [Roadmap](../ROADMAP.md) · [Governance](../GOVERNANCE.md)

## 1. Purpose

Voltis is a native compiled language project focused on readable syntax, strong typing, and direct system-level output. The implementation in this repository is early-stage but real: it already compiles a defined language subset into Windows x64 PE executables.

## 2. Language identity

Voltis is intended to balance:

- systems-level capability
- explicit, predictable typing
- clean, low-noise syntax
- practical Windows-first compiler engineering

It is explicitly **not** designed as:

- an interpreter
- a pure transpiler strategy
- a syntax-only wrapper with no independent semantics

## 3. Current implementation status

The active compiler pipeline is:

```text
Voltis source (.vlt)
  -> Lexer
  -> Parser
  -> AST
  -> Semantic analysis
  -> VIR lowering
  -> Backend abstraction
     -> Windows x64 PE backend (native executable)
     -> LLVM IR text backend
```

### Implemented subset highlights

- top-level `fn` declarations with explicit return types
- static primitive types (`int32`, `float32`, `float64`, `string`, `bool`, `void`)
- local declarations (including `var` inference), assignment, arithmetic/comparison/logical expressions
- control flow: `if/else`, `while`, `break`, `continue`, `return`
- built-in `print(expr)`
- conversion members (`ToString`, `ToInt32`, `ToFloat32`, `ToFloat64`, `ToBool`, `Round`, `Floor`, `Ceil`)

See the normative docs in:

- [Syntax spec](spec/syntax.md)
- [Type spec](spec/types.md)
- [Conversion spec](spec/conversions.md)
- [Control-flow spec](spec/control_flow.md)
- [Backend spec](spec/backend.md)

## 4. Compiler architecture principles

### 4.1 Frontend truth

Language behavior must be defined by Voltis parser and semantic analysis, not by host language behavior.

### 4.2 VIR as language boundary

Voltis IR (VIR) is the typed, explicit bridge between frontend semantics and backend generation. It enables:

- backend decoupling
- future optimization passes
- clearer reasoning about control flow and type-preserving transformations

### 4.3 Backend strategy

The repository currently supports:

- direct native PE output (default path)
- LLVM IR text output for inspection

The long-term strategy keeps backend evolution incremental and test-driven.

## 5. Safety and predictability posture

Voltis prioritizes explicitness over magic:

- explicit return typing
- explicit conversion calls
- semantic diagnostics for invalid symbol/type/control-flow usage
- constrained, documented subset while features mature

## 6. Governance and authority

Voltis language authority is defined in [GOVERNANCE.md](../GOVERNANCE.md). In short:

- this repository is the authoritative source for the Voltis language
- language changes must go through PR review and VEP process
- divergent forks must not present themselves as official Voltis

## 7. Roadmap alignment

Implementation goals are tracked in [ROADMAP.md](../ROADMAP.md). Priority areas include:

1. maturing frontend coverage and diagnostics
2. expanding backend robustness and test depth
3. evolving VIR passes and broader language surface

## 8. Relationship to legacy planning document

The previous consolidated planning draft has been superseded by this document and the split spec docs under `docs/spec/`.
