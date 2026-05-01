# Platform Support Matrix

Comprehensive guide to Voltis target platform support, including production-ready, experimental, and planned targets.

**Navigation:** [README](README.md) · [Compiler Flags](COMPILER_FLAGS.md) · [Getting Started](GETTING_STARTED.md) · [Linker Usage](LINKER_USAGE.md)

## Overview

Voltis uses **target triples** to specify compilation targets. A target triple has the format:

```
ARCHITECTURE-VENDOR-OS-ABI
```

Example: `x86_64-pc-windows-msvc`

- **Architecture:** Processor family (x86_64, aarch64, etc.)
- **Vendor:** Vendor/manufacturer (pc, apple, unknown)
- **OS:** Operating system (windows, linux, macos, etc.)
- **ABI:** Application Binary Interface (msvc, gnu, musl, eabi, etc.)

---

## Production-Ready Platforms ✅

These platforms are **fully supported, tested, and recommended for production use**.

### Windows (x86_64)

| Property | Value |
|----------|-------|
| **Target Triple** | `x86_64-pc-windows-msvc` |
| **Architecture** | x86-64 (Intel/AMD 64-bit) |
| **Operating System** | Windows 7+ |
| **Output Format** | PE32+ (`.exe`) |
| **ABI** | Microsoft x64 |
| **Status** | ✅ Production |
| **Compiler Variant** | MSVC, GCC (MinGW), Clang |
| **DLL Support** | ✅ Yes (`import "kernel32.dll"`) |
| **API Examples** | Windows API (GetCurrentProcessId, GetTickCount) |

**Compile:**
```bash
voltisc hello.vlt -o hello.exe
```

**Run:**
```powershell
./hello.exe
```

---

### Linux (x86_64) — GNU ABI

| Property | Value |
|----------|-------|
| **Target Triple** | `x86_64-unknown-linux-gnu` |
| **Architecture** | x86-64 (Intel/AMD 64-bit) |
| **Operating System** | Linux (glibc) |
| **Output Format** | ELF 64-bit (`.elf`) |
| **ABI** | System V AMD64 + GNU extensions |
| **Status** | ✅ Production |
| **Distros** | Ubuntu, Debian, Fedora, RHEL, CentOS, OpenSUSE, etc. |
| **C Library** | glibc |
| **Dynamic Linking** | ✅ Yes (`.so` libraries) |

**Compile:**
```bash
voltisc hello.vlt --target x86_64-unknown-linux-gnu --emit elf -o hello.elf
```

**Run:**
```bash
./hello.elf
```

---

### Linux (x86_64) — musl ABI

| Property | Value |
|----------|-------|
| **Target Triple** | `x86_64-unknown-linux-musl` |
| **Architecture** | x86-64 (Intel/AMD 64-bit) |
| **Operating System** | Linux (musl libc) |
| **Output Format** | ELF 64-bit (`.elf`) |
| **ABI** | System V AMD64 + musl |
| **Status** | ✅ Production |
| **Distros** | Alpine Linux, others using musl |
| **C Library** | musl libc (lightweight) |
| **Use Cases** | Container images, minimal systems |

**Compile:**
```bash
voltisc hello.vlt --target x86_64-unknown-linux-musl --emit elf -o hello.elf
```

**Run (on Alpine):**
```bash
./hello.elf
```

---

### macOS (x86_64)

| Property | Value |
|----------|-------|
| **Target Triple** | `x86_64-apple-darwin` |
| **Architecture** | x86-64 (Intel 64-bit) |
| **Operating System** | macOS 10.7+ |
| **Output Format** | Mach-O 64-bit (`.macho`) |
| **ABI** | System V AMD64 (Darwin variant) |
| **Status** | ✅ Production |
| **Support** | Intel-based Macs (2006-2021) |
| **Frameworks** | ✅ Yes (via dynamic linking) |

**Compile:**
```bash
voltisc hello.vlt --target x86_64-apple-darwin --emit macho -o hello.macho
```

**Run:**
```bash
./hello.macho
```

---

## Experimental Platforms ⚠️

These platforms have **scaffolding in place but need further hardening**. Use at your own risk; test thoroughly before relying on them in production.

### Linux (ARM64) — GNU ABI

| Property | Value |
|----------|-------|
| **Target Triple** | `aarch64-unknown-linux-gnu` |
| **Architecture** | AArch64 (ARM 64-bit) |
| **Operating System** | Linux (glibc) |
| **Output Format** | ELF 64-bit (`.elf`) |
| **ABI** | AArch64 AAPCS64 + GNU |
| **Status** | ⚠️ Experimental |
| **Hardware** | ARM Cortex-A72+ (Raspberry Pi 4+), AWS Graviton |
| **Codegen** | Partial (needs hardening) |
| **Known Issues** | Prologue/epilogue incomplete, relocation gaps |

**Compile:**
```bash
voltisc hello.vlt --target aarch64-unknown-linux-gnu --emit elf -o hello.elf
```

**Note:** Test on actual ARM64 hardware for verification.

---

### macOS (ARM64/Apple Silicon)

| Property | Value |
|----------|-------|
| **Target Triple** | `aarch64-apple-darwin` |
| **Architecture** | AArch64 (ARM 64-bit, Apple Silicon) |
| **Operating System** | macOS 11.0+ (Big Sur+) |
| **Output Format** | Mach-O 64-bit (`.macho`) |
| **ABI** | System V AMD64 (Darwin ARM64 variant) |
| **Status** | ⚠️ Experimental |
| **Hardware** | M1, M2, M3 Macs |
| **Codegen** | Partial (needs hardening) |
| **Known Issues** | Relocation handling incomplete |

**Compile:**
```bash
voltisc hello.vlt --target aarch64-apple-darwin --emit macho -o hello.macho
```

**Note:** Requires M-series Mac for native execution.

---

### Linux (ARM64) — musl ABI

| Property | Value |
|----------|-------|
| **Target Triple** | `aarch64-unknown-linux-musl` |
| **Architecture** | AArch64 (ARM 64-bit) |
| **Operating System** | Linux (musl libc) |
| **Output Format** | ELF 64-bit (`.elf`) |
| **ABI** | AArch64 AAPCS64 + musl |
| **Status** | ⚠️ Experimental |
| **Use Cases** | Alpine Linux on ARM, containers |
| **Codegen** | Partial |

**Compile:**
```bash
voltisc hello.vlt --target aarch64-unknown-linux-musl --emit elf -o hello.elf
```

---

## Planned Platforms 📋

These platforms are **in the target catalog** but **not yet supported**. Attempting to compile to these targets will result in an error. They are planned for future releases.

### x86 (32-bit) — Linux GNU

| Property | Value |
|----------|-------|
| **Target Triple** | `i686-unknown-linux-gnu` |
| **Architecture** | i386/i686 (32-bit x86) |
| **Operating System** | Linux (glibc) |
| **Status** | 📋 Planned |
| **Reason for Delay** | Lower priority; 32-bit largely obsolete |

---

### x86 (32-bit) — Windows

| Property | Value |
|----------|-------|
| **Target Triple** | `i686-pc-windows-msvc` |
| **Architecture** | i386/i686 (32-bit x86) |
| **Operating System** | Windows 7+ |
| **Status** | 📋 Planned |

---

### ARM (32-bit) — ARMv7 with Hardware FP

| Property | Value |
|----------|-------|
| **Target Triple** | `armv7-unknown-linux-gnueabihf` |
| **Architecture** | ARMv7 (32-bit, hardware FP) |
| **Operating System** | Linux (glibc) |
| **Status** | 📋 Planned |
| **Hardware** | Raspberry Pi 3, BeagleBone, etc. |
| **Note** | ARMv7 + EABI with hardware floating-point |

---

### ARM (32-bit) — ARMv7 Soft FP

| Property | Value |
|----------|-------|
| **Target Triple** | `armv7-unknown-linux-gnueabi` |
| **Architecture** | ARMv7 (32-bit, software FP) |
| **Operating System** | Linux (glibc) |
| **Status** | 📋 Planned |

---

### ARM (32-bit) — ARMv6

| Property | Value |
|----------|-------|
| **Target Triple** | `armv6-unknown-linux-gnueabihf` |
| **Architecture** | ARMv6 (32-bit, hardware FP) |
| **Operating System** | Linux (glibc) |
| **Status** | 📋 Planned |
| **Hardware** | Raspberry Pi Zero, original Raspberry Pi |

---

### RISC-V (64-bit)

| Property | Value |
|----------|-------|
| **Target Triple** | `riscv64-unknown-linux-gnu` |
| **Architecture** | RISC-V 64-bit |
| **Operating System** | Linux (glibc) |
| **Status** | 📋 Planned |
| **Hardware** | SiFive, StarFive boards |

---

### RISC-V (32-bit)

| Property | Value |
|----------|-------|
| **Target Triple** | `riscv32-unknown-linux-gnu` |
| **Architecture** | RISC-V 32-bit |
| **Operating System** | Linux (glibc) |
| **Status** | 📋 Planned |

---

### MIPS (64-bit)

| Property | Value |
|----------|-------|
| **Target Triple** | `mips64-unknown-linux-gnu` |
| **Architecture** | MIPS 64-bit |
| **Operating System** | Linux (glibc) |
| **Status** | 📋 Planned |

---

### MIPS (32-bit)

| Property | Value |
|----------|-------|
| **Target Triple** | `mips-unknown-linux-gnu` |
| **Architecture** | MIPS 32-bit |
| **Operating System** | Linux (glibc) |
| **Status** | 📋 Planned |

---

### PowerPC (64-bit)

| Property | Value |
|----------|-------|
| **Target Triple** | `powerpc64-unknown-linux-gnu` |
| **Architecture** | PowerPC 64-bit |
| **Operating System** | Linux (glibc) |
| **Status** | 📋 Planned |
| **Hardware** | IBM POWER systems |

---

### IBM System z (s390x)

| Property | Value |
|----------|-------|
| **Target Triple** | `s390x-unknown-linux-gnu` |
| **Architecture** | IBM System z (64-bit) |
| **Operating System** | Linux (glibc) |
| **Status** | 📋 Planned |
| **Hardware** | IBM mainframes |

---

### Tensilica Xtensa

| Property | Value |
|----------|-------|
| **Target Triple** | `xtensa-unknown-none-elf` |
| **Architecture** | Tensilica Xtensa |
| **Operating System** | Bare Metal |
| **Status** | 📋 Planned |
| **Hardware** | ESP32 (custom variant) |

---

### Atmel AVR

| Property | Value |
|----------|-------|
| **Target Triple** | `avr-unknown-unknown` |
| **Architecture** | AVR 8-bit |
| **Operating System** | Bare Metal |
| **Status** | 📋 Planned |
| **Hardware** | Arduino, ATmega microcontrollers |

---

## Quick Reference: List All Platforms

### In Your Terminal

```bash
# Production-ready targets only
voltisc --list-targets

# All targets including experimental and planned
voltisc --list-all-targets
```

---

## Platform Support Summary Table

| Platform | Architecture | OS | Status | Output Format | Stability |
|----------|---|---|---|---|---|
| **Windows x86-64** | x86_64 | Windows | ✅ Prod | PE32+ | Production |
| **Linux x86-64 (glibc)** | x86_64 | Linux | ✅ Prod | ELF | Production |
| **Linux x86-64 (musl)** | x86_64 | Linux | ✅ Prod | ELF | Production |
| **macOS x86-64** | x86_64 | macOS | ✅ Prod | Mach-O | Production |
| **Linux ARM64 (glibc)** | aarch64 | Linux | ⚠️ Exp | ELF | Experimental |
| **macOS ARM64** | aarch64 | macOS | ⚠️ Exp | Mach-O | Experimental |
| **Linux ARM64 (musl)** | aarch64 | Linux | ⚠️ Exp | ELF | Experimental |
| **Linux x86 (32-bit)** | i686 | Linux | 📋 Plan | ELF | Planned |
| **Windows x86 (32-bit)** | i686 | Windows | 📋 Plan | PE | Planned |
| **Linux ARMv7 HF** | armv7 | Linux | 📋 Plan | ELF | Planned |
| **Linux ARMv7** | armv7 | Linux | 📋 Plan | ELF | Planned |
| **Linux ARMv6 HF** | armv6 | Linux | 📋 Plan | ELF | Planned |
| **Linux RISC-V 64** | riscv64 | Linux | 📋 Plan | ELF | Planned |
| **Linux RISC-V 32** | riscv32 | Linux | 📋 Plan | ELF | Planned |
| **Linux MIPS 64** | mips64 | Linux | 📋 Plan | ELF | Planned |
| **Linux MIPS 32** | mips | Linux | 📋 Plan | ELF | Planned |
| **Linux PowerPC 64** | ppc64 | Linux | 📋 Plan | ELF | Planned |
| **Linux System z** | s390x | Linux | 📋 Plan | ELF | Planned |
| **Bare Metal (Xtensa)** | xtensa | Bare | 📋 Plan | Binary | Planned |
| **Bare Metal (AVR)** | avr | Bare | 📋 Plan | Binary | Planned |

---

## Choosing Your Target

### For Desktop Applications

- **Windows:** `x86_64-pc-windows-msvc`
- **Linux:** `x86_64-unknown-linux-gnu`
- **macOS (Intel):** `x86_64-apple-darwin`
- **macOS (Apple Silicon):** `aarch64-apple-darwin` ⚠️ (experimental)

### For Server Applications

- **Linux (most distributions):** `x86_64-unknown-linux-gnu`
- **Alpine/lightweight containers:** `x86_64-unknown-linux-musl`

### For Embedded Systems

- **Raspberry Pi 4+:** `aarch64-unknown-linux-gnu` ⚠️ (experimental)
- **Arduino/AVR:** Not yet supported (planned)
- **ESP32:** Not yet supported (planned)

### For Cloud & Containers

- **AWS/Azure/GCP (x86):** `x86_64-unknown-linux-gnu`
- **AWS Graviton/ARM instances:** `aarch64-unknown-linux-gnu` ⚠️ (experimental)

---

## Cross-Compilation Examples

### From Windows to Linux

```bash
voltisc app.vlt \
  --target x86_64-unknown-linux-gnu \
  --emit elf \
  -o app.elf
# Transfer app.elf to Linux and run
```

### From Linux (x86) to ARM64

```bash
voltisc app.vlt \
  --target aarch64-unknown-linux-gnu \
  --sysroot /path/to/arm64-sysroot \
  --emit elf \
  -o app.elf
# Transfer app.elf to ARM64 device and run
```

### From Intel Mac to Apple Silicon (experimental)

```bash
voltisc app.vlt \
  --target aarch64-apple-darwin \
  --emit macho \
  -o app.macho
```

---

## Platform Roadmap

### Current Phase (v0.1-alpha)

✅ **Complete:**
- x86_64 Windows (PE native backend)
- x86_64 Linux (PE scaffolding + ELF wrapper)
- x86_64 macOS (PE scaffolding + Mach-O wrapper)

### Near-Term (P1)

⚠️ **In Progress:**
- Full ELF/Mach-O native writers (replacing PE scaffolding)
- AArch64 codegen hardening
- Complete relocation support

### Medium-Term (P2-P3)

📋 **Planned:**
- ARMv7/ARMv6 full support
- RISC-V support
- 32-bit x86 support

### Long-Term (P4+)

📋 **Future:**
- MIPS, PowerPC, System z
- Bare metal targets
- AVR/embedded microcontrollers

---

## Troubleshooting Platform Issues

### Error: "Target aarch64-unknown-linux-gnu is experimental"

This means you're trying to use an experimental target. Ensure your code is simple and well-tested. Consider using a production target if possible.

### "No such file or directory" when running cross-compiled binary

Ensure the binary format matches your target system:
- Linux: Use `.elf` format with `--emit elf`
- macOS: Use `.macho` format with `--emit macho`
- Windows: Use `.exe` format (default with `-o app.exe`)

### "Unsupported target triple"

The target triple you specified is either:
1. Misspelled (check with `--list-all-targets`)
2. Planned but not yet implemented

---

## See Also

- [COMPILER_FLAGS.md](COMPILER_FLAGS.md) — Detailed flag reference
- [GETTING_STARTED.md](GETTING_STARTED.md) — Quick start guide
- [LINKER_USAGE.md](LINKER_USAGE.md) — Linking examples
- [docs/spec/backend.md](docs/spec/backend.md) — Backend specification
