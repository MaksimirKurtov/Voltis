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

The implemented subset supports imports, struct declarations, extern declarations, and top-level functions:

```voltis
import "kernel32.dll";
struct Vec2 {
    float64 x;
    float64 y;
}
extern fn GetCurrentProcessId() -> int32 from "kernel32.dll";

public fn add(int32 a, int32 b) -> int32 {
    return a + b;
}
```

Alternate angle-bracket imports are accepted:

```voltis
Import <kernel32.dll>;
```

Recognized modifiers are currently lexed/parsed and accepted before declarations/locals:

- `public`, `private`, `protected`, `internal`
- `static`, `readonly`, `const`, `volatile`, `unsafe`

In the current subset, modifier semantics are limited; they are primarily syntactic.

## 3. Declaration syntax

```text
ImportDecl         ::= "import" (StringLiteral | "<" PathTokens ">") ";"
StructDecl         ::= Modifiers? "struct" Identifier "{" StructField* "}"
StructField        ::= Type Identifier ";"
ExternFunctionDecl ::= Modifiers? "extern" "fn" Identifier "(" ParamList? ")" "->" Type "from" (StringLiteral | "<" PathTokens ">") ";"
FunctionDecl       ::= Modifiers? "fn" Identifier "(" ParamList? ")" "->" Type Block
ParamList          ::= Param ("," Param)*
Param              ::= Type Identifier
```

Source-file imports are supported when the imported path ends in `.vlt`. These imports are resolved relative to the importing source file and compiled as part of one module graph. Missing files and import cycles are compiler errors.

Native-library import paths currently recognize:

- `.dll`
- `.lib`
- `.a`
- `.so`
- `.dylib`

For the Windows x64 PE backend, non-`.dll` library paths are normalized to a PE import target name during backend linking.

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

- classes/enums
- `for`/`foreach`/`match`
- generics

Track direction in [whitepaper](../whitepaper.md) and [roadmap](../../ROADMAP.md).
