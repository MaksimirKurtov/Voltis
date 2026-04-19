# Voltis Control Flow Specification

Navigation: [Spec index](README.md) · [Syntax](syntax.md) · [Types](types.md) · [Examples guide](../examples.md)

## 1. Conditionals

Implemented forms:

- `if (...) { ... }`
- `if (...) { ... } else { ... }`
- `else if` chains

Condition expressions must be `bool`.

## 2. Loops

Implemented loop:

- `while (condition) { ... }`

Loop condition must be `bool`.

## 3. Loop control statements

- `break;` exits the nearest enclosing `while`
- `continue;` skips to next `while` iteration

Both statements are only valid inside loops.

## 4. Return behavior

- `return expr;` for non-`void` functions
- `return;` for `void` functions

Semantic rules:

- non-`void` functions require valid return paths
- `void` functions cannot return a value

## 5. Current scope

Not yet implemented in parser/sema subset:

- `for`
- `foreach`
- `match`

Those remain directional targets and are non-authoritative until merged into implementation + spec.
