# Voltis compiler prototype (`voltisc`)

This is a **working prototype compiler** for a **small Voltis subset**, written in **C++17**.
It is not the full whitepaper language yet.

## What it currently does

`voltisc`:
1. lexes a `.vlt` source file
2. parses it into an AST
3. transpiles that AST into portable **C++17**
4. optionally invokes a native C++ compiler to produce a Windows `.exe`

So this is a real compiler pipeline, but the backend target is currently generated C++ instead of direct PE machine code.

## Supported Voltis subset

### File: `examples/hello.vlt`
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

### Features implemented
- `public/private/...` modifiers are accepted and ignored for now
- `fn name(...) -> type {}` functions
- `int32`, `float32`, `float64`, `string`, `bool`, `void`
- local variable declarations
- `var` local inference in basic cases
- `if / else`
- `return`
- arithmetic: `+ - * /`
- comparisons: `== != < <= > >=`
- logical `and / or / not`
- function calls
- built-in `print(...)`
- conversion methods:
  - `.ToString()`
  - `.ToInt32()`
  - `.ToFloat32()`
  - `.ToFloat64()`
  - `.ToBool()`
  - `.Round()`
  - `.Floor()`
  - `.Ceil()`

## Project files
- `src/main.cpp` - CLI driver and native compiler invocation
- `src/lexer.h` / `src/lexer.cpp` - tokenizer
- `src/token.h` - token types
- `src/ast.h` - AST nodes
- `src/parser.h` / `src/parser.cpp` - parser
- `src/codegen.h` / `src/codegen.cpp` - C++ backend
- `examples/hello.vlt` - example Voltis program
- `CMakeLists.txt` - build config

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

## Using the compiler

### Generate C++ only
```bash
voltisc examples/hello.vlt --no-link
```

### Generate C++ and build executable
```bash
voltisc examples/hello.vlt -o hello.exe
```

If your compiler is not found automatically, set:

### Windows PowerShell
```powershell
$env:VOLTIS_CXX = "clang++"
```

or

```powershell
$env:VOLTIS_CXX = "g++"
```

Then run:
```powershell
.\voltisc.exe .\examples\hello.vlt -o hello.exe
```

## Important limitation

This is a **starter compiler**. It does **not** yet implement:
- classes
- structs
- properties
- modules/imports
- arrays/lists/maps
- Windows API extern declarations in Voltis syntax
- direct PE code generation
- direct machine-code backend
- full type checking
- decimal(sig-fig) support
- ownership / region memory semantics from the whitepaper

## Best next steps

1. Add semantic analysis and symbol tables
2. Add classes/structs and method declarations
3. Add richer type checking
4. Add `extern "user32.dll" {}` support
5. Add Windows ABI and import library emission
6. Replace C++ backend with LLVM IR or direct codegen later

