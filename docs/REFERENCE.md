# Copihue Language Reference (v0.20.1)

This document describes the language that `copihue` accepts today. Anything not listed here is not supported.
Planned features are in the README roadmap.

## Source files and compilation

- Source files use the `.lap` extension (from *Lapageria rosea*, the copihue). `.cph` (v0.18.x) and `.ch` (the pre-rename Chronos extension) are still accepted with a deprecation warning.
- A program is a list of `struct` definitions, global `let` declarations, and functions. Hosted programs start at `fn main() -> i32`, whose return value becomes the process exit code (modulo 256).
- `copihue file.lap [-o out]` writes `output.asm` (NASM), assembles it with `nasm -f elf64`, and links it with `ld` into a static ELF64 executable (default `a.out`). There is no libc.
- `-S` writes assembly only. `--freestanding` writes assembly for bare metal (see below). `-O0`/`-O1`/`-O2` select the optimization level. `--flat ADDR` builds a raw bare-metal binary that starts at `fn _start` (see below).
- On error the compiler prints `file:line:col: error: message`, followed by the source line and a caret, and exits with status 1.

## Lexical structure

- Comments: `// to end of line`.
- Identifiers: `[A-Za-z_][A-Za-z0-9_]*`.
- Keywords: `fn let mut if else while for return struct`. `break` and `continue` are statements.
- `packed`, `interrupt`, `syscall`, and `volatile` are contextual words, not keywords. `packed struct`, `interrupt fn`, `syscall fn`, `let volatile`, and `*volatile` use them. A function or variable may still be named `syscall` or `volatile`.
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
| `packed struct Name` | fields laid out at their real size, with no padding |

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

- Functions take up to 16 parameters. The first six are passed in `rdi, rsi, rdx, rcx, r8, r9`. The seventh is at `[rbp+16]`, then one 8-byte slot each. The return value comes back in `rax`.
- A struct parameter (`r: Rect`) is passed by value: the callee gets its own copy. A pointer parameter (`r: *Rect`) refers to the caller's struct; use `r->field`.
- Using an undeclared variable, or assigning to one, is a compile error.
- A direct call of an unknown function is a compile error, and the argument count must match the declaration. A call through a local or a global is indirect and is not checked (see Function pointers).
- Indexing (`x[i]`) requires `x` to be an array or a pointer. Indexing a scalar is a compile error.

### Packed structs

```copihue
packed struct IdtEntry {
    off_lo: u16, sel: u16, ist: u8, typ: u8,
    off_mid: u16, off_hi: u32, zero: u32
}
```

Scalar fields use 1, 2, 4, or 8 bytes and the next field starts immediately after. A pointer field is 8 bytes. A nested struct is embedded at its own size. An x86-64 interrupt gate written this way is 16 bytes. Ordinary `struct` layout is unchanged (8 bytes per scalar field).

A global array of structs can be indexed and its fields assigned (`idt[n].sel = 8`). Packed stores use the field's width, so a `u8` store does not overwrite the next field. Uninitialized global structs and arrays of structs are reserved with `resb` for their real size.

### Volatile globals

```copihue
let volatile ticks: i64 = 0;
```

Every read and every write of a volatile global is a memory operand. The compiler does not fold it or lift it out of a loop, at `-O0` or `-O2`. Use this for a counter that an interrupt handler updates. `*volatile T` and `*mut volatile T` are accepted and mark the pointer variable; they do not change the instruction used to load the pointee. This is not a general memory model.

### Function pointers

There is no `fn(...)` type. `&name`, when `name` is not a local or a global, is the address of that function (an `i64`). Calling a local or a global is indirect (`call rbx`), so this works:

```copihue
let f = &add;
f(20, 22);
set_gate(32, &timer);
```

The address can be stored in an integer, a global, or an array element and called later. An indirect call does not check the argument count.

### Interrupt and syscall functions

```copihue
interrupt fn timer() { ticks = ticks + 1; outb(0x20, 0x20); }
interrupt fn fault(err: i64) { ticks = err; }

syscall fn sys_write(nr: i64, a1: i64, a2: i64, a3: i64, a4: i64, a5: i64) -> i64 {
    return a1;
}
```

`interrupt fn` takes no parameters, or one parameter (the error code the CPU pushed). The prologue saves the general registers and the epilogue ends in `iretq`. `return` leaves through that epilogue. Zero or one parameter is required; a function cannot be both `interrupt` and `syscall`.

`syscall fn` is a `syscall` entry. Arguments arrive in the Linux convention and are presented to the body as ordinary parameters: `nr` from `rax`, then `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9`. At most seven parameters. The prologue switches to the stack last stored by `set_syscall_stack(top)` (the top must be 16-byte aligned; the kernel sets this before any entry) and the epilogue returns with `sysret`. The value in `rax` is the return value. The compiler does not write `STAR`, `LSTAR`, or `EFER`; do that with `wrmsr`.

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

`copihue --freestanding -o out.asm file.lap` emits NASM with no `_start`, no runtime helpers, and no syscalls.
Your own entry code `%include`s the result and calls Copihue functions using the convention above.
The code uses `rax`, `rbx`, `rcx`, `rdx`, `rsi`, `rdi` and `r8`–`r11` freely, so do not assume `rbx` is preserved.
Hosted built-ins are a compile error. These built-ins are available:

| Built-in | Instruction |
|---|---|
| `inb(port)`, `inw(port)`, `inl(port)` | `in al/ax/eax, dx` (zero-extended) |
| `outb(port, v)`, `outw(port, v)`, `outl(port, v)` | `out dx, al/ax/eax` |
| `cli()`, `sti()`, `hlt()`, `pause()`, `swapgs()` | the matching instruction |
| `lidt(ptr)`, `lgdt(ptr)`, `invlpg(ptr)` | `lidt/lgdt/invlpg [rax]`. `ptr` is any address expression, including a local variable |
| `read_cr0()`, `read_cr2()`, `read_cr3()`, `read_cr4()` | `mov rax, crN`. The result is an `i64` |
| `write_cr0(v)`, `write_cr3(v)`, `write_cr4(v)` | `mov crN, rax`. The written value stays in `rax`, so the call is an `i64` expression |
| `rdmsr(msr)` | returns the 64-bit MSR value |
| `wrmsr(msr, value)` | writes a 64-bit MSR |
| `set_syscall_stack(top)` | stack pointer `syscall fn` switches to. Call it with a 16-byte-aligned top before any syscall entry |
| `asm("instr; instr")` | inline NASM. `;` separates instructions; the string must be a literal |
| `bss_start()`, `bss_end()` | address of the first / one past the last zero-initialized global (`--flat` only) |

`read_cr*` and `write_cr*` are a compile error without `--freestanding` or `--flat`. The regression suite assembles them with `nasm` and does not execute them: those instructions fault in Linux user mode, the same way `iretq` and `sysret` do.

Memory-mapped I/O uses typed pointers, for example `let vga: *u16 = 0xB8000; vga[i] = 0x0F00 | 'A';`.

## Flat binaries

`copihue --flat ADDR -o out.bin file.lap` implies `--freestanding` and produces a raw binary (assembled with `nasm -f bin`, `org ADDR`) instead of assembly. Execution starts at the first byte, and the compiler always emits `fn _start` first, wherever it appears in the source; without `_start` compilation fails. `bss_start()` and `bss_end()` bound the zero-initialized globals so `_start` can clear them before calling anything else. `-S` writes the assembly instead of the binary.

`tests/flat/min_kernel.lap` is one such image, loaded at `0x10000`. Its IDT gates use code selector `0x18`, the 64-bit code segment of the AtomicOS bootloader GDT. Selector `0x08` on that loader is a 32-bit code segment; a gate that names it leaves long mode and the image hangs. The binary assumes that GDT is already loaded. It is not a generic bootable kernel.

## Known limitations

- Integer arithmetic is 64-bit two's complement and wraps. Narrow types only affect memory accesses.
- Direct calls check the function name and the argument count. Assignments, argument types, and indirect calls are not type-checked. There is no bounds checking on pointer indexing.
- Struct assignment `a = b` is not supported. Copy fields individually or pass structs by value. Ordinary structs are not packed; nested field access through an ordinary struct still addresses the first 8 bytes of a nested value.
- Indexing a local array inserts a hosted bounds check, so freestanding programs should index globals or pointers.
- There are no modules or imports, no relocatable objects, and no 16-bit or 32-bit codegen. A program is a single file. `interrupt fn`, `syscall fn`, and the `read_cr*` / `write_cr*` builtins can be assembled; running `iretq`, `sysret`, or a move to or from a control register requires a CPU already in ring 0, which the hosted test suite does not do.
- `syscall fn` does not program the syscall MSRs or a TSS. `volatile` covers global scalars only.
