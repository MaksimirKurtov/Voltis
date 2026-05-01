# Voltis Compiler Flags Reference

Complete documentation of all command-line flags and options for the Voltis compiler (`voltisc`).

**Navigation:** [README](README.md) · [Getting Started](GETTING_STARTED.md) · [Platforms](PLATFORMS.md) · [Linker Usage](LINKER_USAGE.md)

## Quick Reference

```bash
# Basic compilation
voltisc <input.vlt> [-o <output>]

# Multiple inputs (single link unit)
voltisc file1.vlt file2.vlt file3.vlt -o app.exe

# Emit intermediate formats
voltisc <input.vlt> --emit-vir -o output.vir
voltisc <input.vlt> --emit-llvm -o output.ll

# Cross-compile to specific target
voltisc <input.vlt> --target x86_64-pc-windows-msvc -o app.exe
voltisc <input.vlt> --target x86_64-unknown-linux-gnu --emit elf -o app.elf
voltisc <input.vlt> --target aarch64-apple-darwin --emit macho -o app.macho

# List available targets
voltisc --list-targets        # Production-ready targets only
voltisc --list-all-targets    # All targets with readiness levels

# Benchmark mode
voltisc --benchmark
```

## Input/Output Flags

### `-o <path>` — Output Path

Specifies the output artifact path.

**Examples:**

```bash
# Windows executable
voltisc hello.vlt -o hello.exe

# Linux ELF binary
voltisc hello.vlt --emit elf -o hello.elf

# Native format (auto-detected from target)
voltisc hello.vlt -o app
```

**Default behavior:** If `-o` is not specified, output filename is derived from input:
- Windows: `input.exe`
- Linux/Unix: `input` (no extension)

---

## Output Format Flags

### `--emit <format>` — Output Binary Format

Specifies the output binary format. Supported formats:

| Format | File Ext | Platforms | Status | Notes |
|--------|----------|-----------|--------|-------|
| `pe` | `.exe` | Windows | ✅ Production | PE32+ COFF format; native Windows executable |
| `elf` | `.elf` | Linux, Unix | ⚠️ Experimental | ELF binary format; may use PE scaffolding |
| `macho` | `.macho` | macOS | ⚠️ Experimental | Mach-O binary format; may use PE scaffolding |
| `bin` | `.bin` | Bare Metal | 📋 Planned | Raw binary output |
| `hex` | `.hex` | Bare Metal | 📋 Planned | Intel HEX format |
| `srec` | `.srec` | Bare Metal | 📋 Planned | Motorola S-record format |

**Examples:**

```bash
# Windows PE executable (default on Windows)
voltisc hello.vlt --emit pe -o hello.exe

# Linux ELF binary
voltisc hello.vlt --emit elf -o hello.elf

# macOS Mach-O binary
voltisc hello.vlt --emit macho -o hello.macho

# Raw binary (embedded systems)
voltisc firmware.vlt --emit bin -o firmware.bin
```

---

### `--emit-vir` — Emit Voltis Intermediate Representation

Outputs the Voltis IR (VIR) in human-readable text format instead of a binary executable. Useful for debugging and understanding the intermediate representation.

**Example:**

```bash
voltisc hello.vlt --emit-vir -o hello.vir
cat hello.vir
```

**Output sample:**
```
function main() -> int32 {
  %0 = string_literal "Hello from Voltis"
  %1 = call @print(%0)
  %2 = const int32 0
  ret %2
}
```

---

### `--emit-llvm` — Emit LLVM Intermediate Representation

Outputs LLVM IR text format (`.ll`). This is an experimental backend useful for inspection and potential future LLVM integration.

**Example:**

```bash
voltisc hello.vlt --emit-llvm -o hello.ll
cat hello.ll
```

**Output sample (excerpt):**
```llvm
@str = private constant [18 x i8] c"Hello from Voltis\00"

define i32 @main() {
  %1 = alloca i32
  call void @print(i8* @str)
  ret i32 0
}
```

---

## Target Selection Flags

### `--target <triple>` — Full Target Triple

Specifies the complete target triple in the format: `ARCH-VENDOR-OS-ABI`

**Format:** `architecture-vendor-os-abi`

**Example triples:**

```bash
# Windows x86-64
voltisc hello.vlt --target x86_64-pc-windows-msvc -o hello.exe

# Linux x86-64 GNU
voltisc hello.vlt --target x86_64-unknown-linux-gnu --emit elf -o hello.elf

# Linux x86-64 musl (Alpine)
voltisc hello.vlt --target x86_64-unknown-linux-musl --emit elf -o hello.elf

# macOS x86-64
voltisc hello.vlt --target x86_64-apple-darwin --emit macho -o hello.macho

# macOS ARM64 (Apple Silicon)
voltisc hello.vlt --target aarch64-apple-darwin --emit macho -o hello.macho

# Linux ARM64
voltisc hello.vlt --target aarch64-unknown-linux-gnu --emit elf -o hello.elf
```

See [PLATFORMS.md](PLATFORMS.md) for the complete target catalog.

---

### `--arch <architecture>` — Override Architecture

Overrides only the architecture component of the target triple.

**Supported architectures:**

- `x86` — 32-bit x86 (i386)
- `x64` (or `x86_64`) — 64-bit x86
- `arm64` (or `aarch64`) — 64-bit ARM
- `armv7` — 32-bit ARM v7
- `armv6` — 32-bit ARM v6
- `riscv32` — 32-bit RISC-V
- `riscv64` — 64-bit RISC-V
- `mips` — 32-bit MIPS
- `mips64` — 64-bit MIPS
- `ppc64` — 64-bit PowerPC
- `s390x` — IBM System z
- `xtensa` — Tensilica Xtensa
- `avr` — Atmel AVR

**Examples:**

```bash
# Keep current target but change to ARM64
voltisc hello.vlt --arch arm64 -o hello.exe

# Change to x86 (32-bit)
voltisc hello.vlt --arch x86 -o hello.exe
```

---

### `--os <operating-system>` — Override OS

Overrides only the OS component of the target triple.

**Supported operating systems:**

- `windows` — Windows
- `linux` — Linux
- `macos` — macOS/Darwin
- `baremetal` — Bare Metal (no OS)
- `unknown` — Unknown/Generic

**Examples:**

```bash
# Change to Windows
voltisc hello.vlt --os windows -o hello.exe

# Change to Linux
voltisc hello.vlt --os linux --emit elf -o hello.elf

# Bare metal (embedded)
voltisc firmware.vlt --os baremetal --emit bin -o firmware.bin
```

---

### `--abi <abi>` — Override ABI

Overrides only the ABI (Application Binary Interface) component of the target triple.

**Supported ABIs:**

- `msvc` — Microsoft Visual C++ ABI (Windows)
- `gnu` — GNU ABI (Linux GCC/Clang)
- `musl` — musl libc ABI (Alpine Linux)
- `eabi` — Embedded ABI (ARM/bare metal)
- `eabihf` — Embedded ABI with hardware floating-point (ARM)
- `none` — No ABI (bare metal)
- `unknown` — Unknown/default ABI

**Examples:**

```bash
# Use musl libc instead of glibc
voltisc hello.vlt --abi musl --emit elf -o hello.elf

# Use embedded ABI
voltisc firmware.vlt --abi eabi --emit bin -o firmware.bin

# EABI with hardware float
voltisc app.vlt --abi eabihf -o app
```

---

### `--sysroot <path>` — Sysroot Path Hint

Provides a sysroot path hint for cross-compilation. This informs the compiler about the target system root directory for header files and libraries.

**Example:**

```bash
# Cross-compile to Linux ARM using a sysroot
voltisc app.vlt \
  --target aarch64-unknown-linux-gnu \
  --sysroot /path/to/sysroot/aarch64-linux-gnu \
  --emit elf -o app.elf
```

---

## Target Discovery Flags

### `--list-targets` — List Production-Ready Targets

Lists all production-ready target triples that have been validated and are recommended for use.

**Example:**

```bash
$ voltisc --list-targets

Production-ready targets:
  x86_64-pc-windows-msvc         Windows x86-64 (MSVC ABI)
  x86_64-unknown-linux-gnu       Linux x86-64 (GNU ABI)
  x86_64-apple-darwin            macOS x86-64
```

---

### `--list-all-targets` — List All Available Targets

Lists all built-in target configurations, including experimental and planned targets with their readiness status.

**Example:**

```bash
$ voltisc --list-all-targets

PRODUCTION:
  x86_64-pc-windows-msvc         Windows x86-64 (MSVC ABI)
  x86_64-unknown-linux-gnu       Linux x86-64 (GNU ABI)
  x86_64-apple-darwin            macOS x86-64

EXPERIMENTAL:
  aarch64-unknown-linux-gnu      Linux ARM64 (experimental)
  aarch64-apple-darwin           macOS ARM64 (experimental)

PLANNED:
  x86-unknown-linux-gnu          32-bit Linux (not yet ready)
  armv7-unknown-linux-gnueabihf  ARMv7 with hardware float (not yet ready)
  ...and more
```

---

## Bootstrap & Intermediate Flags

### `--bootstrap-cpp` — Use C++17 Bootstrap Backend

Experimental flag to use the temporary C++17 bootstrap backend instead of the production native backend. This generates C++17 source code instead of native binary.

**Note:** This is a non-production path used for development and debugging.

**Example:**

```bash
voltisc hello.vlt --bootstrap-cpp --emit-cpp hello.cpp --no-link
cat hello.cpp  # View generated C++17 code
```

---

### `--emit-cpp <path>` — Output C++ Source

When used with `--bootstrap-cpp`, outputs the generated C++17 source code to the specified path.

**Example:**

```bash
voltisc hello.vlt --bootstrap-cpp --emit-cpp hello.cpp --no-link
g++ -std=c++17 hello.cpp -o hello.exe  # Manually compile with host C++
```

---

### `--no-link` — Skip Linking Phase

When used with `--bootstrap-cpp`, skips the final C++ compilation step. Useful for inspecting generated C++ code before compilation.

**Example:**

```bash
voltisc hello.vlt --bootstrap-cpp --emit-cpp hello.cpp --no-link
# C++ code is generated, but not compiled to executable
```

---

## Performance & Analysis Flags

### `--benchmark` — Run Benchmark Mode

Launches the embedded Voltis compiler benchmark. This mode measures compilation time, execution time, and memory usage with a live terminal dashboard.

**Example:**

```bash
$ voltisc --benchmark

╔════════════════════════════════════════════════════════╗
║           Voltis Compiler Benchmark Mode               ║
╠════════════════════════════════════════════════════════╣
║ Compilation Time: 125.34 ms                            ║
║ Execution Time:   2.15 ms                              ║
║ Total Time:       127.49 ms                            ║
║ Peak Memory:      12.5 MB                              ║
╚════════════════════════════════════════════════════════╝
```

Benchmark results are saved to a CSV history file for trend analysis.

**Output:** Live terminal dashboard + CSV history (in temp/AppData directory)

---

## Usage Examples

### Example 1: Simple Windows Compilation

```bash
voltisc hello.vlt -o hello.exe
./hello.exe
```

### Example 2: Cross-Compile to Linux ELF

```bash
voltisc hello.vlt \
  --target x86_64-unknown-linux-gnu \
  --emit elf \
  -o hello.elf

# Transfer hello.elf to Linux system and run
./hello.elf
```

### Example 3: Cross-Compile to macOS ARM64

```bash
voltisc app.vlt \
  --target aarch64-apple-darwin \
  --emit macho \
  -o app.macho
```

### Example 4: Emit All Intermediate Formats

```bash
# Native executable
voltisc app.vlt -o app.exe

# Voltis IR
voltisc app.vlt --emit-vir -o app.vir

# LLVM IR
voltisc app.vlt --emit-llvm -o app.ll
```

### Example 5: Debug with Bootstrap C++

```bash
# Generate C++17 code (no linking)
voltisc app.vlt --bootstrap-cpp --emit-cpp app.cpp --no-link

# Inspect the generated C++17 code
cat app.cpp

# Manually compile if needed
g++ -std=c++17 app.cpp -o app.exe
```

### Example 6: Multi-File Compilation

```bash
# Compile multiple source files in one invocation
voltisc main.vlt utils.vlt helpers.vlt -o myapp.exe

# Or use imports (Voltis will resolve automatically)
voltisc main.vlt -o myapp.exe  # If main.vlt imports utils.vlt
```

---

## Flags Compatibility Matrix

| Flag | With Bootstrap | With --emit-vir | With --emit-llvm | With --benchmark |
|------|---|---|---|---|
| `-o` | ✅ | ✅ | ✅ | ✅ |
| `--emit <format>` | ✅ | ❌ | ❌ | ❌ |
| `--target` | ✅ | ✅ | ✅ | ❌ |
| `--arch` | ✅ | ✅ | ✅ | ❌ |
| `--os` | ✅ | ✅ | ✅ | ❌ |
| `--abi` | ✅ | ✅ | ✅ | ❌ |
| `--sysroot` | ✅ | ✅ | ✅ | ❌ |
| `--emit-cpp` | ✅ | ❌ | ❌ | ❌ |
| `--no-link` | ✅ | ❌ | ❌ | ❌ |

---

## Error Messages

### "Unknown argument: --foo"

The flag you provided is not recognized. Check the spelling and use `--list-targets` to see what's available.

### "Missing value for --target"

You provided `--target` but didn't specify a value. Use `--target x86_64-unknown-linux-gnu` (with a value).

### "--list-targets cannot be combined with other options"

When using `--list-targets` or `--list-all-targets`, these must be used alone without input files or other flags.

### "Unsupported --emit format"

The format you specified with `--emit` is not recognized. Valid formats: `pe`, `elf`, `macho`, `bin`, `hex`, `srec`.

---

## Advanced Tips

1. **Always use absolute/relative paths for `--sysroot` when cross-compiling**
2. **For embedded systems, prefer `--emit bin` for raw binary output**
3. **Use `--emit-vir` to debug compilation issues**
4. **Use `--benchmark` to profile compiler performance**
5. **Use `--list-all-targets` to see experimental and planned target support**

---

## See Also

- [Getting Started](GETTING_STARTED.md)
- [Platform Support Matrix](PLATFORMS.md)
- [Linker Usage & Examples](LINKER_USAGE.md)
- [Build System](BUILD_SYSTEM.md)
- [Architecture Overview](docs/architecture.md)
