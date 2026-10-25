# Copihue Language Reference (v0.22.1)

This document describes the language that `copihue` accepts today. Anything not listed here is not supported.
Planned features are in the README roadmap.

## Source files and compilation

- Source files use the `.lap` extension (from *Lapageria rosea*, the copihue). `.cph` (v0.18.x) and `.ch` (the pre-rename Chronos extension) are still accepted with a deprecation warning.
- A program is a list of `struct` definitions, global `let` declarations, and functions. Hosted programs start at `fn main() -> i32`, whose return value becomes the process exit code (modulo 256).
- `copihue file.lap [-o out]` writes `output.asm` (NASM), assembles it with `nasm -f elf64`, and links it with `ld` into a static ELF64 executable (default `a.out`). There is no libc.
- `-S` writes assembly only. `--freestanding` writes assembly for bare metal (see below). `-O0`/`-O1`/`-O2` select the optimization level. `--flat ADDR` builds a raw bare-metal binary that starts at `fn _start` (see below). `--boot` builds a 512-byte sector at `0x7C00` (see below). `--flat` and `--boot` cannot be combined.
- On error the compiler prints `file:line:col: error: message`, followed by the source line and a caret, and exits with status 1.

## Lexical structure

- Comments: `// to end of line`.
- Identifiers: `[A-Za-z_][A-Za-z0-9_]*`.
- Keywords: `fn let mut if else while for return struct`. `break` and `continue` are statements.
- `packed`, `interrupt`, `syscall`, `volatile`, `bits`, `naked`, and `data` are contextual words, not keywords. `packed struct`, `interrupt fn`, `syscall fn`, `let volatile`, `*volatile`, `bits 16 naked fn`, and `data name { ... }` use them. A function or variable may still be named `syscall`, `volatile`, or `data`.
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
- The type after `->` may be any type: a name (`i64`, `Point`), a pointer (`*u8`, `*mut u8`, `*volatile mut u8`, `**u8`), or an array (`[u8; 4]`, `[*u8; 2]`). `volatile` and `mut` may appear in either order after `*`. A missing piece is `expected a type, found '...'`. The return type is not checked against `return`, and it does not change the calling convention.
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

### Struct assignment

`a = b` copies one struct value onto another of the same type. The copy is exactly the struct's size: whole qwords, then a byte tail, so a packed record does not spill into the next field or array element. It works for locals, globals, array elements (`table[i] = other`), and nested fields (`box.point = p`). A different struct type is a compile error. Nested scalar access (`box.point.x`) addresses that field, not the first eight bytes of the parent.

Passing a struct to a function is still a copy into the callee's frame. There is no move, and the two objects are independent after the assignment.

### Volatile globals

```copihue
let volatile ticks: i64 = 0;
```

Every read and every write of a volatile global is a memory operand. The compiler does not fold it or lift it out of a loop, at `-O0` or `-O2`. Use this for a counter that an interrupt handler updates.

`*volatile T`, `*volatile mut T`, and `*mut volatile T` make accesses through that pointer volatile. `p[i] = v`, `*p = v`, and `*p` load or store the pointee's real width (`u8`/`u16`/`u32`/`u64`) and the assembly is marked `  ; volatile`. A struct field may be declared `volatile name: T`; reads and writes of that field use the same mark and the field's width, including `ptr->field`. `let volatile` on a struct or an array is a compile error: mark the fields, or point at the object with `*volatile T`.

What is guaranteed: each source-level read or write of a volatile global, a `*volatile` pointee, or a volatile field emits a memory instruction of that width. `-O2` does not delete it, fold it, or lift it out of a loop. The compiler has no other reordering pass. What is not guaranteed: this is not a C memory model. There are no atomics and no fence stronger than the instruction itself. A non-volatile access next to it can still be simplified. `let volatile` on a local scalar (not a `*volatile` pointer) does not change codegen.

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

### Naked functions and `bits`

```copihue
bits 16 naked fn real_mode() { asm("xor ax, ax"); }
naked bits 32 fn protected() { hlt(); }
naked fn stub64() { cli(); }
```

`bits` and `naked` stand in either order immediately before `fn`. The default, a function with neither word, is 64-bit and emits no `bits` directive, so existing assembly stays the same.

`naked` emits no prologue and no `leave`. `return`, and falling off the end, are a single `ret` (no `xor rax, rax`). A naked function takes no parameters and cannot contain `let`. It cannot also be `interrupt` or `syscall`. Those two stay 64-bit.

`bits 16` and `bits 32` require `naked`. The ordinary frame uses `rbp` and `rax`, which nasm rejects in 16-bit mode (`instruction not supported in 16-bit mode`). After a 16-bit or 32-bit function the compiler emits `bits 64`, so the next function is 64-bit again. `bits 64` emits that one directive before the function and no extra restore after it.

### Raw data

```copihue
data gdt {
    dq 0;
    code32:
    dw 0xFFFF;
    dw 0;
    db 0;
    db 0x9A;
    db 0xCF;
    db 0;
    align 8;
    times 2 dd 0;
}

data banner { db "Copihue", 13, 10, 0; }
```

`data name { ... }` emits the label and the directives in the text stream. Functions and data blocks stay in source order. In `--flat` and `--boot`, `fn _start` is moved to the front and the rest follows source order, so a `data` block written above `_start` lands between `_start` and the next function. Directives end with `;`.

| Directive | Emitted |
|---|---|
| `db`, `dw`, `dd`, `dq` | that NASM directive. A string expands to decimal bytes, so a `;` inside the text is data. Escapes are `\n \t \r \0 \\ \" \'`. An integer literal is copied as written (`0xFFFF` stays hex). A name is a label (`dd gdt`) |
| `times N db\|dw\|dd\|dq value` | `times N dir value` |
| `align N` | `align N, db 0`. The fill is zero, including inside a code section, so a GDT is not padded with `NOP` |
| `label:` | a NASM label |

`asm("...")` still splits on `;`. It cannot express `db "text"`. Use a `data` block for a GDT, a disk address packet, or a message.

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
| `ljmp(selector, label)` | `jmp sel:name`. `selector` is an integer literal and `label` is a name, not `&label`. `ljmp(0x08, pm)` emits `jmp 8:pm` |
| `int(n)` | `int N`, one integer literal. `int(0x10)` emits `int 16`, which encodes `CD 10` |
| `bss_start()`, `bss_end()` | address of the first / one past the last zero-initialized global (`--flat` only) |

`read_cr*` and `write_cr*` are a compile error without `--freestanding` or `--flat`. `ljmp` and `int` are a compile error without `--freestanding`, `--flat`, or `--boot`. A non-literal `int(n)` is `int(n) takes one integer literal`. The regression suite assembles the control-register moves with `nasm` and does not execute them: those instructions fault in Linux user mode, the same way `iretq` and `sysret` do.

`read_cr*` and `write_cr*` emit `mov rax, crN` and `mov crN, rax`. That form is 64-bit. In `bits 16` or `bits 32`, use `asm`, for example `asm("mov eax, cr0")` and `asm("mov cr0, eax")`.

Memory-mapped I/O uses typed pointers. A device register should be `*volatile`, so the access is not removed:

```copihue
let dev: *volatile u32 = 0xFE000000;
dev[0] = 1;
while (dev[1] == 0) { pause(); }
```

## Flat binaries

`copihue --flat ADDR -o out.bin file.lap` implies `--freestanding` and produces a raw binary (assembled with `nasm -f bin`, `org ADDR`) instead of assembly. Execution starts at the first byte, and the compiler always emits `fn _start` first, wherever it appears in the source; without `_start` compilation fails. `bss_start()` and `bss_end()` bound the zero-initialized globals so `_start` can clear them before calling anything else. `-S` writes the assembly instead of the binary.

`tests/flat/min_kernel.lap` is one such image, loaded at `0x10000`. Its IDT gates use code selector `0x18`, the 64-bit code segment of the AtomicOS bootloader GDT. Selector `0x08` on that loader is a 32-bit code segment; a gate that names it leaves long mode and the image hangs. The binary assumes that GDT is already loaded. It is not a generic bootable kernel. The image also indexes a local array, copies a packed struct, writes a `*volatile u32`, and reads CR2. Indexing a local array in `--freestanding` or `--flat` does not insert the hosted bounds trap.

## Boot sectors

`copihue --boot -o boot.bin file.lap` implies `--freestanding` and `org 0x7C00`. The output file is the sector image (`nasm -f bin`), not assembly and not an ELF. `-S` writes the assembly instead.

The file starts with `org 0x7C00` and the body. There is no leading `bits 64` and no `.data` or `.bss` section in front of the code: either one would move the entry off the first byte or assemble the prologue in the wrong mode. `_start` is emitted first and must be `bits 16 naked fn`. A missing `_start` is `--boot needs a function named _start (the entry point)`. A `_start` that is not `bits 16 naked fn` is `--boot _start must be 'bits 16 naked fn' (real mode, no 64-bit frame)`.

`data` blocks stay in source order after `_start`, still in the text stream, so labels such as `dd gdt` resolve. The compiler then appends:

```
times 510-($-$$) db 0
dw 0xAA55
```

Those two lines are emitted after `bits 64`. `dw` does not depend on the bit mode. Leaving them under `bits 16`, next to a 64-bit prologue, is what makes nasm report `instruction not supported in 16-bit mode`. The code and the data must fit in 510 bytes; the image is then exactly 512 bytes and the last two are `55 aa`. An uninitialized `let` global is not copied into the sector. Put constants in `data`.

`tests/boot/boot_sector.lap` prints `Copihue` with BIOS teletype (`int(0x10)`, AH=`0x0E`) and far-jumps to a 32-bit stub that writes `COPIHUE` to port `0x3F8`. The sector installs its own GDT: selector `0x08` is 32-bit code and `0x10` is data, both base 0. That is not the AtomicOS loader GDT (`0x18` is 64-bit code there). The far jump is the 16-bit form `EA ip16 sel`; with `org 0x7C00` the IP is the label's low 16 bits, and the protected-mode segment has base 0, so execution continues at that linear address. The example does not enter long mode. A page table is larger than the free space in the sector; it has to be written at some other physical address. `bits 64 naked fn` is the form of the long-mode stub.

The regression suite compares the hexdump at `-O0` and `-O2`. When `qemu-system-x86_64` is on `PATH`, it boots the image (`-display none -boot a`, floppy, raw) and requires `COPIHUE` on COM1. That check is skipped, not failed, when QEMU is absent. CI installs the `qemu-system-x86` package.

## Known limitations

- Integer arithmetic is 64-bit two's complement and wraps. Narrow types only affect memory accesses.
- Direct calls check the function name and the argument count. Assignments, argument types, return types, and indirect calls are not type-checked. There is no bounds checking on pointer indexing.
- Ordinary struct fields still occupy 8 bytes each. Only `packed struct` uses the real width of `u8`/`u16`/`u32`. There is no padding or alignment attribute beyond packed-or-not. Struct assignment copies the whole value; it does not assign field by field in the source. Nested fields (`r.b.x`) use the real offset.
- Indexing a local array in hosted mode inserts a bounds check that exits the process. `--freestanding` and `--flat` omit that check. Pointer indexing is never checked.
- There are no modules or imports and no relocatable objects. A program is a single file. `bits 16` and `bits 32` exist only as naked functions: there is no 16-bit or 32-bit frame, and `read_cr*` / `write_cr*` are not valid in those modes. `interrupt fn`, `syscall fn`, and the control-register builtins can be assembled; running `iretq`, `sysret`, or a move to or from a control register requires a CPU already in ring 0, which the hosted test suite does not do. The boot-sector test does execute real mode and protected mode under QEMU when QEMU is installed.
- `syscall fn` does not program the syscall MSRs or a TSS. `volatile` is per access, as described above. It is not a memory model, and `let volatile` on a local scalar does not change codegen.
