# Okin

> A lightweight, bytecode-like language designed for LLMs to execute computational tasks with **minimal token overhead**.

---

## What is Okin?

Okin is an interpreted language built around a small, strict instruction set. Its primary goal is to let language models, particularly **smaller local models under 500M parameters**, offload heavy or repetitive computation without the overhead of booting a full execution environment.

Larger models from major providers already have code execution frameworks, but those come with significant infrastructure cost: spinning up VMs, sandboxed filesystems, and secure runtimes. For simple tasks (which is what most LLM's use it for), that overhead is wasteful.

---

## Who is it for?

Okin targets **local LLMs under 500M parameters** that lack a native code execution layer. Common use cases:

- Counting characters in a sequence
- Basic and nonlinear arithmetic
- Tasks that require tracking state across multiple steps
- Repetitive operations better expressed as a program than prose

---

## Syntax

Every instruction follows this schema:

```
<opcode><argv>;
```

- `<opcode>` - an integer representing the operation
- `<argv>` - comma-separated arguments wrapped in `< >`
- Instructions are separated by `;` and can be written on a **single line**

### Nested blocks use `|` as a delimiter

```
32<I, 0, 5, 1|192~WRITELN<I>>   -- for loop printing I from 0 to 4
```

### Standard library calls use `~`

```
<STD_LIB>~<METHOD><args>

192~WRITELN<Hello, World>        -- IO lib, WRITELN method
208~CONCAT<A, B, DEST>           -- String lib, CONCAT method
```

---

## Key Features

### Single-line programs
Okin is designed to be written without line breaks. A complete program:
```
2<SUM, 0>;2<I, 1>;32<I, 1, 11, 1|64<SUM, I, SUM>>;192~WRITELN<SUM>
```

### Program unfolding
Okin source can be **unfolded** into a BASIC-like representation for easier reading and data flow analysis, then **refolded** back into compact Okin on demand. Models choose when to unfold using specific opcodes, useful when programs grow complex enough to require explicit tracing.

### Soft typing
Variables are dynamically typed. Type is inferred on assignment and can change on reassignment via `SET`. Type mismatches at runtime **throw an error**.

### Python-style scoping
- Variables inside functions are local by default
- Outer variables are **readable** from inner scopes
- Outer variables are only **writable** from inner scopes when explicitly marked with `GLOBAL`

### Error handling
Okin throws on:
- Any operation on `NIL`
- Type mismatches
- Division by zero
- Out-of-bounds array access
- Calling undefined functions

Errors halt execution and surface a message, allowing the model to rewrite and retry.

---

> **TL;DR** - Okin is a bytecode-like language for LLMs to write token-efficient, executable programs with a minimal and predictable instruction set.
