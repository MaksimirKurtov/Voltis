# Voltis compiler prototype (`voltisc`)

This repository contains a **C++17 prototype compiler** for a **small, implemented Voltis subset**.
It follows the whitepaper direction, but it is not the full language/toolchain yet.

## Implemented pipeline (today)

`voltisc` currently runs:

1. Lexer (`.vlt` -> tokens)
2. Parser (tokens -> AST)
3. Semantic analysis (subset checks)
4. Typed lowering (AST -> VIR)
5. Backend abstraction (`IBackend`)
6. Direct Windows x64 PE backend (native executable output)
7. LLVM IR text backend (`.ll`) for inspection/debugging

There is also an explicit temporary path:

- `--bootstrap-cpp`: AST -> generated C++17 -> host C++ compiler (optional)

The direct native backend currently covers the subset exercised by the bundled examples: `int32`, `bool`, `string`, and `float64` with the supported conversions and comparisons.

## Implemented vs scaffolded status

| Area | Status in code |
|---|---|
| Semantic analysis | **Implemented for subset**: function registration, scoped symbols, type checks for expressions/assignments/returns/calls, conversion-member validation |
| VIR | **Implemented for subset**: typed VIR model (`src/vir.*`) and lowering from semantic output (`src/lowering.*`) |
| Backend abstraction + LLVM | **Implemented boundary** (`src/backend.h`) + **implemented LLVM IR text backend** (`src/backend_llvm_ir.*`) |
| Runtime library | **Implemented** (`src/voltis_runtime.cpp`) as `voltis_runtime` CMake target |
| Native `.exe` generation from VIR backend flow | **Implemented**: direct Windows PE backend with default native executable output |

## Supported Voltis subset (actual parser/sema subset)

Syntax in this repo requires braces and semicolons:

```voltis
public fn add(int32 a, int32 b) -> int32 {
    return a + b;
}

public fn main() -> int32 {
    int32 speed = 10;
    float64 precise = 12.75;
    string text = speed.ToString();
    print("speed as text: " + text);

    int32 total = add(speed, 5);
    print(total);

    if ((total > 10) and true) {
        print(precise.Round().ToInt32());
    } else {
        print("small");
    }

    return 0;
}
```

Implemented language pieces:

- top-level `fn name(...) -> type { ... }`
- modifiers are lexed/parsed and currently ignored (`public/private/protected/internal/static/readonly/const/volatile/unsafe`)
- types: `int32`, `float32`, `float64`, `string`, `bool`, `void`
- statements: local declarations, `var` inference, assignment, expression statements, `if/else`, `return`
- expressions: arithmetic, comparisons, logical `and/or/not` (and `!`), direct function calls, conversion member calls
- built-in function: `print(expr)`
- conversion members:
  - `.ToString()`
  - `.ToInt32()` (float -> int truncates toward zero)
  - `.ToFloat32()`
  - `.ToFloat64()`
  - `.ToBool()`
  - `.Round()`
  - `.Floor()`
  - `.Ceil()`

Semantic diagnostics currently include:

- unknown/duplicate symbols
- unknown types
- function argument count/type mismatch
- return type mismatch
- assignment compatibility errors
- invalid conversion member usage/receiver type

## Building the compiler

### Linux / macOS
```bash
cmake -S . -B build
cmake --build build -j
```

### Windows (MSYS2 / MinGW)
```bash
cmake -S . -B build -G "MinGW Makefiles"
cmake --build build -j
```

### Windows (Visual Studio)
```powershell
cmake -S . -B build
cmake --build build --config Release
```

## Running automated tests

```powershell
cmake -S . -B build
cmake --build build --config Release
ctest --test-dir build -C Release --output-on-failure
```

## Using the compiler

### Production-directed default (build native executable)
```bash
voltisc examples/hello.vlt
```

### Emit VIR text
```bash
voltisc examples/hello.vlt --emit-vir
```

### Emit LLVM IR text to custom path
```bash
voltisc examples/hello.vlt --emit-llvm -o hello.ll
```

### Temporary bootstrap C++ path (scaffolding only)
```bash
voltisc examples/hello.vlt --bootstrap-cpp --no-link
voltisc examples/hello.vlt --bootstrap-cpp -o hello.exe
```

If needed, pick host C++ compiler explicitly:

```powershell
$env:VOLTIS_CXX = "clang++"
# or
$env:VOLTIS_CXX = "g++"
```

## Native toolchain requirements

The default production path now emits a Windows PE executable directly. No external clang/LLVM toolchain is required for normal compilation. Use `--emit-llvm` or `--emit-vir` when you want text artifacts for debugging.
