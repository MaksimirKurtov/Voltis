# Voltis Conversion Specification

Navigation: [Spec index](README.md) · [Types](types.md) · [Examples guide](../examples.md) · [Control flow](control_flow.md)

## 1. Conversion model

Voltis uses method-style conversions on values:

- `ToString()`
- `ToInt32()`
- `ToFloat32()`
- `ToFloat64()`
- `ToBool()`
- `Round()`
- `Floor()`
- `Ceil()`

Calls currently take no arguments in the implemented subset.

## 2. Receiver rules

| Conversion | Allowed receiver types | Result type |
|---|---|---|
| `ToString()` | `int32`, `float32`, `float64`, `string`, `bool` | `string` |
| `ToInt32()` | `int32`, `float32`, `float64`, `string`, `bool` | `int32` |
| `ToFloat32()` | `int32`, `float32`, `float64`, `string` | `float32` |
| `ToFloat64()` | `int32`, `float32`, `float64`, `string` | `float64` |
| `ToBool()` | `int32`, `float32`, `float64`, `string`, `bool` | `bool` |
| `Round()` | `float32`, `float64` | same float type |
| `Floor()` | `float32`, `float64` | same float type |
| `Ceil()` | `float32`, `float64` | same float type |

## 3. Numeric behavior

- `float -> int32` via `ToInt32()` truncates toward zero.
- Rounding intent is explicit:

```voltis
int32 a = x.ToInt32();
int32 b = x.Round().ToInt32();
int32 c = x.Floor().ToInt32();
int32 d = x.Ceil().ToInt32();
```

## 4. String conversion validation

For string literals, semantic checks validate parseability for:

- `ToInt32()` (base-10 integer string)
- `ToFloat32()` / `ToFloat64()` (numeric string)
- `ToBool()` (`"true"` or `"false"`)

## 5. Invalid usage

The semantic analyzer emits errors for:

- unknown conversion method names
- unsupported receiver type
- passing arguments to conversion methods

See [examples guide](../examples.md#conversions) for usage patterns.
