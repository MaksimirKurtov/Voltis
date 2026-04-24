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
  -> VIR optimization + verification
  -> backend abstraction
  -> output artifact(s)
```

## 2. VIR role

VIR is the typed internal representation between frontend and backend.

Responsibilities:

- preserve semantic typing information
- represent control flow and operations explicitly
- provide a backend-agnostic compile boundary
- support backend-hardening passes (constant-branch simplification, unreachable block pruning, structural verification)

## 3. Implemented backend targets

### 3.1 Windows x64 PE executable backend

- output kind: native executable
- default compiler mode when no explicit emit flags are set
- used in runtime compile tests
- implemented as an in-tree PE writer/linker stage (`src/backend_pe_x64.cpp`) that lays out sections, patches internal fixups, and emits an import table/IAT
- each emitted executable now runs a backend self-check on PE headers/sections/import directory before artifact publication

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

Current status note: Voltis has a working custom linker stage for the currently supported native feature set, but it is not yet a full COFF object/static-library linker.

## 7. Current DLL interop behavior

- `import` + `extern fn ... from ...;` declarations are lowered into VIR extern metadata.
- The PE backend binds extern calls through the generated IAT and emits indirect calls (`call [rip+disp32]`) to imported symbols.
- The LLVM text backend emits `declare` signatures for extern functions and direct call sites using those declarations.
- The PE linker path resolves native library import names from `.dll`, `.lib`, `.a`, `.so`, and `.dylib` forms into PE import-table DLL targets.

## 8. Linker model and relocation policy

- The PE backend now tracks explicit linker entities (`LinkObject`, `LinkSection`, `LinkSymbol`, `Relocation`, `ImportSymbol`, `LinkedImage`) in `src/linker_model.h`.
- Backend code emission and link/layout responsibilities are split: codegen records symbols/relocations first, then a linker pass resolves symbols, validates relocations, assigns RVAs/raw offsets, and emits the final PE.
- Current relocation kinds in the linker model:
  - `REL32` and `RIP_DISP32` for code-relative patching
  - `DIR64` for absolute image pointers
- Relocation patching happens only after final section layout and import RVAs are known.
- Diagnostics now include object/section/offset context for unresolved symbols and relocation failures.

### Base relocation behavior (`.reloc`)

- If `DIR64` relocations are present, the linker emits a `.reloc` section and populates the Base Relocation Directory using 4KB page blocks with `IMAGE_REL_BASED_DIR64` entries.
- If no absolute image relocations are present (current default codegen path), `.reloc` is intentionally omitted and the Base Relocation Directory is zeroed.
