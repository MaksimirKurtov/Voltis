# Voltis Syntax Specification

Navigation: [Spec index](README.md) · [Types](types.md) · [Control flow](control_flow.md) · [Examples guide](../examples.md) · [README](../../README.md)

## 1. Source model

- File extension: `.vlt`
- Blocks use braces `{ ... }`
- Statements require semicolons `;`
- Comments:
  - `// single-line`
  - `/* multi-line */`

## 2. Top-level declarations (implemented)

The implemented subset supports top-level function declarations:

```voltis
public fn add(int32 a, int32 b) -> int32 {
    return a + b;
}
```

Recognized modifiers are currently lexed/parsed and accepted before declarations/locals:

- `public`, `private`, `protected`, `internal`
- `static`, `readonly`, `const`, `volatile`, `unsafe`

In the current subset, modifier semantics are limited; they are primarily syntactic.

## 3. Function syntax

```text
FunctionDecl ::= Modifiers? "fn" Identifier "(" ParamList? ")" "->" Type Block
ParamList     ::= Param ("," Param)*
Param         ::= Type Identifier
```

## 4. Statements (implemented subset)

- variable declaration with initializer
- assignment
- expression statement
- `if` / `else`
- `while`
- `break`
- `continue`
- `return`
- nested block statements

## 5. Expressions (implemented subset)

- literals: numeric, string, `true`, `false`
- variable references
- direct function calls: `name(args...)`
- member-style conversion calls: `value.ToString()`
- unary operators: `-`, `!`, `not`
- binary operators:
  - arithmetic: `+`, `-`, `*`, `/`
  - comparison: `<`, `<=`, `>`, `>=`, `==`, `!=`
  - logical: `and`, `or`

Operator precedence follows conventional grouping:

1. call/member
2. unary
3. multiplicative
4. additive
5. comparison
6. equality
7. logical and
8. logical or

## 6. Not in current implemented subset

The following may appear in design docs but are not currently implemented as language surface in the parser/sema subset:

- user-defined structs/classes/enums
- modules/imports
- `for`/`foreach`/`match`
- generics
- full interop declaration syntax

Track direction in [whitepaper](../whitepaper.md) and [roadmap](../../ROADMAP.md).
