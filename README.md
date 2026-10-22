<p align="center"><img src="docs/brand/logo.svg" alt="Copihue logo" width="140"></p>

<h1 align="center">Copihue</h1>

<p align="center">
<a href="https://github.com/ipenas-cl/copihue/actions/workflows/ci.yml"><img src="https://github.com/ipenas-cl/copihue/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
<a href="https://github.com/ipenas-cl/copihue/releases/latest"><img src="https://img.shields.io/github/v/release/ipenas-cl/copihue" alt="Release"></a>
<a href="LICENSE"><img src="https://img.shields.io/github/license/ipenas-cl/copihue" alt="License"></a>
</p>

*Formerly Chronos.*

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
cat > hello.cph <<'CPH'
fn main() -> i32 {
    println("Hello, Copihue!");
    print_int(6 * 7);
    println("");
    return 0;
}
CPH
copihue hello.cph -o hello && ./hello
# Hello, Copihue!
# 42
```

```
copihue [-O0|-O1|-O2] [-o out] [-S] [--freestanding | --flat ADDR] [-v] file.cph
```

`-S` stops after writing NASM assembly. `--freestanding` emits assembly for use without an OS (see below).
The value returned by `main` becomes the process exit code.
Old `.ch` files are still accepted, with a deprecation warning.

## What works today

The full language reference is in [docs/REFERENCE.md](docs/REFERENCE.md).

- Functions, recursion, up to 6 parameters, structs passed by value (copied) or by pointer
- `let` locals, globals, integer types `i8 i16 i32 i64 u8 u16 u32 u64`, typed arrays, pointers, `&`, `*`, `->`
- `struct`s, including nested structs, arrays of structs, and nested struct literals
- `if`/`else if`/`else`, `while`, `for`, `break`/`continue`
- Arithmetic with signed division and remainder that truncate toward zero, comparisons, `&&`/`||` (short-circuit),
  and bitwise `& | ^ ~ << >>` with Rust-like precedence. Hex `0xFF`, binary `0b1010`, `1_000_000` and `'A'` literals
- String literals with escapes (`\n \t \r \0 \\ \" \'`)
- Hosted built-ins (Linux syscalls): `print`, `println`, `print_int`, `strlen`, `strcmp`, `strcpy`,
  `malloc`/`free`, `open`/`read`/`write`/`close`, `syscall6`, `exit`
- Freestanding built-ins: `inb/inw/inl`, `outb/outw/outl`, `cli`, `sti`, `hlt`, `pause`, and inline `asm("...")`
- Diagnostics in the form `file:line:col: error: ...`, with the source line and a caret. They cover syntax errors,
  undeclared variables, indexing a non-array, unknown struct fields, bad escapes, and hosted-only calls in freestanding code
- Optimizations: `-O1` constant folding; `-O2` adds strength reduction. Every test must give identical output at `-O0` and `-O2`

### Tested

`make test` runs the following in CI on every push:

| Suite | What it checks |
|---|---|
| 127 test programs + 17 examples | compiled at `-O0` and `-O2`, run, and stdout + exit code compared to golden files in `tests/expected/` |
| 12 error tests | the compiler rejects the program with the exact expected message |
| 2 freestanding tests | `--freestanding` output has no `_start`/syscalls and assembles |
| 1 flat-binary test | `--flat` puts `_start` at offset 0; the raw image matches a golden hexdump |
| `make net-test` | starts `examples/simple_api_server.cph`, queries it with `curl` and with two Copihue HTTP clients, then stops it |

### Known limitations

- Functions take at most 6 parameters. Struct values are copied 8 bytes per field. There is no type checking beyond what the diagnostics above cover.
- Integer overflow wraps (x86-64 behavior). There are no bounds checks on pointer indexing.
- The generated code is simple stack-machine style. `rbx` is used as scratch, so freestanding callers must not rely on it being preserved.
- Linux x86-64 only.

## Freestanding mode (kernels)

```copihue
fn serial_putc(c: i64) -> i64 {
    while ((inb(0x3FD) & 0x20) == 0) { pause(); }
    outb(0x3F8, c);
    return 0;
}
```

`copihue --freestanding -o kernel.asm kernel.cph` produces NASM with no `_start`, no syscalls and no runtime.
Your own entry code `%include`s it and calls your functions using the System V argument registers.
Hosted built-ins such as `println` are a compile error in this mode.

### Flat binaries (`--flat`)

`copihue --flat 0x10000 -o kernel.bin kernel.cph` goes one step further: it builds a raw binary (via `nasm -f bin`) meant to be loaded at the given address, with **no assembly entry stub**. Execution starts at the first byte, which is always `fn _start` (it is emitted first wherever it is defined; a missing `_start` is an error). Two extra built-ins, `bss_start()` and `bss_end()`, return the bounds of the zero-initialized globals so `_start` can clear them. The [AtomicOS](https://github.com/ipenas-cl/AtomicOS) kernel is built this way. Add `-S` to see the assembly instead.

## Repository layout

- `compiler/bootstrap-c/copihue.c`: the compiler, a single C file
- `tests/`: regression suite (`run_tests.sh`, `net_test.sh`, golden files)
- `examples/`: example programs
- `docs/REFERENCE.md`: language reference
- `docs/brand/`: logo (SVG and PNG) and the 1280×640 social preview image
- `compiler/asm/`: an experimental second compiler in pure x86-64 assembly for a minimal declarative syntax. It is an early self-hosting experiment and still uses its old `chronos` names.
- `archive/`, `docs/history/`, `docs/archive/`, `compiler/bootstrap-c/archive/`: historical material from the Chronos era, kept unchanged

## Roadmap

Correctness and tooling come before new features. All items below are **planned**.

- **0.19:** type checking for assignments and calls, more than 6 parameters, `const`, warnings for unused variables
- **0.20:** `u8`/`i32` arithmetic semantics (currently everything computes in 64 bits), struct copy assignment, defined overflow behavior
- **Memory safety:** bounds-checked arrays, null-free references (`Option<T>`), and later an ownership model
- **Systems features driven by [AtomicOS](https://github.com/ipenas-cl/AtomicOS):** `volatile` memory access, interrupt handler functions, packed structs and explicit layout, and function pointers (needed for an IDT, a timer interrupt, and a scheduler)
- **Longer term:** a module system, self-hosting, and an ARM64 back end

Version history: [CHANGELOG.md](CHANGELOG.md). Older design notes: [docs/VISION.md](docs/VISION.md), [ROADMAP.md](ROADMAP.md).

## License

See [LICENSE](LICENSE).
