<p align="center"><img src="docs/brand/pixel/copihue-pixel-wordmark-dark.png" alt="Copihue" width="420"></p>

<h1 align="center">Copihue</h1>

<p align="center">
<a href="https://github.com/ipenas-cl/copihue/actions/workflows/ci.yml"><img src="https://github.com/ipenas-cl/copihue/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
<a href="https://github.com/ipenas-cl/copihue/releases/latest"><img src="https://img.shields.io/github/v/release/ipenas-cl/copihue" alt="Release"></a>
<a href="LICENSE"><img src="https://img.shields.io/github/license/ipenas-cl/copihue" alt="License"></a>
</p>

*Formerly Chronos.*

<p align="center"><img src="docs/media/copihue-code-fib.png" alt="A Copihue program" width="420"> <img src="docs/media/copihue-fib-output.png" alt="Its output" width="300"></p>

**A small systems programming language with its own compiler, written from scratch in C and x86-64 assembly.**
Copihue compiles a C-family language to native Linux x86-64 ELF executables with **no libc and no runtime**.
It can also emit **freestanding** code, with no OS, for kernels and bare-metal programs.
[AtomicOS](https://github.com/ipenas-cl/AtomicOS) is built with `--flat`, with no assembly in the kernel.

> **Status: not production-ready yet.** The goal is to become a real alternative for systems programming.
> Today Copihue is a working, tested compiler for a small language. Memory safety and determinism are
> design goals, not implemented guarantees (see [Roadmap](#roadmap)).

## Install

Requirements: Linux x86-64, `gcc` (or any C99 compiler), `make`, `nasm`, and `ld` (binutils).

```bash
sudo apt-get install -y gcc make nasm binutils
git clone https://github.com/ipenas-cl/copihue.git && cd copihue
make                      # builds ./copihue
make test                 # optional: run the regression suite
make install              # installs copihue to ~/.local/bin (PREFIX=/usr/local to change)
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
copihue [-O0|-O1|-O2] [-o out] [-S] [--freestanding | --flat ADDR] [-v] file.lap
```

`-S` stops after writing NASM assembly. `--freestanding` emits assembly for use without an OS (see below).
The value returned by `main` becomes the process exit code.
Old `.ch` files are still accepted, with a deprecation warning.

## What works today

The full language reference is in [docs/REFERENCE.md](docs/REFERENCE.md).

- Functions, recursion, up to 16 parameters, structs passed by value (copied) or by pointer
- `let` locals, globals, `let volatile` globals, `*volatile T` pointers, `volatile` struct fields, integer types `i8 i16 i32 i64 u8 u16 u32 u64`, typed arrays, pointers, `&`, `*`, `->`
- `struct`s, including nested structs, arrays of structs, nested struct literals, and assignment `a = b` (exact size, packed records included)
- `packed struct` for hardware records (an x86-64 IDT gate is 16 bytes, not eight slots of 8)
- Function addresses (`&timer`) and indirect calls, so a table of handlers can be installed from the language
- `interrupt fn` (full GPR save, optional CPU error code, `iretq`) and `syscall fn` (`syscall`/`sysret` entry, up to 7 arguments)
- `if`/`else if`/`else`, `while`, `for`, `break`/`continue`
- Arithmetic with signed division and remainder that truncate toward zero, comparisons, `&&`/`||` (short-circuit),
  and bitwise `& | ^ ~ << >>` with Rust-like precedence. Hex `0xFF`, binary `0b1010`, `1_000_000` and `'A'` literals
- String literals with escapes (`\n \t \r \0 \\ \" \'`)
- Hosted built-ins (Linux syscalls): `print`, `println`, `print_int`, `strlen`, `strcmp`, `strcpy`,
  `malloc`/`free`, `open`/`read`/`write`/`close`, `syscall6`, `exit`
- Freestanding built-ins: `inb/inw/inl`, `outb/outw/outl`, `cli`, `sti`, `hlt`, `pause`, `swapgs`,
  `lidt`/`lgdt`/`invlpg` (any address expression, including a variable), `rdmsr`/`wrmsr`,
  `read_cr0`/`read_cr2`/`read_cr3`/`read_cr4`, `write_cr0`/`write_cr3`/`write_cr4`,
  `set_syscall_stack`, and inline `asm("...")`. The control-register builtins are a compile error in hosted mode
- Diagnostics in the form `file:line:col: error: ...`, with the source line and a caret. They cover syntax errors,
  undeclared variables, indexing a non-array, unknown struct fields, unknown functions, a call with the wrong
  number of arguments, an interrupt function with the wrong shape, bad escapes, and hosted-only calls in freestanding code
- Optimizations: `-O1` constant folding; `-O2` adds strength reduction. Every test must give identical output at `-O0` and `-O2`

### Tested

`make test` runs the following in CI on every push:

| Suite | What it checks |
|---|---|
| 138 test programs + 18 examples | compiled at `-O0` and `-O2`, run, and stdout + exit code compared to golden files in `tests/expected/` |
| 17 error tests | the compiler rejects the program with the exact expected message |
| 10 freestanding tests | `--freestanding` output has no `_start`/syscalls and assembles; the instruction stream is golden. Two of them also require the same stream at `-O2` |
| 2 flat-binary tests | `--flat` puts `_start` at offset 0; the raw image matches a golden hexdump |
| `make net-test` | starts `examples/simple_api_server.lap`, queries it with `curl` and with two Copihue HTTP clients, then stops it |

### Known limitations

- Ordinary struct fields still occupy 8 bytes each. Only `packed struct` uses the real width of `u8`/`u16`/`u32`. There is no padding or alignment attribute beyond packed-or-not. `a = b` copies the whole struct; it does not check anything beyond the struct type name.
- Call checking is the function name and the argument count. Argument types, assignment types, and `const` are not checked. Function pointers are integer addresses, not a typed `fn` type, and an indirect call is not arity-checked.
- Integer arithmetic is always 64-bit and overflow wraps. Narrow types only change loads and stores. There are no bounds checks on pointer indexing.
- A kernel is still one translation unit: there are no modules and no relocatable ELF objects. There is no 16-bit or 32-bit code generation, so a bootloader that leaves long mode stays assembly.
- `syscall fn` switches stacks and returns with `sysret`, but it does not program `STAR`, `LSTAR`, or `EFER`. The kernel does that with `wrmsr` (or `asm`) and must call `set_syscall_stack` with a 16-byte-aligned top before any entry. There is no TSS or IST setup.
- `volatile` on a global scalar, a `*volatile T` pointee, or a struct field is reloaded and stored on every use, including inside a loop at `-O2`, at the access's real width. It is not a memory model: there are no atomics, and `let volatile` on a local scalar does not change codegen. `let volatile` on a struct or an array is a compile error.
- Indexing a local array in hosted mode emits a bounds check (`syscall` + `exit`). `--freestanding` and `--flat` index the same local array without that trap. Pointer indexing is never checked.
- The generated code is simple stack-machine style. `rbx` is scratch, so freestanding callers must not rely on it being preserved. `iretq`, `sysret`, and moves to or from CR0/CR2/CR3/CR4 are checked by assembling them, not by running them as a Linux process.
- Linux x86-64 only. There is no way to place a page table at a chosen virtual address except by writing through a pointer.

## Freestanding mode (kernels)

```copihue
fn serial_putc(c: i64) -> i64 {
    while ((inb(0x3FD) & 0x20) == 0) { pause(); }
    outb(0x3F8, c);
    return 0;
}
```

`copihue --freestanding -o kernel.asm kernel.lap` produces NASM with no `_start`, no syscalls and no runtime.
Your own entry code `%include`s it and calls your functions using the System V argument registers.
Hosted built-ins such as `println` are a compile error in this mode.

The same mode can express the pieces a small kernel needs after it is already in long mode:

```copihue
packed struct IdtEntry {
    off_lo: u16, sel: u16, ist: u8, typ: u8,
    off_mid: u16, off_hi: u32, zero: u32
}

let volatile ticks: i64 = 0;
let idt: [IdtEntry; 32];

interrupt fn timer() {
    ticks = ticks + 1;
    outb(0x20, 0x20);
}

fn set_gate(n: i64, handler: i64) -> i64 {
    idt[n].off_lo = handler & 0xFFFF;
    idt[n].sel = 0x18;
    idt[n].typ = 0x8E;
    idt[n].off_mid = (handler >> 16) & 0xFFFF;
    idt[n].off_hi = handler >> 32;
    return 0;
}
```

`set_gate(32, &timer)` then `lidt(&idtr)` installs it. The selector `0x18` is the 64-bit code segment of the AtomicOS bootloader GDT; `0x08` on that loader is 32-bit code and a gate that uses it hangs. `tests/flat/min_kernel.lap` is a complete `--flat 0x10000` image for that GDT, not a generic bootable kernel: it clears the BSS, writes a byte to the serial port, bump-allocates from a global buffer, builds a packed IDT, and defines an interrupt handler and a syscall entry. A `syscall fn` takes the Linux register convention (`rax` = number, `rdi`/`rsi`/`rdx`/`r10`/`r8`/`r9` = arguments), switches to the stack given to `set_syscall_stack`, and returns with `sysret`.

### Flat binaries (`--flat`)

`copihue --flat 0x10000 -o kernel.bin kernel.lap` goes one step further: it builds a raw binary (via `nasm -f bin`) meant to be loaded at the given address, with **no assembly entry stub**. Execution starts at the first byte, which is always `fn _start` (it is emitted first wherever it is defined; a missing `_start` is an error). Two extra built-ins, `bss_start()` and `bss_end()`, return the bounds of the zero-initialized globals so `_start` can clear them. The [AtomicOS](https://github.com/ipenas-cl/AtomicOS) kernel is built this way. Add `-S` to see the assembly instead.

## Repository layout

- `compiler/bootstrap-c/copihue.c`: the compiler, a single C file
- `tests/`: regression suite (`run_tests.sh`, `net_test.sh`, golden files)
- `examples/`: example programs
- `docs/REFERENCE.md`: language reference
- `docs/brand/`: logo (SVG and PNG) and the 1280×640 social preview image
- `compiler/asm/`: an experimental second compiler in pure x86-64 assembly for a minimal declarative syntax. It is an early self-hosting experiment and still uses its old `chronos` names.
- `archive/`, `docs/history/`, `docs/archive/`, `compiler/bootstrap-c/archive/`: historical material from the Chronos era, kept unchanged

## Benchmarks

[`make bench`](bench/RESULTS.md) is not part of `make test`. It compiles the same small programs with Copihue (`-O2`), C (`gcc -O2`), Rust (`rustc -O`) and OCaml (`ocamlopt -O3`), checks that they print the same line, and records compile time, file size and run time. The measured numbers and the limits of that comparison are in [bench/RESULTS.md](bench/RESULTS.md).

[docs/DESIGN_TWO_LEVELS.md](docs/DESIGN_TWO_LEVELS.md) is a design proposal for a two-level syntax (an easy mode and an explicit mode). It is not a feature of the compiler in this tree.

## Roadmap

Correctness and tooling come before new features. All items below are **planned**.

- **Types:** checking assignments and argument types (calls already reject an unknown name or the wrong count), `const`, warnings for unused variables
- **Narrow arithmetic:** `u8`/`i32` operations (everything still computes in 64 bits), struct copy assignment, defined overflow behavior
- **Memory safety:** bounds-checked arrays, null-free references (`Option<T>`), and later an ownership model
- **Still needed for a larger [AtomicOS](https://github.com/ipenas-cl/AtomicOS):** separate compilation, a way to name sections and load addresses, and 16/32-bit codegen so the bootloader can leave assembly. User mode can be written with `wrmsr` and `syscall fn`, but the compiler does not build a TSS or program `STAR`/`LSTAR`/`EFER` for you
- **Longer term:** a module system, self-hosting, and an ARM64 back end

Version history: [CHANGELOG.md](CHANGELOG.md). Older design notes: [docs/VISION.md](docs/VISION.md), [ROADMAP.md](ROADMAP.md).

## License

See [LICENSE](LICENSE).
