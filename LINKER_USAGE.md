# Voltis Linker Usage Guide

Comprehensive guide to linking, DLL/SO imports, and calling external functions in Voltis.

**Navigation:** [README](README.md) · [Compiler Flags](COMPILER_FLAGS.md) · [Platforms](PLATFORMS.md) · [Getting Started](GETTING_STARTED.md)

## Table of Contents

- [Linker Model Overview](#linker-model-overview)
- [DLL/SO Import Declarations](#dllso-import-declarations)
- [Extern Function Declarations](#extern-function-declarations)
- [Calling External Functions](#calling-external-functions)
- [Import Examples by Platform](#import-examples-by-platform)
- [ABI and Calling Conventions](#abi-and-calling-conventions)
- [Linker Resolution and Symbol Lookup](#linker-resolution-and-symbol-lookup)
- [Complete Examples](#complete-examples)
- [Troubleshooting](#troubleshooting)

---

## Linker Model Overview

### Design Philosophy

Voltis uses an **automatic import address table (IAT) linking model** for simplicity:

1. **Declaration:** Declare external DLL/SO/dylib imports and extern functions in your code
2. **Resolution:** The compiler resolves symbols at compile time using the platform's import model
3. **Execution:** Runtime calls invoke imported functions via IAT entries (Windows) or dynamic linking (Unix)

### Supported Link Models

| Platform | Link Model | Import Format | Status |
|----------|---|---|---|
| **Windows (PE)** | Import Address Table (IAT) | `.dll` files | ✅ Production |
| **Linux (ELF)** | Dynamic linking (PLT/GOT) | `.so` libraries | ✅ Production |
| **macOS (Mach-O)** | Dynamic linking | `.dylib` libraries | ✅ Production |

### Link Artifacts

| Artifact | Format | Purpose |
|----------|--------|---------|
| **Import Declaration** | `import "dll";` | Register DLL for linking |
| **Extern Function** | `extern fn Name() -> Type from "dll";` | Declare external function |
| **Call Site** | `Name()` | Invoke external function |
| **IAT Entry** | Internal | Windows: jump table to imported symbol |
| **PLT Entry** | Internal | Linux/macOS: procedure linkage table entry |

---

## DLL/SO Import Declarations

### Syntax

Import declarations register a dynamic library (DLL, SO, or dylib) for the linker to include:

```voltis
import "library.dll";
import "libc.so.6";
import "CoreServices.dylib";
```

### Full Syntax Reference

```voltis
// String form (most common)
import "kernel32.dll";

// Angle-bracket form (alternative)
import <kernel32.dll>;

// Relative paths
import "../libs/custom.dll";
import "./native/helper.so";

// Platform-specific naming (normalized at link time)
import "kernel32.dll";    // Windows
import "kernel32.so";     // Linux (alternative name)
import "kernel32.dylib";  // macOS (alternative name)
```

### Import Location

Imports can be declared:
1. **At the file level** (before any functions)
2. **At the function level** (inside functions — resolved at function scope)

**Best practice:** Declare imports at the file level for clarity.

**Example:**

```voltis
// File-level imports
import "kernel32.dll";
import "ntdll.dll";
import "user32.dll";

public fn main() -> int32 {
    // Functions from these DLLs can now be called
    return 0;
}
```

---

## Extern Function Declarations

### Syntax

Extern functions declare external symbols that will be resolved at link time:

```voltis
extern fn FunctionName(arg1: type1, arg2: type2, ...) -> ReturnType from "library.dll";
```

### Examples

**Simple function (no parameters):**
```voltis
extern fn GetCurrentProcessId() -> int32 from "kernel32.dll";
```

**Function with parameters:**
```voltis
extern fn MessageBoxA(hwnd: int32, text: int32, caption: int32, flags: int32) -> int32 from "user32.dll";
```

**Function returning void:**
```voltis
extern fn Sleep(milliseconds: int32) -> void from "kernel32.dll";
```

**Multiple parameters with mixed types:**
```voltis
extern fn CreateFileA(
    filename: int32,
    desiredAccess: int32,
    shareMode: int32,
    securityAttributes: int32,
    creationDisposition: int32,
    flagsAndAttributes: int32,
    templateFile: int32
) -> int32 from "kernel32.dll";
```

### Declaration Rules

1. **Must specify return type** using `-> Type` syntax (even if `void`)
2. **Must specify source library** using `from "library.dll"` syntax
3. **Parameters are optional** (can be zero parameters)
4. **Must be declared before use** (order matters)
5. **Function names are case-sensitive**

---

## Calling External Functions

### Basic Invocation

Once declared, extern functions are called like regular functions:

```voltis
extern fn GetTickCount() -> int32 from "kernel32.dll";

public fn main() -> int32 {
    var ticks = GetTickCount();
    print(ticks.ToString());
    return 0;
}
```

### Calling with Arguments

```voltis
extern fn Sleep(milliseconds: int32) -> void from "kernel32.dll";

public fn main() -> int32 {
    Sleep(1000);  // Sleep for 1 second
    print("Wake up!");
    return 0;
}
```

### Storing Return Values

```voltis
extern fn GetCurrentProcessId() -> int32 from "kernel32.dll";

public fn main() -> int32 {
    var pid = GetCurrentProcessId();
    print("Process ID: ");
    print(pid.ToString());
    return 0;
}
```

### Using Return Values in Expressions

```voltis
extern fn GetTickCount() -> int32 from "kernel32.dll";

public fn main() -> int32 {
    var startTime = GetTickCount();
    var endTime = GetTickCount();
    var elapsed = endTime - startTime;
    print("Elapsed: ");
    print(elapsed.ToString());
    return 0;
}
```

---

## Import Examples by Platform

### Windows (PE / kernel32.dll)

**Example: Process Information**

```voltis
import "kernel32.dll";

extern fn GetCurrentProcessId() -> int32 from "kernel32.dll";
extern fn GetCurrentThreadId() -> int32 from "kernel32.dll";
extern fn GetTickCount() -> int32 from "kernel32.dll";

public fn main() -> int32 {
    var pid = GetCurrentProcessId();
    var tid = GetCurrentThreadId();
    var ticks = GetTickCount();
    
    print("PID: ");
    print(pid.ToString());
    print(", TID: ");
    print(tid.ToString());
    print(", Ticks: ");
    print(ticks.ToString());
    
    return 0;
}
```

**Compile:**
```bash
voltisc windows_info.vlt -o windows_info.exe
./windows_info.exe
```

**Output:**
```
PID: 1234, TID: 5678, Ticks: 123456789
```

---

### Linux (ELF / libc.so)

**Example: Standard C Library Functions**

```voltis
import "libc.so.6";

extern fn printf(format: int32, arg1: int32, arg2: int32) -> int32 from "libc.so.6";
extern fn exit(code: int32) -> void from "libc.so.6";

public fn main() -> int32 {
    print("Hello from Voltis");
    exit(0);
    return 0;
}
```

**Compile:**
```bash
voltisc linux_libc.vlt --target x86_64-unknown-linux-gnu --emit elf -o linux_libc.elf
./linux_libc.elf
```

---

### macOS (Mach-O / libSystem.dylib)

**Example: macOS System Functions**

```voltis
import "libSystem.dylib";

extern fn getpid() -> int32 from "libSystem.dylib";
extern fn getppid() -> int32 from "libSystem.dylib";

public fn main() -> int32 {
    var pid = getpid();
    var ppid = getppid();
    
    print("PID: ");
    print(pid.ToString());
    print(", Parent PID: ");
    print(ppid.ToString());
    
    return 0;
}
```

**Compile:**
```bash
voltisc macos_info.vlt --target x86_64-apple-darwin --emit macho -o macos_info.macho
./macos_info.macho
```

---

## ABI and Calling Conventions

### Supported Calling Conventions

Voltis automatically uses the correct calling convention for the target platform:

| Platform | Calling Convention | Details |
|----------|---|---|
| **Windows x86-64** | Microsoft x64 | RCX, RDX, R8, R9 for first 4 args; return in RAX/RDX |
| **Linux x86-64** | System V AMD64 | RDI, RSI, RDX, RCX, R8, R9 for args; return in RAX/RDX |
| **macOS x86-64** | System V AMD64 | Same as Linux |
| **ARM64 (Linux/macOS)** | AArch64 AAPCS64 | X0-X7 for args; return in X0/X1 |

### Type Mapping to Native Types

Voltis types are mapped to C types for extern function calls:

| Voltis Type | C Type | Size | Details |
|---|---|---|---|
| `int32` | `int32_t` / `int` | 4 bytes | Signed 32-bit integer |
| `float32` | `float` | 4 bytes | IEEE 754 single-precision |
| `float64` | `double` | 8 bytes | IEEE 754 double-precision |
| `string` | `const char*` / `char*` | Pointer | Null-terminated string pointer |
| `bool` | `bool` / `int32_t` | 1-4 bytes | Boolean (0 = false, non-zero = true) |
| `void` | `void` | N/A | No return value |

---

## Linker Resolution and Symbol Lookup

### How Symbols Are Resolved

1. **Compile-time:** Extern function declarations are recorded in VIR
2. **Codegen:** IAT entries or PLT entries are generated
3. **Link-time (automatic):** Compiler bakes in import tables
4. **Runtime (OS):** Operating system loads and resolves symbols

### Import Search Order (Windows PE)

1. System PATH
2. Application directory
3. Windows system directories (System32, SysWOW64)
4. Current working directory (unsafe, not recommended)

### Import Search Order (Linux ELF)

1. `LD_LIBRARY_PATH` environment variable
2. `/etc/ld.so.cache` (precomputed cache)
3. `/lib`, `/usr/lib`, `/usr/local/lib`
4. `/lib64`, `/usr/lib64` (64-bit systems)

### Import Search Order (macOS Mach-O)

1. `DYLD_LIBRARY_PATH` (deprecated in newer macOS)
2. `DYLD_FALLBACK_LIBRARY_PATH`
3. `/usr/local/lib`, `/usr/lib`
4. Frameworks in `/Library/Frameworks`, `/System/Library/Frameworks`

---

## Complete Examples

### Example 1: Windows Process Management

**app.vlt:**
```voltis
import "kernel32.dll";

extern fn GetCurrentProcessId() -> int32 from "kernel32.dll";
extern fn Sleep(milliseconds: int32) -> void from "kernel32.dll";

public fn delayedPrint(message: string, delayMs: int32) -> void {
    Sleep(delayMs);
    print(message);
}

public fn main() -> int32 {
    var pid = GetCurrentProcessId();
    print("Process started with PID: ");
    print(pid.ToString());
    
    delayedPrint("After 1 second...", 1000);
    
    return 0;
}
```

**Compile and run:**
```bash
voltisc app.vlt -o app.exe
./app.exe
```

---

### Example 2: Multi-DLL Import (Windows)

**multi_dll.vlt:**
```voltis
import "kernel32.dll";
import "user32.dll";

extern fn GetTickCount() -> int32 from "kernel32.dll";
extern fn MessageBoxA(hwnd: int32, text: int32, caption: int32, type: int32) -> int32 from "user32.dll";

public fn main() -> int32 {
    var ticks = GetTickCount();
    print("Ticks: ");
    print(ticks.ToString());
    
    // Note: MessageBoxA requires pointer arguments in real Windows API usage
    // This is simplified for demonstration
    
    return 0;
}
```

---

### Example 3: Cross-Platform Compatible Code

**cross_platform.vlt:**
```voltis
// Platform-agnostic function wrapper
public fn getCurrentTime() -> int32 {
    // Could use different imports per platform
    // For now, using Windows as example
    return getTickCountWindows();
}

import "kernel32.dll";
extern fn GetTickCount() -> int32 from "kernel32.dll";

public fn getTickCountWindows() -> int32 {
    return GetTickCount();
}

public fn main() -> int32 {
    var time = getCurrentTime();
    print("Time: ");
    print(time.ToString());
    return 0;
}
```

---

### Example 4: Custom Native Library (Windows)

If you have a custom DLL `mylib.dll`:

**app_custom.vlt:**
```voltis
import "mylib.dll";

extern fn Add(a: int32, b: int32) -> int32 from "mylib.dll";
extern fn Multiply(a: int32, b: int32) -> int32 from "mylib.dll";

public fn main() -> int32 {
    var sum = Add(5, 3);
    var product = Multiply(sum, 2);
    
    print("5 + 3 = ");
    print(sum.ToString());
    print(", (5+3) * 2 = ");
    print(product.ToString());
    
    return 0;
}
```

**Ensure `mylib.dll` is:**
- In the same directory as the executable, OR
- In the system PATH, OR
- Built from C/C++ with proper exports

---

## Troubleshooting

### "Undefined reference to function X"

**Cause:** The extern function is called but not declared.

**Solution:** Add the extern function declaration before calling it:

```voltis
// ❌ Wrong
public fn main() -> int32 {
    GetCurrentProcessId();  // Not declared yet!
    return 0;
}

// ✅ Correct
extern fn GetCurrentProcessId() -> int32 from "kernel32.dll";

public fn main() -> int32 {
    GetCurrentProcessId();
    return 0;
}
```

---

### "DLL not found" or "shared object not found"

**Cause:** The imported DLL/SO/dylib cannot be located at runtime.

**Solution:**

**Windows:**
```bash
# Ensure DLL is in:
# 1. Same directory as executable
# 2. System32 / SysWOW64
# 3. Any directory in PATH

copy mylib.dll .
./app.exe
```

**Linux:**
```bash
# Set library path
export LD_LIBRARY_PATH=.:$LD_LIBRARY_PATH
./app.elf

# Or use rpath (requires build system support)
```

**macOS:**
```bash
# Set library path
export DYLD_LIBRARY_PATH=.:$DYLD_LIBRARY_PATH
./app.macho

# Or use install_name_tool (requires build system support)
```

---

### "Symbol not found" or "Entry point not found"

**Cause:** The function name doesn't exist in the DLL/SO, or is spelled incorrectly.

**Solution:**

1. **Verify the correct function name** (case-sensitive):
   ```bash
   # Windows: Use dependency walker or Sym2Dump
   dumpbin /exports kernel32.dll | find "GetCurrentProcessId"
   
   # Linux: Use nm or objdump
   nm -D /lib/x86_64-linux-gnu/libc.so.6 | grep printf
   
   # macOS: Use nm
   nm /usr/lib/libSystem.dylib | grep getpid
   ```

2. **Update your declaration:**
   ```voltis
   // Make sure the name matches exactly
   extern fn GetCurrentProcessId() -> int32 from "kernel32.dll";  // ✅ Correct
   extern fn getcurrentprocessid() -> int32 from "kernel32.dll";  // ❌ Wrong (case mismatch)
   ```

---

### "Stack alignment error" or "Access violation"

**Cause:** Incorrect calling convention or signature mismatch.

**Solution:**

1. **Verify the function signature** matches the actual C function:
   ```voltis
   // Windows API documentation says:
   // int GetCurrentProcessId(void);
   // So in Voltis:
   extern fn GetCurrentProcessId() -> int32 from "kernel32.dll";  // ✅ Correct
   ```

2. **For complex signatures**, consult the Windows API or man page for the C signature

---

### "Link error" or "Linker failure"

**Cause:** Unrelated to Voltis linker, but a platform linking issue.

**Solution:**

1. **Verify the DLL/SO/dylib is valid:**
   ```bash
   # Windows
   dumpbin /headers kernel32.dll
   
   # Linux
   file /lib/x86_64-linux-gnu/libc.so.6
   
   # macOS
   file /usr/lib/libSystem.dylib
   ```

2. **Check platform compatibility** (32-bit vs 64-bit):
   ```bash
   # Windows: Use dumpbin
   dumpbin /headers kernel32.dll | find "Machine"
   
   # Linux/macOS: Use file
   file ./app.elf
   ```

---

## Advanced Topics

### IAT (Import Address Table) — Windows Details

On Windows PE, imported functions are accessed via an Import Address Table:

```
Assembly pseudocode:
call [rip + iat_entry_GetCurrentProcessId]
```

Voltis generates this automatically; no manual control needed.

### PLT/GOT (Procedure Linkage Table / Global Offset Table) — Linux/Unix Details

On ELF systems, imported functions use PLT/GOT for lazy binding:

```
Assembly pseudocode:
jmp [rip + got_entry_printf]
```

Again, Voltis handles this automatically.

### ABI Compatibility

Ensure the extern function signature **exactly matches** the C signature:

**C header:**
```c
int32_t add(int32_t a, int32_t b);
```

**Voltis declaration:**
```voltis
extern fn add(a: int32, b: int32) -> int32 from "mylib.dll";  // ✅ Matches
```

---

## See Also

- [COMPILER_FLAGS.md](COMPILER_FLAGS.md) — Compiler flag reference
- [PLATFORMS.md](PLATFORMS.md) — Platform support matrix
- [Getting Started](GETTING_STARTED.md) — Quick start guide
- [examples/windows_api.vlt](examples/windows_api.vlt) — Real example
- [docs/spec/backend.md](docs/spec/backend.md) — Linker model specification
