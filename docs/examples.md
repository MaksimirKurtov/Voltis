# Voltis Language Examples

Practical code examples and quick reference for the currently implemented Voltis language subset.

**Navigation:** [README](../README.md) · [Specification](spec/README.md) · [Getting Started](../GETTING_STARTED.md) · [Compiler Flags](../COMPILER_FLAGS.md)

**This is a quick reference for implemented features.** For formal language specification, see [spec/](spec/README.md).

## Downloadable example files

| File | Description |
|---|---|
| [hello.vlt](../examples/hello.vlt?raw=1) | Minimal hello-world program with `main` and `print`. |
| [control_flow.vlt](../examples/control_flow.vlt?raw=1) | Demonstrates `if/else`, `while`, `break`, `continue`, and helper functions. |
| [conversions.vlt](../examples/conversions.vlt?raw=1) | Demonstrates `ToInt32`, `ToFloat32`, `ToFloat64`, `ToBool`, `Round`, `Floor`, and `Ceil`. |
| [advanced_control_flow.vlt](../examples/advanced_control_flow.vlt?raw=1) | Larger control-flow sample with loop accumulation and function calls. |
| [windows_api.vlt](../examples/windows_api.vlt?raw=1) | Demonstrates DLL import declarations and external Windows API calls. |

## Variables

```voltis
int32 count = 10;
float64 ratio = 12.75;
string label = "ready";
bool enabled = true;
```

```voltis
var inferredInt = 42;
var inferredFloat = 3.5f;
```

## Types

Implemented primitive types:

- `int32`
- `float32`
- `float64`
- `string`
- `bool`
- `void`

Frontend-supported composite type forms:

- `StructName`
- `Type*`
- `Type&`
- `Type[N]`
- `Type[]`

## Functions

| Syntax | Explanation |
|--------|------------|
| <pre><code>public fn add(int32 a, int32 b) -&gt; int32 {<br>    return a + b;<br>}</code></pre> | Function with explicit typed parameters and return type |
| <pre><code>extern fn GetCurrentProcessId() -&gt; int32 from "kernel32.dll";</code></pre> | External function declaration bound to an imported DLL |
| <pre><code>public fn banner() -&gt; void {<br>    print("hi");<br>    return;<br>}</code></pre> | `void` function with explicit `return;` |
| <pre><code>public fn main() -&gt; int32 {<br>    int32 total = add(2, 3);<br>    print(total.ToString());<br>    return 0;<br>}</code></pre> | Direct function calls with typed locals |

## Imports and DLL interop

| Syntax | Explanation |
|--------|------------|
| <pre><code>import "math.vlt";</code></pre> | Import another Voltis source file relative to the current file |
| <pre><code>import "kernel32.dll";</code></pre> | Import a DLL by string path |
| <pre><code>import "kernel32.lib";<br>import "libkernel32.a";</code></pre> | Import-library/archive forms accepted by the linker layer |
| <pre><code>import "kernel32.so";<br>import "kernel32.dylib";</code></pre> | Shared-library style names normalized for PE backend linking |
| <pre><code>import &lt;kernel32.dll&gt;;</code></pre> | Alternate angle-bracket import form |
| <pre><code>extern fn GetCurrentProcessId() -&gt; int32 from "kernel32.dll";</code></pre> | Bind an external symbol to an imported DLL |
| <pre><code>int32 pid = GetCurrentProcessId();</code></pre> | Call an external function like any direct function call |

## Conditionals

| Syntax | Explanation |
|--------|------------|
| <pre><code>if (enabled) {<br>    print("on");<br>}</code></pre> | Basic `if` with boolean condition |
| <pre><code>if (score &gt; 90) {<br>    print("high");<br>} else {<br>    print("normal");<br>}</code></pre> | `if/else` branch |
| <pre><code>if (a &gt; b) {<br>    print("a");<br>} else if (a == b) {<br>    print("equal");<br>} else {<br>    print("b");<br>}</code></pre> | `else if` chain |

## Loops

| Syntax | Explanation |
|--------|------------|
| <pre><code>while (x &lt; 10) {<br>    x = x + 1;<br>}</code></pre> | Basic while loop |
| <pre><code>while (true) {<br>    if (done) {<br>        break;<br>    }<br>    if (skip) {<br>        continue;<br>    }<br>}</code></pre> | Loop control with `break` and `continue` |

## Conversions

| Syntax | Explanation |
|--------|------------|
| <pre><code>string text = value.ToString();</code></pre> | Convert primitive value to `string` |
| <pre><code>int32 i = num.ToInt32();</code></pre> | Convert to `int32` (float truncates toward zero) |
| <pre><code>float32 f32 = i.ToFloat32();<br>float64 f64 = i.ToFloat64();</code></pre> | Convert integer to floating types |
| <pre><code>bool ok = "true".ToBool();</code></pre> | Parse bool from string literal (`"true"`/`"false"`) |
| <pre><code>int32 rounded = x.Round().ToInt32();<br>int32 floorV = x.Floor().ToInt32();<br>int32 ceilV = x.Ceil().ToInt32();</code></pre> | Explicit rounding/floor/ceil workflow |

## Built-ins

```voltis
print("hello");
print(42);
print(3.14f);
print(true);
```

`print(expr)` takes exactly one argument and rejects `void` expressions.

---

## Complete Examples

### Example 1: Basic Math and Functions

```voltis
public fn add(a: int32, b: int32) -> int32 {
    return a + b;
}

public fn multiply(a: int32, b: int32) -> int32 {
    return a * b;
}

public fn main() -> int32 {
    var sum = add(5, 3);
    var product = multiply(sum, 2);
    
    print("5 + 3 = ");
    print(sum.ToString());
    print(", (5+3) * 2 = ");
    print(product.ToString());
    
    return 0;
}
```

**Run:**
```bash
voltisc example1.vlt -o example1.exe
./example1.exe
```

**Output:**
```
5 + 3 = 8, (5+3) * 2 = 16
```

---

### Example 2: Loops and Conditionals

```voltis
public fn findMax(a: int32, b: int32, c: int32) -> int32 {
    var max = a;
    
    if (b > max) {
        max = b;
    }
    
    if (c > max) {
        max = c;
    }
    
    return max;
}

public fn sumNumbers(n: int32) -> int32 {
    var sum = 0;
    var i = 1;
    
    while (i <= n) {
        sum = sum + i;
        i = i + 1;
    }
    
    return sum;
}

public fn main() -> int32 {
    var max = findMax(10, 25, 15);
    var sum = sumNumbers(10);
    
    print("Max of (10, 25, 15) = ");
    print(max.ToString());
    print(", Sum of 1..10 = ");
    print(sum.ToString());
    
    return 0;
}
```

**Output:**
```
Max of (10, 25, 15) = 25, Sum of 1..10 = 55
```

---

### Example 3: Using Type Conversions

```voltis
public fn main() -> int32 {
    var temperature = 98.6f;
    var count = 42;
    var active = true;
    var message = "hello";
    
    // Convert to string
    var tempStr = temperature.ToString();
    var countStr = count.ToString();
    var activeStr = active.ToString();
    
    print("Temperature: ");
    print(tempStr);
    print(", Count: ");
    print(countStr);
    print(", Active: ");
    print(activeStr);
    print(", Message: ");
    print(message);
    
    // Numeric conversions
    var floatVal = 3.7f;
    var rounded = floatVal.Round().ToInt32();
    var floored = floatVal.Floor().ToInt32();
    var ceiled = floatVal.Ceil().ToInt32();
    
    print("3.7 rounded: ");
    print(rounded.ToString());
    print(", floored: ");
    print(floored.ToString());
    print(", ceiled: ");
    print(ceiled.ToString());
    
    return 0;
}
```

**Output:**
```
Temperature: 3.7, Count: 42, Active: true, Message: hello
3.7 rounded: 4, floored: 3, ceiled: 4
```

---

### Example 4: Factorial (Recursive)

```voltis
public fn factorial(n: int32) -> int32 {
    if (n <= 1) {
        return 1;
    }
    return n * factorial(n - 1);
}

public fn main() -> int32 {
    var result = factorial(5);
    print("5! = ");
    print(result.ToString());
    return 0;
}
```

**Output:**
```
5! = 120
```

---

### Example 5: Fibonacci Sequence

```voltis
public fn fibonacci(n: int32) -> int32 {
    if (n <= 1) {
        return n;
    }
    var a = 0;
    var b = 1;
    var i = 2;
    
    while (i <= n) {
        var next = a + b;
        a = b;
        b = next;
        i = i + 1;
    }
    
    return b;
}

public fn main() -> int32 {
    var i = 0;
    while (i < 10) {
        var fib = fibonacci(i);
        print("F(");
        print(i.ToString());
        print(") = ");
        print(fib.ToString());
        print(" ");
        i = i + 1;
    }
    return 0;
}
```

**Output:**
```
F(0) = 0 F(1) = 1 F(2) = 1 F(3) = 2 F(4) = 3 F(5) = 5 F(6) = 8 F(7) = 13 F(8) = 21 F(9) = 34
```

---

### Example 6: Windows API Usage (Windows Only)

```voltis
import "kernel32.dll";

extern fn GetCurrentProcessId() -> int32 from "kernel32.dll";
extern fn GetCurrentThreadId() -> int32 from "kernel32.dll";
extern fn GetTickCount() -> int32 from "kernel32.dll";
extern fn Sleep(milliseconds: int32) -> void from "kernel32.dll";

public fn main() -> int32 {
    var pid = GetCurrentProcessId();
    var tid = GetCurrentThreadId();
    var startTicks = GetTickCount();
    
    print("Process ID: ");
    print(pid.ToString());
    print(", Thread ID: ");
    print(tid.ToString());
    print(", Initial Ticks: ");
    print(startTicks.ToString());
    
    // Sleep for 100ms
    Sleep(100);
    
    var endTicks = GetTickCount();
    var elapsed = endTicks - startTicks;
    
    print(", Elapsed Ticks: ");
    print(elapsed.ToString());
    
    return 0;
}
```

**Compile:**
```bash
voltisc windows_example.vlt -o windows_example.exe
./windows_example.exe
```

---

### Example 7: Source File Imports

**main.vlt:**
```voltis
import "math_utils.vlt";

public fn main() -> int32 {
    var sum = add(10, 20);
    var product = multiply(sum, 3);
    
    print("add(10, 20) = ");
    print(sum.ToString());
    print(", multiply(30, 3) = ");
    print(product.ToString());
    
    return 0;
}
```

**math_utils.vlt:**
```voltis
public fn add(a: int32, b: int32) -> int32 {
    return a + b;
}

public fn multiply(a: int32, b: int32) -> int32 {
    return a * b;
}
```

**Compile:**
```bash
voltisc main.vlt -o app.exe
./app.exe
```

**Output:**
```
add(10, 20) = 30, multiply(30, 3) = 90
```

---

### Example 8: Cross-Compilation

Compile for Linux ELF from Windows/macOS:

```bash
# Compile hello.vlt for Linux x86-64
voltisc hello.vlt \
  --target x86_64-unknown-linux-gnu \
  --emit elf \
  -o hello.elf

# Transfer to Linux system and run
scp hello.elf user@linux-server:~
ssh user@linux-server ./hello.elf
```

---

### Example 9: Emit and Inspect Intermediate Formats

**Emit VIR (Voltis Intermediate Representation):**
```bash
voltisc hello.vlt --emit-vir -o hello.vir
cat hello.vir  # View VIR text
```

**Emit LLVM IR:**
```bash
voltisc hello.vlt --emit-llvm -o hello.ll
cat hello.ll   # View LLVM IR text
```

---

### Example 10: Running the Benchmarker

Measure compilation and execution performance:

```bash
voltisc --benchmark
```

**Output:**
```
╔════════════════════════════════════════════════════════╗
║           Voltis Compiler Benchmark Mode               ║
╠════════════════════════════════════════════════════════╣
║ Compilation Time: 125.34 ms                            ║
║ Execution Time:   2.15 ms                              ║
║ Total Time:       127.49 ms                            ║
║ Peak Memory:      12.5 MB                              ║
╚════════════════════════════════════════════════════════╝
```

---

## Quick Syntax Cheat Sheet

### Variables and Types

```voltis
// Explicit types
int32 x = 42;
float32 y = 3.14f;
float64 z = 2.71828;
string name = "Alice";
bool flag = true;
void nothing;  // Not assignable

// Type inference
var a = 42;        // int32
var b = 3.14f;     // float32
var c = "hello";   // string
var d = true;      // bool
```

### Functions

```voltis
// No parameters
public fn greet() -> void {
    print("Hello");
    return;
}

// With parameters
public fn add(a: int32, b: int32) -> int32 {
    return a + b;
}

// Multiple statements
public fn process(value: int32) -> int32 {
    var doubled = value * 2;
    var squared = doubled * doubled;
    return squared;
}
```

### Control Flow

```voltis
// If/else
if (x > 0) {
    print("positive");
} else if (x < 0) {
    print("negative");
} else {
    print("zero");
}

// While loop
var i = 0;
while (i < 10) {
    print(i.ToString());
    i = i + 1;
}

// Break and continue
while (true) {
    if (done) {
        break;       // Exit loop
    }
    if (skip) {
        continue;    // Skip to next iteration
    }
    // ... process ...
}

// Return
public fn divide(a: int32, b: int32) -> int32 {
    if (b == 0) {
        return 0;    // Early return
    }
    return a / b;
}
```

### Type Conversions

```voltis
var intVal = 42;
var floatVal = 3.14f;
var strVal = "123";
var boolVal = true;

// To string
var s1 = intVal.ToString();         // "42"
var s2 = floatVal.ToString();       // "3.14"
var s3 = boolVal.ToString();        // "true"

// To int32
var i1 = floatVal.ToInt32();        // 3 (truncates)
var i2 = strVal.ToInt32();          // 123 (from "123")

// To float32/float64
var f1 = intVal.ToFloat32();        // 42.0
var f2 = intVal.ToFloat64();        // 42.0

// To bool
var b1 = strVal.ToBool();           // Parses "true"/"false"

// Math functions
var r1 = floatVal.Round();          // 3.0
var r2 = floatVal.Floor();          // 3.0
var r3 = floatVal.Ceil();           // 4.0
```

### Imports and External Functions

```voltis
// Import a DLL/SO
import "kernel32.dll";
import "libc.so.6";

// Declare external function
extern fn GetProcessId() -> int32 from "kernel32.dll";

// Call external function
var pid = GetProcessId();

// Import a Voltis source file
import "utils.vlt";

// Use imported function
var result = myUtilFunction(42);
```

---

## Common Patterns

### Pattern 1: Counting with a Loop

```voltis
public fn countTo(n: int32) -> void {
    var i = 1;
    while (i <= n) {
        print(i.ToString());
        print(" ");
        i = i + 1;
    }
}
```

### Pattern 2: Finding Maximum

```voltis
public fn max(a: int32, b: int32) -> int32 {
    if (a > b) {
        return a;
    }
    return b;
}
```

### Pattern 3: Accumulation in a Loop

```voltis
public fn sum(n: int32) -> int32 {
    var total = 0;
    var i = 1;
    while (i <= n) {
        total = total + i;
        i = i + 1;
    }
    return total;
}
```

### Pattern 4: Condition-Based Early Return

```voltis
public fn validate(value: int32) -> bool {
    if (value < 0) {
        return false;
    }
    if (value > 100) {
        return false;
    }
    return true;
}
```

---

Need the full reference docs? See [docs/spec/README.md](spec/README.md).
