# Copihue Language Reference (v0.18)

This document describes the language that `copihuec` accepts today. Anything not listed here is not supported.
Planned features are in the README roadmap.

## Source files and compilation

- Source files use the `.cph` extension. `.ch` (the pre-rename Chronos extension) is accepted with a warning.
- A program is a list of `struct` definitions, global `let` declarations, and functions. Hosted programs start at `fn main() -> i32`, whose return value becomes the process exit code (modulo 256).
- `copihuec file.cph [-o out]` writes `output.asm` (NASM), assembles it with `nasm -f elf64`, and links it with `ld` into a static ELF64 executable (default `a.out`). There is no libc.
- `-S` writes assembly only. `--freestanding` writes assembly for bare metal (see below). `-O0`/`-O1`/`-O2` select the optimization level.
- On error the compiler prints `file:line:col: error: message`, followed by the source line and a caret, and exits with status 1.

## Lexical structure

- Comments: `// to end of line`.
- Identifiers: `[A-Za-z_][A-Za-z0-9_]*`.
- Keywords: `fn let mut if else while for return struct`. `break` and `continue` are statements.
- Integer literals: decimal `42`, hex `0xFF`, binary `0b1010`, with optional `_` separators (`1_000_000`).
- Character literals: `'A'`, and escapes `'\n' '\t' '\r' '\0' '\\' '\''`. Their value is an integer.
- String literals: `"..."` with the escapes `\n \t \r \0 \\ \" \'`. A string literal's value is a pointer to NUL-terminated bytes in the data section.

## Types

| Type | Size in memory |
|---|---|
| `i8`, `u8` | 1 byte |
| `i16`, `u16` | 2 bytes |
| `i32`, `u32` | 4 bytes |
| `i64`, `u64` | 8 bytes |
| `*T` | pointer, 8 bytes |
| `[T; N]` | fixed array of N elements of T |
| `struct Name` | fields laid out in order, 8 bytes per scalar field, nested structs embedded |

All arithmetic is done in 64-bit registers. The element type affects loads and stores through arrays and pointers:
loads from unsigned types zero-extend, and stores truncate. Scalar locals and globals always occupy 8 bytes.

## Declarations

```copihue
let x = 5;                 // local or global, 64-bit
let mut y: i64 = 0;        // `mut` is accepted; all variables are currently mutable
let buf: [i8; 64];         // array (locals and globals)
let p: *u16 = 0xB8000;     // pointer
let v = Point { x: 1, y: 2 };

struct Point { x: i64, y: i64 }
struct Rect { a: Point, b: Point }        // nested structs are embedded

fn add(a: i64, b: i64) -> i64 { return a + b; }
```

- Functions take up to 6 parameters. They are passed in `rdi, rsi, rdx, rcx, r8, r9` and the return value comes back in `rax`.
- A struct parameter (`r: Rect`) is passed by value: the callee gets its own copy. A pointer parameter (`r: *Rect`) refers to the caller's struct; use `r->field`.
- Using an undeclared variable is a compile error.
- Indexing (`x[i]`) requires `x` to be an array or a pointer. Indexing a scalar is a compile error.

## Statements

`let`, assignment (`x = e`, `a[i] = e`, `s.f = e`, `p->f = e`), compound assignment (`+= -= *= /= %=`), `x++`, `x--`,
`if (c) { } else if (d) { } else { }`, `while (c) { }`, `for (init; cond; step) { }`, `break;`, `continue;`, `return e;`, and expression statements (calls).
In a `for` loop, `step` can be any simple statement (`i++`, `i += 2`, `i = i * 2`). `continue` jumps to `step`.

## Operators (highest precedence first)

| Operators | Notes |
|---|---|
| `x[i]` `s.f` `p->f` `f(...)` | postfix |
| `-x` `!x` `~x` `*p` `&x` | unary |
| `*` `/` `%` | signed; `/` and `%` truncate toward zero |
| `+` `-` | |
| `<<` `>>` | `>>` is arithmetic (sign-propagating) |
| `&` | |
| `^` | |
| `\|` | |
| `==` `!=` `<` `>` `<=` `>=` | signed comparison, result 0 or 1 |
| `&&` | short-circuit |
| `\|\|` | short-circuit |

As in Rust, bitwise operators bind tighter than comparisons: `x & 2 == 2` means `(x & 2) == 2`.

## Built-ins (hosted mode)

| Built-in | Meaning |
|---|---|
| `print(s)`, `println(s)` | write a string literal or a NUL-terminated buffer to stdout (`println` adds `\n`) |
| `print_int(n)` | write a signed decimal integer |
| `strlen(s)`, `strcmp(a, b)`, `strcpy(dst, src)` | C-style string helpers |
| `malloc(n)`, `free(p)` | memory from `mmap`/`munmap` |
| `open(path, flags[, mode])`, `read(fd, buf, n)`, `write(fd, buf, n)`, `close(fd)` | Linux syscalls; default mode is `0644` |
| `syscall6(nr, a1, ..., a6)` | any Linux syscall |
| `exit(code)` | terminate the process |

## Freestanding mode

`copihuec --freestanding -o out.asm file.cph` emits NASM with no `_start`, no runtime helpers, and no syscalls.
Your own entry code `%include`s the result and calls Copihue functions using the convention above.
The code uses `rax`, `rbx`, `rcx`, `rdx`, `rsi`, `rdi` and `r8`–`r11` freely, so do not assume `rbx` is preserved.
Hosted built-ins are a compile error. These built-ins are available:

| Built-in | Instruction |
|---|---|
| `inb(port)`, `inw(port)`, `inl(port)` | `in al/ax/eax, dx` (zero-extended) |
| `outb(port, v)`, `outw(port, v)`, `outl(port, v)` | `out dx, al/ax/eax` |
| `cli()`, `sti()`, `hlt()`, `pause()` | the matching instruction |
| `asm("instr; instr")` | inline NASM. `;` separates instructions; the string must be a literal |

Memory-mapped I/O uses typed pointers, for example `let vga: *u16 = 0xB8000; vga[i] = 0x0F00 | 'A';`.

## Known limitations

- Integer arithmetic is 64-bit two's complement and wraps. Narrow types only affect memory accesses.
- There is no type checking of assignments or call arguments yet, and no bounds checking on pointer indexing.
- Struct assignment `a = b` is not supported. Copy fields individually or pass structs by value.
- There are no modules or imports. A program is a single file.
