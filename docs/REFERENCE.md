# Copihue Language Reference (v0.27.4)

This document describes the language that `copihue` accepts today. Anything not listed here is not supported.
Planned features are in the README roadmap.

## Source files and compilation

- Source files use the `.lap` extension (from *Lapageria rosea*, the copihue). `.cph` (v0.18.x) and `.ch` (the pre-rename Chronos extension) are still accepted with a deprecation warning.
- A program is a list of `struct` definitions, global `let` declarations, and functions. Hosted programs start at `fn main() -> i32`, whose return value becomes the process exit code (modulo 256).
- `copihue file.lap [more.lap ...] [-o out]` writes one `output.asm` (NASM), assembles it with `nasm -f elf64`, and links it with `ld` into a static ELF64 executable (default `a.out`). There is no libc. Every file is the same namespace. Declarations are visible in the order the files are compiled.
- `import "path.lap";` at the top level compiles that file there. The path is relative to the importing file's directory. An absolute path is kept as written. A file already loaded under the same path string is skipped. `import of 'path' is a cycle` reports a loop. `cannot open import 'path'` reports a missing file. `import` is not a keyword: it is recognized only at the top level when the next token is a string.
- Defining the same top-level function, global, struct, or `data` name twice is `function 'name' is already defined at file:line:col` (or `global`, `struct`, `data`). The earlier site is the one named in the message. `fn name();` followed by a body, or the reverse, is one function. Two forward declarations are allowed. The path is the string the compiler was given, not a canonical real path.
- `-S` writes assembly only. `--freestanding` writes assembly for bare metal (see below). `-O0`/`-O1`/`-O2` select the optimization level. `--flat ADDR` builds a raw bare-metal binary that starts at `fn _start` (see below). `--boot` builds a 512-byte sector at `0x7C00` (see below). `--flat` and `--boot` cannot be combined.
- `-O1` folds constant arithmetic. Hosted `-O2` also rewrites `*`, `/`, and `%` by a power of two. A binary operation whose operands are registers, immediates, or scalar memory is evaluated directly into the destination register (`add r15, r12`, `imul r12, r12, 13`) instead of copying through `rax` and `rbx`. `/` and `%` still use `rax`/`rdx`. The four hottest scalar locals that are live across a call stay in callee-saved `r12`–`r15` (saved in the frame, restored on every `ret`). Up to two more (`r10`, `r11`, caller-saved) hold scalars whose live range contains no call; a fifth name that is live across a call stays in its stack slot. A local or global array whose element size is 1, 2, 4, or 8 is addressed as `base+index*scale` in one instruction. A local stays on the stack if its address is taken, it is `volatile`, it is an array, a pointer, or a struct, the name is declared more than once, or the function contains `asm()`. Pointer indexing is unchanged. If the function starts with `if (param <op> leaf) { return param; }` and `leaf` is an immediate or another register parameter, that compare runs on the incoming register and the return happens before the frame is built. A direct call to another function in the same program places up to six arguments in `rdi`, `rsi`, `rdx`, `rcx`, `r8`, and `r9` when each argument is a scalar the compiler can put in that register. An argument that is itself a call, an indirect call, a struct passed by value, `syscall`, and the hosted builtins keep the push/pop sequence. A function that never addresses `rbp` and never pushes saves its callee-saved registers with `push`/`pop`. One callee-saved temporary holds the left-hand side of an expression such as `f(n - 1) + f(n - 2)` across the call. `if` and `while` jump on the flags when the condition is a comparison of two scalars or of one array element and a scalar, including `&&` and `||`. The comparison does not become 0 or 1. `&&` still skips the right-hand side. A comparison used as a value still materializes 0 or 1. `if carry` is unchanged. The load that comparison performed is reused by the next use of that same element, including `a[j + 1] = a[j]`. A `while (i < lim)` whose previous statement sets `i` to a non-negative constant, whose body only increments `i` by one at the end, and which contains no call and no address-of, skips the local bounds check when every array it indexes with `i` has at least `lim` elements. A shorter array still traps. The inner product `s = s + a[i * n + k] * b[k * n + j]`, with `k` running from 0 to `n`, walks the two rows with running pointers when `i` and `j` are inside `[0, n)`. Otherwise the original loop runs, bounds checks included. A store of a constant that fits the element width is an immediate `mov`, including a constant index. `i = 0; while (i < lim) { a[i] = C; i = i + 1; }` on a local `u8` or `i8` array, with `lim` inside the length, becomes `rep stosb` from element 0 and sets `i = lim`. If `lim` is not a proved constant, a runtime check keeps the original loop as the slow path. A negative limit, or one past the array, is not rewritten. `s = S; i = I; while (i < N) { s = s + A*i + B; i = i + 1; }` is replaced by the closed sum only when `N`, `A`, `B`, `S`, and `I` are constants, every product `A*i` and every partial sum fits in `i64`, and every term is non-negative or every term is non-positive. A sum that can wrap, a mixed-sign term, or `i * i` keeps the loop. A multiply of a register by 3, 5, or 9 is `lea` (`lea rax, [r10+r10*2]`). `*4` and `*8` stay shifts. `*6` stays `imul`. A straight-line run of two or more `x = (x + y) & imm`, with `imm` from 0 through 2147483647 and with neither `x` nor `y` assigned earlier in the run, emits the adds and then the masks. A later statement that reads an earlier destination keeps the original order. The inner product above takes one branch every two steps when `n` is even, and `k` ends equal to `n`. An odd `n` keeps one step per branch. A `while` is not inverted. There is no general inlining, no unrolling of any other loop, and `fib` is not rewritten. A modulo by a constant that is not a power of two stays `idiv`. These rewrites do not delete an arbitrary loop. `--freestanding`, `--flat`, `--boot`, `-O0`, and `bits 16` / `bits 32` do not do this.
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
| `struct Name` | fields at their real width, naturally aligned, nested structs embedded |
| `packed struct Name` | fields at their real size, with no padding |
| `[T; N]` as a field | `N * sizeof(T)` bytes, packed or not |

All arithmetic is done in 64-bit registers. The element type affects loads and stores through arrays, pointers, and struct fields:
loads from unsigned types zero-extend, signed narrow loads sign-extend, and stores truncate to the field's width. Scalar locals and globals always occupy 8 bytes. `sizeof` reports the type's width, not that slot.

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

Scalar fields use 1, 2, 4, or 8 bytes and the next field starts immediately after. A pointer field is 8 bytes. A nested struct is embedded at its own size. An x86-64 interrupt gate written this way is 16 bytes. A 64-bit TSS stays packed: natural alignment would put `rsp0` at offset 8, and the CPU reads it at offset 4.

A global array of structs can be indexed and its fields assigned (`idt[n].sel = 8`). Packed stores use the field's width, so a `u8` store does not overwrite the next field. Uninitialized global structs and arrays of structs are reserved with `resb` for their real size.

### Struct assignment

An ordinary struct uses the same field widths and inserts padding so each field is naturally aligned: 1 for `u8`/`i8`, 2 for `u16`/`i16`, 4 for `u32`/`i32`, 8 for `u64`/`i64` and every pointer. An array field aligns as its element does. The struct's alignment is the strictest of its fields, and the size is rounded up to that alignment. `struct Pad { a: u8, b: u32, c: u16, d: u64 }` is 24 bytes, with `b` at 4, `c` at 8, and `d` at 16. Packed structs do not get this padding. The rule is the C one, because a `u32` device register should not be forced to 8 bytes, and a following `u64` should still be aligned. Hardware records opt out with `packed`.

A field may be `[T; N]`. It occupies `N * sizeof(T)` in both layouts. `T` is an integer, a pointer, or a struct that is already defined. `e.blocks[i] = 4` stores one element at `offsetof(e.blocks) + i * sizeof(T)`. A packed directory entry

```copihue
packed struct FsEnt {
    name: [u8; 16],
    size: u32,
    nblocks: u32,
    blocks: [u32; 8]
}
```

is 56 bytes: `name` at 0, `size` at 16, `nblocks` at 20, `blocks` at 24. The same fields without `packed` are also 56, because 16 is already a multiple of 4. A count that is not a positive integer, an unknown element type, a struct that is not defined yet, or a size that overflows is `cannot lay out field 'name' of type '[Missing; 4]'`. The compiler exits with status 1. It does not pick a smaller size and succeed.

`sizeof(FsEnt)` and `sizeof(e)` are that size. `sizeof(u32)` is 4. `offsetof(FsEnt, size)` is 16. Both are integers known at compile time. `sizeof()` takes one argument; anything it cannot size is `sizeof() has no size for this type`. `offsetof(Struct, field)` takes a struct name and a field name. `offsetof() unknown struct 'Nope'` and `struct 'FsEnt' has no field 'nope'` are the errors. A scalar local is still an 8-byte slot; `sizeof` of its type is the type width.

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

`interrupt fn` takes no parameters, one integer (the CPU error code), or one pointer to a struct that describes the CPU frame. The prologue saves the general registers and the epilogue ends in `iretq`. `return` leaves through that epilogue. More than one parameter is `an interrupt function takes 0 arguments, or 1 (the CPU error code)`. A function cannot be both `interrupt` and `syscall`. The integer form is unchanged: the error code is copied from `[rbp+128]` into that local.

```copihue
struct InterruptFrame { rip: i64, cs: i64, rflags: i64, rsp: i64, ss: i64 }
struct PfFrame { error_code: i64, rip: i64, cs: i64, rflags: i64, rsp: i64, ss: i64 }

interrupt fn irq(frame: *InterruptFrame) { frame.rip = frame.rip; }
interrupt fn pf(frame: *PfFrame) { frame.rip = frame.rip; }
```

The pointer form does not copy an error code into a local. The parameter is the address of the CPU frame, and the epilogue reads that local again, so `frame = other` makes `iretq` return through another saved frame. A store `frame.rip = x` or `frame.rsp = y` changes the qword `iretq` will pop. That is the context switch: the handler never names `rsp` or `rbp`.

In 64-bit mode the CPU always pushes `SS`, `RSP`, `RFLAGS`, `CS`, `RIP`, and an error code when the vector has one. The prologue then pushes `rax rcx rdx rbx rbp rsi rdi r8 r9 r10 r11 r12 r13 r14 r15`. With no error code the frame pointer is `[rbp+120]` (`rip`). When the struct's first field is named `error_code`, the prologue keeps the alignment push and the frame pointer is `[rbp+128]` (the error code, then `rip` at offset 8). The fifteen saved registers occupy the 120 bytes immediately below that first CPU qword in either layout, low address first: `r15 r14 r13 r12 r11 r10 r9 r8 rdi rsi rbp rbx rdx rcx rax`. A `SavedRegs` struct in that order is `(frame as i64 - 120) as *SavedRegs`. The scheduler's saved kernel stack is that address; pointing `frame` at another task's `saved_ksp + 120` is the timer switch.

Each of those frame fields must occupy 8 bytes at offset `0, 8, 16, ...`. Declare them `i64` or `u64`: those are 8 bytes and naturally aligned, so five of them sit on the CPU qwords. A narrower field, packed or not, is `interrupt frame field 'rip' must be an 8-byte field at offset 0`. Too few fields is `interrupt frame 'Tiny' needs 5 fields` (6 when the first field is `error_code`). `*i64` is `an interrupt frame parameter must point at a struct`. A struct passed by value is `an interrupt frame parameter must be a pointer`.

The epilogue drops the error code only for a handler whose frame starts with `error_code`. Pointing that handler at a frame saved by a handler with no error code would skip one qword. A `#PF` handler patches `rip` inside its own frame; a timer handler swaps one no-error frame for another.

`iretq` returns to ring 3 when the saved `CS.RPL` is 3, and to ring 0 when the interrupted code was ring 0. The compiler does not build the first frame and does not emit `swapgs`. The first entry into ring 3 is still a frame the kernel pushes itself and then `iretq()`.

### Context switch

```copihue
struct Ctx {
    rip: i64, rsp: i64, rbp: i64, rbx: i64,
    r12: i64, r13: i64, r14: i64, r15: i64
}

if (save_context(&ctx) == 0) {
    ctx.rip = label_addr(after);
    restore_context(&ctx);
}
label after;
```

`save_context(p)` and `restore_context(p)` are the framed-function context switch. The argument is a name or `&name` (a local, a global, or a data label). Anything else is `save_context() takes the address of a context`. In `bits 16` or `bits 32` the error is `save_context() is a 64-bit context switch`.

The context is eight qwords: `rip`, `rsp`, `rbp`, `rbx`, `r12`, `r13`, `r14`, `r15`. `save_context` stores the live `rsp` and `rbp`, the callee-saved registers, and the address of the next instruction. It returns 0. `restore_context` loads those registers and jumps to the saved `rip`, so the matching `save_context` then returns 1. It does not return to its caller.

This is safe because the compiler, not the user, names `rsp` and `rbp`. In a function with a frame the prologue has already set `rbp`, so the saved pair is that frame and the epilogue's `leave` still matches after a restore. `rbx` and `r12`–`r15` are restored too, so a value hosted `-O2` kept in a callee-saved register survives the other task. The address is a `lea` into `r11` before those registers are read. `r11` is not part of the context, and a bare `&name` does not itself use `rbx`. The builtin is `restore_context`, not `switch_to`: a kernel may already have a function `switch_to(n)` that selects a process.

`label name;` emits that name as a NASM label. `label_addr(name)` is `lea rax, [name]` for a `label`, a function, or a data label. An unknown name is `unknown label 'nope'`. In `bits 16` or `bits 32` it is `label_addr() is a 64-bit address`. Taking the address does not change `rsp`. Storing it over `ctx.rip` and then calling `restore_context` resumes at that label with the saved frame, which is the point a kernel used to mark with `lea rax, [.after_sched]`. Jumping there with a frame other than the one `save_context` captured is the caller's responsibility; the compiler does not insert an adjustment.

`tests/lang/context.lap` runs this under Linux, at `-O0` and `-O2`: one resume lands on a label, then two tasks switch until both have continued, and a local kept across the switch is still 42. `tests/ctx` boots under QEMU. A `#PF` handler rewrites `rip` and `rsp`, an `int 0x80` handler switches to another stack, and `restore_context` comes back. COM1 shows `COPIHUE`.

`syscall fn` is a `syscall` entry. Arguments arrive in the Linux convention and are presented to the body as ordinary parameters: `nr` from `rax`, then `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9`. At most seven parameters. The prologue switches to the stack last stored by `set_syscall_stack(top)` (the top must be 16-byte aligned; the kernel sets this before any entry) and the epilogue returns with `o64 sysret`. NASM has no `sysretq` mnemonic; `o64 sysret` is REX.W SYSRET (encoding `48 0F 07`). It loads RIP from `rcx` and RFLAGS from `r11`, forces ring 3, and takes `CS`/`SS` from `STAR`. The value in `rax` is the return value. With seven parameters the epilogue drops the spacer and the seventh argument with `add rsp, 16` so that drop does not overwrite `rax`. The `syscall` instruction does not consult the TSS; `set_syscall_stack` is the stack switch. The compiler does not write `STAR`, `LSTAR`, `SFMASK`, or `EFER`; do that with `wrmsr`.

### Naked functions and `bits`

```copihue
bits 16 naked fn real_mode() { asm("xor ax, ax"); }
naked bits 32 fn protected() { hlt(); }
naked fn stub64() { cli(); }
```

`bits` and `naked` stand in either order immediately before `fn`. The default, a function with neither word, is 64-bit and emits no `bits` directive, so existing assembly stays the same.

`naked` emits no prologue and no `leave`. `return`, and falling off the end, are a single `ret` (no `xor rax, rax`). It cannot also be `interrupt` or `syscall`. Those two stay 64-bit. A scalar or pointer `let` is a labeled slot next to the function (`__loc_name_N: dd 0` in `bits 16` / `bits 32`, `dq 0` otherwise). The store is `mov [__loc_name_N], eax` or `mov [__loc_name_N], rax`. It is not a stack frame, so it does not depend on `ss` or `sp`. An array or a struct local is `a naked function cannot have local variables`.

A naked function may take parameters. Nothing is copied into a slot. The name is the register the caller already used. 64-bit code, and a naked function with no `bits`, uses `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`, at most six. That is the same order a Copihue call already uses, so `naked fn inc(n)` can return `n + 1` and a hosted caller sees the result. `bits 32` uses `eax`, `ecx`, `edx`, `esi`, at most four. `bits 16` uses `ax`, `cx`, `dx`, `si`, at most four. `bx`, `ebx`, and `rbx` stay scratch and are not parameter registers. `bx` and `bp` (and `ebx`, `ebp`, `esi`, `edi`) are naked lvalues in `bits 16` and `bits 32`; a 64-bit function still cannot name them, because `rbx` is the scratch and `rbp` is the frame. `ax` and `eax` are also the accumulator, so any later expression overwrites a parameter that lives there. One parameter past the limit is `a naked function takes at most 6 register parameters` (4 in `bits 16` or `bits 32`). A struct parameter is `a naked parameter must be an integer or a pointer`. Taking the address is `naked parameter 'n' has no address`: there is no slot to point at.

These names are registers only inside `naked`, and only when no local or global has that name:

| Name | Width | Notes |
|---|---|---|
| `ds` `es` `ss` `fs` `gs` | 16 | segment. A write is `mov seg, ax`. A read is `mov ax, seg` and then a zero-extend |
| `al` `ah` `dl` | 8 | `ah` is the high byte |
| `ax` `sp` `si` `di` | 16 | `si` and `di` are also accepted in 64-bit naked code |
| `bx` `bp` | 16 | `bits 16` and `bits 32` only. `register 'bx' is not available in bits 64` |
| `eax` `ecx` `edx` `esp` | 32 | |
| `ebx` `ebp` `esi` `edi` | 32 | `bits 16` and `bits 32` only. `register 'ebx' is not available in bits 64` |
| `rax` `rbx` `rsp` `rbp` `rdi` `rsi` `rdx` | 64 | rejected in `bits 16` and `bits 32`. `rdi`/`rsi`/`rdx` are also the first naked parameters |

`cs` is not in the list. Outside `naked` the error is `register 'ax' is only available in a naked function`. A normal function's code generator uses these registers, and writing one would corrupt the frame. An `interrupt fn` is not naked, so it cannot name `rbp`. The frame pointer parameter is how that function reads and changes the saved `rip` and `rsp`. `far_jump` is the way a function that has a frame reloads `CS` without naming a register. A 64-bit name in `bits 16` or `bits 32` is `register 'rax' is not available in bits 16`. A local or a global of the same spelling is that variable: `let ax = 7` in a normal function prints 7. A global whose name is a NASM register (`ax:`, `sp:`) is still that variable in Copihue, and NASM then rejects the label, so a global should not use these names. A local does not become a label.

An immediate writes the register directly. `ah = 0x0E` is `mov ah, 14` and leaves `al` alone. `sp = 0x7C00` is `mov sp, 31744`. Expression literals are decimal; the jump form below keeps the source spelling. Any other right-hand side is computed in the accumulator first, and that clears the other bytes of `eax`/`rax`. A segment write always takes that path and then emits `mov ds, ax`, so `ds = 0` is still `mov eax, 0` / `mov ds, ax`. Reading `ds` cannot be `movzx eax, ds` (NASM: invalid combination). It is `mov ax, ds` and then `movzx eax, ax` or `movzx rax, ax`.

`jmp 0x8200` emits `jmp 0x8200`. `jmp other` emits `jmp other` when `other` is a function or a data label in this assembly unit. `jmp(...)` is still a call. An unknown name is `unknown jump target 'name'` and the `jmp` is still emitted. A direct jump is allowed outside `naked`. In a boot sector (`org 0x7C00`) `jmp 0x8200` is a 16-bit near jump: from the instruction after `E9`, the signed displacement lands on `0x8200`.

`jmp rax` and `jmp [resume_rip]` are indirect, and only inside `naked`. NASM treats `jmp rax` as a register jump, not as a label, so outside `naked` the compiler reports `an indirect jmp is only available in a naked function` and does not emit the instruction. A framed function has no business discarding `rbp`. The register names are the same general registers `push` accepts (`ax` through `r15`, not the segment registers). The width has to match the mode: `jmp rax` in `bits 16` is `register 'rax' is not available in bits 16`. `jmp [name]` reads an address from a global, a `data` label, or a naked local slot. The size prefix is the mode's address width: `jmp qword [resume_rip]` in 64-bit code, `jmp dword [name]` in `bits 32`, `jmp word [name]` in `bits 16`. That is the form AtomicOS used as `asm("jmp qword [resume_rip]")` in `resume_shell`. A name that is not memory is `jmp [name] takes a variable or a data label`, and nothing is emitted. `bx` and `ebx` are the scratch of a binary operator, so a value left there does not survive `a + b`. `bp` and `ebp` are safe only because `naked` has no frame.

`*(0x1000 as *u32) = 0x2003` and `*(rbp + 136) = x` are stores whose address is not a bare name. `*p = v` for a named pointer stays the older store, including a `*volatile` pointee. The width is the cast's pointee: `u8`/`i8` one byte, `u16`/`i16` two, `u32`/`i32` four, `u64`/`i64` eight. With no cast the store is 4 bytes in `bits 16` and `bits 32`, and 8 bytes otherwise. A number, a usable naked register, or `reg+disp` / `reg-disp` is one NASM memory operand (`mov dword [4096], eax`, `mov [rbp+136], rax`). Anything else is evaluated, pushed, and the value is stored through `ebx` or `rbx`. A literal that does not fit the pointee is `expected u8, found integer literal 300`. An 8-byte load or store in `bits 16` or `bits 32` is `an 8-byte store is not available in bits 16` (or `load`). A load of one or two bytes zero-extends. A 32-bit load is `mov eax`, which clears the upper half of `rax`. A signed 8-bit or 16-bit cast is not sign-extended here: `bits 16` has no `movsx rax`, and the zero-extending form is the one that assembles in every mode. `rbp` is readable in a 64-bit naked function, so `rax = *(rbp + 128)` is `mov rax, [rbp+128]`.

`far_jump(0x18)` is the 64-bit far return. `jmp 24:label` is not an encoding in long mode. The builtin pushes the selector literal, `lea`s the address of the next instruction, pushes that address, and executes `retfq`. Execution continues at the label. The two pushes are popped, so a frame that was already on the stack is unchanged. `rax` is clobbered. The selector is one integer literal. In `bits 16` or `bits 32` the error is `far_jump() is a 64-bit far return; use ljmp(selector, label) in bits 16 or 32`. Hosted code is `far_jump() is only available with --freestanding, --flat, or --boot`. `retfq()` takes no arguments and emits `retfq`, or `retf` in `bits 16` and `bits 32`. `ljmp` is unchanged and still emits `jmp sel:label`, which NASM accepts in 16-bit and 32-bit code.

`bits 16` and `bits 32` require `naked`. The ordinary frame uses `rbp` and `rax`, which nasm rejects in 16-bit mode (`instruction not supported in 16-bit mode`). Inside those functions the compiler evaluates integers in `eax` / `ebx` / `ecx` / `edx`, so `while (1)`, `if (e)`, `inb`, `outb`, `read_cr0`, and `wrmsr` assemble. After a 16-bit or 32-bit function the compiler emits `bits 64`, so the next function is 64-bit again. `bits 64` emits that one directive before the function and no extra restore after it.

A call from `bits 16` or `bits 32` uses the same registers as a naked parameter, not `push rax` / `pop rdi`. `push rax` has no encoding in `bits 16`, and a 64-bit `ret` would pop a return address the 16-bit `call` did not push. Every function in these modes is naked, so the callee already reads `ax`, `cx`, `dx`, `si` or `eax`, `ecx`, `edx`, `esi`. The caller evaluates each argument into the accumulator, pushes that width (`push ax` or `push eax`), and pops into those registers in reverse. At most four arguments: `a bits 32 call takes at most 4 register arguments`. The callee has to be the same bits mode. `a bits 16 call cannot target a 64-bit function` and `a bits 16 call cannot target a bits 32 function` are the two mismatches. The `call` is near and relative. In `bits 16` the displacement is 16 bits, so the callee must sit within ±32KiB or nasm rejects the object; both functions share `CS`, and a far call is not emitted. An indirect call has no name to check, so `an indirect call is not available in bits 16`. The result is left in `ax` or `eax`, which is also the accumulator: the callee must copy the first argument out of `ax`/`eax` before the next expression if it still needs it. A 64-bit call is unchanged (`push rax` / `pop rdi`).

### Loose instructions, `extern`, and `syscall()`

`lodsb()`, `stosb()`, `stosd()`, `stc()`, `clc()`, `cld()`, `std()`, and `ud2()` emit that one instruction. `rep_stosb()`, `rep_stosw()`, `rep_stosd()`, `rep_stosq()`, `rep_movsb()`, `rep_movsw()`, `rep_movsd()`, and `rep_movsq()` emit `rep` and the string instruction (`rep stosd`, `rep movsb`). `rep_stosq()` and `rep_movsq()` in `bits 16` or `bits 32` are `rep_stosq() is not available in bits 16`. None of these take arguments. `lodsb` reads `ds:si` or `rsi` into `al` and steps the index. `stosb` writes `al` at `es:di` or `rdi`. `stosd` writes `eax`. `rep` repeats on `cx` or `rcx`. `cld()` clears DF and `std()` sets it. They take no arguments and assemble in `bits 16`, `bits 32`, and 64-bit code. `rep` still uses whatever DF is at the instruction.

`pushfd()` and `popfd()` are `pushfd`/`popfd` in `bits 16` and `bits 32`, and `pushfq`/`popfq` in 64-bit code.

`push(ax)` and `pop(rbx)` name the register. The names are `ax` `bx` `cx` `dx` `si` `di` `bp` `sp`, the `e` and `r` forms of those, `r8` through `r15`, and `ds` `es` `ss` `fs` `gs`. A 64-bit name in `bits 16` is `register 'rax' cannot be pushed in bits 16`. A 16-bit or 32-bit name in 64-bit code is rejected the same way: long mode has no `push eax`. Segments are accepted in every mode. `push(0x33)` is `push 51`. Any other expression is evaluated and then `push rax` (or `push eax`). That evaluation uses the accumulator. `pop(1)` is `pop() takes a register name`. Push and pop are not naked-only. Popping `rbp` in a function that has a frame drops the frame on the floor.

`extern __syscall_rsp;` is a qword with no storage of its own. Reading it is `mov rax, [__syscall_rsp]`. Naming `__syscall_rsp` or `__syscall_user_rsp` makes the compiler emit both quads, which is the same pair `set_syscall_stack` defines. Any other `extern` name needs a definition in this assembly file. `data answer { dq 42; }` is that definition, and the two declarations may come in either order. `let saved_ksp: i64 = 0` is the other form: the NASM label is the Copihue name (`saved_ksp: dq 0`). `extern` of a name that already has a `let` is `extern 'x' is already defined`.

`syscall()` in `--freestanding`, `--flat`, or `--boot` with no arguments is the one instruction `syscall`. The caller has already set `rax`, `rdi`, `rsi`, and `rdx`. `syscall(n, a1, a2, a3, a4, a5, a6)` loads those registers and then executes `syscall`: `rax` is the number, then `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9`. The result stays in `rax`. Six arguments after the number is the maximum (`syscall() takes at most 6 arguments`). A bare `syscall()` stays one instruction, so a program that filled the registers itself does not gain extra moves. In `bits 16` or `bits 32` it is `syscall() is a 64-bit instruction`. Hosted `syscall(n, a1, a2, ...)` is the same register load plus `syscall`, and that helper is still the Linux path. It is not what `--freestanding` calls.

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

Scalar locals and globals still occupy 8-byte slots when they live in memory. Struct fields occupy their real width. Hosted `-O2` may keep a scalar local in `r12`–`r15`, or in `r10`/`r11` when no call is in its live range; the mask or sign-extend is applied before the value is written to that register or to the slot. A struct field, a store through a pointer, and an array element write only the real width, which is the truncation. A load of a `u8`/`u16`/`u32` field zero-extends. A load of an `i8`/`i16`/`i32` field sign-extends. The arithmetic that follows is still 64-bit.

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

A 64-bit task-state segment is 104 bytes. It has to be a `packed struct`: an ordinary struct aligns `u64` to 8, so `rsp0` would land at offset 8 instead of 4.

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

`tests/boot/boot_sector.lap` prints `Copihue` with BIOS teletype (`int(0x10)`, AH=`0x0E`) and far-jumps to a 32-bit stub that writes `COPIHUE` to port `0x3F8`. The sector installs its own GDT: selector `0x08` is 32-bit code and `0x10` is data, both base 0. That is not the AtomicOS loader GDT (`0x18` is 64-bit code there). The far jump is the 16-bit form `EA ip16 sel`; with `org 0x7C00` the IP is the label's low 16 bits, and the protected-mode segment has base 0, so execution continues at that linear address. `tests/boot/jmp_sp.lap` stays in real mode: it sets `ds`, `es`, `ss`, and `sp`, jumps to a label, and writes `COPIHUE`. `tests/boot/call16.lap` calls another `bits 16` function, sets `bp` and `bx`, and `jmp word [land]` continues at `hue`. `tests/boot/call32.lap` enters protected mode, calls a `bits 32` function through `eax`, and `jmp dword [land]` continues at `hue`. `jmp 0x8200` is assembled after the halt loop and is not taken. Those sectors do not enter long mode. A page table is larger than the free space in the sector; it has to be written at some other physical address. `tests/usermode` is the image that does: its sector loads a kernel at `0x8000`, and that kernel's `bits 32 naked fn _start` builds the page tables and far-jumps to 64-bit code.

The regression suite compares the hexdump at `-O0` and `-O2`. When `qemu-system-x86_64` is on `PATH`, it boots the image (`-display none -boot a`, floppy, raw) and requires `COPIHUE` on COM1. That check is skipped, not failed, when QEMU is absent. CI installs the `qemu-system-x86` package.

## Known limitations

- Integer arithmetic is 64-bit two's complement and wraps. Storing into an 8, 16, or 32-bit variable or field truncates as described under Type checking. Operators themselves are not 8-bit or 32-bit. There is no bounds checking on pointer indexing.
- Direct calls check the function name, the argument count, and argument types. Assignments and integer or pointer returns are checked. Indirect calls are not. `*i8` and `*u8` match. An integer literal converts to any pointer; a variable does not, without `as`.
- Struct fields use their real width. Ordinary structs add natural alignment; `packed struct` does not. An array field is `N * sizeof(T)` or a compile error. `sizeof` and `offsetof` are compile-time integers. Operators are still 64-bit. Scalar locals and globals still occupy 8-byte slots. Struct assignment copies the whole value; it does not assign field by field in the source. Nested fields (`r.b.x`) use the real offset.
- Indexing a local array in hosted mode inserts a bounds check that exits the process. `--freestanding` and `--flat` omit that check. Pointer indexing is never checked.
- Several files are one assembly unit and one namespace. There is no per-file object, no linker of Copihue objects, and no visibility. `bits 16` and `bits 32` exist only as naked functions. A scalar or pointer `let` there is a slot in the image; an array or a struct local is rejected. Named registers are readable and writable only inside `naked`. `bx`, `bp`, `ebx`, `ebp`, `esi`, and `edi` are naked lvalues in `bits 16` and `bits 32`, and rejected in 64-bit code. An `interrupt fn` cannot name `rbp`; it receives a pointer to the CPU frame instead. `save_context` / `restore_context` capture `rsp`, `rbp`, the callee-saved registers, and a resume address without naming those registers. Integer expressions in `bits 16` and `bits 32` use `eax`. A call in those modes uses the naked parameter registers (`ax`/`cx`/`dx`/`si` or `eax`/`ecx`/`edx`/`esi`), at most four, and only a callee of the same bits mode. `jmp reg` and `jmp [name]` are naked-only. The hosted suite assembles `iretq`, `sysret`, `ltr`, and the 64-bit control-register moves and does not execute them; it does execute `cpuid` and `int_carry`. The multi-file tests run as a Linux process. `tests/lang/context.lap` runs the framed switch as a Linux process. `tests/ctx` runs a `#PF` handler and an IRQ handler that rewrite the frame, under QEMU. The boot-sector tests execute real mode and protected mode under QEMU, including `lgdt(&gdtr)`, `write_cr0`, `if carry`, and `outb` on `tests/boot/realmode_builtins.lap`, `ds = 0` plus a local on `tests/boot/seg_local.lap`, and `sp` plus `jmp` on `tests/boot/jmp_sp.lap`. `tests/boot/call16.lap` and `tests/boot/call32.lap` call another function and jump through memory. `tests/freestanding/bits32_regs.lap` assembles `write_cr4`, `rdmsr` / `wrmsr`, and `cpuid` in `bits 32` and is not booted. `tests/freestanding/naked_local.lap` assembles the slot and the segment write and is not booted. `tests/freestanding/naked_regs.lap` assembles the named registers, the sized store, `jmp 0x8200`, and `far_jump` / `retfq`, and is not booted. `tests/freestanding/insns.lap` assembles `lodsb`, `rep stos`, `push`/`pop`, `syscall`, and a load of `__syscall_rsp`, and is not booted. `tests/freestanding/jmp_call.lap` assembles the indirect jump, the bits 16/32 call, and `syscall` with arguments, and is not booted. `tests/boot/stc_carry.lap` executes `stc` and `if carry` under QEMU. `tests/lang/naked_param.lap` runs the 64-bit register parameters. `tests/lang/extern_data.lap` runs a load of an `extern` qword. `tests/usermode` executes long mode, `ltr`, the syscall MSRs, `iretq` to ring 3, `syscall`/`sysret`, and `iretq` from an interrupt, also under QEMU.
- `syscall fn` does not program `STAR`, `LSTAR`, `SFMASK`, or `EFER`, and it does not emit `swapgs`. The kernel does that with `wrmsr` and `asm`. The `syscall` stack is `set_syscall_stack`, not `TSS.rsp0`. `volatile` is per access, as described above. It is not a memory model. `let volatile` on a local scalar keeps the stack slot at hosted `-O2`.
