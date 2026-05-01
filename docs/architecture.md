# Voltis Compiler Architecture

Complete technical overview of the Voltis compiler design, implementation status, and architecture.

**Navigation:** [README](../README.md) · [Whitepaper](whitepaper.md) · [Backend spec](spec/backend.md) · [Examples](examples.md) · [Specification](spec/README.md)

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
  -> Direct native executable output (target + readiness gated)
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

### Backend abstraction + native/LLVM emission status

**Implemented:**

- backend boundary (`IBackend`, backend options/results/artifacts)
- LLVM backend module that emits **LLVM IR text** (`BackendOutputKind::LlvmIrText`)
- CLI production path uses semantic -> VIR -> backend flow and can emit:
  - LLVM IR text (`--emit-llvm`)
  - native executable (default mode)
- direct in-tree PE backend for self-contained native output generation
- target catalog with readiness metadata (`Production`, `Experimental`, `Planned`) and CLI gating via `--list-targets` / `--list-all-targets`
- host-canonical default target selection at startup
- default native path no longer depends on a project runtime static library target

Direct PE backend coverage currently includes `int32`, `float32`, `float64`, `bool`, and `string` for the supported example and test paths.

**Not implemented yet:**

- explicit lld-link/link.exe orchestration owned independently from the direct backend
- full `.dll/.lib` workflow on production path
- optimization pipeline between VIR and LLVM lowering

## 3) Component Details

### Lexer (src/lexer.cpp)

**Responsibility:** Tokenization of Voltis source code.

**Features:**
- Recognizes keywords: `fn`, `public`, `extern`, `import`, `return`, `if`, `else`, `while`, `break`, `continue`, `var`, `true`, `false`
- Handles literals: integers, floats, strings
- Generates tokens with source location tracking for diagnostics
- Supports single-line comments (`//`)

**Output:** Token stream ready for parsing

### Parser (src/parser.cpp)

**Responsibility:** Builds Abstract Syntax Tree (AST) from tokens.

**Features:**
- Recursive descent parser with operator precedence
- Parses: imports, extern declarations, function definitions, statements, expressions
- Supports complex expressions with proper precedence (arithmetic, comparison, logical)
- Error recovery for basic syntax errors

**AST Nodes** (defined in src/ast.h):
- `ImportDecl`, `ExternFnDecl`, `FunctionDecl`
- `BlockStmt`, `IfStmt`, `WhileStmt`, `ReturnStmt`, `BreakStmt`, `ContinueStmt`, `ExprStmt`
- `BinaryExpr`, `UnaryExpr`, `CallExpr`, `LiteralExpr`, `IdentifierExpr`

**Output:** Abstract Syntax Tree ready for semantic analysis

### Semantic Analysis (src/sema.cpp)

**Responsibility:** Type checking, symbol resolution, and validation.

**Features:**
- Symbol table management with lexical scoping
- Type checking for assignments, function calls, and operators
- Validation of return types and control flow paths
- Conversion function validation
- Import cycle detection
- Extern function validation

**Scope Handling:**
- Global scope for imports, function declarations, extern declarations
- Local scopes for function bodies and block statements
- Proper symbol shadowing and lookup

**Error Diagnostics:**
- "Undefined symbol" errors with source location
- "Type mismatch" for incompatible assignments
- "Missing return" for functions that might not return
- "Duplicate symbol" for redeclarations

**Output:** Validated AST with type information preserved

### VIR (Voltis Intermediate Representation)

**Responsibility:** Typed, backend-agnostic intermediate representation.

**Data Model** (src/vir.h):
- `Function`: Contains locals, basic blocks, entry/exit blocks
- `BasicBlock`: List of instructions and a terminator
- `Instruction`: Operations (load, store, binary op, call, etc.)
- `Terminator`: Control flow (branch, return, unreachable)
- `Value`: Typed values (constants, locals, instruction results)

**Features:**
- Strongly typed: All values have explicit types
- SSA-like form: Each value assigned once
- Explicit control flow: Basic blocks with terminators
- Backend-agnostic: No platform-specific details

**Example VIR for `print(42)`:**
```
Function main() -> int32 {
  Entry:
    %0 = const int32 42
    %1 = call print(%0)
    %2 = const int32 0
    return %2
}
```

**Output:** VIR ready for optimization and code generation

### VIR Lowering (src/lowering.cpp)

**Responsibility:** Converts AST to VIR.

**Process:**
1. Walk AST in structural order
2. Generate VIR instructions for expressions
3. Build basic blocks for control flow
4. Track local variables and scope
5. Handle function calls and conversions

**Special Cases:**
- `if/else` → Branch instructions and block merging
- `while` → Loop back edges and exit blocks
- `break`/`continue` → Jumps to loop exit/continuation
- Conversions (`ToString()`, etc.) → Intrinsic function calls

**Output:** VIR ready for optimization and code generation

### VIR Optimization and Verification (src/vir_passes.cpp)

**Current Optimizations:**
1. **Constant folding:** Evaluates compile-time constant expressions
2. **Constant branch folding:** Simplifies `if` on boolean literals
3. **Unreachable block elimination:** Removes dead code

**Verification Passes:**
1. **Structural verification:** Ensures all values are defined before use
2. **Type consistency:** Verifies instruction operands have correct types
3. **Block connectivity:** Ensures control flow is valid

**Output:** Optimized, verified VIR ready for code generation

### Backend Abstraction (src/backend.h)

**Interface:** `IBackend` abstract class defining code generation interface.

**Methods:**
- `generateCode()`: Main code generation entry point
- `getBackendName()`: Human-readable backend name
- `getOptions()`: Backend-specific configuration

**Supported Output Kinds:**
- `NativeExecutable` — Binary executable
- `LlvmIrText` — LLVM IR text format
- `VirText` — VIR text representation

**Implementations:**
- `BackendPeX64` — Windows PE x86-64 (production)
- `BackendLlvmIr` — LLVM IR text (experimental)

### Code Generation Pipeline

```
VIR
  ↓
[Instruction Selection]
  ↓ (for each architecture)
  ├─→ x86_64Codegen (x64_codegen.cpp)
  ├─→ Aarch64Codegen (aarch64_codegen.cpp)
  └─→ [Other architectures planned]
  ↓
[Register Allocation & Prologue/Epilogue]
  ↓
[Binary Emission]
  ↓
[Format-Specific Writing]
  ├─→ PE Writer (backend_pe_x64.cpp)
  ├─→ ELF Writer (emit/elf_writer.cpp)
  └─→ Mach-O Writer (emit/macho_writer.cpp)
  ↓
Executable Binary
```

### x86-64 Code Generation (src/codegen/x64_codegen.cpp)

**Architecture:** Intel/AMD 64-bit x86

**Calling Convention:** Microsoft x64 ABI (Windows) or System V AMD64 (Unix)

**Key Registers:**
- Parameter registers: RCX, RDX, R8, R9 (Windows); RDI, RSI, RDX, RCX, R8, R9 (Unix)
- Return register: RAX (primary), RDX (secondary)
- Callee-saved: RBP, RBX, R12-R15
- Caller-saved: RAX, RCX, RDX, RSI, RDI, R8-R11

**Generated Code Includes:**
- Function prologue (stack frame setup)
- Instruction selection from VIR
- Register allocation
- Function epilogue (cleanup)
- Return value setup

### ARM64 Code Generation (src/codegen/aarch64_codegen.cpp)

**Architecture:** ARM 64-bit (AArch64)

**Calling Convention:** AArch64 AAPCS64

**Key Registers:**
- Parameter registers: X0-X7
- Return register: X0 (primary), X1 (secondary)
- Callee-saved: X19-X28, FP, LR

**Status:** Experimental — scaffold in place, needs hardening

### PE/COFF Writer (src/backend_pe_x64.cpp)

**Format:** Windows PE32+ (Portable Executable 64-bit)

**Output:** Self-contained Windows `.exe` executable

**Features:**
- DOS header and PE header
- Section table (`.text`, `.data`, etc.)
- Import Address Table (IAT) for DLL imports
- Base relocation section for address fixups
- Self-validation before writing

**Process:**
1. Allocate memory for PE structure
2. Generate x86-64 machine code
3. Build IAT for imported symbols
4. Create section headers
5. Perform relocations
6. Write binary to file
7. Validate PE integrity

### ELF Writer (src/emit/elf_writer.cpp)

**Format:** ELF (Executable and Linkable Format)

**Platforms:** Linux, Unix

**Current Status:** Scaffolding (PE extraction wrapper)

**Limitations:**
- Single `.text` section only
- No full relocation handling
- No debug information

**Planned Improvements:**
- Full native ELF writer
- Multiple section support
- Complete relocation model
- Symbol table generation

### Mach-O Writer (src/emit/macho_writer.cpp)

**Format:** Mach-O (Mach Object)

**Platforms:** macOS

**Current Status:** Scaffolding (PE extraction wrapper)

**Similar limitations as ELF writer**

**Planned improvements same as ELF**

---

## 4) What Remains for Full Native Toolchain Maturity

Current repository can produce native executables from Voltis without C++ transpilation. Remaining milestones are:

### P0 — Critical (next sprint)

- [ ] Sysroot validation and robustness
- [ ] Artifact writing hardening
- [ ] Target readiness guardrails

### P1 — Native Writers (primary)

- [ ] Full ELF writer (sections, relocations, symbols, imports)
- [ ] Full Mach-O writer (sections, relocations, symbols, imports)
- [ ] Complete relocation support across all formats
- [ ] Debug information translation (DWARF for ELF, dsymutil for Mach-O)

### P2 — AArch64 Uplift

- [ ] Complete AArch64 codegen (prologue/epilogue hardening)
- [ ] AAPCS64 calling convention full support
- [ ] ELF/Mach-O AArch64 backend dispatch
- [ ] Execution smoke tests on ARM64 hardware

### P3 — Optimization & Language

- [ ] VIR optimization pass pipeline
- [ ] Dead code elimination (DCE)
- [ ] Constant propagation
- [ ] User-defined types with full semantics
- [ ] Module system improvements

### P4 — Tooling & Ecosystem

- [ ] Formatter
- [ ] Language Server Protocol (LSP)
- [ ] Package manager
- [ ] Debugger integration

---

## 5) Build and Development

### Building the Project

**Requirements:**
- CMake 3.16+
- C++17 compiler (MSVC, GCC, Clang)

**Steps:**
```bash
cmake -S . -B build
cmake --build build -j
```

### Running Tests

```bash
cd build
ctest --output-on-failure
```

### Adding a New Pass

1. Create pass file in `src/` (e.g., `my_pass.cpp/.h`)
2. Implement pass logic
3. Register in `vir_passes.cpp`
4. Build and test

### Debugging the Compiler

```bash
# Build with debug symbols
cmake -S . -B debug -DCMAKE_BUILD_TYPE=Debug
cmake --build debug

# Run under debugger
gdb ./debug/voltisc

# Emit VIR for inspection
./debug/voltisc input.vlt --emit-vir -o output.vir
cat output.vir
```

---

## 6) Architecture Decisions

### Why VIR?

- **Backend independence:** Same IR can target multiple architectures
- **Optimization:** IR-level optimizations work for all backends
- **Verification:** Type-safe IR catches many errors early
- **Debugging:** Can emit VIR for inspection and analysis

### Why No LLVM Dependency?

- **Self-contained:** Entire compiler in one repo with no external C++ deps
- **Learning:** Building backends teaches compiler internals
- **Control:** Full control over code generation
- **Simplicity:** Fewer build dependencies, easier to maintain

### Why PE First?

- **Windows focus:** Most accessible platform for development
- **Completeness:** PE backend is full implementation, not scaffolding
- **Direct emission:** No dependency on external linker/tools
- **Production ready:** Can generate real executables immediately

---

## 7) Performance Characteristics

### Compilation Speed

Typical compilation times (simple programs):
- Lexing: < 1ms
- Parsing: < 5ms
- Semantic analysis: < 5ms
- Code generation: < 20ms
- Total: ~25-50ms for typical program

### Code Quality

Generated code uses:
- Calling conventions matching target ABI
- Efficient register usage
- Minimal stack frame overhead
- Direct function calls (no trampolines)

### Further Optimization

Current implementation prioritizes correctness over optimization. Future work will add:
- Dead code elimination
- Constant propagation
- Loop unrolling (potential)
- Inlining (potential)

---

## See Also

- [Backend Specification](spec/backend.md) — Detailed backend architecture
- [Whitepaper](whitepaper.md) — Design rationale and vision
- [Examples](examples.md) — Code examples
- [Build System](../BUILD_SYSTEM.md) — Build configuration details
- [Linker Usage](../LINKER_USAGE.md) — Linking and imports

Bootstrap C++ mode remains temporary scaffolding and is not required for the default production-directed compile path.

## 4) Whitepaper alignment and scope guard

Whitepaper target pipeline remains:

```text
Lexer -> Parser -> AST -> Semantic analysis -> Typed IR -> Optimization -> Backend lowering -> Object generation -> Link -> PE output
```

Current branch is aligned directionally and includes production x86_64 target support plus experimental target scaffolding, with backend/linker hardening still in progress.

## 5) Implemented syntax subset guard (docs/examples)

The active parser subset in this repo is declaration/function-centric and semicolon-based:

- top-level `import`, `extern fn ... from ...;`, and `fn` declarations
- braces for blocks
- semicolons required for statements
- primitives: `int32`, `float32`, `float64`, `string`, `bool`, `void`
- `if/else`, `while`, `break`, `continue`, `return`, local declarations/assignment, direct calls, conversion members

This document intentionally avoids claiming support for classes, module/package systems, advanced interop ABI surface, or full backend maturity (DLL/import-lib workflow controls, optimizer, custom COFF backend), even though native exe generation is now available.
