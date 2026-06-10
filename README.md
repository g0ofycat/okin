# Okin

> A lightweight, bytecode-like language designed for LLMs to execute computational tasks with **minimal token overhead**.

<p align="center">
    <img src="images/okin_icon.png" style="width: 50%; height: 50%"/>
</p>

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
2<SUM, 0>;32<I, 1, 11, 1|64<SUM, I, SUM>>;192~WRITELN<SUM>
```

### Soft typing
Variables are dynamically typed. Type is inferred on assignment and can change on reassignment via `SET`. Type mismatches at runtime **throw an error**.

### Python-style scoping
- Variables inside functions are local by default
- Outer variables are **readable** from inner scopes
- Outer variables are only **writable** from inner scopes when explicitly marked with `GLOBAL`

## VM Flag

For faster code execution *(~10-20x speed)*, use the **"-vm"** flag to go from: Walking an AST and then running line by line; to instead compiling the parsed tokens, and then using a VM to execute actual bytecode (not Okin) to make execution much faster.

**Tests (Using [Google Benchmark](https://github.com/google/benchmark)):**

The following tests is a naive implementation of fib(25) without memoization

- Without the **-vm** flag:

```
g0ofycat@workspace:~/projects/okin$ ./build/okin_bench '16<FIB, N|112<85<N,1>|18<N>>; 18<64<17<FIB,65<N,2>>,17<FIB,65<N,1>>>>; >; 17<FIB,25>'
2026-06-10T14:05:48-04:00
Running ./build/okin_bench
Run on (4 X 3493.49 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x2)
  L1 Instruction 32 KiB (x2)
  L2 Unified 1024 KiB (x2)
  L3 Unified 16384 KiB (x1)
Load Average: 0.84, 0.31, 0.12
-----------------------------------------------------
Benchmark           Time             CPU   Iterations
-----------------------------------------------------
BM_okin     312515571 ns    312519192 ns            2
g0ofycat@workspace:~/projects/okin$
```

- With the **-vm** flag:

```
g0ofycat@workspace:~/projects/okin$ ./build/okin_bench '16<FIB, N|112<85<N,1>|18<N>>; 18<64<17<FIB,65<N,2>>,17<FIB,65<N,1>>>>; >; 17<FIB,25>' -vm
2026-06-10T14:09:33-04:00
Running ./build/okin_bench
Run on (4 X 3493.49 MHz CPU s)
CPU Caches:
  L1 Data 32 KiB (x2)
  L1 Instruction 32 KiB (x2)
  L2 Unified 1024 KiB (x2)
  L3 Unified 16384 KiB (x1)
Load Average: 1.06, 0.72, 0.33
-----------------------------------------------------
Benchmark           Time             CPU   Iterations
-----------------------------------------------------
BM_okin_vm   19668391 ns     19640071 ns           36
g0ofycat@workspace:~/projects/okin$
```
