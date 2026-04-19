# Voltis Examples Quick Reference

Navigation: [README](../README.md) · [Whitepaper](whitepaper.md) · [Syntax spec](spec/syntax.md) · [Type spec](spec/types.md) · [Conversion spec](spec/conversions.md) · [Control-flow spec](spec/control_flow.md)

This page is a practical quick reference for the **currently implemented subset**. It is not a replacement for the spec.

## Downloadable example files

| File | Description |
|---|---|
| [hello.vlt](../examples/hello.vlt?raw=1) | Minimal hello-world program with `main` and `print`. |
| [control_flow.vlt](../examples/control_flow.vlt?raw=1) | Demonstrates `if/else`, `while`, `break`, `continue`, and helper functions. |
| [conversions.vlt](../examples/conversions.vlt?raw=1) | Demonstrates `ToInt32`, `ToFloat32`, `ToFloat64`, `ToBool`, `Round`, `Floor`, and `Ceil`. |
| [advanced_control_flow.vlt](../examples/advanced_control_flow.vlt?raw=1) | Larger control-flow sample with loop accumulation and function calls. |

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

## Functions

| Syntax | Explanation |
|--------|------------|
| <pre><code>public fn add(int32 a, int32 b) -&gt; int32 {<br>    return a + b;<br>}</code></pre> | Function with explicit typed parameters and return type |
| <pre><code>public fn banner() -&gt; void {<br>    print("hi");<br>    return;<br>}</code></pre> | `void` function with explicit `return;` |
| <pre><code>public fn main() -&gt; int32 {<br>    int32 total = add(2, 3);<br>    print(total.ToString());<br>    return 0;<br>}</code></pre> | Direct function calls with typed locals |

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

Need the full reference docs? See [docs/spec/README.md](spec/README.md).
