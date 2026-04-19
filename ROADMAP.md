# Voltis Roadmap

Navigation: [README](README.md) · [Whitepaper](docs/whitepaper.md) · [Backend spec](docs/spec/backend.md) · [Contributing](CONTRIBUTING.md)

Voltis is in early alpha. This roadmap tracks expected direction, not strict delivery guarantees.

## Short-term goals

- Expand parser/sema coverage within the current function-centric language subset
- Harden Windows x64 PE backend behavior and diagnostics
- Increase compiler test coverage for syntax, sema, and backend edge cases
- Improve docs/spec consistency for implemented behavior
- Stabilize command-line UX and error reporting

## Mid-term goals

- Add richer language constructs (user-defined types, broader expression/statement surface)
- Introduce optimization passes over VIR
- Improve LLVM backend parity with native backend output expectations
- Add DLL and import-library workflows for Windows targets
- Strengthen CI automation for build + test matrices

## Long-term goals

- Mature language specification with compatibility policy
- Develop robust tooling (formatter, language server, debug workflow)
- Expand backend strategy and platform support over time
- Establish stable release channels and versioned language milestones
