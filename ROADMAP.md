# Voltis Roadmap

Navigation: [README](README.md) · [Whitepaper](docs/whitepaper.md) · [Backend spec](docs/spec/backend.md) · [Contributing](CONTRIBUTING.md)

Voltis is in early alpha. This roadmap tracks expected direction, not strict delivery guarantees.

## Planning principles

- Security and safety first: no feature ships without defensive validation paths and negative tests.
- Performance by measurement: optimize only behind benchmarks and profile data.
- One source of truth: target capability and ABI policies stay centralized.
- Incremental rollout: ship behind readiness gates (`Production` / `Experimental` / `Planned`) and advance only with exit criteria.

## Priority-ordered next work

### P0 — Reliability and security hardening (next sprint)

1. **Finalize safe artifact writing edge-cases**
   - Harden `safeWriteFile` temp-name generation to avoid collisions under high parallelism.
   - Add failure-path cleanup tests (write failure, rename failure, permission denied).
   - Add symlink/reparse-point regression tests for both file and parent path handling.

2. **Close sysroot correctness gaps**
   - Thread `sysroot` through all import and runtime lookup call sites.
   - Add tests for absolute-vs-relative import precedence and host fallback ordering.
   - Add startup validation for unreadable sysroot directories.

3. **Target readiness guardrails**
   - Ensure every target selection path consistently enforces readiness policy.
   - Add explicit UX tests for planned-target hard errors and experimental warnings.

**Exit criteria:** all new negative tests pass on Linux/Windows CI; no unchecked direct output writes remain.

### P1 — Genuine native writer completion (x86_64 production path)

1. **ELF writer completion**
   - Implement complete section/symbol/relocation tables (`.text/.data/.rodata/.bss`, `.symtab`, `.strtab`, `.rela.text`).
   - Support required relocations (`R_X86_64_64`, `R_X86_64_PC32`, `R_X86_64_PLT32`).
   - Validate with `readelf -h -S -r -s` in CI.

2. **Mach-O writer completion**
   - Implement full load-command and section population (`__TEXT/__text`, `__DATA/__data`, `__DATA/__bss`).
   - Support core relocations for x86_64 and arm64 scaffolding.
   - Validate with `otool -hvl -l` on macOS runners.

3. **Execution smoke tests**
   - Add per-platform "return 42" executable tests where native runners exist.

**Exit criteria:** non-wrapper ELF/Mach-O binaries execute on native runners and pass structural validation.

### P2 — AArch64 experimental pipeline uplift

1. **Codegen implementation pass**
   - Complete AArch64 prologue/epilogue, arithmetic, load/store, and branch coverage.
   - Enforce AAPCS64 calling convention and callee-save handling.

2. **Backend dispatch integration**
   - Route AArch64 targets to matching writer path (ELF on Linux, Mach-O on macOS).

3. **Validation suite**
   - Add structural tests (`readelf`/`otool`) on non-native runners.
   - Add execution tests on ARM64 runners when available.

**Exit criteria:** AArch64 remains `Experimental` but compiles the core sample corpus with zero structural errors.

### P3 — Language and optimizer expansion

1. **Language surface growth**
   - User-defined aggregates and richer expression coverage.
   - Stronger module/import ergonomics with clearer diagnostics.

2. **VIR optimization pipeline**
   - Dead-code elimination, CFG simplification, and constant-propagation improvements.
   - Deterministic optimization pass ordering with pass-level tests.

3. **Diagnostics quality**
   - Source-span precision and actionable fix suggestions.

**Exit criteria:** measurable compile-time/runtime improvements on benchmark suite with no semantic regressions.

### P4 — Tooling and release engineering

1. **Developer tooling**
   - Formatter and language-server baseline.
   - Improved debug workflow documentation.

2. **CI and quality gates**
   - Matrix builds (Linux/macOS/Windows; x64 + ARM64 where available).
   - Sanitizer jobs (ASan/UBSan) and artifact verification jobs.

3. **Release policy**
   - Versioned compatibility promises and changelog discipline.

**Exit criteria:** reproducible release pipeline with platform-tagged artifacts and signed release notes.

## Suggested immediate execution order

1. P0 reliability/security hardening.
2. P1 x86_64 ELF/Mach-O native writer completion.
3. P2 AArch64 uplift while preserving experimental gating.
4. P3 language + optimization expansion.
5. P4 tooling and release automation.

## What we should not do next

- Do **not** broaden target catalog readiness labels without implementation depth.
- Do **not** add new features to PE-extraction wrapper paths intended for replacement.
- Do **not** claim production support for formats/architectures lacking execution-grade tests.
