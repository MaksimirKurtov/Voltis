# Voltis

> A native, systems-capable programming language with Python-like readability, C#-style declarations, Windows-first interop, safer defaults than C++, and a **true compiled backend** that produces native Windows binaries.

---

## Document status

**Project state:** Foundational whitepaper + execution plan  
**Target audience:** Claude Code, LLM coding agents, compiler engineers, systems programmers  
**Primary target platform:** Windows x86-64  
**Implementation language for compiler:** C++  
**Runtime model:** Native compiled language, **not** an interpreter, **not** a C++ transpiler  
**Primary output goal:** `.exe` and `.dll` binaries for Windows  
**Long-term goal:** Independent frontend, semantic analyzer, IR, optimizer, code generator, object writer, linker integration, standard library, package manager, tooling

---

# 1. Executive summary

**Voltis** is a statically typed, native programming language intended to occupy the space between:

- **C++** power and low-level reach
- **Python** readability and directness
- **C#** declaration style and access modifiers
- **Rust**-like safety boundaries without forcing Rust’s full ownership ergonomics

Voltis is explicitly intended to be a **real language**, not a façade over another language.

That means the final production compiler must follow this pipeline:

```text
Voltis source (.vlt)
  -> Lexer
  -> Parser
  -> AST
  -> Semantic analysis
  -> Typed IR
  -> Optimization passes
  -> Target lowering
  -> Machine code / object generation
  -> COFF object files (.obj)
  -> Linking
  -> PE executable (.exe) / DLL (.dll)
```

## Non-goal

Voltis is **not** intended to be:

- a Python-style interpreter
- a C++ transpiler in its final form
- a macro-heavy C clone
- a syntax-only skin over LLVM IR with no real language identity

## Why this language should exist

There is a real gap for developers who want:

- native binaries
- low-level system access
- clean syntax
- explicit typing
- modern tooling
- Windows API friendliness
- safer defaults than C++
- less symbolic clutter than C++
- fewer runtime constraints than managed-only ecosystems

Voltis is designed to fill that gap.

---

# 2. Core language identity

## 2.1 Design pillars

Voltis is built around six pillars:

### 1. Readability
Readable like Python, but with braces and semicolons.

### 2. Native output
Voltis must compile to native binaries, not require interpretation.

### 3. Safety by default
Unsafe operations must be explicit.

### 4. Low-level reach
Pointers, memory layout, calling conventions, Windows handles, DLL imports, register-level or driver-adjacent subsets in later stages.

### 5. Strong typing
Types are explicit and compile-time known.

### 6. Windows-first engineering
Win32, COFF, PE, DLLs, calling conventions, wide strings, COM support, native toolchain support.

---

# 3. Locked design decisions from prior planning

These decisions are already effectively locked and should be treated as baseline constraints unless a later formal RFC changes them.

## 3.1 Syntax style

Voltis should feel like **Python and C++ had a baby**, but with more disciplined design.

### Keep

- Curly braces for block structure
- Semicolons required at end of statements
- Readable keywords
- Minimal punctuation overload
- Strongly typed variables
- C#-style access and behavior modifiers
- Method-based conversions like `.ToString()` and `.ToInt32()`

### Avoid

- Python indentation-based blocks
- Excessive C++ punctuation overload
- Angle bracket hell like nested `>>>` patterns in templates/generics
- Too much symbolic shorthand with unclear meaning
- Multiple unrelated cast syntaxes

## 3.2 Numeric conversion rule

Old idea removed:

- automatic float-to-int rounding as default

New locked rule:

- `ToInt32()` truncates toward zero by default
- explicit numeric intent required for rounding/floor/ceil

Examples:

```voltis
float64 x = 2.9;
int32 a = x.ToInt32();          // 2
int32 b = x.Round().ToInt32();  // 3
int32 c = x.Floor().ToInt32();  // 2
int32 d = x.Ceil().ToInt32();   // 3
```

## 3.3 Decimal / sig-fig controlled numeric family

The original `float(4)` idea is not retained literally.

Reason:

- It mixes machine representation, decimal precision, significant digits, and compiler inference in a way that becomes inconsistent and hard to reason about.

Replacement concept:

- standard binary floats for machine-friendly math
- a separate decimal-like family for precision/significant-digit constrained values

The exact final design is still open, but the current preferred model is:

```voltis
float32
float64
decimal(4)
decimal(8)
```

Where `decimal(n)` is an explicitly decimal-domain type and **not** a hidden machine-float optimization flag.

## 3.4 Conversions

The language uses a uniform conversion model:

```voltis
value.ToString();
value.ToInt32();
value.ToFloat64();
value.ToBool();
```

This should remain a signature feature of Voltis.

---

# 4. Language goals in practical terms

Voltis should be suitable for:

- native desktop applications
- game tools and pipelines
- systems utilities
- file/process/network tools
- Win32 GUI wrappers and tooling
- compilers, parsers, editors
- performance-sensitive services
- scripting replacement in some toolchains where Python is too weak or too runtime-dependent

Voltis should later be able to support:

- game engine code
- embedded subsets
- kernel-adjacent subsets
- native plugins
- shell extensions
- DLL projects

Voltis should **not** initially optimize for:

- mobile app frameworks
- browser-only work
- ultra-high-level data science workflows
- runtime-heavy reflection-first programming

---

# 5. Full syntax design

This section documents the intended **language surface**.

## 5.1 Source files

File extension:

```text
.vlt
```

Suggested project file extensions later:

```text
.vproj      // Voltis project file
.vpkg       // package manifest
.vlock      // dependency lock file
```

## 5.2 Comments

```voltis
// Single-line comment

/*
    Multi-line comment
*/
```

## 5.3 Naming conventions

Recommended but not mandatory:

- `PascalCase` for types
- `camelCase` for variables and functions
- `UPPER_SNAKE_CASE` for compile-time constants if desired

---

# 6. Type system

## 6.1 Type categories

Voltis uses static typing and should separate types into:

- primitive scalar types
- compound value types
- reference types
- pointer types
- nullable forms
- generic types
- function types
- platform/interop types

## 6.2 Primitive numeric types

```voltis
int8
int16
int32
int64

uint8
uint16
uint32
uint64

float16    // optional on backend support; may lower to software or target-specific
float32
float64

decimal(n) // decimal-domain controlled precision type
```

## 6.3 Other primitive types

```voltis
bool
char
string
byte      // alias for uint8
void
```

## 6.4 Nullable types

```voltis
string? name = null;
int32? score = null;
```

Rule:

- Non-nullable by default
- Nullable access requires explicit handling or compiler-proven safety

## 6.5 Type inference

Voltis supports `var` for local inference only when fully inferable.

```voltis
var x = 10;
var speed = 10f;
var name = "Max";
```

This is **not dynamic typing**.

## 6.6 Type declarations with C#-style modifier feel

Examples:

```voltis
public float32 speed = 10f;
private readonly int32 maxLives = 3;
internal static bool isReady = false;
```

---

# 7. Modifiers

These modifiers should exist in the language model and be validated semantically.

## 7.1 Access modifiers

```voltis
public
private
protected
internal
```

## 7.2 Behavior/storage modifiers

```voltis
static
readonly
const
volatile
unsafe
extern
pinned
```

## 7.3 Parameter modifiers

```voltis
ref
out
in
```

Not all modifiers apply everywhere. The semantic analyzer must enforce valid placement.

---

# 8. Control flow syntax

## 8.1 If / else

```voltis
if (speed > 5f) {
    print("Fast");
}
else if (speed > 0f) {
    print("Moving");
}
else {
    print("Stopped");
}
```

## 8.2 While

```voltis
while (running) {
    update();
}
```

## 8.3 For

```voltis
for (int32 i = 0; i < 10; i += 1) {
    print(i.ToString());
}
```

## 8.4 Foreach

```voltis
foreach (string item in names) {
    print(item);
}
```

## 8.5 Match

```voltis
match (command) {
    case "start" {
        run();
    }
    case "stop" {
        stop();
    }
    default {
        print("Unknown");
    }
}
```

---

# 9. Operators

## 9.1 Arithmetic

```voltis
+
-
*
/
%
```

## 9.2 Assignment

```voltis
=
+=
-=
*=
/=
%=
```

## 9.3 Comparison

```voltis
==
!=
<
<=
>
>=
```

## 9.4 Logical

Preferred human-readable forms:

```voltis
and
or
not
```

Symbolic aliases may exist in early versions if needed:

```voltis
&&
||
!
```

Recommendation:

- accept both in parser initially
- prefer `and/or/not` in official formatting and docs

Reason:

- matches the desire for less symbol-heavy code
- improves readability
- makes conditionals feel closer to Python without using Python’s indentation model

## 9.5 Bitwise

```voltis
&
|
^
~
<<
>>
```

Use `<<` and `>>` for bit shifts only.

Avoid introducing extra overloaded meanings for angle-bracket forms.

## 9.6 Ternary

This remains open.

Initial support may use:

```voltis
value = condition ? a : b;
```

Later possibility:

```voltis
value = if (condition) then a else b;
```

Recommendation:

- keep `?:` only if needed for compatibility and parsing simplicity
- consider keyword ternary later for consistency with readability goals

---

# 10. Functions

## 10.1 Preferred syntax

Current preferred function form:

```voltis
public fn add(int32 a, int32 b) -> int32 {
    return a + b;
}
```

Why this is currently preferred:

- short and explicit
- clear return type marker
- reduces some left-heavy complexity versus large typed signatures
- works well in parser design

Alternative possible later syntax:

```voltis
public int32 add(int32 a, int32 b) {
    return a + b;
}
```

For now, **lock `fn ... -> Type`** for compiler implementation unless a later syntax RFC changes it.

## 10.2 Return type inference

Not recommended initially.

Reason:

- native systems code benefits from explicit signatures
- improves diagnostics and tooling

## 10.3 Default parameters

```voltis
public fn connect(string host, int32 port = 80) -> bool {
    return true;
}
```

## 10.4 Overloading

Allowed if signatures are distinguishable.

Semantic analyzer must disallow ambiguous overload sets.

---

# 11. Structs, classes, enums, properties

## 11.1 Structs

Value types, fixed-layout friendly, interop-friendly.

```voltis
public struct Vec3 {
    public float32 x;
    public float32 y;
    public float32 z;
}
```

## 11.2 Classes

Reference-capable high-level types.

```voltis
public class Player {
    public string name;
    public float32 speed = 10f;
    private int32 health = 100;

    public fn move(float32 delta) -> void {
        speed += delta;
    }
}
```

## 11.3 Enums

```voltis
public enum WindowMode {
    Windowed,
    Borderless,
    Fullscreen
}
```

## 11.4 Properties

```voltis
public property int32 Health {
    get {
        return health;
    }
    set {
        health = value;
    }
}
```

Properties may come after core compiler stages. They are not required for the first fully native backend milestone.

---

# 12. Memory model

## 12.1 High-level principle

Voltis should be safer than C++ by default, but still allow low-level control.

The intended model is **hybrid**, not purely GC-only and not purely manual.

## 12.2 Memory domains

### Managed/reference domain
Good for general objects and ergonomics.

```voltis
Player p = new Player();
```

### Stack domain
For fast local storage.

```voltis
stack byte buffer[256];
```

### Region / arena domain
For grouped lifetimes.

```voltis
region frameMemory {
    var particles = new ParticleList();
    simulate(particles);
}
```

### Unsafe/raw domain
For explicit pointer-level programming.

```voltis
unsafe {
    byte* ptr = getRawPointer();
    ptr[0] = 255;
}
```

## 12.3 Early implementation reality

The first native compiler milestone should **not** try to fully implement the final memory model.

Instead:

### Phase 1 memory model

- stack locals
- globals/static storage
- no full managed heap object model yet
- raw pointers in unsafe blocks
- string runtime support
- arrays later

### Phase 2 memory model

- heap allocation runtime
- deterministic destruction for selected types
- simple runtime ownership model for reference types

### Phase 3 memory model

- regions/arenas
- better lifetime analysis
- optional safety instrumentation

This is important. Trying to solve the entire final memory model before a real backend exists is too much at once.

---

# 13. Conversion system

A signature Voltis feature.

## 13.1 Standard explicit conversions

```voltis
value.ToString();
value.ToInt32();
value.ToFloat64();
value.ToBool();
value.ToDecimal(4);
```

## 13.2 Numeric conversion rules

### Integer -> Integer
- checked by default
- overflow either errors, traps in checked mode, or requires explicit unchecked/cast path

### Integer -> Float
- allowed
- compiler may warn if exactness not guaranteed

### Float -> Integer
- truncates toward zero

### Explicit rounding helpers

```voltis
x.Round().ToInt32();
x.Floor().ToInt32();
x.Ceil().ToInt32();
```

### String -> Numeric

```voltis
int32 count = "123".ToInt32();
float64 ratio = "3.14".ToFloat64();
```

Should support `TryTo...` forms later:

```voltis
bool ok = text.TryToInt32(out value);
```

## 13.3 Why this matters

Most languages make conversion syntax fragmented and inconsistent.

Voltis should keep one discoverable model.

---

# 14. Windows-first interop design

This is one of the language’s strongest differentiators.

## 14.1 DLL imports

```voltis
extern "user32.dll" stdcall {
    public fn MessageBoxW(HWND hwnd, string text, string caption, uint32 type) -> int32;
}
```

## 14.2 Handle and pointer aliases

Built-in platform interop aliases should exist:

```voltis
IntPtr
UIntPtr
Handle
HWND
HINSTANCE
WPARAM
LPARAM
LRESULT
```

## 14.3 Layout attributes

```voltis
@[Layout(Sequential)]
@[Pack(1)]
public struct POINT {
    public int32 x;
    public int32 y;
}
```

## 14.4 Calling conventions

Support target syntax like:

```voltis
extern "kernel32.dll" stdcall {
    public fn Beep(uint32 freq, uint32 duration) -> bool;
}
```

Required convention support for Windows-first implementation:

- platform native default ABI
- `stdcall`
- `cdecl`
- later `fastcall` if needed

## 14.5 Wide string handling

Must support UTF-16 interop cleanly.

### Recommendation

Internally, the language can use UTF-8 strings for most semantics, but interop layers must support:

- conversion to UTF-16 when calling `W` Win32 APIs
- pinned buffers where necessary
- ABI-safe string marshalling

---

# 15. What “real backend” means

This section is critical.

A true backend means Voltis does **not** output C++ in production.

Instead, it lowers from typed IR into a native binary generation pipeline.

## 15.1 Final native backend pipeline

```text
Source (.vlt)
 -> Lexer
 -> Parser
 -> AST
 -> Name resolution
 -> Type checking
 -> Lowering to Voltis IR (VIR)
 -> Optimization passes
 -> Instruction selection
 -> Register allocation
 -> Machine code emission
 -> COFF object writing (.obj)
 -> Linker
 -> PE output (.exe/.dll)
```

## 15.2 Two realistic implementation routes

### Route A: LLVM backend

```text
Voltis frontend -> LLVM IR -> LLVM codegen -> COFF -> linker -> PE
```

Pros:

- faster path to real native output
- mature optimization pipeline
- mature x86-64 backend
- object file emission already solved
- easier stepping stone

Cons:

- backend dependency on LLVM
- some loss of full control over low-level codegen details
- toolchain complexity

### Route B: Custom backend

```text
Voltis frontend -> Voltis IR -> custom x86-64 backend -> COFF -> linker -> PE
```

Pros:

- fully ours
- maximum control
- language identity fully independent end-to-end

Cons:

- much harder
- slower to first success
- object format + relocation + debug info + linker integration become major projects

## 15.3 Recommended strategy

Use a **staged architecture**:

### Stage 1
Temporary bootstrap frontend and language validation work.

### Stage 2
Build **true frontend + Voltis IR**.

### Stage 3
Add **LLVM backend** for real native output.

### Stage 4
Optionally replace or supplement with a **custom x86-64/COFF backend**.

This gives a real language sooner while preserving the option for full independence later.

---

# 16. The recommended true backend architecture

This section describes the recommended production compiler architecture for coding agents.

## 16.1 Frontend

### Components

- Lexer
- Parser
- AST node model
- Symbol tables
- Type resolver
- Semantic diagnostics
- AST lowering

### Responsibilities

- parse Voltis syntax
- enforce language rules
- build typed semantic representation
- never rely on C++ semantics to decide Voltis semantics

## 16.2 Voltis IR (VIR)

Voltis needs its own intermediate representation.

This is non-negotiable if the language is to stay coherent.

### Why not lower straight from AST to machine code?

Because that makes:

- optimization harder
- diagnostics harder
- control-flow analysis harder
- future backend portability harder

### VIR should represent

- typed values
- variables and storage classes
- branches and basic blocks
- arithmetic
- comparisons
- conversions
- calls
- loads/stores
- memory intrinsics
- constants
- platform intrinsics

### Example pseudo VIR

```text
func main() -> i32 {
entry:
    %0 = const_string "Hello World"
    call print(%0)
    %1 = const_i32 0
    ret %1
}
```

## 16.3 Backend abstraction layer

Define a backend interface like:

```cpp
class IBackend {
public:
    virtual void emitModule(const VIRModule& module, const BackendOptions& opts) = 0;
    virtual ~IBackend() = default;
};
```

Implementations:

- `LlvmBackend`
- later `X64CoffBackend`

This prevents frontend lock-in.

## 16.4 Machine model assumptions for first real target

Target only:

- Windows
- x86-64
- PE/COFF
- MS x64 ABI

Do not attempt multi-arch too early.

---

# 17. LLVM route: the practical real backend

If the goal is to become a real native compiler soon, LLVM is the best practical first backend.

## 17.1 LLVM architecture in Voltis

```text
Voltis AST
 -> semantic analysis
 -> VIR
 -> LLVM IR generation
 -> LLVM target machine
 -> COFF object file
 -> lld-link / link.exe
 -> PE executable
```

## 17.2 Why LLVM still counts as “real”

Using LLVM does **not** make Voltis fake.

The language is still real because:

- syntax is ours
- semantics are ours
- type system is ours
- parser is ours
- diagnostics are ours
- IR lowering is ours

LLVM is only the code generation backend.

That is standard compiler engineering.

## 17.3 Required LLVM responsibilities

- target triple setup: `x86_64-pc-windows-msvc` or compatible
- data layout selection
- function/code emission
- global emission
- string literal emission
- intrinsics for arithmetic and conversions
- object file generation

## 17.4 Linking on Windows

Use either:

- `lld-link`
- or Microsoft `link.exe`

Prefer `lld-link` where possible for toolchain portability.

The compiler driver should be able to produce:

```text
hello.obj
hello.exe
```

and later:

```text
mylib.dll
mylib.lib
```

---

# 18. Custom backend route: the true independent backend

Long-term, Voltis may want a full custom backend.

## 18.1 Scope of a custom backend

A custom backend must implement:

- instruction selection
- register allocation
- stack frame layout
- calling convention handling
- relocation records
- symbol emission
- object file writing (COFF)
- later debug info support

## 18.2 First target subset for custom backend

Keep the initial subset tiny:

- integer arithmetic
- function calls
- returns
- local variables
- string literal references
- external function calls
- conditional branching

## 18.3 Why COFF first

Because on Windows:

- `.obj` generation is the clean interface to linkers
- direct `.exe` writing is possible, but object-based flow is more maintainable
- COFF gives better composability with external libraries and system linkers

## 18.4 Why not write PE directly first

You *can* write PE directly, but doing so immediately makes the project much harder because you must solve:

- section layout
- imports
- relocations
- startup code conventions
- symbol resolution
- linker behavior in compiler code

Writing COFF first is the more scalable approach.

---

# 19. Runtime model

Even a native compiled language usually needs some runtime support.

Voltis should keep runtime support minimal and modular.

## 19.1 Minimal runtime for first native milestone

Required runtime pieces:

- program startup glue
- basic string support
- print/console output support
- allocation stubs if heap allocations exist
- numeric conversion helpers where backend intrinsic lowering is not enough

## 19.2 Runtime philosophy

- keep runtime small
- keep runtime optional by feature set where possible
- allow “no-std” or minimal runtime profiles later

## 19.3 Likely runtime layers

```text
voltis_rt_core
voltis_rt_console
voltis_rt_memory
voltis_rt_windows
```

---

# 20. Standard library plan

The standard library should be staged.

## 20.1 Core library (`std.core`)

- primitive helpers
- string utilities
- conversion helpers
- result/optional types
- basic math

## 20.2 Console library (`std.console`)

- `print()`
- `readLine()`
- `readKey()`
- console color later if wanted

## 20.3 Collections (`std.collections`)

- `List<T>`
- `Map<K, V>`
- arrays
- slices/spans later

## 20.4 Filesystem (`std.fs`)

- file reading/writing
- path utilities

## 20.5 Windows library (`std.win32`)

- handle types
- safe wrappers for common APIs
- DLL loading later

## 20.6 Memory (`std.memory`)

- allocators
- arenas/regions later
- byte buffers

---

# 21. Diagnostics philosophy

A language like Voltis will live or die by tooling quality.

Diagnostics must be:

- clear
- exact
- actionable
- type-aware
- source-located

## Example desired diagnostics

```text
error V1021: cannot convert 'string' to 'int32' using ToInt32() because the source expression is not a numeric string
  --> src/main.vlt:14:18
```

```text
error V2043: modifier 'readonly' cannot be applied to a local variable
  --> src/main.vlt:7:5
```

```text
error V3110: nullable value 'name' may be null here
  --> src/main.vlt:28:12
help: add a null check or use a non-null assertion if intentional
```

---

# 22. Toolchain components

A full Voltis toolchain should eventually include these executables.

## 22.1 Compiler driver

```text
voltisc
```

Responsibilities:

- parse CLI flags
- choose target/configuration
- run frontend/backend pipeline
- invoke linker when needed
- emit diagnostics

## 22.2 Package manager / build tool

```text
voltis
```

Potential subcommands:

```text
voltis new
voltis build
voltis run
voltis test
voltis fmt
voltis check
voltis add
```

## 22.3 Formatter

```text
voltisfmt
```

Must enforce the language’s readability goals.

## 22.4 Language server

```text
voltis-lsp
```

Needed for:

- IDE support
- completions
- go-to-definition
- diagnostics
- semantic tokens

---

# 23. Recommended repository layout

```text
voltis/
├─ docs/
│  ├─ whitepaper.md
│  ├─ spec/
│  │  ├─ syntax.md
│  │  ├─ types.md
│  │  ├─ semantics.md
│  │  ├─ ir.md
│  │  ├─ backend.md
│  │  └─ runtime.md
│  └─ roadmap.md
├─ compiler/
│  ├─ include/
│  │  ├─ lexer/
│  │  ├─ parser/
│  │  ├─ ast/
│  │  ├─ sema/
│  │  ├─ ir/
│  │  ├─ backend/
│  │  ├─ coff/
│  │  ├─ driver/
│  │  └─ common/
│  ├─ src/
│  │  ├─ lexer/
│  │  ├─ parser/
│  │  ├─ ast/
│  │  ├─ sema/
│  │  ├─ ir/
│  │  ├─ backend/
│  │  ├─ coff/
│  │  ├─ driver/
│  │  └─ main.cpp
│  └─ CMakeLists.txt
├─ runtime/
│  ├─ core/
│  ├─ console/
│  ├─ memory/
│  └─ windows/
├─ stdlib/
│  ├─ core/
│  ├─ console/
│  ├─ collections/
│  ├─ fs/
│  └─ win32/
├─ tests/
│  ├─ lexer/
│  ├─ parser/
│  ├─ sema/
│  ├─ ir/
│  ├─ backend/
│  └─ integration/
├─ examples/
│  ├─ hello/
│  ├─ win32_messagebox/
│  ├─ structs/
│  ├─ conversions/
│  └─ unsafe/
└─ tools/
   ├─ formatter/
   └─ lsp/
```

---

# 24. Milestone plan

This section is meant to be operational for coding agents.

## Milestone 0 — Project reset and architecture lock

### Deliverables

- Lock whitepaper
- Lock initial syntax subset
- Lock frontend/backend separation
- Decide whether stage 1 real backend is LLVM-based or custom COFF from day one

### Recommendation

Choose:

- **frontend designed for custom backend compatibility**
- **LLVM backend as first true native backend**

## Milestone 1 — Frontend foundation

### Scope

- Lexer
- Parser
- AST
- basic diagnostics
- source spans
- test harness

### Supported syntax subset

- `fn`
- variables
- integer and string literals
- `if`
- `return`
- function calls
- unary/binary expressions

### Exit condition

- parse a multi-file sample program into AST successfully with tests

## Milestone 2 — Semantic analysis

### Scope

- symbol tables
- scopes
- type checking
- overload resolution (simple)
- builtin functions
- modifier validation
- conversion validation

### Exit condition

- compiler can reject invalid programs correctly
- typed AST or typed semantic graph exists

## Milestone 3 — Voltis IR (VIR)

### Scope

- define VIR
- lowering from typed AST
- basic block and CFG support
- constants
- calls
- arithmetic
- branching

### Exit condition

- dump VIR for valid sample programs

## Milestone 4 — First true backend

### Recommended path

LLVM backend first.

### Scope

- LLVM IR emission from VIR
- console app generation
- object file output
- linker invocation on Windows

### Exit condition

- compile `hello.vlt` to a real `.exe` without any C++ generation stage

## Milestone 5 — Runtime and stdlib core

### Scope

- string runtime
- `print`
- `readLine`
- basic math and conversions
- startup/runtime integration

### Exit condition

- real console demos compile and run

## Milestone 6 — Structs, classes, arrays, interop

### Scope

- structs
- arrays
- external functions
- Win32 imports
- simple class support

### Exit condition

- `MessageBoxW` demo works directly from Voltis

## Milestone 7 — Optimizer and code quality

### Scope

- constant folding
- dead code elimination
- simplified control-flow cleanup
- maybe rely on LLVM for many low-level optimizations if on LLVM route

### Exit condition

- reasonable code quality and optimization modes

## Milestone 8 — Tooling polish

### Scope

- formatter
- language server
- package/build tool
- docs site

---

# 25. Initial syntax subset for the first true-native compiler

Do **not** try to implement the full whitepaper on day one.

Use this minimal subset first.

## Included in subset v0

```voltis
public fn main() -> int32 {
    print("Hello World");
    return 0;
}
```

### Required features

- function definitions
- integer/string/bool literals
- local variables
- explicit typed declarations
- `var` local inference
- binary arithmetic on ints/floats
- comparisons
- `if/else`
- `return`
- function calls
- builtin `print`
- basic `.ToString()` on primitive values

### Excluded initially

- classes
- properties
- generics
- regions
- advanced nullability
- async
- full stdlib
- decimal(n)
- traits/interfaces

This is how you keep momentum.

---

# 26. Concrete implementation notes for the compiler in C++

## 26.1 Language implementation language

C++ is appropriate for implementing the compiler because:

- native performance
- mature ecosystem
- LLVM integration if chosen
- easy Windows toolchain access
- low-level file/object handling

## 26.2 C++ coding guidelines for the compiler

- avoid giant monolithic source files
- AST nodes should be structured and source-spanned
- use arenas or bump allocators for compiler-owned nodes if desired
- keep diagnostics and spans attached early
- separate parse-time syntax nodes from semantically typed nodes if needed
- do not bake backend assumptions into parser or sema

## 26.3 Suggested core classes

```cpp
SourceFile
Token
Lexer
Parser
AstNode
Expr
Stmt
Decl
Type
Symbol
Scope
SemaAnalyzer
VirModule
VirFunction
VirInstruction
BackendOptions
IBackend
LlvmBackend
Driver
DiagnosticEngine
```

## 26.4 Source span discipline

Every token and meaningful AST node should carry span info.

Without this, diagnostics will become weak fast.

---

# 27. Parsing strategy

Use a clean recursive descent parser initially.

Why:

- easy to control
- easy to debug
- works well for a custom-designed language
- simple to extend as grammar grows

## Expression parsing

Use precedence climbing or Pratt parsing.

Recommended:

- Pratt parser for expressions
- recursive descent for declarations/statements

This keeps operator handling manageable.

---

# 28. Semantic analysis rules to enforce early

### Must exist early

- duplicate symbol detection
- undefined symbol detection
- type mismatch checks
- conversion validity checks
- return path checking
- function argument count/type checking
- invalid modifier placement
- disallow unsafe-only constructs outside `unsafe` blocks

### Can come later

- complex overload sets
- full data-flow nullability
- advanced escape/lifetime analysis

---

# 29. Object generation and linking details

This section explains the real native output path in concrete terms.

## 29.1 Object file target

First real native output should be **COFF object files** on Windows.

Why:

- native Windows format
- linker-friendly
- easier to integrate with existing tools
- better than writing raw PE immediately

## 29.2 Executable format target

Final output for apps:

- PE32+ executable (`.exe`) for x86-64

Final output for libraries:

- PE DLL (`.dll`)

## 29.3 Startup strategy

Early console app builds can target standard C runtime startup or a small custom startup bridge.

LLVM route can usually rely on standard linker/runtime integration.

Custom backend route may eventually need:

- startup object
- entrypoint glue
- runtime initialization

## 29.4 Linker integration

Compiler driver should support:

```text
voltisc main.vlt -o app.exe
voltisc main.vlt --emit-obj
voltisc main.vlt --emit-ir
voltisc main.vlt --emit-llvm
```

Recommended linker integration order:

1. `lld-link`
2. fallback to `link.exe` if configured

---

# 30. Example “hello world” in the real language

```voltis
public fn main() -> int32 {
    print("Hello World");
    return 0;
}
```

## Example Win32 interop

```voltis
extern "user32.dll" stdcall {
    public fn MessageBoxW(HWND hwnd, string text, string caption, uint32 type) -> int32;
}

public fn main() -> int32 {
    MessageBoxW(0, "Hello from Voltis", "Voltis", 0u);
    return 0;
}
```

## Example conversions

```voltis
public fn main() -> int32 {
    int32 value = 42;
    string text = value.ToString();
    print(text);

    float64 x = 2.9;
    int32 y = x.ToInt32();
    int32 z = x.Round().ToInt32();

    print(y.ToString());
    print(z.ToString());
    return 0;
}
```

---

# 31. Risks and likely failure points

## 31.1 Biggest design risk

Trying to design the entire final language and compiler simultaneously.

Fix:

- lock the language direction
- implement a strict small subset first
- grow in layers

## 31.2 Biggest implementation risk

Backend complexity explosion.

Fix:

- define VIR early
- use LLVM as first true backend
- do not jump straight to writing full PE by hand unless absolutely necessary

## 31.3 Biggest product risk

Language identity drift.

Fix:

- preserve the core syntax and readability principles
- do not let backend convenience reshape frontend semantics

## 31.4 Biggest tooling risk

Weak diagnostics.

Fix:

- source spans everywhere
- diagnostics engine from the start

---

# 32. Recommended immediate next tasks for coding agents

## Task 1
Split any existing prototype into:

- lexer
- parser
- AST
- diagnostics
- driver

## Task 2
Delete the C++ codegen backend as the primary output path from the roadmap.

It can remain as a temporary historical bootstrap branch, but not the production direction.

## Task 3
Define VIR formally in `docs/spec/ir.md`.

## Task 4
Implement semantic analysis for the minimal subset.

## Task 5
Choose and wire the first real backend:

- **LLVM strongly recommended**

## Task 6
Compile `hello.vlt` to `.exe` without generating C++.

That is the first major legitimacy milestone.

---

# 33. Final project stance

Voltis is no longer in the “fun fake language concept” phase.

The project stance is now:

- **real native language**
- **real compiler frontend**
- **real backend**
- **real object/executable generation**
- **Windows-first**
- **C++ implementation of compiler**
- **not an interpreter**
- **not a transpiler in the final design**

---

# 34. Concise directive for LLM coding agents

When contributing to Voltis, follow these rules:

1. Do **not** redesign the language into a clone of C++, Rust, C#, or Python.
2. Do **not** convert the final architecture into a transpiler.
3. Do **not** hide unsafe operations behind implicit behavior.
4. Do **not** bloat syntax with symbolic clutter.
5. Keep the frontend independent of backend implementation details.
6. Build a real typed IR.
7. Target native Windows output through a real backend.
8. Prefer an LLVM backend first, but preserve abstractions for a future custom backend.
9. Prioritize diagnostics, spans, and testability.
10. Grow the language in strict milestones rather than implementing everything at once.

---

# 35. Final vision statement

> **Voltis is a Windows-strong, systems-capable, readable native language with C-like reach, C#-style declarations, Python-like clarity, safer defaults than C++, and a true compiled backend that produces real native binaries.**

