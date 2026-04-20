# Voltis Backend Specification

Navigation: [Spec index](README.md) · [Whitepaper](../whitepaper.md) · [Architecture status](../architecture.md) · [README](../../README.md)

## 1. Pipeline contract

Current production-directed pipeline:

```text
source (.vlt)
  -> lexer
  -> parser
  -> AST
  -> semantic analysis
  -> VIR lowering
  -> backend abstraction
  -> output artifact(s)
```

## 2. VIR role

VIR is the typed internal representation between frontend and backend.

Responsibilities:

- preserve semantic typing information
- represent control flow and operations explicitly
- provide a backend-agnostic compile boundary

## 3. Implemented backend targets

### 3.1 Windows x64 PE executable backend

- output kind: native executable
- default compiler mode when no explicit emit flags are set
- used in runtime compile tests

### 3.2 LLVM IR text backend

- output kind: LLVM IR text artifact (`.ll`)
- used for inspection/debugging

## 4. CLI output modes

- default: native executable
- `--emit-vir`: VIR text dump
- `--emit-llvm`: LLVM IR text output
- `--bootstrap-cpp`: temporary scaffolding path, not production direction

## 5. Target scope

Current implementation is Windows x64 focused for native PE emission. Cross-platform expansion is roadmap work, not current guaranteed behavior.

## 6. Maturity goals

- broaden backend type/operation coverage
- improve diagnostics and artifact validation
- add stronger linker/DLL workflow support
- introduce optimization passes over VIR

## 7. Current DLL interop behavior

- `import` + `extern fn ... from ...;` declarations are lowered into VIR extern metadata.
- The PE backend binds extern calls through the generated IAT and emits indirect calls (`call [rip+disp32]`) to imported symbols.
- The LLVM text backend emits `declare` signatures for extern functions and direct call sites using those declarations.
