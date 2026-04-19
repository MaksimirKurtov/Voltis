# Voltis Type Specification

Navigation: [Spec index](README.md) · [Syntax](syntax.md) · [Conversions](conversions.md) · [Examples guide](../examples.md)

## 1. Primitive types (implemented)

- `int32`
- `float32`
- `float64`
- `string`
- `bool`
- `void`

## 2. Literal typing

- integer literals -> `int32`
- floating literals:
  - `12.5` -> `float64`
  - `12.5f` -> `float32`
- string literals -> `string`
- `true`/`false` -> `bool`

## 3. Variable declarations

Explicit type:

```voltis
int32 count = 10;
float64 ratio = 1.5;
```

Local inference with `var`:

```voltis
var count = 10;      // infers int32
var ratio = 1.5f;    // infers float32
```

`var` inference requires a non-`void` initializer.

## 4. Assignability rules (implemented)

Allowed:

- exact type match
- `int32` -> `float32`
- `int32` -> `float64`
- `float32` -> `float64`

Not implicitly assignable:

- narrowing float conversions (e.g., `float64` -> `float32`, `float64` -> `int32`)
- string/boolean cross-domain assignments without explicit conversion members

## 5. Function typing rules

- parameters and return type are explicit
- non-`void` functions must return a value on all required paths
- `void` functions may use `return;` and must not return a value

## 6. Current boundary

Not yet part of implemented type surface:

- user-defined aggregate types
- nullable/reference categories
- generics/templates

Design direction remains in [whitepaper](../whitepaper.md) and [roadmap](../../ROADMAP.md).
