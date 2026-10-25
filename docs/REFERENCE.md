# Copihue Language Reference (v0.25.0)

This document describes the language that `copihue` accepts today. Anything not listed here is not supported.
Planned features are in the README roadmap.

## Source files and compilation

- Source files use the `.lap` extension (from *Lapageria rosea*, the copihue). `.cph` (v0.18.x) and `.ch` (the pre-rename Chronos extension) are still accepted with a deprecation warning.
- A program is a list of `struct` definitions, global `let` declarations, and functions. Hosted programs start at `fn main() -> i32`, whose return value becomes the process exit code (modulo 256).
- `copihue file.lap [more.lap ...] [-o out]` writes one `output.asm` (NASM), assembles it with `nasm -f elf64`, and links it with `ld` into a static ELF64 executable (default `a.out`). There is no libc. Every file is the same namespace. Declarations are visible in the order the files are compiled.
- `import "path.lap";` at the top level compiles that file there. The path is relative to the importing file's directory. An absolute path is kept as written. A file already loaded under the same path string is skipped. `import of 'path' is a cycle` reports a loop. `cannot open import 'path'` reports a missing file. `import` is not a keyword: it is recognized only at the top level when the next token is a string.
- Defining the same top-level function, global, struct, or `data` name twice is `function 'name' is already defined at file:line:col` (or `global`, `struct`, `data`). The earlier site is the one named in the message. `fn name();` followed by a body, or the reverse, is one function. Two forward declarations are allowed. The path is the string the compiler was given, not a canonical real path.
- `-S` writes assembly only. `--freestanding` writes assembly for bare metal (see below). `-O0`/`-O1`/`-O2` select the optimization level. `--flat ADDR` builds a raw bare-metal binary that starts at `fn _start` (see below). `--boot` builds a 512-byte sector at `0x7C00` (see below). `--flat` and `--boot` cannot be combined.
- `-O1` folds constant arithmetic. Hosted `-O2` also rewrites `*`, `/`, and `%` by a power of two, evaluates a simple right-hand side into `rbx` instead of `push`/`pop`, and keeps the four hottest scalar locals in `r12`–`r15` (saved in the frame, restored on every `ret`). A local stays on the stack if its address is taken, it is `volatile`, it is an array, a pointer, or a struct, the name is declared more than once, or the function contains `asm()`. `--freestanding`, `--flat`, `--boot`, and `-O0` do not do this. There is still no loop simplification.
- On error the compiler prints `file:line:col: error: message`, followed by the source line and a caret, and exits with status 1.

## Lexical structure

- Comments: `// to end of line`.
- Identifiers: `[A-Za-z_][A-Za-z0-9_]*`.
- Keywords: `fn let mut if else while for return struct`. `break` and `continue` are statements.
- `packed`, `interrupt`, `syscall`, `volatile`, `bits`, `naked`, `data`, and `import` are contextual words, not keywords. `packed struct`, `interrupt fn`, `syscall fn`, `let volatile`, `*volatile`, `bits 16 naked fn`, `data name { ... }`, and `import "path.lap";` use them. A function or variable may still be named `syscall`, `volatile`, `data`, or `import`.
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
- The type after `->` may be any type: a name (`i64`, `Point`), a pointer (`*u8`, `*mut u8`, `*volatile mut u8`, `**u8`), or an array (`[u8; 4]`, `[*u8; 2]`). `volatile` and `mut` may appear in either order after `*`. A missing piece is `expected a type, found '...'`. An integer or pointer `return` is checked against that type (see Type checking). An array or struct return type is parsed and is not checked. The value still comes back in `rax`. The same form compiles in hosted, `--freestanding`, and `--flat` code.
- A name is visible from its `let` to the end of the block that contains it. `{ ... }` is a block, and so is the body of `if`, `while`, and `for`. A nested block may declare the same name again (the inner one hides the outer one until the inner block ends). Declaring it twice in the same block is `redeclaration of 'name' in the same block`. Parameters belong to the function body, so `fn f(b: i64) { let b = 1; ... }` is that error. `for (let i = 0; ...)` scopes `i` to the loop. Using a name after its block, with no outer declaration, is `use of undeclared variable 'name'`.
- A struct parameter (`r: Rect`) is passed by value: the callee gets its own copy. A pointer parameter (`r: *Rect`) refers to the caller's struct; use `r->field`.
- Using an undeclared variable, or assigning to one, is a compile error.
- A direct call of an unknown function is a compile error, and the argument count must match the declaration (`function 'f' takes N argument(s), not M`). When the count matches, each argument is checked against the parameter type. A call through a local or a global is indirect and is not checked (see Function pointers).
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

What is guaranteed: each source-level read or write of a volatile global, a `*volatile` pointee, or a volatile field emits a memory instruction of that width. `-O2` does not delete it, fold it, or lift it out of a loop. The compiler has no other reordering pass. What is not guaranteed: this is not a C memory model. There are no atomics and no fence stronger than the instruction itself. A non-volatile access next to it can still be simplified. `let volatile` on a local scalar keeps that local in its stack slot when hosted `-O2` puts other scalars in registers. At `-O0`, and with `--freestanding`, `--flat`, or `--boot`, every local is already on the stack.

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

In 64-bit mode the CPU's interrupt frame is `RIP`, `CS`, `RFLAGS`, `RSP`, `SS`, and the epilogue leaves that frame intact. `iretq` therefore returns to ring 3 when the saved `CS.RPL` is 3, and to ring 0 when the interrupted code was ring 0. The compiler does not build that frame and does not emit `swapgs`. The first entry into ring 3 is a frame the kernel pushes itself and then `iretq()`.

`syscall fn` is a `syscall` entry. Arguments arrive in the Linux convention and are presented to the body as ordinary parameters: `nr` from `rax`, then `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9`. At most seven parameters. The prologue switches to the stack last stored by `set_syscall_stack(top)` (the top must be 16-byte aligned; the kernel sets this before any entry) and the epilogue returns with `o64 sysret`. NASM has no `sysretq` mnemonic; `o64 sysret` is REX.W SYSRET (encoding `48 0F 07`). It loads RIP from `rcx` and RFLAGS from `r11`, forces ring 3, and takes `CS`/`SS` from `STAR`. The value in `rax` is the return value. With seven parameters the epilogue drops the spacer and the seventh argument with `add rsp, 16` so that drop does not overwrite `rax`. The `syscall` instruction does not consult the TSS; `set_syscall_stack` is the stack switch. The compiler does not write `STAR`, `LSTAR`, `SFMASK`, or `EFER`; do that with `wrmsr`.

### Naked functions and `bits`

```copihue
bits 16 naked fn real_mode() { asm("xor ax, ax"); }
naked bits 32 fn protected() { hlt(); }
naked fn stub64() { cli(); }
```

`bits` and `naked` stand in either order immediately before `fn`. The default, a function with neither word, is 64-bit and emits no `bits` directive, so existing assembly stays the same.

`naked` emits no prologue and no `leave`. `return`, and falling off the end, are a single `ret` (no `xor rax, rax`). A naked function takes no parameters and cannot contain `let`. It cannot also be `interrupt` or `syscall`. Those two stay 64-bit.

`bits 16` and `bits 32` require `naked`. The ordinary frame uses `rbp` and `rax`, which nasm rejects in 16-bit mode (`instruction not supported in 16-bit mode`). Inside those functions the compiler evaluates integers in `eax` / `ebx` / `ecx` / `edx`, so `while (1)`, `if (e)`, `inb`, `outb`, `read_cr0`, and `wrmsr` assemble. A naked function still cannot contain `let`, and it still takes no parameters. `ds = 0` is not an assignment. A call from `bits 16` or `bits 32` to another Copihue function is still the 64-bit `push rax` / `call` sequence. After a 16-bit or 32-bit function the compiler emits `bits 64`, so the next function is 64-bit again. `bits 64` emits that one directive before the function and no extra restore after it.

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
| `db`, `dw`, `dd`, `dq` | that NASM directive. A string expands to decimal bytes, so a `;` inside the text is data. Escapes are `\n \t \r \0 \\ \" \'`. An integer literal is copied as written (`0xFFFF` stays hex). A name is a label (`dd gdt`). A label and an integer may be added or subtracted (`dq page + 3`, `dq page - 8`, `dq page + 0x83`); NASM evaluates the address. `1 * 2` is not an operand |
| `times N db\|dw\|dd\|dq value` | `times N dir value` |
| `align N` | `align N, db 0`. The fill is zero, including inside a code section, so a GDT is not padded with `NOP` |
| `label:` | a NASM label |

`asm("...")` still splits on `;`. It cannot express `db "text"`. Use a `data` block for a GDT, a disk address packet, or a message.

## Statements

`let`, assignment (`x = e`, `a[i] = e`, `s.f = e`, `p->f = e`), compound assignment (`+= -= *= /= %=`), `x++`, `x--`,
a block `{ ... }`, `if (c) { } else if (d) { } else { }`, `if carry { } else { }`, `while (c) { }`, `for (init; cond; step) { }`, `break;`, `continue;`, `return e;`, and expression statements (calls).
`if carry` has no parentheses. It branches on CF (`jnc` skips the body). `if (carry)` is still a call. CF must still be set: `println` and most arithmetic clear it before a later `int_carry()`.
In a `for` loop, `step` can be any simple statement (`i++`, `i += 2`, `i = i * 2`). `continue` jumps to `step`.

## Operators (highest precedence first)

| Operators | Notes |
|---|---|
| `x[i]` `s.f` `p->f` `f(...)` | postfix |
| `-x` `!x` `~x` `*p` `&x` | unary |
| `e as T` | cast. Looser than unary and postfix, tighter than `*` `/` `+`. `as` is not a keyword |
| `*` `/` `%` | signed; `/` and `%` truncate toward zero |
| `+` `-` | |
| `<<` `>>` | `>>` is arithmetic (sign-propagating) |
| `&` | |
| `^` | |
| `\|` | |
| `==` `!=` `<` `>` `<=` `>=` | signed comparison, result 0 or 1 |
| `&&` | short-circuit |
| `\|\|` | short-circuit |

As in Rust, bitwise operators bind tighter than comparisons: `x & 2 == 2` means `(x & 2) == 2`. `1 + 2 as u8` is `1 + (2 as u8)`, and `&e as *u8` is `(&e) as *u8`.

## Type checking

A direct assignment (`let`, `x = e`, `a[i] = e`, `s.f = e`, `p->f = e`), a direct call, and `return` check integer and pointer types. A mismatch is `file:line:col: error: expected T, found U`, with the source line and a caret. A call says `argument N of 'f': expected T, found U` (N starts at 1). A return says `return value: expected T, found U`. The compiler keeps going and ends with `1 error, compilation failed` or `N errors, compilation failed`.

`as` converts on purpose. It is allowed between any two types. A cast to a pointer or to `i64` / `u64` emits no instruction. A cast to `i8` / `u8` / `i16` / `u16` / `i32` / `u32` keeps the low bits: unsigned zero-extends (`300 as u8` is 44), signed sign-extends (`200 as i8` is -56). That explicit cast is not a range error.

These implicit conversions stay legal, because kernels and the existing programs rely on them:

- An integer literal, including `0` and `0xB8000`, converts to any pointer. `let vga: *u16 = 0xB8000` needs no `as`. A variable that is an integer does not: `let n = 1; let p: *u8 = n` is an error, and `let p: *u8 = n as *u8` is the fix.
- `*i8` and `*u8` are the same byte pointer. `*u8` and `*Point` are not, and neither are `*u8` and `**u8`, or `*i32` and `*i64`.
- `mut` and `volatile` are not part of the comparison. `*mut u8`, `*volatile u8`, and `*u8` match.
- A narrower integer promotes to a wider one (`u8` passed to `putc(c: i64)`, `return` of a `u8` from `-> i64`).
- A pointer plus or minus an integer has that pointer's type and may also be used as `i64`. An array decays to a pointer to its element and may not be used as `i64` without `as`. `&function` is `i64`, not a pointer. `&struct` is `*Struct` and does not become `*u8` without `as`.
- A string literal is `*u8`. `malloc` returns an address that converts to any pointer and to an integer. `bss_start()` and `bss_end()` are `*u8`; `bss_end() - bss_start()` is `i64`.
- `read` / `write` buffers, `free`, `lidt`, `lgdt`, and `invlpg` accept an integer or any pointer. `syscall` and `syscall6` arguments are raw register values, so a pointer or an integer is accepted.

A literal that does not fit the destination is an error: `expected u8, found integer literal 300`. The ranges are `u8` 0..255, `i8` -128..127, `u16` 0..65535, `i16` -32768..32767, `u32` 0..4294967295, `i32` -2147483648..2147483647. `-1` does not fit an unsigned type. `0xFF` fits `u8` and does not fit `i8`. `i64` accepts any integer literal; a negative literal does not fit `u64`.

Storing a non-literal into an 8, 16, or 32-bit variable or field truncates. Unsigned keeps the low bits (zero-extend). Signed sign-extends. `let wide = 300; let z: u8 = wide` stores 44. A narrow parameter is truncated the same way when it is received. A literal that fits is stored as it is, with no extra instruction. Arithmetic operators are still 64-bit; the truncation happens at the store, at a narrow parameter, and at an explicit `as` to a narrow integer. At `-O1` and `-O2`, constant folding turns `1 + 2` into a literal, so a folded sum that does not fit is an error at those levels and a truncating store at `-O0`.

Scalar locals, globals, and ordinary struct fields still occupy 8-byte slots when they live in memory. Hosted `-O2` may keep a scalar local in `r12`–`r15`; the mask or sign-extend is applied before the value is written to that register or to the slot. A `packed` field, a `volatile` field, and a store through a pointer or an array element already write only the real width, which is the truncation.

Not checked: indirect calls, comparisons, bitwise mixes, `mut` used as a permission, `volatile` as a type error, and array or struct return types. An untyped `let x = e` infers `i64` from an integer, including a narrow load, the pointer type from a pointer, and an address from `malloc`. It does not infer a struct onto that slot: write `let p: Point = ...` or a struct literal. An expression the compiler cannot type is accepted.

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
| `lidt(ptr)`, `lgdt(ptr)`, `invlpg(ptr)` | `lidt/lgdt/invlpg [rax]` in 64-bit code. `ptr` is any address expression, including a local variable. `&name` or `name` may be a `data` label (`lgdt(&gdtr)`). In `bits 16` / `bits 32` a label is `lgdt [gdtr]`; another address is `lgdt [bx]` or `lgdt [ebx]` |
| `read_cr0()`, `read_cr2()`, `read_cr3()`, `read_cr4()` | `mov rax, crN` in 64-bit code. In `bits 16` and `bits 32`, `mov eax, crN`. There is no `mov ax, crN` |
| `write_cr0(v)`, `write_cr3(v)`, `write_cr4(v)` | `mov crN, rax` in 64-bit code, `mov crN, eax` in `bits 16` / `bits 32`. The written value stays in the accumulator |
| `rdmsr(msr)` | in 64-bit code, returns the 64-bit MSR (`shl rdx, 32` / `or rax, rdx`). In `bits 16` / `bits 32`, leaves the low half in `eax` and the high half in `edx` |
| `wrmsr(msr, value)` | in 64-bit code, writes a 64-bit value. In `bits 16` / `bits 32`, writes `edx:eax`. ORing a small integer into `rdmsr(m)` keeps the high half; any other value is written with `edx = 0` |
| `cpuid(leaf)` | `cpuid`, returns `eax`. Also allowed in hosted code. `cpuid()` with no argument is `cpuid(leaf) takes 1 argument` |
| `cpuid_edx(leaf)` | same instruction, returns `edx` (long-mode bit is `1 << 29` of leaf `0x80000001`) |
| `int_carry()` | `setc`; 1 if CF is set, else 0. Also allowed in hosted code. `if carry` is the branch form |
| `ltr(selector)` | `ltr ax`. `selector` is the TSS selector (for example `0x28`), not the address of the TSS |
| `iretq()` | `iretq`, for a frame the kernel pushed itself. An `interrupt fn` already ends with this instruction |
| `sysretq()` | `o64 sysret` (NASM's spelling of `sysretq`). `rcx` must be the user RIP and `r11` the user RFLAGS. A `syscall fn` already ends with this instruction |
| `set_syscall_stack(top)` | stack pointer `syscall fn` switches to. Call it with a 16-byte-aligned top before any syscall entry |
| `asm("instr; instr")` | inline NASM. `;` separates instructions; the string must be a literal |
| `ljmp(selector, label)` | `jmp sel:name`. `selector` is an integer literal and `label` is a name, not `&label`. `ljmp(0x08, pm)` emits `jmp 8:pm` |
| `int(n)` | `int N`, one integer literal. `int(0x10)` emits `int 16`, which encodes `CD 10` |
| `bss_start()`, `bss_end()` | address of the first / one past the last zero-initialized global (`--flat` only) |

`read_cr*` and `write_cr*` are a compile error without `--freestanding` or `--flat`. `ljmp`, `int`, `ltr`, `iretq`, and `sysretq` are a compile error without `--freestanding`, `--flat`, or `--boot`. A non-literal `int(n)` is `int(n) takes one integer literal`. `ltr()` with no argument is `ltr(selector) takes 1 argument`. The hosted regression suite assembles the privileged instructions with `nasm` and does not execute them.

### TSS and user syscall

A 64-bit task-state segment is 104 bytes. It has to be a `packed struct`: an ordinary struct stores every field in an 8-byte slot, so `rsp0` would not land at offset 4.

```copihue
packed struct Tss {
    reserved0: u32,   // 0
    rsp0: u64,        // 4   ring 0 stack on an interrupt from ring 3
    rsp1: u64,        // 12
    rsp2: u64,        // 20
    reserved1: u64,   // 28
    ist1: u64,        // 36  then ist2..ist7, 8 bytes each
    ist2: u64,
    ist3: u64,
    ist4: u64,
    ist5: u64,
    ist6: u64,
    ist7: u64,
    reserved2: u64,   // 92
    reserved3: u16,   // 100
    iomap: u16        // 102  set to 104 when there is no I/O bitmap
}

fn set_rsp0(p: *Tss, sp: i64) -> i64 {
    p->rsp0 = sp;
    return p->rsp0;
}
```

`set_rsp0(&tss, new_sp)` is the context-switch write. It stores eight bytes at offset 4 and does not touch the IST slots. The GDT entry that describes this memory is a 16-byte system descriptor (type `0x89`, limit 103). `ltr` reads that descriptor; it does not take the TSS address. Build the descriptor, `lgdt` the table, then `ltr(selector)`.

Syscall MSRs, written with the existing `rdmsr` / `wrmsr`:

| MSR | Address | Role |
|---|---|---|
| `IA32_EFER` | `0xC0000080` | set bit 0 (`SCE`) without clearing `LME` (bit 8) |
| `IA32_STAR` | `0xC0000081` | `SYSCALL` CS is bits 47:32; `SYSRET` SS is bits 63:48 plus 8, CS plus 16, RPL forced to 3 |
| `IA32_LSTAR` | `0xC0000082` | RIP of the `syscall fn` |
| `IA32_FMASK` | `0xC0000084` | RFLAGS bits cleared on `syscall` (`SFMASK`) |

`tests/flat/user_syscall.lap` is that sequence plus `ltr` and `set_rsp0`, assembled at `-O0` and `-O2`. It is not executed. `tests/usermode/kernel.lap` is the same sequence in a kernel QEMU boots from `tests/usermode/boot.lap`. The 2MB identity map in that kernel is marked user-accessible so ring 3 can fetch its instructions. COM1 shows `USER`: a syscall, an `int 0x80` (iretq back to ring 3), a syscall that stores a new `rsp0`, and a second `int 0x80` whose frame is on that new stack. `ltr`, `iretq`, `sysret`, and the MSR writes run on that QEMU CPU. The freestanding and flat goldens only assemble them.

`read_cr*` and `write_cr*` emit `mov rax, crN` and `mov crN, rax` in a 64-bit function. In `bits 16` or `bits 32` they emit `mov eax, crN` and `mov crN, eax`. `outb(0x92, inb(0x92) | 2)`, `write_cr4(read_cr4() | 32)`, and `wrmsr(0xC0000080, rdmsr(0xC0000080) | 0x100)` assemble in the mode of the function that contains them. `if carry` after `int(0x13)` is the disk-error branch. `cpuid(0x80000000)` and `cpuid_edx(0x80000001)` are the long-mode check.

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

`tests/boot/boot_sector.lap` prints `Copihue` with BIOS teletype (`int(0x10)`, AH=`0x0E`) and far-jumps to a 32-bit stub that writes `COPIHUE` to port `0x3F8`. The sector installs its own GDT: selector `0x08` is 32-bit code and `0x10` is data, both base 0. That is not the AtomicOS loader GDT (`0x18` is 64-bit code there). The far jump is the 16-bit form `EA ip16 sel`; with `org 0x7C00` the IP is the label's low 16 bits, and the protected-mode segment has base 0, so execution continues at that linear address. The example does not enter long mode. A page table is larger than the free space in the sector; it has to be written at some other physical address. `tests/usermode` is the image that does: its sector loads a kernel at `0x8000`, and that kernel's `bits 32 naked fn _start` builds the page tables and far-jumps to 64-bit code.

The regression suite compares the hexdump at `-O0` and `-O2`. When `qemu-system-x86_64` is on `PATH`, it boots the image (`-display none -boot a`, floppy, raw) and requires `COPIHUE` on COM1. That check is skipped, not failed, when QEMU is absent. CI installs the `qemu-system-x86` package.

## Known limitations

- Integer arithmetic is 64-bit two's complement and wraps. Storing into an 8, 16, or 32-bit variable or field truncates as described under Type checking. Operators themselves are not 8-bit or 32-bit. There is no bounds checking on pointer indexing.
- Direct calls check the function name, the argument count, and argument types. Assignments and integer or pointer returns are checked. Indirect calls are not. `*i8` and `*u8` match. An integer literal converts to any pointer; a variable does not, without `as`.
- Ordinary struct fields still occupy 8 bytes each. Only `packed struct` uses the real width of `u8`/`u16`/`u32`. There is no padding or alignment attribute beyond packed-or-not. Struct assignment copies the whole value; it does not assign field by field in the source. Nested fields (`r.b.x`) use the real offset.
- Indexing a local array in hosted mode inserts a bounds check that exits the process. `--freestanding` and `--flat` omit that check. Pointer indexing is never checked.
- Several files are one assembly unit and one namespace. There is no per-file object, no linker of Copihue objects, and no visibility. `bits 16` and `bits 32` exist only as naked functions: there is no 16-bit or 32-bit frame, so `let` is rejected and segment registers cannot be assigned. Integer expressions there use `eax`. Calls to other Copihue functions still use the 64-bit sequence. The hosted suite assembles `iretq`, `sysret`, `ltr`, and the 64-bit control-register moves and does not execute them; it does execute `cpuid` and `int_carry`. The multi-file tests run as a Linux process. The boot-sector tests execute real mode and protected mode under QEMU, including `lgdt(&gdtr)`, `write_cr0`, `if carry`, and `outb` on `tests/boot/realmode_builtins.lap`. `tests/freestanding/bits32_regs.lap` assembles `write_cr4`, `rdmsr` / `wrmsr`, and `cpuid` in `bits 32` and is not booted. `tests/usermode` executes long mode, `ltr`, the syscall MSRs, `iretq` to ring 3, `syscall`/`sysret`, and `iretq` from an interrupt, also under QEMU.
- `syscall fn` does not program `STAR`, `LSTAR`, `SFMASK`, or `EFER`, and it does not emit `swapgs`. The kernel does that with `wrmsr` and `asm`. The `syscall` stack is `set_syscall_stack`, not `TSS.rsp0`. `volatile` is per access, as described above. It is not a memory model. `let volatile` on a local scalar keeps the stack slot at hosted `-O2`.
