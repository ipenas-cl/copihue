<p align="center"><img src="docs/brand/pixel/copihue-pixel-wordmark-dark.png" alt="Copihue" width="420"></p>

<h1 align="center">Copihue</h1>

<p align="center">
<a href="https://github.com/ipenas-cl/copihue/actions/workflows/ci.yml"><img src="https://github.com/ipenas-cl/copihue/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
<a href="https://github.com/ipenas-cl/copihue/releases/latest"><img src="https://img.shields.io/github/v/release/ipenas-cl/copihue" alt="Release"></a>
<a href="LICENSE"><img src="https://img.shields.io/github/license/ipenas-cl/copihue" alt="License"></a>
</p>

A systems programming language with its own compiler, written in C with an x86-64 NASM back end. It produces Linux x86-64 ELF executables **with no libc and no runtime**, and it can also produce freestanding code for kernels and boot sectors. *Formerly Chronos.*

Current version: **0.45.3** (`compiler/bootstrap-c/copihue.c`, `LANG_VERSION`). Release notes are in [CHANGELOG.md](CHANGELOG.md).

> **Status: not production-ready.** It is a tested compiler for a small but real language. Memory safety is partial (checked slices and indexes, `own *T` moves) and there is no borrow checker: a `vec` slice can dangle after a push. Linux x86-64 only. The goal is a language that beats C++, Rust and OCaml; today it does not, and the numbers below say where it stands.

## Install

Requirements: Linux x86-64, `gcc`, `make`. Building a program, or a `-c` object, needs no `nasm` and no `ld`: the compiler writes the ELF file itself. `nasm` and `ld` (binutils) are needed for a program with `asm("...")` or a `naked`, `interrupt` or `syscall` function, for `--freestanding -c`, `--flat` and `--boot`, for `--nasm`, and by `make test`.

```bash
sudo apt-get install -y gcc make nasm binutils
git clone https://github.com/ipenas-cl/copihue.git && cd copihue
make            # builds ./copihue
make test       # regression suite
make install    # to ~/.local/bin (PREFIX=/usr/local to change)
```

## Hello world

```bash
cat > hello.lap <<'LAP'
fn main() -> i32 {
    println("Hello, Copihue!");
    print_int(6 * 7);
    println("");
    return 0;
}
LAP
copihue hello.lap -o hello && ./hello
# Hello, Copihue!
# 42
```

```
copihue [-O0|-O1|-O2] [-o out] [-S] [-c] [--nasm | --native] [--freestanding | --flat ADDR [--pic | --axe1 | --axe2] [--sizes] | --boot | --pe-efi] [-v] file.lap [more.lap ...]
```

`-S` writes NASM assembly only. `-c` writes one ELF64 relocatable object (link with `ld` or `gcc`). `--freestanding`, `--flat` and `--boot` are for kernels (see [docs/KERNELS.md](docs/KERNELS.md)). `--flat 0x400000 --axe1` writes the one-page AXE1 file (16-byte header plus image) and `--flat 0x400000 --axe2` the AXE2 file (32-byte header plus image, data on the page after the text) and `--sizes` prints the text, data and bss sizes; `align(4096) fn f() { ... }` puts a function on a page boundary (see [docs/REFERENCE.md](docs/REFERENCE.md), "AXE2 images" and "Alignment"). The value returned by `main` is the exit code.

**The compiler writes the ELF itself.** A hosted program, and a `-c` object, is assembled by the compiler's own assembler and ELF writer (`compiler/native`): no `nasm`, no `ld`, and no `output.asm` or `output.o` left on disk (the assembly goes from memory to the assembler; `-S` is still the way to see it). The executable carries `.symtab`, `.strtab` and `.shstrtab` with minimal section headers, so `readelf -S -s` and `objdump -d` show the functions and data labels by name. A program the built-in assembler does not take (`asm("...")`, a `naked`, `interrupt` or `syscall` function, a function with `bits 16` or `bits 32`) is built with `nasm` and `ld` instead, and the compiler says so on stderr: `copihue: warning: the built-in assembler does not take this program (line 5: asm("...") (user assembly)); falling back to nasm and ld`. `--freestanding -c` is `nasm`'s too, without a warning.

- `--nasm` always uses `nasm -f elf64` and `ld`, as before the built-in back end; it leaves `output.asm` and `output.o`.
- `--native` is the default, but strict: it refuses what the built-in assembler does not take (the error names the construct and its line) instead of falling back. It is accepted with `-c`; with `-S`, `--freestanding`, `--flat`, `--boot` or `--elf` it is an error. Scripts that pass `--native` keep working.
- `--native` and `--nasm` cannot be combined.

The `-c` object is `ET_REL` with `.text`, `.rodata`, `.data`, `.bss` (those it has), `.rela.*`, `.symtab`, `.strtab` and `.shstrtab`; `extern` names are undefined global symbols. It is compared with `nasm -f elf64`'s object of the same text in `make native-test` (bytes, relocations, symbols) and, on Linux, linked and run with `ld` and `gcc`. It carries no `.note.GNU-stack` section, like `nasm`'s. Status and what is verified: [docs/DESIGN_NATIVE.md](docs/DESIGN_NATIVE.md).

## What the language has

Two levels of syntax. The **easy level** lets the compiler decide: an omitted parameter type or `->` is `i64`, `let x = e` infers the type, `vec T` grows by itself and frees at the end of the block, `own *T` is an owning pointer that moves on `return` or when passed on. The **explicit level** gives the developer the width of every integer, `*volatile T`, `align(N)`, `packed struct`, atomics, `naked` and `interrupt` functions, and named sections (`section "name" @ addr`). Design: [docs/DESIGN_TWO_LEVELS.md](docs/DESIGN_TWO_LEVELS.md).

- Integers `i8` to `i64` and `u8` to `u64`, with arithmetic at the width of the type, and checked assignments (a wider value needs `as`).
- Microsoft x64 convention (0.44.6), to call the UEFI firmware or be called by it: `ms64 fn f(...)` defines or declares a function that receives `rcx, rdx, r8, r9` and the stack above 32 bytes of shadow space and keeps `rdi`, `rsi`, `rbx`; `let f: ms64 fn = ...;` (also a global and a parameter) is a function address whose calls pass `rcx, rdx, r8, r9`, then `[rsp+32]`, `[rsp+40]`, ..., reserve the shadow space, and align `rsp` to 16 at the `call`. Integers and pointers only (a struct, a slice, or an enum is an error). `naked ms64 fn` is an assembly routine with that convention. Hosted, `--nasm`, `--freestanding`, `--flat`, `-O0` and `-O2`. See the [reference](docs/REFERENCE.md#microsoft-x64-convention-ms64).
- Position-independent flat images (0.44.7), for a UEFI stub that the firmware loads at an address unknown at build time: `copihue --flat 0x1000 --pic` reaches globals, strings, `data` blocks and function addresses (`&f`, `ms64 fn` included) relative to RIP (`lea reg, [rel label]`, `default rel`), so the same bytes run at any address. `&block`, `&global` and `&local` (an output parameter) work in it. What would need a relocation (a global that holds the address of a string or of another global, `dd label` in a `data` block, a `bits 16/32` function, `section ... @ addr`, a label used as an immediate in `asm()`) is an error with its line and column. `--pic` needs `--flat` and cannot be combined with `--boot`, `--elf`, `--axe1` or `--axe2`. See the [reference](docs/REFERENCE.md#position-independent-images---pic).
- UEFI applications (0.44.8): `copihue --pe-efi -o BOOTX64.EFI app.lap` writes a PE32+ file (x86-64, subsystem 10 EFI application, `.text`, `.data`, `.bss` and `.reloc` on their own pages) that the firmware loads and runs; it replaces the hand-written PE header around a `--flat 0x1000` stub. It is `--flat 0x1000 --pic` plus the header, so the `--pic` rules apply, and the entry is `ms64 fn _start(image: i64, st: *u64) -> i64` (`rcx` the image handle, `rdx` the system table; the result is the EFI_STATUS). Refused with `--flat`, `--boot`, `--elf`, `--axe1`, `--axe2`, `-c`, `--sizes` and `--native`; `-S` writes the assembly. Checked by a C reader of the headers, by 17 broken-on-purpose headers, and by QEMU with OVMF. See the [reference](docs/REFERENCE.md#uefi-applications---pe-efi).
- Assignment targets: a store is never dropped in silence. `mem(8)[0] = 5;` with `fn mem(off: i64) -> *volatile u32` stores four bytes, volatile, through the pointer the call returns, as it would through a pointer variable; `f(x) = 5`, `3 = x`, `-x = 5` and the like are compile errors with the line. A store into a wider field or element computes `a + b` at the destination's width ([REFERENCE](docs/REFERENCE.md), Assignment targets).
- Named integer constants: `const N = 8;` at module or function level, usable in `[T; N]` and in any expression, folded at compile time ([REFERENCE](docs/REFERENCE.md), Constants).
- Global initializers: a constant integer expression (`let p: i64 = 0 - 1;`, `let m: u32 = 1 << 31;`, `let k: i64 = N * 4 + 2;`) is folded into `.data`; anything else is an error on the `let` ([REFERENCE](docs/REFERENCE.md), Initializers of globals).
- Slices `[]T` with bounds-checked indexing in hosted code, `for x in s`, `unsafe { }`, and `str` as a `[]u8` ([DESIGN_SLICES](docs/DESIGN_SLICES.md), [DESIGN_STRINGS](docs/DESIGN_STRINGS.md)).
- Structs and slices as values (0.45.0): `fn f() -> Point`, `-> str` and `-> []T` return the value through a destination the caller passes in `rdi` (the enum convention), and `str` / `[]T` can be struct fields (`struct Tok { kind: i64, text: str, line: i32 }`) with `.len`, checked `[i]`, `==`, assignment and whole-struct copies. Returning a view of the callee's own frame (a local array, a local `vec`, `&local`) is a compile error where the compiler can see it. Before 0.45.0 a returned struct was the address of the callee's dead frame. Details: [docs/DESIGN_SELFHOST_S1.md](docs/DESIGN_SELFHOST_S1.md).
- No silent memory corruption from struct arguments and local arrays (0.45.1): a struct argument is passed by the address of its bytes in every form (a literal is built in a temporary of the caller, not over its last local; a field, a global, `*q`, `q->a` no longer pass their first 8 bytes as the address); `let a: [u8; 3] = [97, 98, 99]` stores each element at its own width; `&a[i]` of a local array is checked against its length and steps by the element size; `p->f[i]` reads the element (it crashed the compiler). The forms that are not supported are compile errors that say what to write instead. See the [reference](docs/REFERENCE.md#passing-a-struct) and [local arrays](docs/REFERENCE.md#local-arrays-and-ai).
- Modules with `pub` and `name::item`, several files per command, `import`, and `-c` objects ([DESIGN_MODULES](docs/DESIGN_MODULES.md)).
- Enums with data and exhaustive `match`, `Option<T>`, `Result<T, E>` and `?` ([DESIGN_ENUMS](docs/DESIGN_ENUMS.md), [DESIGN_ERRORS](docs/DESIGN_ERRORS.md)).
- Generics by monomorphization and traits without vtables ([DESIGN_GENERICS](docs/DESIGN_GENERICS.md), [DESIGN_TRAITS](docs/DESIGN_TRAITS.md)).
- Atomics, `fence`, and hosted threads (`thread_spawn`, `thread_join`, `sched_setaffinity`) without libc.
- Struct rules, below: the bound of an array of structs, the fields a literal does not name, and the type of a struct argument (0.45.2); the address of a literal, `&Point { ... }`, and which values have fields (0.45.3).

### Structs: the rules (0.45.2, 0.45.3)

**1. An index past the end of a local array stops the program, also for an array of structs.** In hosted code (and with `--checked` in kernel modes) every read and every store of `a[i]`, `a[i].x`, `a[i] = q` and `&a[i]` compares `i` with the length `N` of `[T; N]`. Before 0.45.2 an array of structs larger than 8 bytes was compared with its count of 8-byte slots, and a store to a field of an element was not compared at all, so `pts[2].x = 7` on `[Point; 2]` wrote past the array and the program went on.

```
struct Point { x: i64, y: i64 }

fn main() -> i32 {
    let pts: [Point; 2];
    let i: i64 = 1;
    pts[i].x = 7;      // fine: 1 < 2
    print_int(pts[i].x);
    println("");
    i = 2;
    pts[i].x = 7;      // stops here
    return 0;
}
```

The output is `7`, then `Array bounds error\` on standard error (the text ends in a backslash, without a newline), and the exit code is 1. Fix: keep the index below the length (with `for i in 0..2` the compiler knows it and omits the check), or declare a longer array. Global arrays and `[T; N]` fields are not checked.

**2. A field a struct literal does not name is 0.** `let p: Point = Point { y: 9 }` gives `p.x == 0`, in a `let`, an element of an array literal, a nested literal, an argument, a `return` and `&Point { ... }`. Before 0.45.2 a `let` and an array element kept whatever the stack held there. A global cannot take a literal: `let origin: Point = Point { y: 0 };` is

```
origin.lap:3:27: error: the initializer of global 'origin' is not a constant expression: a struct literal; declare the global without an initializer (every field starts at 0) and set its fields at run time, e.g. at the start of main
      3 | let origin: Point = Point { y: 0 };
        |                           ^
1 error, compilation failed
```

Fix, as the message says (since 0.45.3; before, it did not say): `let origin: Point;`, then `origin.x = 1;` and the other fields at the start of `main` (`tests/lang/global_struct_fields.lap`).

**3. A struct argument must be the parameter's struct, and a pointer parameter takes `&x`, not `x`.** With `fn norm1(p: Point)` and `fn move_right(p: *Point)`, `norm1(p)` and `move_right(&p)` compile. These do not:

```
points.lap:10:21: error: argument 1 of 'norm1': expected Point, found Size; pass a Point value
     10 |     print_int(norm1(s));
        |                     ^
points.lap:11:16: error: argument 1 of 'move_right': expected *Point, found Point; pass its address with '&'
     11 |     move_right(p);
        |                ^
2 errors, compilation failed
```

Before 0.45.2 both compiled: `norm1(s)` read 16 bytes from an 8-byte `Size`, `norm1(5)` read at address 5 and crashed, and a struct given to `*Point` was taken as a `Point`. Fix: pass a value of the parameter's struct (`norm1(p)`), its address for a pointer (`move_right(&p)`), or `*pp` when you hold a `*Point`. Each message says which. This rule (R2 of [DESIGN_SECURITY](docs/DESIGN_SECURITY.md)) can break code that compiled before; see [CHANGELOG.md](CHANGELOG.md) 0.45.2. Details and every message: [reference, Passing a struct](docs/REFERENCE.md#passing-a-struct).

**4. `&Point { ... }` is the address of the literal (0.45.3).** The literal is built in a slot of its own in the function's frame (the fields it does not name are 0), and that slot lives until the function returns, like a local declared there. Each `&Point { ... }` in the source has its own slot, so two in one call are two structs. This prints 20:

```
struct Point { x: i64, y: i64 }

fn norm1(p: *Point) -> i64 { return p.x + p.y; }

fn main() -> i32 {
    let a: *Point = &Point { x: 3, y: 4 };
    let b: *Point = &Point { x: 10 };
    print_int(norm1(a) + norm1(b) + norm1(&Point { x: 1, y: 2 }));
    println("");
    return 0;
}
```

Before 0.45.3 it compiled without computing the address: the pointer was whatever a register held, and the program read another variable or crashed (`segfault` on Linux). Where the slot would not be safe, it is a compile error (`lit_bad.lap`): after `return` the frame is gone, and in a loop every iteration would reuse the one slot while a pointer from an earlier one may still be held.

```
lit_bad.lap:4:12: error: cannot return &Point { ... }: the literal lives in this function's frame, which is gone after the return; declare the function '-> Point' and return Point { ... }
      4 |     return &Point { x: 0, y: 0 };
        |            ^
lit_bad.lap:10:17: error: &Point { ... } in a loop: every iteration would reuse its one slot while a pointer from an earlier one may still be held; declare it before the loop (let t: Point = Point { ... };) and take &t, or, for one per iteration, an array before the loop (let a: [Point; N];) and take &a[i]
     10 |         ps[i] = &Point { x: i, y: 0 };
        |                 ^
2 errors, compilation failed
```

Fix: `fn origin() -> Point { return Point { x: 0, y: 0 }; }`, and for the loop `let pts: [Point; 3];` before it, then `pts[i].x = i; ps[i] = &pts[i];` inside. A pointer kept in a variable and returned later (`let q = &Point { ... }; return q;`) is not detected, the same as `return &t` of a local: do not return the address of anything in the frame. Details: [reference, The address of a struct literal](docs/REFERENCE.md#the-address-of-a-struct-literal).

**5. Only a struct, or a pointer to one, has fields (0.45.3).** `x.f` reads or stores the field `f` when `x` is a struct or a pointer to a struct that has `f`: a local, a global, a parameter, an element (`pts[i].x`), a field (`r.a.x`), and through a pointer at any step (`head.next.v` with `next: *Node`, a global `*Node`, `ps[i].x` with `ps: [*Point; N]`, `getp().x`). An element of an array field takes a field store like any element: `s.pts[i].y = 5` with `pts: [Point; 3]`. This prints 28:

```
struct Point { x: i64, y: i64 }
struct Node { v: i64, next: *Node }

let head: *Node = 0;

fn main() -> i32 {
    let pts: [Point; 4];
    let i: i64 = 1;
    pts[i].x = 7;
    let a: Node;
    let b: Node;
    a.v = 1;
    b.v = 2;
    a.next = &b;
    b.next = 0;
    a.next.v = 20;
    head = &a;
    print_int(pts[i].x + head.v + head.next.v);
    println("");
    return 0;
}
```

Anything else is a compile error that names the value and its type (`field_bad.lap`, `shape_bad.lap`):

```
field_bad.lap:8:5: error: a[i] has type i64, which has no field 'x'; only a struct or a pointer to a struct has fields
      8 |     a[i].x = 7;
        |     ^
field_bad.lap:9:5: error: a has type [i64; 4], which has no field 'x'; only a struct or a pointer to a struct has fields
      9 |     a.x = 7;
        |     ^
field_bad.lap:10:5: error: struct 'Point' has no field 'z'
     10 |     p.z = 7;
        |     ^
3 errors, compilation failed
shape_bad.lap:6:5: error: s.pts has type [Point; 3], which has no field 'y'; index it first: s.pts[i].y
      6 |     s.pts.y = 5;
        |     ^
1 error, compilation failed
```

Before 0.45.3 `a[i].x = 7` compiled and the store was dropped (`a[i] = 0; a[i].x = 7; print_int(a[i])` printed 0), and a read of `a[i].x`, `a.x` or `p.z` gave whatever a register held, with no message (their stores were already errors, with other messages). `head.next.v`, a field through a global pointer and `ps[i].x = v` loaded nothing or dropped the store too, though they are correct; `s.pts[i].y = 5` was refused (`the assignment target is not assignable: the expression has type Point, not a pointer`), and `q.arr[i].x = v` with `arr: [*Point; 2]` wrote into the array itself, over the next field. Fix: give the value a struct type (`let a: [Point; 4]`), store the element itself (`a[i] = 7`), index an array before its field, or use a field the struct has. This can break code that compiled before; see [CHANGELOG.md](CHANGELOG.md) 0.45.3. Details: [reference, Field access](docs/REFERENCE.md#field-access).

A trait and a generic function (from `tests/lang/trait.lap`):

```
trait Ord { fn lt(a: Self, b: Self) -> bool }

impl Ord for i64 {
    fn lt(a: Self, b: Self) -> bool { return a < b; }
}

fn max<T: Ord>(a: T, b: T) -> T {
    if (a.lt(b)) { return b; }
    return a;
}
```

A growing vector (from `examples/lowlat/vec.lap`):

```
fn main() -> i32 {
    let v: vec i64;
    let i = 0;
    let s = 0;
    while (i < 100) { vec_push(v, i); i = i + 1; }
    for x in v { s = s + x; }
    print_int(s);
    println("");
    return 0;
}
```

Early return with `?` (from `tests/lang/result.lap`):

```
enum Result<T, E> { Ok(T), Err(E) }

fn bump(n: i64) -> Result<i64, i64> {
    let x = ok_i(n)?;
    return Result<i64, i64>::Ok(x + 1);
}
```

The full reference is [docs/REFERENCE.md](docs/REFERENCE.md). Detailed limits of each feature, and the test inventory, are in [docs/STATUS.md](docs/STATUS.md).

## Kernels

`--flat ADDR` emits a raw image, `--flat ADDR --elf` emits it as an ELF64 executable with named sections, `--boot` emits a 512-byte boot sector, and `bits 16` / `bits 32` functions allow the real-mode and protected-mode stages. [AtomicOS](https://github.com/ipenas-cl/AtomicOS) is built this way. Some of this runs under QEMU in `make test` (`tests/boot`, `tests/usermode`, `tests/link`) and some is only assembled; [docs/KERNELS.md](docs/KERNELS.md) and [docs/STATUS.md](docs/STATUS.md) say which.

## Performance, measured

These numbers are from the `CHANGELOG.md` entry of each version, on one machine, with alternated binaries because code placement alone moves a run by about 10 ms. Treat them as one sample, not a guarantee.

| Program | Copihue | Compared with | Version |
|---|---|---|---|
| Order book (`examples/lowlat/book`) | 36.9 ms | gcc 37.4 ms | 0.40.1 |
| Parser | about 1.05× gcc | gcc | 0.39.1 |
| `fib(40)` | 231 ms | gcc 130 ms, rustc 193 ms | 0.42.2 |
| Growing `vec` (`vec_push`) | 4.50 ms | g++ 6.89 ms, rustc 3.99 ms | 0.42.3 |
| Matching engine (`examples/lowlat/match`) | about 1.4× gcc | gcc | see CHANGELOG |

Copihue is slower on recursive code (`fib`), on the matching engine, and on a growing `vec` compared with Rust. Methods, ranges and more cases: [bench/RESULTS.md](bench/RESULTS.md) (measured with an older version, 0.31.1, and not refreshed), [examples/lowlat/RESULTS.md](examples/lowlat/RESULTS.md) and [bench/lowlat/RESULTS.md](bench/lowlat/RESULTS.md). The `examples/lowlat/` programs (pool, order book, matching engine, SPSC ring, false sharing, CPU affinity, watchdog, `vec`) are the demonstration cases.

## Repository layout

- `compiler/bootstrap-c/copihue.c`: the compiler, one C file
- `tests/`: regression suite (`make test`), golden outputs in `tests/expected/`
- `examples/`: example programs; `examples/lowlat/` has the low-latency cases
- `bench/`: benchmarks outside `make test`
- `docs/REFERENCE.md`: language reference; `docs/DESIGN_*.md`: one note per feature
- `docs/DESIGN_SECURITY.md`: security rules proposed after the SSHole bug (errors are not domain values, unused `Result` is an error, constant-time compare); mostly a proposal: of R2, the struct argument rule is implemented (0.45.2), `==` between structs is not: [docs/DESIGN_SECURITY.md](docs/DESIGN_SECURITY.md)
- `docs/STATUS.md`, `docs/KERNELS.md`: detailed limits, test inventory and kernel modes
- `compiler/native/x64.c`: x86-64 instruction encoder, the first piece of writing the ELF without `nasm` and `ld`; `make native-test` compares it with `nasm`. Inventory and design: [docs/DESIGN_NATIVE.md](docs/DESIGN_NATIVE.md)
- `compiler/native/asm.c`: assembler for the NASM text the compiler emits (labels, jump relaxation, `call`, `.data`/`.rodata`/`.bss`, `extern` in `-c` objects); `make native-test` checks the bytes, the `.bss` size and the relocations against `nasm -f elf64` on all the corpus programs. Used by `copihue`: [docs/DESIGN_NATIVE.md](docs/DESIGN_NATIVE.md)
- `compiler/native/elf.c`: writes the ELF64 executable and the `-c` relocatable object. Used by `copihue`.
- `compiler/native/x64.c`: a second x86-64 encoder that also returns relocations; `make native-test` compares it with `nasm -f elf64` (bytes and relocations): [docs/DESIGN_NATIVE_X64.md](docs/DESIGN_NATIVE_X64.md)
- `compiler/asm/`: an early experimental compiler in x86-64 assembly (still uses `chronos` names)
- `archive/`, `docs/history/`, `docs/archive/`: Chronos-era material, kept unchanged

## What is missing

- Borrow or lifetime checking, and generics over enums, slices, `vec` and `own *T`.
- `+=`, `-=` and `++` on an element, a field or a dereference (only on variables); `getp()->x = v` (a call before `->`; `getp().x = v` works); `a + b` in a call argument or a `return` is still cut to the operands' width.
- `str` is bytes only (no UTF-8, no heap strings); a `vec` cannot be returned, passed or stored in a struct; a `str` cannot be an enum field or an array element; a returned or stored slice is a view with no lifetime check beyond the cases above.
- Generic bodies in `-c` objects: the same monomorphized body emitted in two objects fails at `ld`.
- The compiler does not emit GDT, page tables or the `STAR`/`LSTAR`/`EFER` writes; the kernel does.
- Speed: recursion, the matching engine and `vec_push` against Rust (see above).
- Self-hosting and other targets (ARM64) are not started. The self-hosting plan (phases S0–S5) was approved on 2026-11-05: [docs/DESIGN_SELFHOST.md](docs/DESIGN_SELFHOST.md). Phase S0 (module inventory, equality corpus, language gaps for S1) is written: [docs/SELFHOST_S0.md](docs/SELFHOST_S0.md). Phase S1 (return a struct by value; return `str`/`[]T`; `str`/`[]T` as a struct field) is in 0.45.0 and waits for review: [docs/DESIGN_SELFHOST_S1.md](docs/DESIGN_SELFHOST_S1.md). No self-hosted code exists yet. `nasm` and `ld` are no longer needed for hosted programs and `-c` objects, but they still build programs with user assembly or `naked`/`interrupt`/`syscall` functions, `--freestanding -c`, `--flat` and `--boot`. The author's development machine (macOS) cannot run Linux ELF files, so running what the compiler writes is the CI's job ([docs/DESIGN_NATIVE.md](docs/DESIGN_NATIVE.md)).

Plans: [ROADMAP.md](ROADMAP.md) and [docs/VISION.md](docs/VISION.md).

## License

See [LICENSE](LICENSE).
