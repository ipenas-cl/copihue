<p align="center"><img src="docs/brand/pixel/copihue-pixel-wordmark-dark.png" alt="Copihue" width="420"></p>

<h1 align="center">Copihue</h1>

<p align="center">
<a href="https://github.com/ipenas-cl/copihue/actions/workflows/ci.yml"><img src="https://github.com/ipenas-cl/copihue/actions/workflows/ci.yml/badge.svg" alt="CI"></a>
<a href="https://github.com/ipenas-cl/copihue/releases/latest"><img src="https://img.shields.io/github/v/release/ipenas-cl/copihue" alt="Release"></a>
<a href="LICENSE"><img src="https://img.shields.io/github/license/ipenas-cl/copihue" alt="License"></a>
</p>

A systems programming language with its own compiler, written in C with an x86-64 NASM back end. It produces Linux x86-64 ELF executables **with no libc and no runtime**, and it can also produce freestanding code for kernels and boot sectors. *Formerly Chronos.*

Current version: **0.42.3** (`compiler/bootstrap-c/copihue.c`, `LANG_VERSION`). Release notes are in [CHANGELOG.md](CHANGELOG.md).

> **Status: not production-ready.** It is a tested compiler for a small but real language. Memory safety is partial (checked slices and indexes, `own *T` moves) and there is no borrow checker: a `vec` slice can dangle after a push. Linux x86-64 only. The goal is a language that beats C++, Rust and OCaml; today it does not, and the numbers below say where it stands.

## Install

Requirements: Linux x86-64, `gcc`, `make`, `nasm`, `ld` (binutils).

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
copihue [-O0|-O1|-O2] [-o out] [-S] [-c] [--freestanding | --flat ADDR | --boot] [-v] file.lap [more.lap ...]
```

`-S` writes NASM assembly only. `-c` writes one ELF64 relocatable object (link with `ld`). `--freestanding`, `--flat` and `--boot` are for kernels (see [docs/KERNELS.md](docs/KERNELS.md)). The value returned by `main` is the exit code.

## What the language has

Two levels of syntax. The **easy level** lets the compiler decide: an omitted parameter type or `->` is `i64`, `let x = e` infers the type, `vec T` grows by itself and frees at the end of the block, `own *T` is an owning pointer that moves on `return` or when passed on. The **explicit level** gives the developer the width of every integer, `*volatile T`, `align(N)`, `packed struct`, atomics, `naked` and `interrupt` functions, and named sections (`section "name" @ addr`). Design: [docs/DESIGN_TWO_LEVELS.md](docs/DESIGN_TWO_LEVELS.md).

- Integers `i8` to `i64` and `u8` to `u64`, with arithmetic at the width of the type, and checked assignments (a wider value needs `as`).
- Slices `[]T` with bounds-checked indexing in hosted code, `for x in s`, `unsafe { }`, and `str` as a `[]u8` ([DESIGN_SLICES](docs/DESIGN_SLICES.md), [DESIGN_STRINGS](docs/DESIGN_STRINGS.md)).
- Modules with `pub` and `name::item`, several files per command, `import`, and `-c` objects ([DESIGN_MODULES](docs/DESIGN_MODULES.md)).
- Enums with data and exhaustive `match`, `Option<T>`, `Result<T, E>` and `?` ([DESIGN_ENUMS](docs/DESIGN_ENUMS.md), [DESIGN_ERRORS](docs/DESIGN_ERRORS.md)).
- Generics by monomorphization and traits without vtables ([DESIGN_GENERICS](docs/DESIGN_GENERICS.md), [DESIGN_TRAITS](docs/DESIGN_TRAITS.md)).
- Atomics, `fence`, and hosted threads (`thread_spawn`, `thread_join`, `sched_setaffinity`) without libc.

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
- `docs/STATUS.md`, `docs/KERNELS.md`: detailed limits, test inventory and kernel modes
- `compiler/asm/`: an early experimental compiler in x86-64 assembly (still uses `chronos` names)
- `archive/`, `docs/history/`, `docs/archive/`: Chronos-era material, kept unchanged

## What is missing

- Borrow or lifetime checking, and generics over enums, slices, `vec` and `own *T`.
- `str` is bytes only (no UTF-8, no heap strings); a `vec` cannot be returned, passed or stored in a struct.
- Generic bodies in `-c` objects: the same monomorphized body emitted in two objects fails at `ld`.
- The compiler does not emit GDT, page tables or the `STAR`/`LSTAR`/`EFER` writes; the kernel does.
- Speed: recursion, the matching engine and `vec_push` against Rust (see above).
- Self-hosting and other targets (ARM64) are not started.

Plans: [ROADMAP.md](ROADMAP.md) and [docs/VISION.md](docs/VISION.md).

## License

See [LICENSE](LICENSE).
