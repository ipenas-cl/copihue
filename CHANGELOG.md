# Changelog

Todos los cambios importantes del proyecto.

## [0.43.0] - 2026-10-26

### Added
- `str` is `[]u8`. Sixteen bytes: the pointer at offset 0 and the `i64` length at offset 8. It is not a keyword, not a class, and not a heap string. `sizeof(str)` is 16. `let str = 1` is still a name. There is no libc. `vec` is not rewritten.
- A literal `"..."` in a `str` or `[]u8` destination stores the label and the decoded length. An interior NUL counts (`"a\0b"` has length 3). The extra trailing NUL that keeps `.ptr` a C string does not. `let p = "..."` and a `*u8` destination stay the pointer. `print("...")` is unchanged. A global `let s: str = "..."` is those two qwords.
- `s.len` is `i64`. `s[i]` is the existing slice check (`index out of range`, exit 1). Hosted code checks. `--freestanding`, `--flat`, and `--boot` check only with `--checked`. `s.ptr[i]` and `unsafe` do not check.
- `a == b` and `a != b`, when both names are a `str` or a `[]u8`, compare the length and then the bytes. Before the frame, that is the `if` and the `for i in 0..a.len` a programmer writes. There is no `memcmp`. `let x = a == b` and `if (a == b)` store 0 or 1. A compare in a `while` condition is `'==' on str in a while condition needs a local`. A slice compared with a scalar is still `a slice is not a scalar; read .ptr or .len`. A `[]i64` is not compared by content.
- `s[i..j]` is a local slice: pointer `s.ptr + i`, length `j - i`, `j` exclusive. The check is unsigned `i <= j` and `j <= s.len`. An empty range is legal. Failure is the same `index out of range`. `-> str` is `a slice cannot be returned`. An enum field is still not a slice, so `Result` does not carry a `str`.
- Hosted `print(s)` and `println(s)` on a `str` or `[]u8` are `write(1, s.ptr, s.len)`. `println` then writes the newline. The syscall uses the length. It does not call `__strlen`. A `*u8` still does. Freestanding stays `'print' needs the hosted runtime`.
- `tests/lang/str.lap` prints 5, 104, 1, 0, 0, 1, 3, `ell`, `hello`, 0, 0, `hello`, `via`, 0, 1, 3, 2, 2, 1, 16, 16, 2, and 1 at `-O0` and `-O2`. `tests/lang/str_oob.lap` and `tests/lang/str_range_oob.lap` write `index out of range` and exit 1. `tests/flat/str.lap` is the same image at both levels and is not booted. Two `-c` objects link `slen` (`T slen`, `U slen`, prints 5). `examples/words.lap` counts words and returns `Result<i64, i64>`: `one two three` prints 0 then 3, `  hi  ` prints 0 then 1, an empty string prints 0 then 0, a tab at index 3 prints 1 then 3, the prefix prints `one`, and the comparisons print 1, 1, and 0. The note is `docs/DESIGN_STRINGS.md`.

### Left out
- UTF-8, a heap `String`, format strings, interpolation, capacity, and `append`.
- An enum field of type `str`, and `Result` carrying a `str`. The text stays in the `str`. The `Result` carries an integer or a pointer.
- Returning a `str`. That is still `a slice cannot be returned`.
- Rewriting every existing `"..."` use into a `str`. An untyped `let` and a `*u8` stay a pointer. `vec` is not rewritten.
- Content comparison of a slice whose element is not `u8`. `memcmp`. Changing `strlen`, `strcpy`, or `strcmp`.
- `bits 16` and `bits 32` beyond the existing `[]T is a 64-bit value`.
- `sizeof([]u8)` as syntax. That form does not parse. `sizeof(str)` and `sizeof` of a slice value are 16.
- Hoisting `==` out of a `while` condition.

### Changed
- Language version is 0.43.0, on top of 0.42.1. 0.42.1 changed only the version string. The hosted `-O2` `fib` frame stays the 16-instruction path, and that window was not run again. The 0.42.0 `Result` / `?` lowering is unchanged: it is still the match, with no exceptions and no extra instruction. 0.41.0 traits and static dispatch are unchanged: the copy still calls the impl symbol, and there is still no vtable. The 0.40.1 hosted `-O2` `while` whose equal arm is cold stays: that arm is `je` after the back edge, the tail falls through, and `align 32` is not applied to that shape. The 0.39.1 `while` stays: the not-equal arm falls through, one global array is `[r8+index*scale]`, and that loop label is `align 32`. The 0.38.1 note rejected sharing one register between two scalars whose live ranges do not overlap. That pass is not emitted. The 0.37.2 note rejected aligning `.text` to 16 or 32, and 0.37.1 rejected a direct `arr[i].field` store. Neither is emitted here.

### Measured
- No timed bench was added to `make test`. `bench/lowlat`, the SPSC ring, `bench/slice`, the 0.39 enum-versus-`if` windows, the 0.40.1 `book` and `match` windows, the 0.41 trait listing, the 0.42 `Result` listing, and the 0.42.1 `fib` window were not run again.
- NASM listing (`nasm -f elf64 -l`), hosted. Each pair is the same function written with `str` and with `[]u8`. Local labels (`.L0`, `.Lslice_trap_0`) stay inside the function. The bytes are the same, including the cold trap that follows the last `ret`. The counts below stop at that `ret` and leave the trap out of the number. The frame is the slice-parameter frame (the callee copies 16 bytes). It is not the 0.41 slim frame and not the 0.42 enum-return frame. Listing produced 2026-10-26T11:11:12Z. Sources stayed in `/tmp/s43b`.
- `fn n(s: str) -> i64 { return s.len; }` and the `[]u8` form. `sub rsp` is `4883EC50` (80) at `-O0` and at `-O2`. `-O0`, from the label through the first `ret`: 29 and 29. The whole function through the last `ret` is 34 and 34. Hosted `-O2`: 37 and 37 through the first `ret`, 46 and 46 through the last.
- `fn nth(s: str, i: i64) -> i64 { return s[i]; }` and the `[]u8` form. The first `ret` is the successful return. `sub rsp` is `4883EC60` (96) at both levels. `-O0`: 55 and 55 through the first `ret`, 60 and 60 through the last. `-O2`: 69 and 69 through the first `ret`, 82 and 82 through the last.
- `fn eq(a: str, b: str) -> i64 { return a == b; }` against the handwritten `[]u8` function: if the lengths differ, return 0; `for i in 0..a.len`, if the bytes differ, return 0; return 1. The first `ret` is that length mismatch. `sub rsp` is `4883EC70` (112) at both levels. `-O0`: 63 and 63 through the first `ret`, 197 and 197 through the last. `-O2`: 95 and 95 through the first `ret`, 241 and 241 through the last.
- `let t: str = s[i..j]; return t.len` and the same statements with `[]u8`. `sub rsp` is `4883EC70` (112) at both levels. `-O0`: 83 and 83 through the first `ret`, 88 and 88 through the last. `-O2`: 103 and 103 through the first `ret`, 120 and 120 through the last.
- `fn show(s: str) -> i64 { print(s); return 0; }` against `write(1, s.ptr, s.len)` on a `[]u8`. `sub rsp` is `4883EC50` (80) at both levels. `-O0`: 56 and 56 through the first `ret`, 61 and 61 through the last. `-O2`: 64 and 64 through the first `ret`, 73 and 73 through the last. No `__strlen` and no extra instruction.

## [0.42.0] - 2026-10-26

### Added
- `enum Result<T, E> { Ok(T), Err(E) }` is a generic enum the program writes. It is not a compiler type. `Ok` and `Err` each have one field, and that field is still an integer or a pointer. `sizeof(Result<i64, i64>)` is 16. There are no exceptions, no unwinding, and no libc. `vec` is not rewritten.
- `expr?` returns early on `Err` and yields the `Ok` field. Before the frame is chosen it is the match a programmer writes: `Ok` then `Err`, and the `Err` arm is `{ return Result<T, E>::Err(e); }`. A call is bound with `let` immediately before the statement, because `match` of a call still needs a local. `a`, `s.o`, and `p->o` are places and are not lifted. `?` is postfix. Prefix `*` and `&` wrap it, so `*p?` is `*(p?)`. The early return uses the existing enum return (hidden pointer in `rdi`). An arm block may `return` instead of yielding a value. A block that does neither is still `match arm block needs a result`.
- The operand is not required to be named `Result`. It must have `Ok(T)` and `Err(E)`, and the function's `Err` field must be the same type. No conversion. A function that does not return an enum is `'?' needs a Result return`. A local that is not an enum is `'?' needs a local Result`. An `Option` is `'?' needs variants Ok(T) and Err(E) on 'Option'`. `Err` of `*u8` against `i64` is `error type '*u8' does not match 'i64' in '?'`. `?` on a call in a `while` condition is `'?' in a while condition needs a local Result` (the call is not hoisted). Omitting `Err` from `match` is still `match does not cover 'Err'`. `bits 16`, `bits 32`, and `naked` stay `an enum is a 64-bit value`.
- `tests/lang/result.lap` prints 8, -3, 9, 104, -4, and 16 at `-O0` and `-O2`. `tests/flat/result.lap` is the same image at both levels and is not booted. Two `-c` objects link `add1` (`T add1`, `U add1`, prints 9). `examples/parse_result.lap` parses a sum with `Result` instead of a magic code: `12+3` prints 0 then 15, `7` prints 0 then 7, `12+` prints 1 then 3, `12a` prints 1 then 2, and an empty string prints 1 then 0. The note is `docs/DESIGN_ERRORS.md`.

### Left out
- Exceptions, `panic`, unwinding, libc, and a heap `Result`.
- `From` / `Into`, and `?` converting `E`.
- `try` blocks, `?` on `Option`, and `Ok` or `Err` with zero fields or more than one.
- An `Ok` whose field is an enum. An enum field is still not an enum.
- Hoisting `?` out of a `while` condition.
- Rewriting `bench/src/parse.lap`, rewriting `vec`, or a trait-based `Result`.
- A second lowering that branches on the raw tag. The only lowering is the match.

### Changed
- Language version is 0.42.0, on top of 0.41.0. 0.41.0 traits and static dispatch are unchanged: the copy still calls the impl symbol, and there is still no vtable. The 0.40.1 hosted `-O2` `while` whose equal arm is cold stays: that arm is `je` after the back edge, the tail falls through, and `align 32` is not applied to that shape. The 0.39.1 `while` stays: the not-equal arm falls through, one global array is `[r8+index*scale]`, and that loop label is `align 32`. The 0.38.1 note rejected sharing one register between two scalars whose live ranges do not overlap. That pass is not emitted. The 0.37.2 note rejected aligning `.text` to 16 or 32, and 0.37.1 rejected a direct `arr[i].field` store. Neither is emitted here.

### Measured
- No timed bench was added to `make test`. `bench/lowlat`, the SPSC ring, `bench/slice`, the 0.39 enum-versus-`if` windows, the 0.40.1 `book` and `match` windows, and the 0.41 trait listing were not run again.
- NASM listing (`nasm -f elf64 -l`), hosted. `fn add1(a: Result<i64, i64>) -> Result<i64, i64>` with `let x = a?` against the same function written `let x = match a { Ok(v) => v, Err(e) => { return Result<i64, i64>::Err(e); } }`, then `return Result<i64, i64>::Ok(x + 1)`. Local labels (`.L0`, `.L1`) stay inside the function. The first `ret` is the `Err` return. The last `ret` is the unreachable `leave` / `ret` that already follows the `Ok` return. Listing produced 2026-10-26T10:36:47Z.
- Those two `add1` bodies are the same bytes. The frame is the enum-return frame (`push rbp`), not the 0.41 slim frame. `sub rsp` is `4881EC90000000` (144) at `-O0` and at `-O2`. `-O0`, from the label through the first `ret`: 122 and 122. The whole function through the last `ret` is 204 and 204. Hosted `-O2`: 138 and 138 through the first `ret`, 221 and 221 through the last.
- `let x = ok_i(n)?` against `let t = ok_i(n); let x = match t { ... }` is also the same bytes. `sub rsp` is `4881ECA0000000` (160) at both levels. `-O0`: 130 and 130 through the first `ret`, 212 and 212 through the last. `-O2`: 172 and 172 through the first `ret`, 282 and 282 through the last. No exception machinery and no extra instruction.

## [0.41.0] - 2026-10-26

### Added
- `trait Ord { fn lt(a: Self, b: Self) -> bool }`, `impl Ord for i64`, and `fn max<T: Ord>(a: T, b: T) -> T`. The bound is checked when the copy is generated. That copy calls the impl by its symbol (`lt$Ord$i64`, `call lt$Ord$i64`). There is no vtable and no extra instruction. `T: Ord + Eq` is two bounds, and both must hold. A failed bound is `type 'Point' does not implement 'Ord'`: the message names the type and the trait, and the copy is not created. An unknown trait name on the template is `unknown trait 'Nope'`, once.
- `a.lt(b)` is sugar for `lt(a, b)`. `p->f(a)` is `f(*p, a)`. The `(` is what makes it a call: `p.x` stays a field. If no user function has that name, one matching impl rewrites the call to the impl symbol. None, on a dot call, is `no method 'lt' for 'Point'`. Two is `ambiguous method 'lt' for 'i64'`. A user function named `lt` wins, including `a.lt(b)`.
- `bool` is a 64-bit signed integer, the same width as `i64`. A comparison used as a value is still `i64`. `if` tests nonzero in `rax`, so `print_int` of that `bool` prints 0 or 1. `trait`, `impl`, `Self`, and `bool` are not keywords.
- `enum Option<T> { None, Some(T) }` when the substituted field is an integer or a pointer. `Option<i64>::Some(7)`, `Option<*u8>::Some("hi")`, and `fn wrap<T>(v: T) -> Option<T>` use the existing enum return (hidden pointer in `rdi`). `sizeof(Option<i64>)` is 16. A field that becomes a struct is `an enum field must be an integer or a pointer`. A bare name is `enum 'Option' needs type arguments`. An enum is still not a type argument. The concrete `enum Option { None, Some(i64) }` tests are unchanged. `vec` is not rewritten.
- A mismatched method is `method 'lt' of impl 'Ord' for 'i64' does not match the trait`. A second impl is `type 'i64' already implements 'Ord'`. A missing method is `impl 'Ord' for 'i64' is missing method 'lt'`. Traits and impls are flat only (`a trait must be in the flat namespace`, `an impl must be in the flat namespace`). `--freestanding` and `--flat` call `max$1$i64` and `lt$Ord$i64` with no `syscall`. The note is `docs/DESIGN_TRAITS.md`.

### Left out
- Default method bodies, associated types, `trait Foo<T>`, `where` clauses, specialization, trait objects, and a vtable.
- UFCS (`Ord::lt`), a generic method, and `x.f::<T>()`.
- An impl for a slice, an array, a `vec`, or `own *T`.
- Rewriting `vec`, or rewriting every existing `Option`. The generic enum is one proof. `match` of a call still needs a local enum.
- A trait or an impl inside a module, `pub trait`, and a method with no parameter. Resolution needs the receiver.
- Const generics, higher-kinded types, and using a trait name as a value.

### Changed
- Language version is 0.41.0, on top of 0.40.1. 0.40.0 monomorphization is unchanged. The 0.40.1 hosted `-O2` `while` whose equal arm is cold stays: that arm is `je` after the back edge, the tail falls through, and `align 32` is not applied to that shape. The 0.39.1 `while` stays: the not-equal arm falls through, one global array is `[r8+index*scale]`, and that loop label is `align 32`. The 0.38.1 note rejected sharing one register between two scalars whose live ranges do not overlap. That pass is not emitted. The 0.37.2 note rejected aligning `.text` to 16 or 32, and 0.37.1 rejected a direct `arr[i].field` store. Neither is emitted here.

### Measured
- No timed bench was added to `make test`. `bench/lowlat`, the SPSC ring, `bench/slice`, the 0.39 enum-versus-`if` windows, and the 0.40.1 `book` and `match` windows were not run again. The sources stayed in `/tmp/gbytes`.
- NASM listing (`nasm -f elf64 -l`), hosted, after merging 0.40.1. Handwritten `fn lt_i64(a: i64, b: i64) -> i64 { return a < b; }` and `fn max_i64` that calls it, against `impl Ord for i64` (`lt$Ord$i64`) and `fn max<T: Ord>` instantiated as `max<i64>` (`max$1$i64`), whose body is `if (lt(a, b)) { return b; } return a;`. Local labels (`.L0`, `.L1`) stay inside the function. Listing produced 2026-10-26T09:33:41Z.
- `lt_i64` and `lt$Ord$i64` are the same bytes. `-O0`, from the label through the first `ret`: 41 and 41. The whole function through the last `ret` is 46 and 46. Hosted `-O2`: 33 and 33 through the first `ret`, 45 and 45 through the last. Both `-O0` frames are `sub rsp` with immediate `4883EC50` (80). Both `-O2` frames are the slim frame, `sub rsp, 8` (`4883EC08`).
- `max_i64` and `max$1$i64` are the same length and the same instructions. The only differing bytes are the 4-byte displacement of `call` (`E8`). In the generic file `main` sits between `lt$Ord$i64` and `max$1$i64`. The handwritten file emits `max_i64` immediately after `lt_i64`. Masking that displacement makes the bytes identical. `-O0`: 44 and 44 through the first `ret` (they differ at offset 29), 57 and 57 through the last. `-O2`: 42 and 42 through the first `ret` (they differ at offset 21), 68 and 68 through the last. Both `-O2` bodies are `push r12`, `push r13`, `sub rsp, 8`, the two moves into those registers, `call`, `test rax, rax`, `jz`, `mov rax, r13`, `add rsp, 8`, `pop r13`, `pop r12`, `ret`. No vtable and no extra instruction.

## [0.40.1] - 2026-10-26

### Changed
- Language version is 0.40.1, on top of 0.40.0. 0.40.0 monomorphization is unchanged. The numbers below were measured against 0.39.1, before 0.40.0 landed on main. This merge does not retime them. Hosted `-O2` rewrites one more shape of `while`. The body is straight-line statements around a single `if (x == k) { cold }`. The equal arm has a nested `if`, `while`, `match`, `break`, `continue`, `return`, or a call. The other statements do not, and they do not call. The tail after the `if` is not a `let`. The equal arm is placed after the back edge (`je`). The tail falls through and jumps back once. A straight `else` stays on that path. This is the 0.39.1 layout extended to a cold equal arm, including a field compare, which is not a direct leaf branch. `align 32` is not applied here. When that arm always returns or breaks, the compare is one field of `arr[i]`, and the tail loads another field of the same element into a register before writing `i`, the address is computed once (`imul` when the size is not a power of two) and both loads use it. `-O0`, `--freestanding`, `--flat`, `--boot`, `bits 16` / `bits 32`, a `let` in the tail, and a volatile array stay on the ordinary top-tested loop. `tests/lang/o2_outline.lap` prints the same four lines at `-O0` and `-O2`. The 0.39.1 `if`/`else` form is unchanged, including `align 32`.

### Not shipped
- `align 32` on the cold-arm loop. On the book, padding that loop to a 32-byte boundary moved `main` by 17 bytes. Book median 45.8 (45.3–49.7) against 48.7 (48.2–51.4) without a clean separation, and `match` went the wrong way: 47.5 (47.4–51.0) against 46.3 (46.1–47.2), the new minimum above the old maximum. Without the pad the book separates and `match` does not move. The pad is not emitted.
- The address alone, and the branch layout alone. With `insert` and `main` held at the same addresses, the 0.39.1 book loop is 48.74 (48.14–49.05). Moving the cold arm and still computing `*12` twice is 45.18 (44.79–46.11). Computing the address once and leaving the cold arm in the middle is 47.92 (47.61–48.28); that range overlaps the old one. Both changes together are 37.49 (36.79–37.59), and gcc `-O2` in those rounds is 37.56 (37.23–38.04). The ranges of the pair and gcc overlap. Neither change is the whole gap. The compiler emits the pair as one shape of `while`.
- Replacing `imul $12` with `lea (i,i,2)` / `lea (base,i,4)`. On a 256-node miss walk, 400000 times, the two-`lea` form is 267.54 (267.35–267.76) and one `imul` of the same address is 267.61 (267.12–269.91). The ranges overlap. `*12` stays `imul`. `*10` stays `imul`, as in 0.30.1 and 0.39.1.
- Hoisting the id reload. The same walk with `id` reloaded from a stack slot each iteration is 293.68 (293.43–293.91), against 293.66 (293.25–295.93) when `id` is already in a register and copied to `rbx`. The ranges overlap. The book still compares with `[rbp-8]`.
- `fib`. Copihue is the two-call form. gcc `-O2` splits even and odd. rustc `-O` turns the second call into a loop (`n = n - 2`). Those are algorithm changes. The bytes are the 0.39.1 bytes. In the bench window below, copihue 392.17 (391.90–395.80), gcc 129.81 (129.53–130.21), rustc 192.69 (192.49–193.03). Checksum 102334155.
- `vec` push. The bench is `examples/lowlat/bench/vec.lap`, checksum `499999 500000 1000000`. Copihue and gcc tie: 8.44 (7.30–11.14) and 7.33 (7.07–8.48). rustc `-O` is 4.28 (4.10–5.03). The rust maximum is under the copihue minimum. The push loop is a length/capacity compare and an indexed store. The sum that follows is `paddq` unrolled by 16. That SIMD sum is not emitted. The bytes are the 0.39.1 bytes.
- The motor in `match` (`cross`) is calls and nested tests, not this `while`. It is not rewritten. A general inversion of `while` stays out, as in 0.27.2.

### Measured
- Probe window 2026-10-26T08:29:56Z — 2026-10-26T08:30:00Z, `taskset -c 0`, one warmup and seven runs, alternating order. Three loops that are not the book. A counted scan of a 12-byte struct (`k` then `v`, cold `return`) goes from 34.70 (33.89–37.40) to 23.06 (22.60–23.23). The new maximum is under the old minimum. A 32-element `i64` scan whose equal arm is twenty nested `if`s goes from 10.36 (10.15–10.81) to 7.27 (6.89–8.61). The new maximum is under the old minimum. That second loop has no second field load, so the difference is the branch layout. A 256-node link walk, 400000 misses, goes from 293.30 (293.10–295.28) to 267.21 (266.84–268.19). The new maximum is under the old minimum. In the earlier probe window 2026-10-26T08:28:00Z — 2026-10-26T08:28:07Z the same walk is 267.33 (266.89–270.25) against gcc `-O2` 266.93 (266.65–268.83). Those ranges overlap. Checksums stayed 200000, 12800000, and 102000000.
- Paired window 2026-10-26T08:31:01Z — 2026-10-26T08:31:24Z, one warmup and seven runs, alternating which binary ran first inside each round. Milliseconds, median (min–max). The before binary is 0.39.1. The after binary is this rewrite. `book`, `match`, `pool`, `fib`, `parse`, `sort`, and `list` use `taskset -c 0`. `spsc` uses `0,1`. `spmc`, `mpmc`, and `tpool` use `0,1,2,3`. `lpool` is `bench/lowlat/pool.lap` on cpu 0. `vec` is `examples/lowlat/bench/vec.lap` on cpu 0. Checksums stayed 2160927, 2853449, 2052000, 102334155, 41421435022848, 1070217027633, 274869518336, 500000500000, 1000000, and `499999 500000 1000000`. Every bench except `book` and `match` is the same bytes as 0.39.1. `parse` `.text` stays 618. `book` `.text` went from 3812 to 3793. `match` `.text` went from 5024 to 5005. `insert` stays at `0x40166b`. The cancel loop head stays at `0x401990` (`cancel.L52`). `main` moved from `0x401cfa` to `0x401ce7`.
- `book` 48.22 (48.11–48.55) to 36.86 (36.63–37.35). The new maximum is under the old minimum. gcc `-O2` in the same rounds is 37.36 (37.16–37.65), checksum 2160927. The ranges overlap. The ratio of the medians to gcc is 0.99.
- `match` 46.08 (45.97–46.47) to 45.66 (45.40–45.79). The new maximum is under the old minimum. gcc `-O2` in the same rounds is 31.93 (31.78–32.94), checksum 2853449. The ratio of the medians to gcc is 1.43. The inner cancel walk is the loop above. `cross` is unchanged.
- `pool` 12.18 (11.94–12.32) and 12.17 (11.95–12.49). `fib` 392.07 (391.86–396.00) and 392.17 (391.90–395.80). `parse` 13.59 (13.39–13.68) and 13.57 (13.51–13.72). `sort` 51.79 (51.71–52.55) and 51.70 (51.64–52.07). `list` 39.66 (39.51–39.93) and 39.76 (39.23–40.01). `lpool` 19.78 (19.59–20.16) and 19.89 (19.76–20.12). The ranges overlap. Same bytes.
- `spsc` 51.16 (47.58–53.12) and 55.96 (50.02–62.72). `spmc` 288.28 (221.77–327.95) and 291.40 (252.35–318.76). `mpmc` 191.71 (164.04–226.11) and 182.85 (166.34–241.68). `tpool` 273.08 (230.32–344.69) and 277.44 (266.22–352.68). The ranges overlap. Same bytes.
- `vec` 8.44 (7.30–11.14) and 7.48 (7.32–9.70). The ranges overlap. Same bytes. gcc and rustc are in the not-shipped note above; they ran in this window.

## [0.40.0] - 2026-10-26

### Added
- `fn max<T>(a: T, b: T) -> T` and `struct Par<A, B>` are generic. The compiler emits one ordinary copy for each combination the program uses. There is no dictionary and no runtime cost. A type argument is an integer, a pointer (`*T`, `*mut T`, `*volatile T`), or a struct, including an instantiated generic struct. `mut` is not part of the type, so `*mut i32` and `*i32` are one copy. An enum, a slice, an array, a `vec`, or `own *T` is rejected. `vec` and `Option` are not rewritten.
- `max(a, b)` infers `T` from the arguments. A bare `T` binds the whole argument type, including a pointer. A literal is `i64`, so `max(1, 2)` is `max<i64>`. Two arguments that disagree are `type argument 'T' is 'i32' and 'i64' in 'max'`. Nothing to infer from is `cannot infer type argument 'T' for 'make'`. `*T` against `*volatile u8` is `cannot infer type argument 'T' for 'star' from '*volatile u8'`. `max<i32>(a, b)` is the explicit form; a literal that fits is then checked as usual. The wrong count is `function 'max' takes 1 type argument, not 2`.
- The symbol is stable and works with modules and `-c`. `max<i32>` is `max$1$i32`. `id<*u8>` is `id$1$p$u8`. `Par<i32, *u8>` is `Par$2$i32$p$u8`. `Box<Par<i32, u8>>` is `Box$1$Par$2$i32$u8`. Inside a module that name is wrapped: `mem__max$1$i32`. The template is not a symbol. The defining object emits `global max$1$i32`. `extern fn max<T>` emits `extern max$1$i32`. `>>` inside a type-argument list is two `>`. A value `x >> 2` stays a shift, and `a < b` stays a comparison.
- There are no traits. The operation is checked when the copy is generated. `cannot apply '+' to 'Point' in 'add<Point>'` names the operator, the type, and the instantiation. The diagnostic points at the template. Integer copies keep that width's truncation. A parameter `s: []T` infers `T` from the element. `[]T` is still not a type argument. A struct literal writes the arguments (`Par<i32, i64> { a: 4, b: 5 }`). `--freestanding` and `--flat` use the same code when the function does not need `malloc`. The note is `docs/DESIGN_GENERICS.md`.

### Left out
- Traits, interfaces, `where` clauses, and specialization.
- Generic enums, generic methods, and using a generic name as a value (`let f = max`).
- A type argument that is an enum, a slice, an array, a `vec`, or `own *T`.
- Rewriting `vec` or `Option`.
- Const generics, higher-kinded types, and a generic global.
- COMDAT or a weak symbol, so two `-c` objects that both emit the same body still fail at `ld`.
- Inferring the arguments of a struct literal. Returning a struct from a generic function stays the unchecked `rax` path of a handwritten struct return.
- A `bits 16` or `bits 32` generic is not in the suite. The copy keeps `bits` and `naked`, and a `bits 16` `max<i32>` assembled; it was not booted.

### Changed
- Language version is 0.40.0, on top of 0.39.1. The 0.39.1 hosted `-O2` `while` whose body is one `if (x == k)` stays: the not-equal arm falls through, one global array is `[r8+index*scale]`, and that loop label is `align 32`. The 0.39.0 enum return (hidden pointer in `rdi`), pass-by-value, `_`, block arms, globals, and struct fields stay. Hosted `-O2` still loads an enum tag once into `rbx`. The 0.38.1 note rejected sharing one register between two scalars whose live ranges do not overlap. That pass is not emitted. The 0.37.2 note rejected aligning `.text` to 16 or 32, and 0.37.1 rejected a direct `arr[i].field` store. Neither is emitted here.

### Measured
- No timed bench was added to `make test`. `bench/slice`, `bench/lowlat`, and the 0.39 enum-versus-`if` windows were not run again. The sources stayed in `/tmp`.
- NASM listing (`nasm -f elf64 -l`), hosted, of `fn max_i32(a: i32, b: i32) -> i32 { if (a > b) { return a; } return b; }` against `fn max<T>(a: T, b: T) -> T` instantiated as `max<i32>`. The label length is not counted. `sub rsp, __frame_...` resolves to the same immediate (`4883EC50`, 80) in both. Listing produced 2026-10-26T08:11:52Z.
- `-O0`, from the label through the first `ret`: 52 bytes and 52 bytes. That `ret` is `return a`. After it, both still have the unreachable `jmp` and `return b`. The whole function through the last `ret` is 65 bytes and 65 bytes.
- Hosted `-O2`, from the label through the first `ret`: 43 bytes and 43 bytes. The first instruction is `cmp rdi, rsi`. That `ret` is the `a <= b` path (`movsxd` into `r12` and `r13`, then `mov rax, r13`). The `a > b` path is `jg` to `mov rax, rdi` / `ret` after that first `ret`. The whole function through the last `ret` is 59 bytes and 59 bytes. The instantiated copy adds no instruction. The same listing after merging 0.39.1, at 2026-10-26T08:18:23Z, is those same four counts.

## [0.39.1] - 2026-10-26

### Changed
- Language version is 0.39.1, on top of 0.39.0. Hosted `-O2` rewrites one shape of `while`. The body is straight-line statements around a single `if (x == k) { A } else { B }`. There is no nested `if`, `while`, `match`, `break`, or `continue`, and neither the condition nor the body calls. The compare is one `cmp` / `jcc` (`can_branch_cond`). The equal arm is the branch (`je`). The not-equal arm falls through, and each arm jumps back to the test. If the body indexes exactly one non-volatile global array, `lea r8, [name]` runs once before the loop and that array is `[r8+index*scale]` instead of a disp32 index. The loop label is NASM `align 32`. `-O0`, `--freestanding`, `--flat`, `--boot`, `bits 16` / `bits 32`, two global arrays, a call, and `asm()` stay on the ordinary top-tested loop. `tests/lang/o2_eqfall.lap` prints the same five lines at `-O0` and `-O2`.

### Not shipped
- The other differences in the hot loops, measured and left as they were. Multiplying by 10 stays `imul`. A modulo by a constant that is not a power of two stays `idiv`. A `while` is not inverted. Sort and `vec` push are not rewritten. `align` is not applied to every function or every loop (that was 0.37.2, and it made `parse` slower).
- `parse`, instruction by instruction. The 0.39.0 digit path, same bytes as 0.38.1, ten instructions, two taken branches: `movzbq 0x402004(%r12)`, `cmp $44` / `jne` into the digit arm, `lea -0x30`, `imul $10`, `add`, then a join `add $1` / `jmp`. gcc `-O2` walks a pointer. The digit path is the back edge: `lea (%rdx,%rdx,4)` and `lea -0x30(%rcx,%rdx,2)`, eight instructions, one taken `jne`. The fill is `mul` by `0xcccccccccccccccd`, `shr $3`, and `cmov`, not `idiv`. rustc `-O` unrolls the same loop by two and uses the same two `lea`s plus `add $-48`. The load is `movzbl disp(%rsp,%rdx,1)`.
- `sort`. Both inner shifts are already a taken `jg` back to the memory move. Copihue addresses `a[j]` as `disp(%rbp,%r13,8)` and keeps the bounds compare on the outer index. gcc `-O2` keeps the base in `%rcx`, shifts with `mov %rdx,(%rcx,%rax,8)`, and strength-reduces the fill `i * 97` to `add $0x61` / `and $0xfff`.
- `vec` push, fast path. Copihue compares length with capacity, takes `jb` over the grow, then scales the index by 8. gcc `-O2` keeps the end pointer in `%rsi` and the capacity in `%rbp`: `cmp` / `jne` / `mov %rbx, (%rsi)` / `add $8`. One taken branch, no scale.
- Which difference is the time. The fill's two `idiv`s are not the hot loop. A probe runs that fill 64 times (`/tmp`, not a bench). `idiv` median 21.75 (21.48–25.41) ms, the gcc magic multiply 6.32 (6.08–6.42). The ranges do not overlap. One fill is about 0.24 ms. `parse` spends about 8 ms in the inner loop, so the magic multiply is not the change. Replacing `imul $10` in the aligned loop with two `lea`s (`*5` then `*2`) is slower: 14.39 (14.17–14.65) against `imul` 13.56 (13.16–13.77). The minimum of the two-`lea` form is above the `imul` maximum. The gcc three-component `lea -48(c,v*2)` after `lea (v,v,4)` is faster by less: 13.09 (12.92–13.24), and its maximum is under the `imul` minimum. That is about 0.5 ms, not the 8 ms. It is not emitted. `*10` stays `imul`, as in 0.30.1.
- The same new loop bytes are slow when the test shares its 32-byte chunk with the `lea`, and fast when the label is on the next boundary. With no pad, `L10` is `0x4011aa`. A pad of 21 bytes puts it at `0x4011bf` (median 23.26 ms). A pad of 22 bytes puts it at `0x4011c0` (13.73 ms). Pads 23 through 28, and pad 32, stay at 13.5–13.7 ms. `align 32` is that boundary for this loop only. It is not the 0.37.2 pass.
- `sort`, copihue against gcc `-O2`, same checksum 1070217027633. Copihue 51.69 (51.60–51.73), gcc 72.43 (72.17–72.50). gcc's minimum is above copihue's maximum. Copying the gcc shape is not a win.
- `vec`, the bench in `examples/lowlat/bench/vec.lap`, checksum `499999 500000 1000000`. Copihue 7.16 (7.02–7.24), gcc 6.97 (6.78–7.50). The ranges overlap. The end pointer is not emitted.
- rustc `-O` `parse` in the same probe window: 13.26 (12.53–15.16) against the new Copihue binary 13.54 (13.38–13.72). The ranges overlap. The unroll and the SIMD fill are not emitted. Probe window 2026-10-26T07:29:10Z — 2026-10-26T07:29:12Z, `taskset -c 0`, one warmup and seven runs, alternating order.

### Measured
- Paired window 2026-10-26T07:26:52Z — 2026-10-26T07:27:13Z, one warmup and seven runs, alternating which binary ran first inside each round. Milliseconds, median (min–max). The before binary is 0.39.0. The after binary is this rewrite. The twelve benches are the same bytes as 0.38.1; 0.39.0 does not change them. `book`, `match`, `pool`, `fib`, `parse`, `sort`, and `list` use `taskset -c 0`. `spsc` uses `0,1`. `spmc`, `mpmc`, and `tpool` use `0,1,2,3`. `lpool` is `bench/lowlat/pool.lap` on cpu 0. Checksums stayed 2160927, 2853449, 2052000, 102334155, 41421435022848, 1070217027633, 274869518336, 500000500000, and 1000000. Every bench except `parse` is the same bytes as 0.39.0. `parse` `.text` went from 586 to 618. The inner label is `main.L10` at `0x4011c0`.
- `parse` 21.7 (21.3–21.9) to 13.6 (13.5–14.2). The new maximum is under the old minimum. gcc `-O2` in the same rounds is 13.0 (12.9–13.3), checksum 41421435022848. The ratio of the medians to gcc goes from 1.67 to 1.05.
- `book` 48.3 (48.1–48.9) and 48.4 (48.2–48.6). Same bytes. `match` 46.1 (45.9–46.7) and 46.1 (45.8–46.5). `pool` 12.2 (12.1–12.3) and 12.1 (11.9–13.4). `fib` 392.7 (391.9–399.5) and 392.0 (391.9–394.4). `sort` 52.3 (52.1–52.5) and 52.7 (52.2–54.1). `list` 39.9 (39.7–42.1) and 39.9 (39.6–40.0). `lpool` 20.0 (19.8–20.1) and 19.9 (19.8–20.3). The ranges overlap. Nothing in those binaries changed.
- `spsc` 47.2 (40.5–54.8) and 54.2 (44.5–60.8). `spmc` 223.6 (211.3–262.7) and 256.5 (235.2–268.9). `mpmc` 155.1 (150.0–167.5) and 157.8 (147.9–164.3). `tpool` 231.8 (207.8–259.2) and 230.4 (198.0–266.8). The ranges overlap. Those four binaries match 0.39.0 byte for byte.

## [0.39.0] - 2026-10-26

### Added
- `fn make(n: i64) -> Option` returns the enum through a hidden caller pointer in `rdi`. `rax:rdx` is not used: `Color` is 8 bytes, `Option` is 16, and `Pair` is 24, so one ABI covers every size. The callee saves `rdi` in an anonymous frame slot and writes the bytes through that pointer. After the drops, `rax` holds the address. Real arguments shift: the first five use `rsi`, `rdx`, `rcx`, `r8`, and `r9`, and the sixth is at `[rbp+16]`. `let x = give()` writes `x`. `return give()` forwards the caller's pointer. A discarded call allocates a temporary. `return o` from `-> i64` is still `an enum cannot be returned`.
- A parameter is still passed by address and copied. A global, a struct field, and a call that returns the enum take the same path. Assigning the parameter does not change the caller. Passing the same value twice is a copy, not a move.
- `_` covers the variants that are left and skips `ud2`. `Option::_` is `'_' is not a variant`. `_(x)` is `'_' has no fields` and does not cover. A second `_` is `'_' is already matched`. An arm after `_` is `arm after '_' is unreachable`. A block after `=>` is the arm. The last statement has to be an expression, or the caret is on `{` and the message is `match arm block needs a result`.
- `let g: Option;` is a zeroed BSS global. Tag 0 is `None` when `None` is first. An initializer is `an enum global starts at zero; assign it in a function`. `struct Box { o: Option, n: i64 }` can be stored, copied, returned, and matched, including `p->o`. An array of enums is still `an enum cannot be an array element`.
- Hosted `-O2` loads the tag once (`mov rbx, [slot]`, or `mov rbx, [name]`, or one load through a saved field pointer) and each arm is `cmp rbx, imm` / `jne`. The body may clobber `rbx`; the next compare is reached only by `jne`. `-O0`, `--freestanding`, and `--flat` still compare the tag in memory, so `tests/freestanding/enum.lap` and `tests/flat/enum.lap` keep the same bytes. The `Some` field is still spilled to a slot and reloaded.

### Left out
- `match` of a constructor, of a call, or of `*p` when `p` points at the enum (`match needs a local enum`).
- `|` patterns, guards, named fields, and an enum nested as a field.
- An enum inside a `module`, `bits 16`, `bits 32`, `naked`, `interrupt fn`, and `syscall fn`. An indirect call that would return an enum is `an indirect call cannot return an enum`.
- A slice element, an array element, and a `vec` element. An `own *T` field is still rejected, so a variant does not drop.
- Removing the payload spill. The register compare does not change `-O0`.

### Changed
- Language version is 0.39.0, on top of 0.38.1. The 0.38.1 note rejected sharing one register between two scalars whose live ranges do not overlap. That pass is not emitted. The 0.38.0 enum layout, exhaustive `match`, and `ud2` stay. The 0.37.2 note rejected aligning `.text` to 16 or 32, and 0.37.1 rejected a direct `arr[i].field` store. Neither is emitted here.

### Measured
- No timed bench was added to `make test`. `bench/slice` and `bench/lowlat` were not run again. The sources stayed in `/tmp`. Both programs print `80000000` and then `1`. The value is built once, outside an 80000000-iteration loop. `taskset -c 0`, one warmup and seven runs, `python` `time.perf_counter`. The 0.38 binary is `cb4dd52`. The `-O0` assembly of the `match` program is the same in both compilers.
- NASM listing (`nasm -f elf64 -l`), `-O0`, of `pick`, from the label through the first `ret`: 70 bytes on 0.38.0 and on 0.39.0. The `if` on `struct Opt { tag: i64, val: i64 }` is 76. A difference of 6. The 70 includes `ud2` (2 bytes), which a tag of 0 or 1 does not execute. After that `ret` both functions still have the unreachable `xor rax` / `leave` / `ret` (5 bytes). Hosted `-O2` is the change: 0.38 compares `qword [rbp-16]` twice; 0.39 does `mov rbx, [rbp-16]` once and then `cmp rbx, imm` twice. Both still spill the `Some` field (`mov [rbp-40], rax` / `mov rax, [rbp-40]`) and `push`/`pop` the sum. The `if` loads the tag once into `rax` and adds the field. Neither loop is unrolled.
- Window 2026-10-26T06:58:33Z — 2026-10-26T06:58:34Z, 0.39 `match` then 0.39 `if` on each round. `match`: 62.4, 60.8, 59.8, 63.5, 63.8, 63.6, 65.7 ms, median 63.5 (59.8–65.7). `if`: 57.0, 62.8, 63.7, 66.7, 67.5, 56.4, 64.0 ms, median 63.7 (56.4–67.5). The ranges overlap.
- Window 2026-10-26T06:58:34Z — 2026-10-26T06:58:35Z, 0.39 `if` then 0.39 `match`. `if`: 63.0, 70.8, 64.9, 65.5, 68.4, 67.5, 70.7 ms, median 67.5 (63.0–70.8). `match`: 61.9, 61.8, 61.5, 61.4, 59.9, 65.3, 62.1 ms, median 61.8 (59.9–65.3). The ranges overlap. The `if` median moved (63.7 against 67.5). The `match` median moved (63.5 against 61.8). The ratio of the medians is not stable.
- Window 2026-10-26T06:58:35Z — 2026-10-26T06:58:36Z, 0.38 `match` then 0.39 `match`. 0.38: 63.2, 65.1, 75.1, 62.3, 62.4, 64.7, 65.4 ms, median 64.7 (62.3–75.1). 0.39: 59.9, 61.3, 57.1, 59.2, 67.8, 57.9, 64.5 ms, median 59.9 (57.1–67.8). The ranges overlap.
- Window 2026-10-26T06:58:36Z — 2026-10-26T06:58:37Z, 0.39 `match` then 0.38 `match`. 0.39: 58.4, 66.5, 58.3, 63.2, 65.3, 65.9, 64.2 ms, median 64.2 (58.3–66.5). 0.38: 62.4, 62.4, 62.4, 62.4, 64.6, 62.4, 62.3 ms, median 62.4 (62.3–64.6). The ranges overlap. In this window the 0.38 median is the lower one. The register load does not separate from the spill, the second compare, or the `push`/`pop` of the sum.

## [0.38.1] - 2026-10-26

### Changed
- Language version is 0.38.1, on top of 0.38.0. The only edit in `compiler/bootstrap-c/copihue.c` on top of 0.38.0 is that version string. No instruction selection changed. The 0.38.0 enums and `match` are unchanged.

### Not shipped
- Hosted `-O2` sharing of one register by two scalars whose live ranges do not overlap. The first assignment stayed the 0.33.1 plan: callee-saved `r12`–`r15` first, names that cross a call before names that do not, then `r10` and `r11` only for names that do not cross a call. A second pass, only in a function that contains a call, placed a name that had stayed on the stack into a register that already held another name. It did not open another callee-saved slot. A name live across a call still could not use `r10` or `r11`. `own *T` was kept live until the return, because the drop is not an AST use. The overlap test was a backward walk of `if`, `while`, `break`, `continue`, and `return`. That is not the 0.33.1 share, which used a straight first-to-last tick, recorded a `while` condition before the body, and miscompiled the order book. `-O0`, `--freestanding`, `--flat`, `--boot`, and `bits 16` / `bits 32` were unchanged. This pass is not emitted.
- Choice, before the change. Hardware `cycles` and `instructions` are not available in this guest (`perf stat` reports them as not supported), so the split is dynamic instruction counts from one counter on each basic block of the 0.37.2 binaries, and `perf` `cpu-clock` at 4999 Hz for which function is running. The cpu-clock samples inside `fib` land on one `cmp` often enough that they are not an instruction mix. `book`, `fib`, and `parse` do not use `enum`, and their 0.38.0 text matches that count. Checksums stayed 102334155, 41421435022848, and 2160927.
- `fib`. 3311603014 instructions. `call`, `ret`, the frame (`sub`/`add rsp`), and the `push`/`pop` of `r12`/`r13` are 50.0%. The other 50.0% is the compare, the two `lea`s, and the add. Stack slots of a local that missed a register are 0. A leaf inliner does not apply: `fib` calls itself. The register plan already keeps `n` and the first result in `r12` and `r13`.
- `parse`. 337246641 instructions. Work is 100.0%. The inner loop is `movzx`, `imul`, `add`, and `jmp`, all in registers. The only stack operand on the timed path is `cmp r15, [rbp-16]` for the outer trip count, 513 times, 0.0002%. There is one call, the final print.
- `book`. 759730534 instructions. Work is 93.1%. Stack slots are 5.1%, and 4.03% of all instructions are one reload, `mov rbx, [rbp-8]`, the id in `cancel`'s order walk (30610896 times). Call, return, and frame traffic is 1.7%. The scratch `push`/`pop` of a field store is 0.1%. `cpu-clock` (3586 samples in the binary): `cancel` 71.8%, `find_level` 10.3%, `main` 7.2%, `best_ask` 3.3%, `rnd` 3.2%, `best_bid` 2.4%, `take` 0.9%, `insert` 0.8%.
- That is why the register pass was the one measured, not a leaf inliner. The 0.33.1 inliner (a direct call, no nested call, at most six scalar parameters, body weight at most 40, four passes, the result written into a destination local during codegen) made the book 57.21 against 49.74 and the pool 14.94 against 14.25. An AST inliner before register allocation would still be inlining leaves. Leaf-call overhead is 1.7% of the book's instructions, none of `parse`'s, and `fib`'s calls are not leaves. The reload in `cancel` is a name that dies before `nxt` is born on the match path and stays in `r10` on the miss path, so the two can share `r10`. On that build, `cancel` used `mov rbx, r10` instead of `mov rbx, [rbp-8]`. `main` kept `side` in `r15` after `op` died. `match`, `spmc`, `mpmc`, and `tpool` dropped a few stack slots the same way. `fib`, `parse`, `pool`, `sort`, `list`, `spsc`, and `bench/lowlat/pool` were the same bytes.
- Paired window 2026-10-26T06:21:59Z — 2026-10-26T06:22:27Z, one warmup and seven runs, alternating which binary ran first inside each round. Milliseconds, median (min–max). The before binary is 0.38.0. The after binary is the share pass. `book`, `match`, `pool`, `fib`, `parse`, `sort`, and `list` use `taskset -c 0`. `spsc` uses `0,1`. `spmc`, `mpmc`, and `tpool` use `0,1,2,3`. `lpool` is `bench/lowlat/pool.lap` on cpu 0. Checksums stayed 2160927, 2853449, 2052000, 102334155, 41421435022848, 1070217027633, 274869518336, 500000500000, and 1000000.
- `book` 49.4 (49.1–50.6) to 50.1 (49.7–50.9). The ranges overlap. `.text` went from 3812 to 3800. The median did not fall.
- `match` 47.0 (46.6–47.2) to 46.9 (46.5–47.4). Overlap. `.text` 5024 to 4995. `pool` 13.0 (12.8–13.7) to 13.1 (12.8–14.4). Overlap. Same 869 bytes. `fib` 393.3 (392.8–395.0) to 393.2 (392.7–396.4). Overlap. Same 385 bytes. `parse` 22.8 (21.9–23.0) to 22.8 (22.0–23.2). Overlap. Same 586 bytes. `sort` 52.9 (52.6–85.0) to 53.4 (52.7–75.3). Overlap. Same 659 bytes. `list` 40.6 (40.3–41.3) to 41.0 (40.6–42.0). Overlap. Same 586 bytes. `lpool` 21.0 (20.7–22.2) to 21.3 (20.6–21.7). Overlap. Same 775 bytes.
- `spsc` 73.3 (51.2–84.1) to 58.7 (52.9–65.9). Overlap. Same 706 bytes. `spmc` 326.0 (274.9–393.0) to 269.9 (237.4–298.9). Overlap. `.text` 1663 to 1658. `mpmc` 216.4 (198.1–254.9) to 216.8 (190.5–260.2). Overlap. `.text` 1932 to 1927. `tpool` 304.6 (285.9–330.9) to 282.0 (243.2–329.5). Overlap. `.text` 1506 to 1501.
- No bench has a new maximum under the old minimum, and the book median moved the wrong way inside the old range. The pass is not kept. `fib` stays about 3× C. The book stays about 1.28× C and 1.48× Rust, `match` about 1.43×, `parse` about 1.6×, and `vec` push about 1.6× Rust. Those ratios are the earlier windows. They were not retimed here.

## [0.38.0] - 2026-10-26

### Added
- `enum Name { None, Some(i64), ... }` is a sum type. A variant has zero or more fields. A field is an integer or a pointer. The value is an `i64` tag at offset 0 and a payload from offset 8, aligned to each field and rounded up to 8. `sizeof` is that size. `Option` is not a compiler type: the program writes the enum. `enum` and `match` are contextual, not keywords, so `let enum = 1` and `match(1)` stay a name and a call.
- `Option::Some(7)` and `Option::None` write a local: the slot is zeroed, then the tag and the fields. `x = y` copies the bytes when both sides are the same enum. A parameter is passed by address and copied, the same way a struct is. A constructor argument fills a temporary of that size.
- `match e { None => 0, Some(n) => n }` is an expression. `e` is a local or a parameter. Every variant is required (`match does not cover 'None'`). A duplicate arm is `variant 'Some' is already matched`. A field of the wrong type is `expected i64, found *u8`. The wrong arity is `variant 'Some' takes 1 field, not 0`. The lowering is `cmp qword [slot], tag` / `jne`. A tag that matches nothing is `ud2`. There is no heap and no libc. `--freestanding` and `--flat` use the same code when the enum does not need `malloc`.
- Hosted `-O2` keeps the enum on the stack and builds a normal `rbp` frame for a function with an enum parameter. The short `push`/`pop` frame has no `rbp`. Copying the tag through that frame wrote the caller's slots: `take` of an `Option` that was not the first local returned the wrong value. `tests/lang/enum_opt.lap` prints 2 and then 40 for that call. The `i64` beside the enum stays 40.

### Left out
- Returning an enum. `rax` holds one scalar, and a struct declared `-> T` already comes back only in `rax`. `-> Option` and `return o` are `an enum cannot be returned`.
- An enum global (`an enum must be a local or a parameter`), a struct field, a slice element, an array element, and a `vec` element.
- An enum inside a `module`. `pub enum` outside a module is the existing `pub is only valid inside a module`.
- `bits 16`, `bits 32`, and `naked`, including 64-bit `naked` (`an enum is a 64-bit value`).
- `match` of a pointer, of `*p`, or of a constructor that is not stored in a local (`match needs a local enum`). `&o` is the address of the bytes.
- `_`, `|` patterns, guards, block arms, named fields, and an enum nested as a field. An `own *T` field is rejected, so a variant does not drop. Reassignment copies the bytes.
- A `match` of only literals is `i64`, so `let x: i8 = match ... => 1` is `expected i8, found i64`. An arm that is itself an enum is `an enum is not a scalar; match it`.

### Changed
- Language version is 0.38.0, on top of 0.37.2. The 0.37.2 note rejected aligning `.text` to 16 or 32, and 0.37.1 rejected a direct `arr[i].field` store. Neither is emitted here. The 0.37.0 `vec` of structs, owning move, and slice view are unchanged.

### Measured
- No timed bench was added to `make test`. `bench/slice` and `bench/lowlat` were not run again. The sources of this comparison stayed in `/tmp`.
- NASM listing (`nasm -f elf64 -l`), `-O0`, of `pick`. The enum `pick` copies an `Option` parameter, matches `None => 0` and `Some(v) => v`, stores that in a local, and returns it. Bytes from `pick` through the first `ret`: 70. The same shape on `struct Opt { tag: i64, val: i64 }`, with `if (o.tag == 0) n = 0; else n = o.val; return n`: 76. A difference of 6. The match compare is `cmp qword [rbp-16], imm`. The `if` compare is `push` / `mov rbx` / `pop` / `cmp rax, rbx`. The 70 includes `ud2` (2 bytes), which a tag of 0 or 1 does not execute. After that `ret` both functions have the unreachable `xor rax` / `leave` / `ret` (5 bytes).
- Hosted `-O2`, `taskset -c 0`, one warmup and seven runs. The value is built once, outside the loop. The loop adds the payload `1`, 80000000 times. Both programs print `80000000` and then `1`. The `-O2` loop is not the `-O0` `pick`: `match` compares the tag in memory twice, spills the `Some` field into a slot, and `push`/`pop`s the sum. The `if` compares the tag once from a register and adds the field. Neither loop is unrolled.
- Window 2026-10-26T06:09:47Z — 2026-10-26T06:09:48Z, `match` then `if` on each round. `match`: 63.3, 63.0, 62.9, 62.9, 63.1, 62.9, 63.2 ms, median 63.0 (62.9–63.3). `if`: 62.3, 58.3, 55.5, 56.0, 55.8, 54.4, 56.1 ms, median 56.0 (54.4–62.3). The ranges do not overlap. The edges are 0.6 ms apart (62.3 against 62.9). The ratio of the medians is 1.13.
- Window 2026-10-26T06:10:09Z — 2026-10-26T06:10:10Z, `if` then `match` on each round. `match`: 63.1, 63.0, 63.1, 63.7, 63.2, 63.8, 62.9 ms, median 63.1 (62.9–63.8). `if`: 51.4, 48.8, 56.9, 50.9, 54.3, 54.5, 51.6 ms, median 51.6 (48.8–56.9). The ranges do not overlap. The edges are 6.0 ms apart. The ratio of the medians is 1.22.
- The `if` median moves between those windows (56.0 and 51.6). The `match` median does not (63.0 and 63.1). The fourteen `if` samples still top out at 62.3, which is under the `match` minimum of 62.9. The gap is small, and the ratio is not stable. `match` is the slower of the two in both windows.

## [0.37.2] - 2026-10-26

### Changed
- Language version is 0.37.2, on top of 0.37.1. The only edit in `compiler/bootstrap-c/copihue.c` on top of 0.37.1 is that version string. No instruction selection changed.

### Not shipped
- Hosted `-O2` NASM `align` before every function (`_start`, the four hosted helpers, and each user function) at 16 bytes and, separately, at 32 bytes. The head of each `while`, the `thread_join` spin, and the hosted rewrites that still have a counted back edge (the dot product, the link walk, the byte bump, the byte sum, and the insertion shift) were `align 16` in both builds. `-O0`, `--freestanding`, `--flat`, and `--boot` were not aligned. The padding is `nop`. It is not emitted.
- Displacement window 2026-10-26T05:44:30Z — 2026-10-26T05:44:45Z, `taskset -c 0`, one warmup and seven runs, rotating the three compilers and six pads (`times N nop` immediately after `section .text`, `N` in 0, 5, 11, 17, 23, 29). Milliseconds, median. Book spread of those six medians: 0.37.1 was 47.0 to 56.2 (9.1). Align 16 was 47.9 to 56.4 (8.5). Align 32 was 48.9 to 60.6 (11.8). Match spread fell: 47.3 to 48.4 (1.1), then 47.7 to 48.2 (0.5), then 47.6 to 47.8 (0.2). Checksums stayed 2160927 and 2853449.
- Align 32 does not pin the book. `insert` stays 0 mod 32, but two addresses 32 bytes apart are different mod 64. The binary this compiler emitted puts `insert` at `0x401720` (32 mod 64) and the median is 60.6 (60.2–60.9). The other five pads share `insert` at `0x401740` (0 mod 64) and the medians are 48.9 to 49.0. Same alignment class, different cache line slot, about 12 ms. Align 16 still has two slots mod 32, and the emitted one (`insert` at `0x4016c0`) is the slow one, 56.4.
- Paired window 2026-10-26T05:45:36Z — 2026-10-26T05:46:11Z, one warmup and seven runs, rotating 0.37.1, align 16, and align 32. These are the emitted binaries, not the pads. Milliseconds, median (min–max). `book`, `match`, `pool`, `fib`, `parse`, `sort`, and `list` use `taskset -c 0`. `spsc` uses `0,1`. `spmc`, `mpmc`, and `tpool` use `0,1,2,3`. `lpool` is `bench/lowlat/pool.lap` on cpu 0. Checksums stayed 2160927, 2853449, 2052000, 102334155, 41421435022848, 1070217027633, 274869518336, 500000500000, and 1000000.
- `book` 49.7 (49.4–50.7), align 16 is 56.6 (56.2–56.9), align 32 is 60.5 (60.2–61.1). Both new minimums are above the old maximum.
- `parse` 22.8 (22.6–23.1), align 16 is 29.5 (29.3–30.0), align 32 is 27.2 (26.8–27.7). Both new minimums are above the old maximum.
- `fib` 393.8 (393.3–394.6), align 16 is 346.2 (345.8–348.0), align 32 is 395.6 (395.1–404.0). Align 16 is below the old minimum. Align 32's minimum is above the old maximum. `.text` is 385, 430, and 478 bytes.
- `match` 47.7 (47.3–49.1), 48.2 (47.8–48.3), 48.0 (47.7–48.4). The ranges overlap. `pool` 13.3 (13.1–13.5), 13.4 (13.1–14.0), 13.7 (13.3–13.9). Overlap. `sort` 53.4 (52.9–54.1), 53.2 (53.0–54.0), 53.3 (53.3–53.8). Overlap. `list` 41.3 (40.8–41.6), 41.1 (40.8–41.3), 41.0 (40.8–41.4). Overlap. `lpool` 21.2 (20.9–21.8), 21.3 (20.9–21.6), 21.2 (20.8–21.9). Overlap.
- `spsc` 58.4 (34.1–76.1), align 16 is 8.3 (7.0–28.5), align 32 is 24.2 (7.3–78.9). Align 16's maximum is below the old minimum in this window. The run is a two-thread ring and the ranges are wide. `spmc` 298.4 (241.4–361.2), 281.0 (265.9–329.0), 249.9 (229.8–384.6). Overlap. `mpmc` 201.4 (172.3–235.0), 198.9 (189.7–251.2), 210.2 (188.0–237.7). Overlap. `tpool` 291.5 (257.2–332.2), 290.5 (222.7–300.6), 275.3 (244.6–303.2). Overlap.
- `.text` bytes, 0.37.1 then align 16 then align 32: book 3812, 3976, 4088; match 5024, 5240, 5384; pool 869, 944, 992; parse 586, 648, 680; sort 659, 718, 750; list 586, 640, 672; spsc 706, 783, 815; spmc 1663, 1805, 1853; mpmc 1932, 2085, 2133; tpool 1506, 1612, 1644; lpool 775, 864, 896. The book spread did not fall, and `book` and `parse` got slower at both alignments, so neither build is kept. `fib` at 16 bytes is not kept on its own.

## [0.37.1] - 2026-10-26

### Changed
- Language version is 0.37.1, on top of 0.37.0. The only edit in `compiler/bootstrap-c/copihue.c` on top of 0.37.0 is that version string. No instruction selection changed. The 0.37.0 struct `vec`, owning move, and slice view stay. The 0.36.1 `lock add` and direct `&name` operand stay.

### Not shipped
- Hosted `-O2` store of `arr[i].field = v` as one memory operand. The index had no call and no assignment. The value was an immediate, a callee-saved register (`r12`–`r15`), or a scalar slot. The index was scaled once (`shl` for a power of two, `imul` otherwise, including `*12`) and the store was `mov dword [orders+reg+off], …`. A call, an assignment in the index, a volatile immediate, and any other value stayed on the push path. `-O0`, `--freestanding`, `--flat`, and `--boot` were unchanged. This is not the 0.33.1 leaf inliner, not a fifth callee-saved register, and not the `*12` `lea`.
- Paired window 2026-10-26T05:20:04Z — 2026-10-26T05:20:17Z, same machine, one warmup and seven runs, alternating which binary ran first. Milliseconds, median (min–max). The before binary is 0.36.1. The after binary is that store. Checksums stayed 2160927, 2853449, 2052000, 274869518336, and 500000500000.
- `examples/lowlat/bench/book`, `taskset -c 0`: 49.4 (48.9–49.6) to 59.2 (58.9–59.8). The new minimum is above the old maximum. The text went from 1330 lines to 1116. `insert` moved from `0x40166b` to `0x40157d` and `cancel` from `0x4018f9` to `0x40174d`. The stores are shorter. The book is slower.
- `examples/lowlat/bench/match`, `taskset -c 0`: 46.8 (46.5–49.1) to 47.1 (46.9–47.4). The ranges overlap.
- `examples/lowlat/bench/pool`, `taskset -c 0`: 13.1 (12.6–13.3) to 13.3 (12.9–13.5). The ranges overlap. `bench/lowlat/pool.lap` was the same bytes and was not retimed.
- `bench/src/list`, `taskset -c 0`: 41.0 (40.8–41.4) to 41.1 (40.8–41.4). The ranges overlap. The walk is still the load. The difference is the init stores.
- `bench/lowlat`, `taskset -c 0,1,2,3`: `mpmc` 163.7 (158.0–170.9) to 162.8 (140.7–171.3), `spmc` 236.6 (207.2–287.5) to 264.1 (214.6–285.4), `tpool` 219.1 (205.5–255.1) to 230.5 (202.9–256.8). All three ranges overlap. `spsc` was the same bytes and was not retimed.
- The same bytes as 0.36.1, so not retimed: `bench/src` `fib`, `parse`, `matmul`, `arith`, `chain`, `fold`, `array`, `array_global`, `sort`, `fill`, `copy`, `hash`, `sieve`, and `examples/lowlat/bench` `vec`, `share`, `split`, `pin`, `same`, `free`, `watch`. `fib` stays about 3× C. `parse` and `vec` were not this change.
- A diagnostic pass, not the compiler, put 462 `nop`s in that book assembly so `book_init`, `take`, `give`, `find_level`, `add_level`, `drop_level`, `insert`, `cancel`, `best_bid`, `best_ask`, `rnd`, and `main` start at the 0.36.1 addresses. Checksum still 2160927. Against 0.36.1, `taskset -c 0`, one warmup and seven runs, alternating: 49.7 (49.3–50.1) and 49.4 (49.1–49.8). The ranges overlap. The shorter stores are not a win once the later functions stay put, and the unpadded form is a regression. The `nop`s are not emitted. This is the same address shift as the 0.35.1 note that kept the pointer walk off a loop with no call.

## [0.37.0] - 2026-10-26

### Added
- `let v: vec T` accepts a struct. `vec_push(v, x)` copies `sizeof(T)` bytes from an addressable value (a local, an index, or a field) into `ptr + len * sizeof(T)`. Growth is unchanged: capacity 4, then double, `rep movsb`, `free` of the old buffer. A struct literal is `cannot copy this struct value`. A name that is not an integer, a pointer, or a struct is `a vec element must be an integer, a pointer, or a struct`. A slice, an array, a `vec`, or `own *T` is `a vec element cannot be a slice, an array, a vec, or an owning pointer`. `for x in` of a struct element stays `for in of a struct element; index the slice`, and that diagnostic points at the loop name.
- `let s: []T = v` copies the pointer and the length. The slice does not own the buffer. A later `vec_push` that reallocates, or the vec's drop, leaves it dangling. There is no lifetime check. A different element type is `expected []U, found vec T`.
- `fn give() -> own *T` and `fn take(p: own *T)` move an owning pointer. `return p` loads it and stores null before the callee's drops, so that drop is `free` of null. Passing an owning local stores null after the pointer is in the argument register. The same owner twice in one call, or a borrow beside that move, is `an owning pointer cannot be moved twice`. A raw `*T` is `an owning parameter takes an owning pointer`. `let q: *T = give()`, a discarded `give();`, and passing `give()` to `*T` are `an owning pointer must be moved into 'own *T'`. `let q: own *T = give()` and `take(give())` are the move. Passing an owning local to `*T` still borrows it and does not store null. `let q: own *T = p` is still `an owning pointer cannot be copied`. `return p` from a function that is not `-> own *T` is still `an owning pointer cannot be returned`. `--freestanding`, `--flat`, `--boot`, `bits 16`, `bits 32`, `naked`, `interrupt fn`, and `syscall fn` still reject both.

### Left out
- Inferring a pointer parameter from an index or a dereference. `fn sumar(datos, n) { return datos[0]; }` is still `cannot index 'datos'`. A load used as an integer does not fix the pointee: a narrower integer still type-checks in a wider destination, `*i8` and `*u8` are the same width, and `*p` of a non-volatile pointer is a 64-bit load. A field does not pick one struct. Declaring `: *T` is the type. `vec` and `own` are still not globals or struct fields. A `vec` is still not a parameter or a return. An indirect call does not move. A second drop after an explicit `return` is still emitted and is not executed. `let q: own *T = p` does not move; it is still a copy error. Reassignment still does not free and replace.

### Changed
- Language version is 0.37.0, on top of 0.36.1. The 0.36.1 `lock add` and direct field address are unchanged.

### Measured
- No timed bench was added to `make test`. `bench/slice` and `bench/lowlat` were not run again.
- NASM listing (`nasm -f elf64 -l`), `-O0`, of `give` / `take` / `go`. `give(n)` does `let p: own *i64 = malloc(8); *p = n; return p` with `-> own *i64`, against the same function returning `*i64` and not freeing. Live bytes until the first `ret`: 163 versus 106, a difference of 57. That is `mov qword [rbp-16], 0` (8) and the drop of that null, which runs before `ret` (`push rax` / `pop rax`, the `free` of null, and the slot clear). After that `ret` the unreachable second drop is 52 bytes. The explicit function's dead epilogue is 5 (`xor rax` / `leave` / `ret`).
- `take(p: own *i64)` against `take(p: *i64)` that `free`s and returns the word: 78 versus 64, a difference of 14. That is `push rax` / `pop rax` (1 + 1), `push r11` / `pop r11` (2 + 2), and `mov qword [rbp-8], 0` (8). The explicit `free` does not save `r11` and does not clear a slot. Its dead tail is 5 bytes; the owning parameter's unreachable second drop is 52.
- `go` does `let p: own *i64 = give(1); return take(p)` against `let p: *i64 = give(1); let n = take(p); p = 0; return n`. Live bytes: 94 versus 54, a difference of 40. Eight of those are `mov qword [rbp-8], 0` after the pointer is in the argument register and before `call take`. The rest is the exit drop of that now-null local. The explicit `p = 0` is `mov rax, 0` and `mov [rbp-8], rax` and does not free.
- Hosted `-O2`, `taskset -c 0`, one warmup and seven runs. Window 2026-10-26T05:05:54Z — 2026-10-26T05:05:59Z. Both struct programs print 20000100000. `vec_push` of a packed 12-byte `Cell` (`i64` plus `i32`), 200000 times, then a sum of the fields: samples 3.3, 3.8, 3.7, 3.9, 3.7, 4.0, 3.5 ms, median 3.7 (3.3–4.0). The same growth written with `malloc`, a copy of whole qwords and a byte tail, and `free`: 4.4, 4.1, 4.1, 4.3, 4.3, 4.2, 4.3 ms, median 4.3 (4.1–4.4). The ranges do not overlap. The ratio of the medians is 1.16. The explicit copy is a call; the vec growth is `rep movsb` and the element copy is inlined. The gap is one tenth of a millisecond at the edges (4.0 against 4.1), so this is not a large or stable advantage.
- The same window, 100000 moves of `take(give(i))`. Both print 4999950000. Owning return and parameter: 354.5, 357.3, 356.0, 356.0, 358.9, 360.0, 360.2 ms, median 357.3 (354.5–360.2). The explicit `*i64` return and `free` in `take`: 361.4, 361.0, 360.4, 362.2, 361.0, 361.1, 349.6 ms, median 361.0 (349.6–362.2). The ranges overlap. The time is `mmap` and `munmap`. The null stores are not visible in the run time.

## [0.36.1] - 2026-10-26

### Added
- Hosted `-O2` emits a discarded `fetch_add` or `atomic_add` as `lock add`. An addend that fits in a signed 32-bit immediate is immediate (`lock add qword [cell], 1`). When the caller uses the old value or the new value, the instruction stays `lock xadd`. `-O0`, `--freestanding`, `--flat`, and `--boot` keep `lock xadd`.
- The same mode addresses `&name` and `&name.field` as one memory operand when `name` is a global or a stack slot and not a pointer or an array. A field of a field adds the offsets. A pointer variable and an indexed element still compute the address.
- `tests/lang/o2_atomic.lap` prints 0, 6, 10, 2, 9 at `-O0` and `-O2`. The `-O2` assembly contains `lock add qword [cell], 1`, `lock add qword [pair], 1`, `lock add qword [pair+8], 2`, and `lock xadd`.
- `examples/lowlat/vec.lap` pushes `0..99` into an empty `vec i64` and sums the walk. It prints `100 4950 128` (length, sum, capacity). The first growth allocates 4 and later growth doubles.
- `examples/lowlat/bench/vec.lap`, `vec.cpp`, and `vec.rs` push `0..999999` with no reserve and sum the elements. The line is `499999 500000 1000000`: the sum 499999500000 split into two fields under 9 digits, then the length. C++ is `std::vector`. Rust is `Vec`.

### Changed
- Language version is 0.36.1, on top of 0.36.0. The 0.36.0 parameter and return inference, `vec`, and `own` are unchanged.

### Measured
- The counted `fetch_add` loop was reloading the cell (`lea`, `push`, `pop r11`) and doing `mov rax, 1` / `lock xadd`. `xadd` writes `rax`, so the next `mov rax, 1` waits on it. The index already lived in `r12` and the limit was an immediate `cmp`. Nothing in the loop spilled the counter. gcc `-O2` and rustc `-O` emit `lock add [rip], 1` (and unroll). This change does not unroll and does not align the loop.
- Paired window 2026-10-26T04:44:36Z — 2026-10-26T04:44:44Z, same machine, no `taskset`, one warmup and seven runs, alternating which Copihue binary ran first. Milliseconds, median (min–max). The before binary is 0.35.2. The after binary is this `lock add`. Checksums stayed 8000000.
- `split` 34.8 (34.6–34.9) to 20.4 (20.3–20.6). The ranges do not overlap. C++ in that window was 21.4 (20.9–21.7) and Rust 21.1 (20.8–21.4). The new maximum is below both minimums.
- `free` 34.7 (30.1–34.9) to 20.4 (20.4–20.6). C++ 21.3 (21.1–21.4), Rust 21.2 (20.7–21.3). The new maximum is below both minimums.
- `same` 60.4 (60.2–60.4) to 39.1 (38.8–39.3). C++ 39.7 (39.7–39.8), Rust 39.6 (39.4–39.7). The new maximum is below both minimums. Pinning both threads to cpu 0 is still slower than pinning them apart: `same` stays above `pin` and `free`.
- `pin` 35.0 (34.7–45.9) to 20.6 (20.3–27.6). The ranges do not overlap. C++ 21.5 (21.4–21.6) and Rust 21.1 (20.7–21.4) overlap the new range.
- `share` 160.0 (149.8–184.5) to 147.0 (121.9–155.3). The ranges overlap, and the new range overlaps C++ 137.4 (131.2–151.6) and Rust 154.3 (137.5–159.5). The shared line still dominates. The same `lock add` stays, because `split`, `free`, `same`, and `pin` got faster.
- `examples/lowlat/bench/pool`, `book`, and `match`, and `bench/lowlat` `spsc`, `pool`, `spmc`, `mpmc`, and `tpool`, are the same bytes as 0.36.0, and 0.36.0 is the same bytes as 0.35.2. `watch` is the same bytes too. They were not retimed. The five atomic benches compiled by 0.36.0 are also the same bytes as 0.35.2, and this tree's binaries match the after side of the paired window.
- `examples/lowlat/RESULTS.md` is the 2026-10-26T04:56:51Z — 2026-10-26T04:57:04Z window. The header prints Copihue 0.36.1. It is not the paired window above. Pool, book, match, and watch are unchanged bytes, so those rows are a new window of the same code. In this window `split`, `same`, and `free` are below both C++ and Rust. `share`, `pin`, and `watch` overlap both.
- `vec` in that window is 8.1 (7.7–8.4). C++ `std::vector` is 7.7 (7.5–8.3) and the ranges overlap. Rust `Vec` is 5.1 (4.7–5.4). Copihue's minimum is above Rust's maximum. The line is `499999 500000 1000000`. `taskset -c 0`. No reserve on any of the three.

## [0.36.0] - 2026-10-26

### Added
- An omitted parameter type and an omitted `->` are `i64`. An explicit `: T` or `-> T` still wins. `let x = e` is unchanged: an integer, including a narrow load, is `i64`; a pointer is that pointer; a struct is not inferred. Indexing does not turn an untyped parameter into `*i64`. `fn sumar(datos, n) { return datos[0]; }` is `cannot index 'datos'`. `for x in` on a slice was already 0.34.0.
- `let v: vec T` is a hosted local of 24 bytes: a pointer, an `i64` length, and an `i64` capacity, starting empty. `T` is an integer or a pointer. `vec_push(v, x)` appends one element and returns the new length. The first growth allocates 4 elements and later growth doubles, using the existing `malloc` and `free` (`mmap` / `munmap`, no libc). `v[i]` checks the length and writes `index out of range` (exit 1). `v.ptr`, `v.len`, and `v.cap` are readable. `mmap` failure writes `out of memory` and exits 1. `for x in v` copies the pointer and the length at entry and does not own the buffer.
- `let p: own *T = e` is a hosted pointer local. It is `free`d when its block ends, and on `return`, `break`, and `continue`. `free` of null does nothing, and the slot is then set to null. An uninitialized owner is null. Passing it to a `*T` parameter borrows it: the callee must not free it, and the owner still does.

### Left out
- Inferring `*i64` from a later index. Move, free-on-reassign, returning or copying an owner, a `vec` of structs, and `let s: []T = v` (`a vec cannot be copied`). `vec` and `own *T` are not parameters, returns, globals, or struct fields. A second drop after an explicit `return` is still emitted and is not executed. `vec` and `own` stay contextual, so a parameter named `vec` still compiles.

### Changed
- Language version is 0.36.0, on top of 0.35.2.

### Measured
- No timed bench was added to `make test`. `bench/slice` and `bench/lowlat` were not run again.
- `fn fib(n)` and `fn fib(n: i64) -> i64` assemble to the same bytes at `-O0` and at `-O2` (`diff -q` empty). `let x = 5` and `let x: i64 = 5` match at `-O0`. The type default adds 0 bytes.
- NASM listing (`nasm -f elf64 -l`) of `let p: own *i64 = malloc(8); *p = 1; return *p;` against `let p: *i64 = malloc(8); *p = 1; let r = *p; free(p); return r;`. Live bytes of `go` until the first `ret`: 155 versus 149, a difference of 6. That is `push r11` / `pop r11` (2 + 2) and `mov qword [rbp-8], 0` (8), against the explicit function's second local (`mov [rbp-16], rax` and `mov rax, [rbp-16]`, 4 + 4) replaced by `push rax` / `pop rax` (1 + 1). After that `ret` the owner still has an unreachable drop. The owner function is 207 bytes and the dead tail is 52; the explicit function's dead epilogue is 5 (`xor rax` / `leave` / `ret`). The unreachable drop is 47 bytes. It is not removed: a function that returns and also falls off the end would need the later drop, and zeroing the slot makes a second `free` a no-op when it does run.
- Hosted `-O2`, `taskset -c 0`, one warmup and seven runs, summing `0 .. 999999` by growing a buffer from capacity 4 and doubling. Window 2026-10-26T04:24:56Z. `vec_push` samples were 7.8, 7.5, 7.5, 7.6, 7.7, 7.4, 7.5 ms, median 7.5 (7.4–7.8). The same growth written with `malloc`, a copy, and `free` was 7.1, 6.7, 7.4, 7.1, 7.2, 7.0, 6.9 ms, median 7.1 (6.7–7.4). Both printed 499999500000. The ranges meet at 7.4 ms, so this is not a stable speedup or slowdown. An earlier window of 100000 elements (2026-10-26T04:24:37Z) overlapped more: `vec` median 1.9 (1.8–2.2), manual median 1.8 (1.5–2.0).

## [0.35.2] - 2026-10-26

### Added
- `examples/lowlat/share.lap` runs two threads. Each `fetch_add`s only its own counter, 1000 times. One struct is `align(64)` with both fields at their natural offsets (`sizeof` 64, same cache line). The other repeats `align(64)` on each field (`sizeof` 128, second counter at offset 64). The lines are `share 64`, `2000`, `split 128`, `2000`.
- `examples/lowlat/pin.lap` calls `sched_setaffinity` on the current thread. Cpu 0 returns 0. Cpu 1024 returns -1 and does not enter the kernel. Two workers then pin themselves to cpu 0 and cpu 1 and add their own counters. The lines are `self 0`, `bad -1`, `cpu0 0`, `cpu1 0`, `2000`.
- `examples/lowlat/watch.lap` starts a thread that release-stores a heartbeat from 1 to 1000 and then stops. Main waits for that stop and acquire-loads the word until 100 samples match. The quiet window is a count of loads, not a wall-clock timeout. The lines are `beat 1000`, `miss 0`.
- The timed forms are `examples/lowlat/bench/share`, `split`, `pin`, `same`, `free`, and `watch`, each with a C++ file and a Rust file. `share` and `split` do 4000000 `fetch_add`s per thread and print 8000000. `pin` fixes the threads to cpu 0 and cpu 1, `same` fixes both to cpu 0, and `free` does not call `sched_setaffinity`. Those three use the split layout. A failed pin prints 0. `watch` stores 4000000 heartbeats and then requires 1000000 identical loads. It prints `4000000 0`. Rust calls syscall 203 itself: `std` has no affinity function. C++ uses `sched_setaffinity`.

### Changed
- Language version is 0.35.2, on top of 0.35.1. The only edit in `compiler/bootstrap-c/copihue.c` is that version string. No instruction selection changed.
- `examples/lowlat/bench/run.sh` still times the pool, the book, and the engine with `taskset -c 0` against C and Rust. It also times the six programs above with no `taskset`, against C++ and Rust, and rewrites `examples/lowlat/RESULTS.md`.

### Measured
- `examples/lowlat/RESULTS.md` is the 2026-10-26T04:26:27Z — 2026-10-26T04:26:40Z window. The binary prints 0.35.2. One warmup and seven runs. The machine reports 4 CPUs. Milliseconds, median (min–max).
- False sharing is visible on all three compilers. `share` against `split`: Copihue 158.1 (155.0–170.3) against 34.5 (33.7–34.8), C++ 144.8 (132.1–155.9) against 21.0 (20.7–21.4), Rust 151.0 (136.7–255.0) against 20.8 (20.5–21.6). Each minimum of `share` is above the maximum of `split`. The ratio of the Copihue medians is 4.58.
- On the shared line Copihue overlaps C++ and Rust. On the split line Copihue is above both: 34.5 (33.7–34.8) against C++ 21.0 (20.7–21.4) and Rust 20.8 (20.5–21.6). The ratio to C++ is 1.64. The ratio to Rust is 1.66. The field `fetch_add` is `lea`, `push`, `pop r11`, `lock xadd`. C++ and Rust emit `lock xadd` on the symbol. That was not changed.
- Affinity succeeded: `pin` and `same` print 8000000, not 0. Pinning both threads to cpu 0 is slower than pinning them to cpu 0 and cpu 1, on all three compilers. `same` against `pin`: Copihue 60.2 (60.0–64.0) against 34.7 (34.5–45.8), C++ 39.4 (39.3–39.7) against 21.1 (20.9–21.4), Rust 39.2 (39.0–39.3) against 20.7 (20.3–21.1). The ranges do not overlap. The ratio of the Copihue medians is 1.73.
- Pinning to cpu 0 and cpu 1 overlaps leaving the threads unpinned. `pin` against `free`: Copihue 34.7 (34.5–45.8) against 34.6 (34.3–34.9), C++ 21.1 (20.9–21.4) against 21.2 (20.6–21.5), Rust 20.7 (20.3–21.1) against 20.9 (20.6–21.5). On this machine the scheduler already spreads the two threads, so a separate pin does not show a further gain.
- Copihue is above C++ and Rust on `pin`, `same`, and `free`. `pin` 34.7 (34.5–45.8) against C++ 21.1 (20.9–21.4) and Rust 20.7 (20.3–21.1): ratios 1.64 and 1.68. `same` 60.2 (60.0–64.0) against C++ 39.4 (39.3–39.7) and Rust 39.2 (39.0–39.3): ratios 1.53 and 1.54. `free` 34.6 (34.3–34.9) against C++ 21.2 (20.6–21.5) and Rust 20.9 (20.6–21.5): ratios 1.63 and 1.66.
- The watchdog prints `4000000 0` in all three languages. Copihue 2.8 (2.5–3.2) overlaps C++ 3.2 (3.1–3.4) and Rust 2.9 (2.3–3.3). The run is a few milliseconds. There is no order to claim.
- The pool, the book, and the engine were timed again because the script rewrites the whole file. The generator is the 0.35.1 one. Pool 13.1 (12.8–13.4) overlaps the 0.35.1 window 13.0 (12.6–13.2) and is still above C 8.8 (8.4–9.0) and Rust 9.0 (8.6–9.4). Book 49.4 (49.1–50.1) overlaps 49.4 (49.1–49.9) and is still above C 38.5 (38.2–38.9) and Rust 33.4 (33.1–33.7). Match 47.3 (46.7–47.4) overlaps 47.1 (46.9–47.5) and is still above C 33.1 (32.5–33.3) and Rust 33.4 (33.0–33.6). Checksums 2052000, 2160927, 2853449. `bench/lowlat` was not run again.

## [0.35.1] - 2026-10-26

### Added
- Hosted `-O2` rewrites a struct-array index in a loop that contains a call. The index must be the only write of an `i64`, `u64`, or untyped local, and that write must be `i = i + K` or `i = i - K` with `K` a positive constant. The element size must not be a power of two (8, 16, and 32 stay `shl` or a SIB scale). The loop then keeps a pointer and adds `sizeof(element) * K` on the back edge. `sum = sum + p->field` through that pointer still adds into a callee-saved sum. A local array is rewritten only when `i` starts at a nonnegative constant, the condition is `i < N`, and `N` fits the declared length. A name that starts with `__`, a name whose address is taken, a narrow integer, a volatile array, and a whole-element load or store stay indexed. A loop with no call stays indexed.
- `tests/lang/o2_stride.lap` prints 36, 36, 16, 36, 18, 46, 8 at `-O0` and `-O2`. The two loops that call another function are the ones the rewrite applies to. `tests/lang/o2_stride_oob.lap` loads past a local `[Cell; 4]` and exits 1 with `Array bounds error` at both levels. `tests/lang/o2_ix12.lap` walks `orders[cur].next` on a 12-byte struct and prints 120 at both levels. That walk is data-dependent, so it stays an index.

### Changed
- Language version is 0.35.1, on top of 0.35.0. The 0.35.0 `--flat --elf` output and the `--checked` local-array trap are unchanged. `-O0`, `--freestanding`, `--flat`, `--boot`, and `bits 16` / `bits 32` do not do this rewrite.

### Measured
- `examples/lowlat/RESULTS.md` is the 2026-10-26T04:05:14Z — 2026-10-26T04:05:17Z window. The binary prints 0.35.1. One warmup and seven runs, `taskset -c 0`. The checksums are 2052000, 2160927, and 2853449. Milliseconds, median (min–max).
- Pool 13.0 (12.6–13.2). The previous examples window, 2026-10-26T02:32:11Z — 02:32:14Z, binary 0.33.1, was 14.2 (13.6–15.1). The new maximum is below the old minimum. Copihue is still above C 8.8 (8.5–9.1) and Rust 9.1 (8.7–9.5). The ratio to C is 1.48. It was 1.61. The release loop is now one `imul` before the loop and `add r14, 24` on the back edge. `alloc` and `release` are still calls. gcc `-O2` still inlines both and walks with `addq $24`.
- Book 49.4 (49.1–49.9) against 49.5 (49.1–51.6). The ranges overlap. The book binary is the same as 0.35.0. Copihue is still above C 38.6 (38.3–38.9) and Rust 33.4 (32.9–33.5). The ratio to C is 1.28. The ratio to Rust is 1.48.
- Match 47.1 (46.9–47.5) against 47.1 (47.0–47.3). The ranges overlap. The match binary is the same as 0.35.0. Copihue is still above C 33.0 (32.8–33.3) and Rust 33.2 (32.8–33.5). The ratio to C is 1.43. The ratio to Rust is 1.42.
- `bench/lowlat` and `bench/slice` were not run again.

### Not shipped
- The same pointer walk for a power-of-two element. On this machine, before the call gate, a millisecond pass moved the pool from 14 to 13 and the book from 49 to 57 (57–58). The level scans (`find_level`, `best_bid`, `best_ask`) gained an `add` of 16 on the back edge. They stay `shl`.
- The same walk in a loop with no call. With power-of-two sizes excluded, that pass left the pool at 13 and moved the book from 49–50 to 62–64 and the match from 47 to 48. `book_init` got about 30 bytes shorter and `insert` / `cancel` moved earlier by those 30 bytes. Their instructions were the ones from 0.34.0. Putting the 30 bytes back as `nop`s, so those functions stayed at the same addresses, restored 49–50. The walk stays off unless the loop contains a call, which is where the address is recomputed on both sides of the call.
- `i * 12` and `i * 24` as `lea reg, [reg+reg*2]` plus a SIB scale, instead of `imul`. Against the call-gated compiler, the book went from 49.759 (49.435–50.113) to 55.799 (55.776–56.373) and the match from 47.290 (46.960–47.528) to 48.023 (47.703–48.181). The pool ranges overlapped. The index stays `imul`.
- Hoisting a loop-invariant load out of a `while` condition (`i < nbid`, `i < g.n`, `i < p->n`), including a struct field through a pointer that is not written. A call, a store that might alias, a volatile, or an address-taken name keeps the load. Against the call-gated compiler the book was 49.176 (48.626–49.418) against 49.581 (49.401–50.240) and the match was 47.020 (46.695–47.288) against 47.229 (47.108–47.613). Both overlap. The pool binary did not change. `insert` and `cancel` do not reload a field through a pointer that stays put: `orders[cur].next` changes `cur`, and `bids[i].price` changes `i`.
- The comparisons in `insert`, `cancel`, `best_bid`, and `main` already branch on flags. They do not materialize 0/1. `insert` spills `id` and `qty` because `r12`–`r15` hold `side`, the level, the order, and the price across `find_level` and `take`. `cancel`'s fast path computes `cur * 12` twice (`id`, then `next`). gcc `-O2` and rustc `-O` inline the book into `main`, keep the generator and the free list in registers, walk levels by adding 16, and address a 12-byte order with `lea` of `*3` and a scale of 4. `% 10` in the driver is a multiply there and `idiv` here. None of that was a separate win once the inliner (0.33.1) and the `*12` `lea` above had been measured.

## [0.35.0] - 2026-10-26

### Added
- `--flat ADDR --elf` writes one ELF64 executable (`ET_EXEC`) for every file on the command and every `import`. A `module` item's symbol is `mod__item`. `pub` stays source visibility: a defined function, `let`, or `data` name is a global symbol either way. `section "name"` is an ELF section. `section "name" @ address` keeps that virtual address. `.text` is executable, `.data` and `.bss` are writable, and any other name is readable, writable, and executable. Each non-empty section is its own program header, aligned to 1. The section bytes are the raw `--flat` image. `--flat` without `--elf` is still `nasm -f bin`. `--elf` without `--flat` is rejected. `--elf` with `--boot` is rejected. `-S` still writes assembly and does not write the ELF.
- `--checked` also bounds-checks a local array in `--freestanding`, `--flat`, and `--boot`. The trap is the hosted one: `test` / `js` / `cmp` / `jge`, then `Array bounds error` (19 bytes) on fd 2 and exit 1. `unsafe { a[i] }` skips it. `for i in 0..N` skips it when the array has at least `N` elements, at `-O0` and at `-O2`. `bits 16` and `bits 32` do not insert it. Without `--checked`, those modes still omit it. Hosted checks are unchanged.

### Changed
- Language version is 0.35.0, on top of 0.34.1.

### Measured
- No timed bench. Hosted local-array checks did not change, and nothing was added to `make test` beyond the ELF header check and the assembly counts. `bench/slice` and `bench/lowlat` were not run again.
- NASM listing (`nasm -f elf64 -l`) of one freestanding `a[i]` on `[i64; 4]`. `--checked` inserts 13 bytes before the scale: `test rax, rax` (3), `js` (2), `cmp rax, 4` (4), `jge` (2), `jmp` (2). The trap that follows is 39 bytes, from `mov rsi, str_0` through the exit `syscall`. The jumps are the short form because that trap is 39 bytes, inside the 127-byte short-jump range. A trap past that range would widen each of those three jumps from 2 bytes to 6, and the 13 bytes would become 25. The 19-byte message is one shared literal. Without `--checked` those bytes are absent.
- `tests/link/parts/flat_entry.lap`, `flat_console.lap`, and `flat_note.lap` at `--flat 0x10000`. The raw image is 102 bytes. The ELF is 1544 bytes. Both files are identical at `-O0` and `-O2`. The 1442 bytes between them are the ELF header, three program headers, and the section and symbol tables. No instruction was added for the ELF. The image was not booted.

## [0.34.1] - 2026-10-26

### Fixed
- In `bits 16` and `bits 32`, `/` and `%` restore `edx`. `idiv` (`cdq`) and `div` of a `u32` (`xor edx, edx`) both `push edx` before the divide and `pop edx` after it. A remainder copies `edx` to `eax` before the pop. The zero-divisor path jumps to the same pop. `+`, `-`, `&`, `|`, `^`, `*`, and a shift still do not touch `edx`, so `rdmsr(m) | imm` is unchanged. `wrmsr(m, rdmsr(m) / 2)` keeps the high half the same way the OR does. 64-bit `/` and `%` are unchanged (`rdx` stays scratch).
- In `bits 16` and `bits 32`, a store whose address is not a number, a naked register, or `reg±disp` pushes `ebx` before the address and pops it after the store. The address is in `ebx` only for the `mov`. A named `ebx` is unchanged. `*p = v` for a named pointer is still the older store. 64-bit code still stores a computed address through `rbx` and leaves that scratch as it is.
- In `bits 16` and `bits 32`, `lgdt`, `lidt`, and `invlpg` of a pointer that is not a data label push `ebx`, use `[bx]` or `[ebx]`, and pop `ebx`. `mov bx, ax` writes only the low half; the pop restores all of `ebx`. A data label stays `lgdt [gdtr]` and does not touch `ebx`. 64-bit code stays `lgdt [rax]`.

### Added
- `tests/boot/edx_keep.lap` is a real-mode sector. QEMU prints `COPIHUE` only when `idiv` of 100/7 leaves 14, a `u32` `/` of 4294967295 leaves 2147483647, `*(bp + di)` stores 65, and `lgdt` of a register (not the `gdtr` label) leaves `edx` at 4660 and `ebx` at 7. `-O0` and `-O2` are the same 512-byte image. The GDT limit is 0; the sector does not enter protected mode.
- `tests/freestanding/edx_keep.lap` is the instruction stream at `-O0` and `-O2` for those operations in `bits 16` and `bits 32`, plus a remainder, a zero divisor, `lidt`, `invlpg`, and `wrmsr` of `rdmsr / 2`. It is assembled, not booted. `lidt` and `invlpg` are not executed: a new IDT with interrupts enabled can triple-fault, and this sector does not need them.

### Changed
- Language version is 0.34.1, on top of 0.34.0. The 0.34.0 slice rules are unchanged.

### Measured
- No timed bench. These are a few instructions in `bits 16` and `bits 32`, not a hosted loop, and nothing was added to `make test` beyond the sector and the assemble check. `bench/slice` and `bench/lowlat` were not run again.
- NASM listing of `tests/freestanding/edx_keep.lap`. In `bits 16`, `push edx` / `pop edx` and `push ebx` / `pop ebx` are `66 52`, `66 5A`, `66 53`, and `66 5B` (2 bytes each). In `bits 32` the same opcodes have no operand-size prefix (`52`, `5A`, `53`, `5B`, 1 byte each). Each `/` or `%` adds one push and one pop of `edx`: 4 bytes in `bits 16`, 2 bytes in `bits 32`. Each computed store and each non-label `lgdt` / `lidt` / `invlpg` adds one push and one pop of `ebx`: the same 4 or 2 bytes. The caller already push/pops `ebx` and `edx` around the other operand; those pairs were already there.

## [0.34.0] - 2026-10-26

### Added
- `[]T` is a slice: a pointer at offset 0 and an `i64` length at offset 8, 16 bytes. `T` is an integer, a pointer, or a struct. `sizeof` of a slice is 16. `let s: []T = arr` converts a local or global `[T; N]` of the same element. `let s: []T = slice(p, n)` builds one from a pointer and a length. `let s: []T;` is a null pointer and length 0. `s = t` copies the 16 bytes. `s.ptr` is `*T` and `s.len` is `i64`; assigning either is `slice field 'ptr' is read-only` (or `'len'`). A parameter is the address of those 16 bytes and the callee copies them. The design is `docs/DESIGN_SLICES.md`.
- `s[i]` and `s[i] = v` compare the index as unsigned with the length. A negative index fails. On failure the program writes `index out of range` and a newline to fd 2 and exits with code 1. There is no libc. That is the hosted default at `-O0` and `-O2`. `--freestanding`, `--flat`, and `--boot` do not insert it. `--checked` asks for it in those modes and does not turn on the local-array check. `unsafe { ... }` skips the slice check and the hosted local-array check. `s.ptr[i]` is a pointer and never checks.
- `for x in e` walks a slice or an array from 0 to its length and loads the element without a check. `for i in start..end` is exclusive. The check of `s[i]` is omitted when the range is `0..s.len` and the body does not assign `i` or the slice. `a[i]` in `for i in 0..N` is omitted when `N` is a literal, the array has at least `N` elements, and the body does not assign `i`. A shorter array, another index, or an assignment of `i` keeps the check. This holds at `-O0` and at `-O2`. `for (init; cond; step)` is unchanged.
- A slice field, a slice return, a slice element that is a slice or an array, and `[]T` in `bits 16` or `bits 32` are compile errors (`a slice cannot be a struct field`, `a slice cannot be returned`, `a slice element must be an integer, a pointer, or a struct`, `[]T is a 64-bit value`). An array parameter is `an array parameter is a pointer or a slice; use *T or []T`. A negative literal length is `negative slice length`. A hosted non-literal negative length, and the same length with `--checked`, writes `negative slice length` and exits 1.
- `tests/lang/slice.lap` prints 4, 10, 20, 100, 100, 100, 100, 4, 0, 30, 40, 1, 16 at `-O0` and `-O2`. `tests/lang/slice_oob.lap` prints `index out of range` and exits 1. `tests/lang/slice_neg.lap` prints `negative slice length` and exits 1. `tests/errors/slice.lap` is the seven diagnostics above that apply to a field, a return, an array parameter, a mismatched array, a write of `.len`, a negative literal, and a bad initializer. `tests/freestanding/slice.lap` assembles with no `syscall` and no slice trap, at `-O0` and `-O2`, and is not booted. `tests/link/parts/slice_chk.lap` is not a program test: hosted `-O0` and `-O2` each emit three `jae .Lslice_trap_`, `--freestanding` and `--flat` emit none and no `syscall`, and `--checked` brings the three traps back.

### Changed
- Language version is 0.34.0, on top of 0.33.1. The 0.33.1 pointer registers and immediate field stores stay.

### Measured
- `bench/slice/` is the same `while (j < n)` over 1024 `i64`s, 100000 times, with `s[j]` and with `s.ptr[j]`. It is not part of `make test`. The sum is 52377600000. On this machine, 2026-10-26T02:51:32Z, `taskset -c 0`, one warmup and seven runs, `python3` `time.perf_counter`. Milliseconds, median (min–max). `-O2` checked 59.9 (59.5–60.8), unchecked 53.1 (51.9–53.6). Those ranges do not overlap. `-O0` checked 134.7 (134.1–136.1), unchecked 134.7 (133.3–136.0). Those ranges overlap. The `-O2` checked loop reloads the length, `cmp`s, and `jae`s to the trap; the unchecked loop does not. Both still reload the pointer.

## [0.33.1] - 2026-10-26

### Added
- Hosted `-O2` can keep a pointer local or a pointer parameter in `r12`–`r15`, `r10`, or `r11`, under the same plan as a scalar. An array, a struct, a `volatile` local, or a name whose address is taken stays in its slot. Callee-saved registers are still filled first. A field load or store through a pointer already in a register is one memory operand. A numeric constant of that width is an immediate (`mov qword [r12], 0`, `mov byte [r12+8], 1` in `alloc`). `sum = sum + a[i].field` and `sum = sum - a[i].field` add or subtract that load into `sum` when `sum` is already in a callee-saved register and the field is a scalar.
- `tests/lang/o2_leaf.lap` prints the same 21 lines at `-O0` and `-O2`. That covers an early return, a call on the right of `&&` (the right-hand side does not run), and a call in a `while` condition (it is not hoisted).

### Changed
- Language version is 0.33.1, on top of 0.33.0. The 0.33.0 module rules are unchanged. `-O0`, `--freestanding`, `--flat`, `--boot`, and `bits 16` / `bits 32` do not put pointers in those registers.

### Measured
- `examples/lowlat/RESULTS.md` is the 2026-10-26T02:32:11Z — 2026-10-26T02:32:14Z window. The binary prints 0.33.1. One warmup and seven runs, `taskset -c 0`. The checksums are 2052000, 2160927, and 2853449. Milliseconds, median (min–max).
- Pool 14.2 (13.6–15.1). The previous examples window, 2026-10-26T00:51:15Z — 00:51:19Z, binary 0.32.1, was 20.8 (20.6–20.9). The new maximum is below the old minimum. Copihue is still above C 8.8 (8.5–9.1) and Rust 8.9 (8.4–9.3). The ratio to C is 1.61. It was about 2.3. gcc `-O2` inlines `alloc` and `release` and walks the blocks with `addq $24`. Copihue still calls both, and the release loop computes `i * 24` twice (`imul`). `* 24` is not a SIB scale.
- Book 49.5 (49.1–51.6) against 49.3 (49.2–49.8) in that earlier window. The ranges overlap. Copihue is still above C 38.6 (38.1–38.7) and Rust 33.2 (32.9–33.6). The ratio to C is 1.28. The ratio to Rust is 1.49.
- Match 47.1 (47.0–47.3) against 47.1 (46.8–47.4). The ranges overlap. Copihue is still above C 33.0 (32.5–33.1) and Rust 33.3 (33.1–33.6). The ratio to C is 1.43. The ratio to Rust is 1.41.
- `bench/lowlat` was not run again. The timed SPSC, SPMC, MPMC, and thread pool stay the 0.32.2 numbers.

### Not shipped
- Filling `r10` and `r11` before `r12`–`r15` for locals that do not cross a call. Same method, interleaved with the plan that fills callee-saved registers first: the book went from 49.61 (49.39–50.07) to 54.48 (54.23–55.35). `insert`, `cancel`, and `main` kept the same instructions. The leaf loops moved into caller-saved registers. The pool did not need that order, so the old plan stays.
- A hosted `-O2` leaf inliner. A direct call, no nested user call, at most six scalar or pointer parameters, no struct passed or returned by value, body weight at most 40, four passes, and a pointer, `i64`, or `u64` result written straight into the destination local. With the register plan above, the book was 57.21 (56.77–57.45) against 49.74 (49.29–50.12) without the inliner, and the pool was 14.94 (14.87–15.56) against 14.25 (14.10–14.39). Both are worse. gcc inlines those leaves. Copihue does not.
- Sharing one physical register across names whose recorded ranges do not overlap. That miscompiled the order book (the checksum left 2160927) and the matcher did not finish. A name used in a `while` condition stays live through the body, and a straight walk records that use before the body.

## [0.33.0] - 2026-10-26

### Added
- `module name;`, `pub`, and `name::item` for several `.lap` files. A file with no `module` stays in the flat namespace, so existing `import`, duplicate, and `-c`/`extern` programs keep their names and their diagnostics. The design is `docs/DESIGN_MODULES.md`.
- Inside a module, an item is private unless `pub`. An unqualified name sees that module and the builtins. `mem::alloc` names a public item of a module loaded in the same compilation. The link symbol is `mem__alloc`. A flat name stays as written. `fn main` and `fn _start` are entries only in the flat namespace. One `copihue` invocation is still one ELF, or one `-c` object. `pub` does not change the NASM `global` or `extern`. Inside `module mem`, `extern fn alloc` is `extern mem__alloc`. The caller object declares the same module and the same `extern`, and `ld` links the objects. `--freestanding`, `--flat`, and `--boot` use the same rules.
- A file that declared `module` reports `module:file:line:col: error: message`. A cycle in that file is `import of 'path' (module a) is a cycle`. A cycle in a file with no module stays `import of 'path' is a cycle`.
- `tests/lang/modules.lap` imports `a`, `b`, and `c` and prints 42, 32, and 12 at `-O0` and `-O2`. `tests/errors/module_cycle.lap` is the cycle. `tests/errors/module_private.lap` is `item 'hidden' of module 'secret' is private`. `tests/freestanding/module.lap` assembles `mfs__answer` and is not booted. `tests/link` links `module mem` across two `-c` objects (`T mem__add`, `D mem__bias`, `U mem__add`, `U mem__bias`, `T mem__use_it`, `T main`, `T _start`) and the program prints 42 at `-O0` and `-O2`.

### Changed
- Language version is 0.33.0, on top of 0.32.2.

## [0.32.2] - 2026-10-26

### Added
- Hosted `-O2` uses the address of a scalar global or a scalar stack slot directly in an atomic `mov`. The name must not be a pointer or an array. A pointer variable still goes through `r11`. A load of any other address is `mov r11, rax` and then the load from `[r11]`, instead of `push` / `pop`. A store or a read-modify-write still saves the address, because the value expression runs first. A statement that is only a store, `fence()`, or `cpu_pause()` does not emit `xor eax, eax`. The same stream is produced at `-O0` for that last point, so `tests/freestanding/atomics.lap` stays identical at both levels. `&cells[i].seq` is the field. `tests/lang/field_atomic.lap` prints 7, 9, 64, 7, 8, 0, 1.
- A hosted `-O2` `while` whose body is only `cpu_pause()` jumps to that `pause` when the condition is true. The `pause` is emitted after `ret`. The exit falls through. Any other `while`, and every `while` at `-O0` or in freestanding code, stays top-tested. This is not an inversion of `while`.
- `tests/lang/queues.lap` runs three lock-free rings at `-O0` and `-O2`. SPMC is one producer and three consumers. The producer keeps a private cursor, waits until `seq == pos`, writes the value, and release-stores `seq = pos + 1`. MPMC is two producers and two consumers. Both sides claim a ticket with `compare_exchange`, which returns the old value. The thread pool is the same single-producer ring: main pushes and three workers pop. Each cell is `align(64)` with `seq` and `data` on that line. After the producers have been joined, a release store of `done` makes a consumer that sees an empty ring pop once more. Each phase pushes 1..4096. The sum is 8390656, the duplicate count from `fetch_add` on `seen[]` is 0, and the pop count is 4096.
- `bench/lowlat/` adds the same SPMC ring, MPMC ring, and thread pool in Copihue, C++, and Rust, next to the existing SPSC ring and Treiber pool. The SPSC buffer is 64-byte aligned in all three. The consumer waits with `while (r == load) pause` instead of an `if`. `examples/lowlat` (the fixed-block pool, the book, the engine, and the capacity-8 ring) landed in 0.32.1 and is not copied here.

### Changed
- Language version is 0.32.2, on top of 0.32.1.
- The SPSC push and pop were compared with the C++ and Rust loops and with `nm` of the linked Copihue binary. There is no `mfence` on that path: acquire is `mov` and release is `mov`. `pause` is on the full path and the empty path. `write_idx` is at `0x402040`, `read_idx` at `0x402080`, and `buf` at `0x4020c0`, so the two indexes and the buffer start on distinct 64-byte lines. `tid` shares the `read_idx` line and is written by the kernel at thread start and exit, not on the push or the pop. The old loop wrapped every atomic in `lea` / `push` / `pop r11` and pushed the payload around the slot store. Those are gone. The fast path is a not-taken `je` to the outlined `pause` and one `jmp` back to the counted `while`. C++ and Rust keep the bound in that back edge (`jne`). That general rotation of `while` was measured slower in 0.27.2 and is not emitted.
- On this machine, `bench/lowlat/run.sh` at 2026-10-26T01:06:45Z, one warmup and seven runs. SPSC was `taskset -c 0,1`. The pool was `taskset -c 0`. SPMC, MPMC, and the thread pool were `taskset -c 0,1,2,3`. Copihue `-O2`, `g++ -O2`, `rustc -O`. Milliseconds, median (min–max). SPSC Copihue 44.8 (38.9–57.3), C++ 45.1 (7.6–53.8), Rust 9.1 (7.0–13.2). The Copihue range overlaps C++. It does not overlap Rust. The 0.32.0 window at 2026-10-26T00:18:42Z was Copihue 45.0 (18.4–76.1), C++ 34.1 (11.2–44.4), Rust 15.8 (10.5–30.6). The Copihue min–max narrowed. Pool Copihue 21.1 (20.6–24.5), C++ 21.0 (20.6–21.3), Rust 20.4 (19.9–23.8). Those three ranges overlap. SPMC Copihue 272.3 (231.7–285.3), C++ 250.1 (227.2–256.8), Rust 266.0 (232.3–290.9). Those three ranges overlap. MPMC Copihue 162.9 (132.2–183.3), C++ 162.0 (148.3–178.0), Rust 281.5 (264.2–310.0). Copihue overlaps C++. Copihue does not overlap Rust. Thread pool Copihue 236.6 (222.2–275.1), C++ 246.3 (207.4–271.8), Rust 256.0 (236.2–262.2). Those three ranges overlap. The ring checksum is 500000500000. The pool checksum is 1000000.

## [0.32.1] - 2026-10-26

### Added
- `examples/lowlat/` is four hosted programs on top of 0.32.0. The pool, the order book, and the matching engine are the same as before: a pointer free list (`alloc` / `free` are O(1); a full pool returns a null pointer), price levels with FIFO orders (insert, cancel, best bid/ask; an unknown id prints `miss`), and limit and market orders with price-time priority and a partial cross. The ring is a two-thread SPSC. Capacity 8, free-running `u64` indices, `align(64)` so the two indices do not share a cache line. Full when their difference is 8; empty when they are equal. Every slot is usable. The slot store is a statement before `atomic_store_release`. The other side uses `atomic_load_acquire`. `cpu_pause` waits. A single-thread prelude checks empty, full, and a drain (sum 44). A producer thread then sends 1 through 24. Main checks the order (`bad 0`) and the sum (`300`). `-O0` and `-O2` print the same lines.
- Each program still has a note on the capability it shows: real field widths (`Block` is 24 bytes, `Order` 12, `Level` 16), pointers, `as`, and, for the ring, `align(64)`, acquire/release, and `thread_spawn` without libc. The golden files are `tests/expected/examples/lowlat/`.
- `examples/lowlat/bench/` is still the pool, the book, and the engine in Copihue, C, and Rust. It is not a second copy of `bench/lowlat`. That directory, from 0.32.0, is the timed SPSC (one million messages, capacity 1024) and the Treiber pool, against C++ and Rust. This release does not run that bench again. In the 2026-10-26T00:18:42Z window those SPSC ranges overlap, and those pool ranges overlap.

### Changed
- Language version is 0.32.1, on top of 0.32.0. The code generator is the 0.32.0 code generator. The 0.31.1 rewrites (`psadbw` and `add byte`) stay.

### Measured
- On this machine, `examples/lowlat/RESULTS.md` is the 2026-10-26T00:51:15Z — 2026-10-26T00:51:19Z window (the binary prints 0.32.1). Copihue is above C and Rust on all three rows. The ranges do not overlap. Pool 20.8 ms (20.6–20.9) against C 8.9 (8.6–9.2) and Rust 8.9 (8.7–9.2). Book 49.3 (49.2–49.8) against C 38.7 (38.4–38.9) and Rust 33.3 (33.0–33.5). Match 47.1 (46.8–47.4) against C 32.9 (32.7–33.4) and Rust 33.6 (33.4–34.3). Nothing here is faster. Against the previous examples window (2026-10-26T00:27:12Z — 00:27:15Z, binary 0.31.2) each Copihue range overlaps the earlier one, so this release does not move the pool, the book, or the engine. The timed SPSC was not measured again.

## [0.32.0] - 2026-10-26

### Added
- Atomic builtins for `*volatile u32`, `*volatile i32`, `*volatile u64`, and `*volatile i64`. `atomic_load_acquire` is a `mov`. `atomic_load_seq_cst` is a `mov` and then `mfence`. `atomic_store_release` is a `mov` and returns 0. `atomic_store_seq_cst` is `xchg` and returns 0. `fetch_add` is `lock xadd` and returns the old value. `atomic_add` is `lock xadd` plus the addend and returns the new value. `swap` is `xchg` and returns the old value. `compare_exchange` is `lock cmpxchg` and returns the old value. `fence` is `mfence` and returns 0. A 32-bit operation uses `eax` and zero-extends. A non-volatile pointer is `fetch_add() takes a *volatile u32 or *volatile u64, found *u64`. A `*volatile u8` is the same error. They are rejected in `bits 16` and `bits 32`. They compile in hosted code and with `--freestanding`.
- `align(N)` on a struct, a field, or a global. `N` is a power of two from 1 to 4096. A field's `align` replaces its natural alignment, including in a `packed struct`. The struct size rounds up, so `sizeof` and `offsetof` count the padding. A global stricter than 8 bytes is a NASM `align` (`resb` in `.bss`). `align(64) struct Lines { align(64) a: u64, align(64) b: u64 }` is 128 bytes with `b` at 64. A local cannot carry `align`.
- Hosted threads without libc. `thread_spawn(fn, stack_top, tid)` is `clone` (syscall 56) with `CLONE_VM|FS|FILES|SIGHAND|THREAD|SYSVSEM|PARENT_SETTID|CHILD_CLEARTID` (`0x350F00`). The child calls `fn` with no arguments and exits that thread (syscall 60). The stack top is aligned down to 16 bytes. `tid` is a 32-bit word the kernel fills and then clears. `thread_join` waits on that word with `futex` (syscall 202, `FUTEX_WAIT`). The pointer stays in `rbx` because `syscall` overwrites `r11`. `sched_setaffinity(cpu)` is syscall 203 with a 128-byte mask; `cpu >= 1024` returns -1. `cpu_pause()` is `pause` and returns 0, including in freestanding code.
- `tests/lang/atomics.lap` prints the single-thread sequence at `-O0` and `-O2`. `tests/lang/align64.lap` prints the offsets, the sizes, a gap of 64, and the low 6 bits of the addresses. `tests/lang/threads.lap` runs two threads, each adding 1000000 with `fetch_add`; the total is 2000000. `tests/freestanding/atomics.lap` is the instruction stream at `-O0` and `-O2` (`lock xadd`, `xchg`, `lock cmpxchg`, `mfence`) and is assembled, not booted. `tests/errors/atomic_ptr.lap`, `atomic_width.lap`, `align_pow2.lap`, and `thread_hosted.lap` are the rejections.
- `bench/lowlat/` is the same SPSC ring and the same Treiber pool in Copihue, C++, and Rust. It is not part of `make test`. On this machine, 2026-10-26T00:18:42Z, one warmup and seven runs. The SPSC process was `taskset -c 0,1` (two CPUs). The pool was `taskset -c 0`. Copihue `-O2`, `g++ -O2`, `rustc -O`. Milliseconds, median (min–max): SPSC Copihue 45.0 (18.4–76.1), C++ 34.1 (11.2–44.4), Rust 15.8 (10.5–30.6). Those three ranges overlap. Pool Copihue 21.7 (21.3–22.0), C++ 21.4 (21.2–23.6), Rust 20.4 (20.2–21.4). Those three ranges overlap. The checksums were 500000500000 and 1000000. Pinning both SPSC threads to one CPU made the same programs take about 7.8 seconds each, because the producer and the consumer then wait inside `pause`; that figure is not the table.

### Changed
- Language version is 0.32.0, on top of 0.31.1. The 0.31.1 `-O2` rewrites stay (`psadbw` and `add byte`). The address of a volatile scalar or a volatile field is `*volatile T`, so `fetch_add(&cell, 1)` matches when `cell` is `let volatile cell: u64`.

## [0.31.1] - 2026-10-26

### Added
- Hosted 64-bit `-O2` rewrites the loop `i = 0; while (i < lim) { sum = sum + a[i]; b[i] = (b[i] + imm) & 255; i = i + 1; }` when `lim` is a proved multiple of 16 inside both lengths and `a` is `u8`. The old bytes are summed with SSE2 `psadbw` and the addend's low byte is `paddb`. `i` becomes `lim`. An `i8` sum stays scalar. `(e) as u8` around the add is the same low byte. A byte store `a[i] = (a[i] + imm) & 255`, `a[i] = a[i] + imm`, or `dst[i] = src[i] + imm` that is not that loop is `add byte` when both sides are the same array, and `movzx` / `add al` / `mov byte` when they differ. The two `movzx` that follow a narrow add and a narrow mask are not emitted for that store.
- `tests/lang/o2_mem.lap` adds a 16-byte sum-then-increment (same array and two arrays), a 15-byte `add byte`, a cross-array `dst[i] = src[i] + 1`, and `add byte` of 200. `-O0` and `-O2` print the same lines. The `-O2` assembly must also contain `psadbw` and `add byte`.
- `bench/src/copy.lap`, `fill.lap`, `hash.lap`, and `parse.lap` store the byte with `as u8`. 0.31.0 rejected the wider integer. The bytes are the same.

### Changed
- Language version is 0.31.1, on top of 0.31.0. `-O0`, `--freestanding`, `--flat`, `--boot`, and `bits 16` / `bits 32` do not take the new rewrites. `fill`, `list`, `hash`, `sort`, `fold`, `chain`, `parse`, and `fib` keep the 0.31.0 instruction stream aside from the version line and, where a bench only needed `as u8` on the setup store, that cast.
- On this machine, the 00:11:59–00:12:47 UTC window (the binary prints 0.31.1). `copy` is 4.7 ms (4.4–5.2), against the 23:29:22–23:30:09 UTC window 42.0 ms (41.5–42.2). Those ranges do not overlap. `text` is 557 → 602. In this window `copy` is below C (9.7–10.6), Rust (17.2–19.5), and OCaml (163.6–164.7). The ratio to C is 0.47. The first loop is still `rep movsb`. gcc still calls `memcpy` for that copy. `fill` is 17.5 ms (16.7–17.6) against 17.2 (16.6–17.7). Those ranges overlap. It is still 1.52× C. `list` is 41.0 ms (40.7–41.4) against 40.4 (40.3–41.0). Those ranges overlap. It is still above C and, in this row and these runs, below Rust (42.0–42.3). The ratio to Rust is 0.97. `parse` is 22.3 ms (21.9–23.1) against 22.5 (22.2–24.1). Those ranges overlap. It is still 1.60× C. `fib` is 393.4 ms (392.9–397.3), 3.00× C. The prologue stays 16 instructions.

### Not shipped
- The `fib` prologue stayed 16 instructions. Placing `fib(n - 2)` before reusing `r12` for `fib(n - 1)` removes the second callee-saved register and the alignment pad and still makes two calls. On this machine, one warmup and seven runs, `taskset -c 0`: 0.31.0 was 393, 393, 393, 393, 393, 393, 394 (median 393) and the reuse form was 393, 394, 394, 394, 394, 396, 397 (median 394). That overlaps 0.31.0 and the 0.30.1 window 392.7–394.0. gcc `-O2` still splits even and odd arguments and makes one recursive call; that changes the call count and is not emitted.
- `parse` stays `imul`. Four programs with the same `n` and `reps`, same method: the real loop 22 ms (22–22); load, compare, and `v = v * 10 + 1` 22 ms (22–24); load, compare, and `v = v + 1` 22 ms (22–23); nine dependent `imul`s and a reset, with no byte load, 10 ms (10–14). Dropping the `imul` does not move the per-byte loop. The load and the compare are the cost that shows up, and no rewrite of that loop was shown faster on the official bench, so the code stays.

## [0.31.0] - 2026-10-25

### Added
- An assignment, a direct call, and `return` reject a wider integer. `let wide = 300; let z: u8 = wide` is `expected u8, found i64`. `return n` from `-> u8` is `return value: expected u8, found i64`. `take_u8(n)` is `argument 1 of 'take_u8': expected u8, found i64`. The caret is on the expression, with `file:line:col`. The compiler keeps going and ends with `N errors, compilation failed`.
- The conversion is `as`. `let z: u8 = wide as u8` stores 44. `as` binds tighter than `+`, `|`, `&`, and the shifts, so the whole right-hand side is parenthesized: `vga[i] = ((attr << 8) | c) as u16`. A cast into a memory slot of that width is the store: it does not add a second mask. A scalar slot truncates through the cast, and the store does not mask again.
- A narrower integer still promotes. The same width still matches, including a change of sign (`i64` assigned to `u64`, `i8` to `u8`, `i32` to `u32`). A literal that fits is still implicit. A narrow parameter is still truncated on entry. Two untyped literals stay `i64`, so `let x: u8 = 200 + 100` is now `expected u8, found i64` at `-O0` as well as a literal error at `-O1` and `-O2`. Pointer rules are unchanged: `*u8` and `*u16` are `expected *u16, found *u8`; `*i8` and `*u8` match; dropping `volatile` is still an error.
- `tests/lang/assign_cast.lap` prints `44`, `44`, `-1`, `65` at `-O0` and `-O2`. `tests/errors/narrow_assign.lap` is the five diagnostics above.

- These conversions stay legal, and `tests/flat/ptr_keep.lap` is the automatic check (`// also-o2`, same image at both levels). An integer literal still becomes any pointer: `*u16` at `0xB8000` and `0x8000`, `*u8` at `0x10000`, `0x200000`, and `0x400000`. `bss_end() - bss_start()` is `i64`. `&` of a struct is accepted by `lgdt`, `lidt`, `save_context`, and `restore_context`. `tests/lang/ptr_literal.lap` prints those five addresses at `-O0` and `-O2` (`753664`, `32768`, `65536`, `2097152`, `4194304`).

### Changed
- Language version is 0.31.0, on top of 0.30.1. The 0.30.1 `-O2` rewrites stay. `tests/lang/o2_mem.lap` stores the byte updates with `as u8`. The `paddb` matcher peels that cast, so the assembly still contains `rep movsb`, `paddb`, and `shl rax, 4`.
- Programs that declared `-> i32` or an `i32` parameter but computed an untyped `i64` now say `i64`. The return was never truncated, so the instruction stream stays the same. A narrow memory store of a wider integer is `(expr) as T`. `tests/boot/rax_keep.lap`, `tests/freestanding/naked_local.lap`, `tests/freestanding/vga.lap`, `tests/flat/min_kernel.lap`, `tests/usermode/kernel.lap`, and `tests/ctx/kernel.lap` assemble to the same bytes as 0.30.0 aside from the version banner.

### AtomicOS
Checked read-only. The casts below are the migration; they are not applied in that repository. `as` wraps the whole right-hand side. After those edits, the assembly matches 0.30.0 except the version line.

`src/user/zona.lap` and `src/user/hola.lap` compile unchanged. Their `--flat 0x400000` images are identical on 0.30.0 and 0.31.0. `kernel.lap` itself has no new error.

On `43f5fcf` (`cursor/pin-copihue-v0294-65f7`), the kernel is `kernel.lap` with its imports, `--flat 0x10000`. These right-hand sides need `as`:

- `console.lap` 39 and 56: `vga[i] = ((con_attr << 8) | ' ') as u16`. Line 80: `vga[con_row * 80 + con_col] = ((con_attr << 8) | ' ') as u16`. Line 84: `((con_attr << 8) | c) as u16`. Line 108: `digits[count] = ('0' + n % 10) as u8`.
- `keyboard.lap` 45: `kbd_buf[kbd_head] = c as u8`.
- `shell.lap` 30: `line[line_len] = c as u8`.
- `src/kernel/copihue/zone.lap` (the kernel module, not `src/user/zona.lap`) 269, 313, and 325: the byte store is `c as u8`.
- The same wrap, with the type the diagnostic names, on `pmm.lap` 51 `u8`, 65 `u8`; `gdt.lap` 53, 54, 58, 59, 67–69, 72–76 `u8`; `idt.lap` 180 `u16`, 184 `u16`, 185 `u32`, 260 `u16` (`idtr.limit = (48 * 16 - 1) as u16`); `ata.lap` 96 and 97 `u8`; `blk.lap` 76 and 77 `u32`, 138 and 168 `u8`; `fs.lap` 180 `u8`, 248, 251, 324, 394, 424, 440, 497, 498, 592, 629, 671 `u32`, 712 `u8`; `net.lap` 169 `u32`, 314–319 and 331–336 `u8`, 446 `u16`, 588, 589, 601, 602, 641, 642, 646, 647 `u8`.
- `src/bootloader/boot.lap` 41 and 47: `al as u8`. Line 59: `dl as u8`. `stage2.lap` 127 and 133: `al as u8`. Line 181: `eax as u32`.

With those casts the kernel (`-S`), the boot sector, and stage 2 match the 0.30.0 assembly. Default `main` (`7e4ad6c`) has no `zona.lap` and no `keyboard.lap` error. It still needs the five `console.lap` lines and `shell.lap` line 30. Concatenating those four files then fails only on those six sites.

## [0.30.1] - 2026-10-25

### Added
- Hosted 64-bit `-O2` rewrites three loops. `i = 0; while (i < lim) { dst[i] = src[i]; i = i + 1; }` on two different `u8` or `i8` arrays, local or global, with `lim` a proved constant inside both lengths, is `rep movsb` and then `i = lim`. A parameter limit, a copy that does not start at 0, and a copy of an array onto itself stay scalar. `i = 0; while (i < lim) { a[i] = (a[i] + i + r) & 255; i = i + 1; }` with `lim` a proved multiple of 16 inside the length is an SSE2 `paddb` loop (`movdqu`). The index bytes start at 0..15 and add 16, so they wrap like `i & 255`. `r` contributes its low byte, including when it is negative. A length that is not a multiple of 16 stays scalar. `while (i >= 0) { sum = sum + g[i].f; i = g[i].h; }` and `while (i > -1)` with the same body, on a global array of structs whose size is a power of two and whose two fields are 8-byte non-volatile scalars, is one `shl` (`sal` is the same opcode), one `lea` of `base+scaled`, an add from memory, and a load of the next index. A local array is not rewritten. A size that is not a power of two stays `imul`. A power-of-two stride that is not 1, 2, 4, or 8 is `shl` at every hosted `-O2` index that used to emit `imul reg, stride`. 2, 4, and 8 stay a SIB scale.
- `tests/lang/o2_mem.lap` prints the same lines at `-O0` and `-O2`: copies of 0, 1, 7, 9, 16, and 17 bytes, a parameter limit, a local copy, a copy that starts at 1, the byte update at 16 and 256 (including a negative addend) and at 15, walks of struct sizes 16, 32, and 24, and a wrapping `* 10`. The `-O2` assembly must contain `rep movsb`, `paddb`, and `shl rax, 4`.

### Changed
- Language version is 0.30.1, on top of 0.30.0. `-O0`, `--freestanding`, `--flat`, `--boot`, and `bits 16` / `bits 32` do not take these rewrites. The `ebx` fix from 0.29.4 stays. Narrow arithmetic and `volatile` on the pointer type stay as in 0.30.0.
- On this machine, the 23:29:22–23:30:09 UTC window (the binary prints 0.30.1), against the 22:33:23–22:34:10 UTC window (that binary printed 0.29.2; hosted `-O2` of 0.29.3 matches it except the version line). Ranges that do not overlap, and the later one is faster: `copy` 47.8 ms (47.5–49.0) to 42.0 ms (41.5–42.2), `fill` 32.4 ms (32.1–32.7) to 17.2 ms (16.6–17.7), `list` 53.4 ms (53.0–54.2) to 40.4 ms (40.3–41.0). `text` is 534 → 557, 493 → 582, and 627 → 586. `parse` is 22.5 ms (22.2–24.1) against 22.3 ms (21.9–22.5). Those ranges overlap. Its `-O2` assembly matches 0.29.3 except the version line, and `text` stays 586. `fib`, `sieve`, `matmul`, `arith`, `chain`, `fold`, `array`, `sort`, `hash`, `array_global`, `fill`, and `list` match a binary that printed 0.29.5 (23:19:30–23:20:17 UTC) except the version line. `copy` does not: 0.30.0 truncates the `u8` add and the `u8` mask, so the second loop (`src[i] = (src[i] + 1) & 255`) gains two `movzx rax, al`. In that earlier window, without the two `movzx`, `copy` was 37.7 ms (37.5–38.4) and `text` was 549. The `rep movsb` itself is unchanged. Hosted `-O2` matches 0.30.0 except the version line and the three rewrites.
- Multiplying by 10 stays `imul`. A `lea` of `(x + x*4)*2`, including `v = v * 10 + (c - 48)` as two `lea`s, was measured before the 23:19 window (one warmup and seven runs, `taskset -c 0`). `parse` was 22.6 ms (22.2–22.9) against the 22:33 window 22.3 ms (21.9–22.5). The ranges overlap, so that form is not in this release.
- In the 23:29 window, `copy` is still above C (9.2–10.2) and Rust (16.6–17.4) and below OCaml (162.6–163.5). The ratio to C is 4.35. The copy itself is `rep movsb`; the following loop, which sums `dst` and adds 1 to each source byte, stays scalar and carries the two `movzx`. `fill` is still above C (10.9–11.7) and Rust (11.9–13.6) and below OCaml (91.0–91.8). The ratio to C is 1.52. `list` is still above C (32.8–33.7) and OCaml (30.3–31.1). It is below Rust in this row and these runs (40.3–41.0 against 41.5–42.3). The ratio to C is 1.22 and to Rust 0.97. `parse` is still above C (13.5–14.2) and Rust (13.7–15.0). The ratio to C is 1.60. `hash` overlaps C (44.6–45.3 against 45.1–47.2); the median ratio is 0.99. `sort` overlaps Rust (52.6–53.0 against 52.6–53.7); the median ratio is 0.99. `fold` overlaps C and Rust. `chain` stays under the three in this row and these runs; the binary is the one from 0.25.1. gcc `-O2` of `matmul` still uses `pmuludq`, and that is not emitted. The `fib` prologue stays 16 instructions. In this window `fib` is 3.01× C.

## [0.30.0] - 2026-10-25

### Added
- `u8`, `u16`, `u32`, `i8`, `i16`, and `i32` operators truncate to that width after the instruction. Unsigned keeps the low bits. Signed sign-extends. The width is the wider typed operand. The result is signed only when both operands are signed. A shift uses the left operand and does not widen to the count. A fitting untyped literal adopts the other operand, so `let x: u8 = 200; x + 100` is 44 and `x + 100 < 50` is true. A literal that does not fit stays 64-bit (`x + 300` is 500). Two untyped literals stay `i64`: `let x: u8 = 200 + 100` is still a literal error at `-O1` and `-O2`, and a truncating store at `-O0`. `i64` and `u64` are unchanged.
- In 64-bit code a narrow unsigned value is zero-extended, so `sar`, `idiv`, and a signed condition match the unsigned operation. In `bits 16` and `bits 32`, a `u32` with bit 31 set does not: `>>` is `shr`, `/` and `%` are `div`, and `<` `>` `<=` `>=` use `jb` / `ja` / `setb` / `seta`. `u8` and `u16` stay on `sar`, `idiv`, and the signed condition. `idiv` and that `div` still overwrite `edx`. The 0.29.4 rule stays: every operator push/pops `ebx`, and a shift push/pops `ecx`, inside a condition and outside one.
- `volatile` is part of the pointer type. `*volatile mut T` and `*mut volatile T` are `*volatile T`. `mut` is not. Assigning `*volatile T` to a non-volatile pointer is `expected *u8, found *volatile u8`. Assigning `*u8` to `*volatile u8` is allowed, including an array of `u8`. `*volatile i8` and `*volatile u8` match. An integer literal still converts to any pointer. `as` drops volatile and emits no instruction. Each source-level volatile read or write is still one memory instruction of that width. `-O2` does not delete it, fold it, or reorder it with another volatile access. There are no atomics and no fence stronger than the instruction. A non-volatile neighbor can still be simplified.
- `tests/lang/narrow_arith.lap` prints the wrapped values at `-O0` and `-O2`, including `u32` division and shifts and a `*volatile u8` load. `tests/errors/volatile_drop.lap` is the drop error. `tests/boot/narrow_arith.lap` is a 512-byte sector: `COPIHUE` is written only when the `u8` sum wrapped before the compare and the `u32` compare, `>>`, and `/` are unsigned. QEMU reads `COPIHUE` on COM1. `-O0` and `-O2` are the same image.

### Fixed
- Hosted `-O2` truncates a narrow result in the register that holds it. `rdi`, `rsi`, `r8`, and `r9` used to fall back to `eax` / `ax` / `al`, so `lea rdi, [r12-1]` was followed by `movsxd rdi, eax`. The recursive call kept the old `n` and ran off the stack (`fibonacci`, `factorial`). The low names are `edi` / `esi` / `r8d` / `r9d`, `di` / `si` / `r8w` / `r9w`, and `dil` / `sil` / `r8b` / `r9b`.

### Changed
- Language version is 0.30.0, on top of 0.29.4.
- `tests/lang/o2_reduce.lap` `holes_neg` prints 2. Both elements are `i8`, so `-1 + 3` is an `i8` sum, not the old zero-extended `255 + 3` (258). `-O0` and `-O2` agree. `holes` stays 258: the accumulator is an untyped `i64`.
- A `u8` subtraction such as `line[i] - '0'` emits `movzx rax, al` after `sub`. On the AtomicOS kernel (`kernel.lap`, `console.lap`, `keyboard.lap`, `shell.lap`, `--flat 0x10000`) that is the only new instruction, in `parse_int`. The image grows from 6004 to 6008 bytes. A digit in `'0'..'9'` has the same value. Jump displacements that cross the instruction move. `tests/flat/min_kernel.hex` is unchanged.
- `tests/lang/ptr_return.lap` drops `*volatile mut u8` with `as *u8`. The cast emits no instruction. Adding volatile in `as_vol` stays legal. `tests/flat/min_kernel.hex` is unchanged: `let p: *volatile u32 = regs` adds volatile.

## [0.29.4] - 2026-10-25

### Fixed
- In `bits 16` and `bits 32`, an arithmetic expression outside `if` / `while` no longer leaves the other operand in `ebx`. `ebx = 7; di = di + 24` used to store 24 in `ebx`, so the next `int 0x15` continued the E820 map with the summand instead of the token. `+`, `-`, `&`, `|`, `^`, `*`, `/`, `%`, and a shift now `push ebx` before the right-hand operand and `pop ebx` after the ALU, the same pair an `if` or `while` already used. A shift also `push`/`pop ecx` (`shl`/`sar` use `cl`) outside the condition, not only inside it. A comparison used as a value (`let x = a == b`) `pop`s `ebx` after `cmp` and before `sete`; `pop` does not change the flags. A `let` of one of those expressions, a compound assignment (`di += 4` is `di = di + 4`), and each argument of a call take that path. The call itself still uses `ax`/`cx`/`dx`/`si` or `eax`/`ecx`/`edx`/`esi`. `idiv` still overwrites `edx`. A store whose address is not a bare operand still leaves that address in `ebx`. `lgdt`, `lidt`, and `invlpg` of a pointer that is not a data label still move the address into `bx` or `ebx`. 64-bit `rbx` stays scratch.

### Added
- `tests/boot/ebx_expr.lap` is a 512-byte sector. `ebx` is 7, then `di` is updated with `+` `-` `*` `&` `|` `<<` `>>`, `di += 4`, and a call whose body adds. `COPIHUE` is written only when `ebx` is still 7. QEMU reads `COPIHUE` on COM1. `-O0` and `-O2` are the same image. A `let` and a comparison used as a value do not fit in the sector.
- `tests/freestanding/ebx_expr.lap` is the instruction stream at `-O0` and `-O2` for that sequence in `bits 16` (including the `let`, the call, and `si = di == 0`) and for `+`, `<<`, `+=`, and a value compare in `bits 32`. `ax = ebx` and `bp = ecx` (and `eax = ebx`, `edx = ecx`) follow the pops. It is assembled, not booted.

### Changed
- Language version is 0.29.4, on top of 0.29.3.
- Three boot hex dumps grew by `push ebx` / `pop ebx` (`66 53` / `66 5b` in real mode, the same opcodes without the operand-size prefix in `bits 32`) around an operator that was not inside a condition, and the short jumps after them moved: `tests/expected/boot/ebx_keep.hex` (`ebx = ebx - 1` in the countdown body), `call32.hex` (`read_cr0() | 1`), and `realmode_builtins.hex` (the same `|`). Each image is still 512 bytes ending in `55 aa`, and QEMU still reads `COPIHUE`. `tests/expected/freestanding/ebx_cond.asm.txt` gained the same pair around `ebx = ebx - 1` in `bits 16` and `bits 32`. `naked_local.asm.txt` gained it around `ch = ch + 1`. `bits32_regs.asm.txt` gained it around each `|` (`inb | 2`, `cr0 | 1`, `cr4 | 32`, `rdmsr | 0x100`, `cr0 | 0x80000000`); the `while` `&` was already saved. Sectors and images with no `bits 16`/`bits 32` operator outside a condition are the same bytes: `boot_sector`, `call16`, `jmp_sp`, `section`, `stc_carry`, `rax_keep`, `seg_local`, the flat images, `tests/usermode`, and `tests/ctx`.

## [0.29.3] - 2026-10-25

### Added
- Five hosted benchmarks in `bench/`: `fill` (add `(i + r)` into each byte, mask with 255, then sum the buffer), `copy` (copy bytes, sum the destination, then add 1 to each source byte), `hash` (FNV-1, offset basis `2166136261` masked to 31 bits, prime `16777619`, mask `2147483647` after every byte), `parse` (ASCII digits and commas, nine digits then a comma), and `list` (an array of `{val, next}` nodes, `next` an index, sentinel `-1`). The `.lap`, `.c`, `.rs`, and `.ml` sources share the algorithm, the limits, and the one-integer output. Copihue and C keep the byte buffers in `.bss`, one byte per element. A local `u8[n]` would reserve `n * 8` stack bytes. Rust uses a stack array. OCaml uses `Bytes` on the heap. OCaml `list` stores the pair in a flat int array (`2 * i`, `2 * i + 1`) because a record is a heap block, not 16 contiguous bytes. The index chain is the same. There is no `malloc`.

### Changed
- Language version is 0.29.3, on top of 0.29.2. This round does not change the code generator. Hosted 64-bit `-O2` of the thirteen bench programs matches the measured binary, which printed 0.29.2, except the version line. `bits 16` and `bits 32` are the 0.29.2 `ebx` fix.
- gcc `-O2` of `matmul.c` keeps the `i, j, k` order and vectorizes the inner product with `pmuludq` and `paddq`. The result buffer is cleared with `memset`. SSE is not emitted. The even unroll of the inner product and the bottom-tested `k` loop stay out; both were slower when measured.
- gcc `-O2` of `fib.c` is 416 lines. It splits the recurrence into even and odd steps (`sub rsp, 200`, one `call fib`). That is not a deletion of redundant moves in the direct prologue. The rewrite is not emitted. For `n >= 2` the prologue stays 16 instructions: `mov r12, rdi` holds `n` across the two calls, and `sub rsp, 8` is the alignment adjustment.
- On this machine, the 22:33:23–22:34:10 UTC window (the binary printed 0.29.2; hosted `-O2` matches 0.29.3 except the version line) overlaps the 21:45 window on all eight previous programs. The new rows, with ranges that do not overlap: `copy` is 47.8 ms (47.5–49.0) against C 9.6 (9.0–10.0), 4.97×, because gcc emits `call memcpy` and Copihue copies one byte per iteration. `fill` is 32.4 ms (32.1–32.7) against C 11.9 (11.2–12.9), 2.72×; gcc vectorizes with `movdqa` and this loop is not a constant `rep stosb`. `parse` is 22.3 ms (21.9–22.5) against C 13.8 (13.2–14.2), 1.62×; gcc turns `* 10` into `lea`, Copihue emits `imul`. `list` is above all three: 53.4 ms (53.0–54.2) against C 33.3 (33.0–33.7), Rust 41.8 (41.4–42.1), and OCaml 30.9 (30.4–31.2). Each hop is two `imul` by 16; gcc is `sal rax, 4` and two loads. `hash` overlaps C (44.6–45.4 against 45.1–45.7); the median ratio is 1.00. It is above Rust (38.7–39.0) and below OCaml (46.0–47.0). The printed decimal of these five is not in any `.s`. `chain` stays under the three in this row and these runs; the binary is the one from 0.25.1. `sort` overlaps Rust (52.3–53.0 against 52.7–53.5); the median ratio is 0.99.

## [0.29.2] - 2026-10-25

### Fixed
- In `bits 16` and `bits 32`, an `if` or `while` no longer leaves the other operand in `ebx`. The compare is still `cmp eax, ebx`, with `push ebx` before it and `pop ebx` after it. `pop` does not change the flags. A named `ebx` therefore survives `while (ebx != 0)`. That is the INT 15h continuation token: the old `mov ebx, eax` stored the zero of the comparison and the next `int 0x15` restarted the map. A `+`, `-`, `&`, `|`, `^`, `*`, `/`, `%`, or shift inside that condition push/pops `ebx` as well, and a shift push/pops `ecx` (`shl`/`sar` use `cl`). The same operator outside the condition still uses `ebx` as scratch. A comparison used as a value (`let x = a == b`) still writes `ebx` and does not restore it. `idiv` still overwrites `edx`. 64-bit conditions are unchanged.

### Added
- `tests/boot/ebx_keep.lap` is a 512-byte sector. `ebx` counts down through `while (ebx != 0)`, then `COPIHUE` is written from `ebx` after a false `if` and a false `while`, including `ebx & 1`. QEMU reads `COPIHUE` on COM1. `-O0` and `-O2` are the same image.
- `tests/freestanding/ebx_cond.lap` is the instruction stream at `-O0` and `-O2` for the same `while` in `bits 16` and `bits 32`, plus `<<` restoring `ecx`. It is assembled, not booted.

### Changed
- Language version is 0.29.2, on top of 0.29.1.
- Four boot hex dumps grew by `push ebx` / `pop ebx` (`66 53` / `66 5b` in real mode, the same opcodes without the operand-size prefix in `bits 32`) and the short jumps after them moved: `tests/expected/boot/rax_keep.hex`, `call32.hex`, `realmode_builtins.hex`, and `seg_local.hex`. Each image is still 512 bytes ending in `55 aa`, and QEMU still reads `COPIHUE`. `tests/expected/freestanding/bits32_regs.asm.txt` gained those pushes around the UART wait's `&` and its compare. Sectors and images with no `bits 16`/`bits 32` compare are the same bytes: `boot_sector`, `call16`, `jmp_sp`, `section`, `stc_carry`, the flat images, `tests/usermode`, and `tests/ctx`.

## [0.29.1] - 2026-10-25

### Added
- Hosted 64-bit `-O2` rewrites the insertion shift `j = i - 1; while (j >= 0 && a[j] > key) { a[j + 1] = a[j]; j = j - 1; } a[j + 1] = key` when it sits in `while (i < N)`, `N` is a sole positive constant, the local `i64` array has at least `N` elements, `i` starts at a non-negative constant, and `i` is incremented only at the end of that body. The taken path is six instructions and does not compare `j` with the length. `j` starts at `i - 1` and only decreases by one after `j >= 0`, so it does not go below -1. `key < a[j]` matches. `a[j] > key && j >= 0` does not. A `>=` test, an extra statement, a narrow or spilled name, or a failed proof of `i` keeps the bounds check.
- `tests/lang/o2_shift.lap` checks a reverse run, an already sorted run, duplicates, negative keys, `n = 1`, a shift down to `j == -1`, `key < a[j]`, a `let` inside the body, a `>=` loop that must keep counting shifts, an extra statement in the body, and dot products of `n = 1` and of negative factors, against `-O0`.

### Changed
- Language version is 0.29.1. It includes `-c` from 0.29.0 and the `if` / `while` fix from 0.28.1. The bench binary printed 0.28.2, before 0.29.0 landed on main; the eight `-O2` programs match that measurement except the version line. A hosted `-O2` comparison that is not a leaf (`i * i < limit`, `i < n * n`) jumps on the flags and does not write 0 or 1. The scalar flag path is unchanged. `-O0`, `--freestanding`, `--flat`, and `--boot` match 0.29.0 except the version line. `bits 16` and `bits 32` do not take the shift. The `-O2` assembly of `fib`, `arith`, `chain`, `fold`, `array`, and `sort` matches the 21:11:18–21:11:53 UTC binary (it printed 0.27.5) except the version line. `sieve` `text` goes from 682 to 672 and `matmul` from 1028 to 1018. `sort` stays at 659 / 21. The taken shift is 6 instructions. The `matmul` `k` loop stays the 10-instruction top-tested loop. `fib` is still the 16-instruction prologue. gcc `-O2` emits 416 lines for `fib.c`, with `sub rsp, 200` and a tree that subtracts 2. It is still exponential. That rewrite is not in this compiler.
- On this machine, the 21:45:10–21:45:46 UTC window (a binary that printed 0.28.2; the eight `-O2` programs match 0.29.1 except the version line) against that 21:11 window: `sieve` 69.7 ms (69.2–70.0) to 72.3 ms (71.9–72.5). Those ranges do not overlap, and the later one is slower. The inner mark loop is unchanged; the changed compare is `i * i < limit`. `matmul` 46.2 ms (46.1–46.7) to 47.9 ms (46.4–48.6). Those ranges overlap. The changed compare is `i < n * n`. The other six overlap 21:11. In the 21:45 window, `sort` is 52.6 ms (52.4–53.1), below C (72.9–73.7) and OCaml (153.7–154.3), and it overlaps Rust (52.6–53.6), so it is not below Rust; the median ratio is 0.99. `sieve` overlaps C (69.9–72.4), stays above Rust (42.7–43.3), and stays below OCaml (128.0–134.6). `matmul` stays above C (32.0–34.0) and below Rust (50.3–51.3) and OCaml (78.4–81.3). `fib` is 3.01× C. `chain` stays under the three in this row and these runs; the binary is the one from 0.25.1.
- Against the 20:44:22–20:44:58 UTC window (Copihue 0.27.4), the shift in the 21:11 window was `sort` 77.9 ms (77.6–78.4) to 52.6 ms (52.4–55.3). Those ranges do not overlap. The 21:45 `sort` binary matches that one except the version line, and 52.4–53.1 does not overlap 77.6–78.4 either.
- A bottom-tested `k` loop with `imul rax, qword [rsi]` was measured on 2026-10-25 21:06:42–21:07:17 UTC and not kept. `matmul` went from 46.9 ms (46.3–47.4) to 48.0 ms (47.6–48.2). The ranges do not overlap, and the later one is slower. `text` of that binary was 1021. This release keeps the 10-instruction top-tested loop (`text` 1018, ten bytes less than 1028 because the checksum compare no longer writes 0 or 1).

## [0.29.0] - 2026-10-25

### Added
- `-c` writes one ELF64 relocatable object (`nasm -f elf64`) and does not run `ld`. `-o` names that object (default `output.o`). `-c -S` writes the assembly of that object and does not assemble. Definitions in the object are NASM `global`: functions, `let` globals, and `data` names. `extern name;` with no definition in this object is NASM `extern`. `extern fn name(...) -> T;` and `extern bits 16|32|64 naked fn name(...);` are the same for a function: the call still checks the argument count and the argument types, and the object has no body. Without `-c` that declaration is `extern fn is only available with -c`. A body is `extern fn has no body`. A body later in the same object defines the function, and no `extern` is emitted.
- The hosted object that defines `fn main` emits `_start` (`call main`, then `syscall` 60) and the hosted helpers (`__print_int`, `__strcmp`, `__strcpy`, `__strlen`), and those helpers are `global`. An object with no `main` has no `_start` and no copy of the helpers. A call to `print_int`, `strcmp`, `strcpy`, `strlen`, `print`, or `println` from that object is an `extern` of the helper. Two objects that both define `main` both define `_start`; `ld` reports the duplicate. Two objects that define the same function do too.
- `--flat` and `--boot` stay `nasm -f bin`. `-c` with either one is `copihue: error: -c cannot be combined with --flat or --boot`. `section "name" @ address` is a bin load address (`vstart`). `-c` rejects it: `section '@' is a load address for --flat and --boot; -c does not emit vstart`. A named section without `@` is still emitted, with no `vstart`, so `nasm -f elf64` does not warn. `--freestanding` without `-c` still stops at assembly and still emits `vstart` when the source asked for it; assembling that file with `nasm -f elf64` still warns and ignores the attribute. `import` and several files on one command are still one object, not one object per file.
- `tests/link` compiles two hosted files with `-c`, links them with `ld` and no libc, and runs the program (`42`) at `-O0` and `-O2`. The library object exports `add` and `bias` and has no `_start`. Linking that object to itself fails. A `--freestanding -c` section without `@` assembles with no `vstart` warning. Two `bits 32` freestanding objects are linked at `0x8000` and booted from `tests/usermode/boot.lap`. QEMU reads `COPIHUE` on COM1 at `-O0` and `-O2`.

### Changed
- Language version is 0.29.0, on top of 0.28.1.

## [0.28.1] - 2026-10-25

### Fixed
- An `if` or `while` whose condition is a comparison, `&&`, `||`, or `!` jumps on the flags. It does not write 0 or 1 into `rax` (`eax` in `bits 16` and `bits 32`). `while (rax == 0)` leaves the compared value in `rax` when the loop ends, and the body of `if (rax == 0 - 1)` still sees it. A comparison used as a value still writes 0 or 1. Hosted `-O2` already branched on scalar flags; that path is unchanged. `if carry` still reads CF.
- `*(addr) = imm` is `mov size [addr], imm` when the address is a number, a naked register, or `reg+disp` / `reg-disp` and the literal fits the pointee. The constant is not moved through `rax` first. Any other store still evaluates the value in the accumulator.

### Added
- `tests/freestanding/rax_branch.lap` checks the instruction stream at `-O0` and `-O2`: `syscall`, `while (rax == 0)`, `mov byte [4200200], 0`, and `if (rax == 0 - bias)` leaving `rax` for `rdi = rax`. `let r = syscall(8)` stores the slot before that byte store, and `return r` reloads it. The file is assembled, not booted. `syscall` itself is not executed.
- `tests/boot/rax_keep.lap` is a 512-byte sector. `ax` is the character, a false `if` and a false `while` do not replace it, and `*(addr) = 0` does not either. QEMU reads `COPIHUE` on COM1.

### Changed
- Language version is 0.28.1, on top of 0.28.0.

## [0.28.0] - 2026-10-25

### Added
- `section "name" { ... }` and `section "name" @ address { ... }` at the top level, with `--freestanding`, `--flat`, or `--boot`. The functions, `let` globals, and `data` blocks inside the braces go in that NASM section. `section` is contextual (the next token is a string), so a call named `section` is still a call. A nested section is `a section cannot contain another section`. `import` inside a section is `import is not allowed inside a section`. Hosted code is `section is only available with --freestanding, --flat, or --boot`. A name that is not a NASM section name is `section name 'boot rom' is not a NASM section name`. More than 32 sections is `too many sections`.
- `@ address` is NASM `vstart`: labels in the section resolve to that load address, and the bytes stay packed. It is not `start=`, which would insert a hole (`@ 0x100000` would write a megabyte of zeros). The address is one non-negative integer (`section load address must be a non-negative integer`), including `0`. The first `@` fixes it. Opening the same name again appends. A different address is `section '.a' already has load address 0x1000`. A later block without `@` keeps the first address. The directive is lowercase hex (`vstart=0xffff800000000000`). A named section is `align=1`, so nasm's bin format does not insert 4-byte padding in front of it.
- `section_start("name")` and `section_end("name")` return `i64`: the load address of the first byte and one past the last byte. `.text`, `.data`, and `.bss` are known even if the source never declared them. `bits 16` emits `mov ax`, `bits 32` emits `mov eax`, and 64-bit code emits `mov rax`. An unknown name is `unknown section '.nope'`. A `vstart` above `0xFFFF` used from `bits 16` is `section '.high' does not fit in bits 16`. The symbols are `__text_start` / `__text_end`, `__data_start` / `__data_end`, `__bss_start` / `__bss_end`, and `__sec_` plus the name with `.` turned into `_` (`.boot` is `__sec__boot_start`). Two names that share a symbol are `section 'a_b' uses the same bounds symbol as 'a.b'`.
- `bss_start()` and `bss_end()` are unchanged: `*u8`, `--flat` only, `mov rax, __bss_start` / `__bss_end`. Flat mode still emits those two labels around `.bss`. A program that never mentions `section` or `section_start` / `section_end` keeps the same instruction stream and the same flat bytes. Bound labels are emitted only when the program asks for that name or declares that section. `_start` is still first. If it is inside a named section, that section is opened before it, so the entry is byte 0 of the section. A `let` or `data` written before `_start` is still emitted after it.
- Only the section named `.bss` is omitted from a flat file. `resq` there is nobits. The same `resq` in any other section is progbits and occupies zeros. An initialized scalar stays `name: dq value`.
- The boot pad `times 510-($-$$) db 0` / `dw 0xAA55` is written in `_start`'s section, after that section's end label, so the measured span excludes the pad. A one-section image is still 512 bytes ending in `55 aa`. Bytes in a second section still count toward the file, so a multi-section sector can grow past 512.
- `tests/boot/section.lap` is one section `.boot` at `0x7C00`. The image starts with `b8 00 7c` (`mov ax, __sec__boot_start`) and QEMU checks `COPIHUE`. `tests/freestanding/sections.lap` is the instruction stream at `-O0` and `-O2`, including a `let`, a `data` block, and `section_start` / `section_end` of `.extra`, `.text`, `.data`, and `.bss`. It is assembled, not booted. `nasm -f elf64` warns that it ignores `vstart`; `nasm -f bin` applies it.

### Changed
- Language version is 0.28.0. It includes the hosted `-O2` rewrites from 0.27.4. `-O0`, `--freestanding`, `--flat`, and `--boot` stay the same as 0.27.4 when the program does not use `section`.

## [0.27.4] - 2026-10-25

### Added
- Hosted 64-bit `-O2` turns a register times 3, 5, or 9 into `lea`. The product wraps mod 2^64, the same as `imul`. Powers of two stay shifts. `*6` stays `imul`.
- A straight-line run of at least two `x = (x + y) & imm`, with `imm` in `0..2147483647` and with neither `x` nor `y` assigned earlier in the run, emits every `add` and then every `and`. One statement, a narrow local, a spilled local, or a source that is an earlier destination in the run stays in the original order.
- `tests/lang/o2_hot.lap` checks the multiply and the mask order against `-O0`, including a wrapping product, a dependent mask that must stay in order, and a dot product with even and odd `n`.

### Changed
- Language version is 0.27.4. `-O0`, `--freestanding`, `--flat`, and `--boot` generate the same code as 0.27.3. `bits 16` and `bits 32` do not take these paths. The `-O2` assembly of `fib`, `sieve`, `chain`, `fold`, and `sort` matches 0.27.3 except the version line. `matmul` changes only the fill (`i * 3` is `lea`); the inner product is still 10 instructions. `text` is unchanged.
- On this machine, against the 17:30:43–17:31:18 UTC window (a 0.27.2 binary whose eight `-O2` programs match 0.27.3 except the version line), the 20:33:00–20:33:36 UTC window is `array` 57.1 ms (56.9–57.5) to 45.6 ms (45.3–46.0) and `arith` 66.6 ms (66.4–67.1) to 63.5 ms (63.3–63.8). Those ranges do not overlap. `array` is below OCaml (58.6–59.2) and overlaps C (43.1–49.8), so it is not below C, and it stays above Rust (33.2–34.5). The local and the global overlap (45.3–46.0 and 45.0–46.0). `arith` is below OCaml (66.2–69.0) and above C (59.1–59.7) and Rust (55.6–56.2). `fib`, `sieve`, `matmul`, `chain`, `fold`, and `sort` overlap the reference. `fib` is still 3.00× C. `sort` stays above C (73.0–73.9) and Rust (53.1–53.4). `chain` stays under the three in this row and these runs; the binary is the one from 0.25.1.
- Two steps per branch on the even inner product were measured (2026-10-25 17:40:12–17:40:48 UTC) and not kept. `matmul` went from 46.5 ms (45.9–47.1) to 47.6 ms (47.4–48.1). The ranges do not overlap, and the later one is slower. That binary also used `lea` for `i * 3` in the fill. The hot loop was the unroll, and this release does not emit it. Inverting `while` stays out, as measured in 0.27.2.

## [0.27.3] - 2026-10-25

### Added
- `cld()` and `std()` emit that instruction and take no arguments, in `bits 16`, `bits 32`, and 64-bit code. `rep` still honors whatever DF is set.
- `jmp rax` and `jmp [resume_rip]` are indirect jumps, and only inside `naked`. Outside it, the error is `an indirect jmp is only available in a naked function` and the instruction is not emitted: NASM would treat `jmp rax` as a register jump and drop the frame. The register is any general register `push` accepts, at the width the mode can encode (`register 'rax' is not available in bits 16`). `jmp [name]` reads a global, a `data` label, or a naked local. The size is `jmp qword [name]` in 64-bit code, `jmp dword [name]` in `bits 32`, and `jmp word [name]` in `bits 16`. A name that is not memory is `jmp [name] takes a variable or a data label`. A direct `jmp 0x8200` or `jmp label` is unchanged, including outside `naked`.
- `bx` and `bp`, and `ebx`, `ebp`, `esi`, and `edi`, are naked lvalues and rvalues in `bits 16` and `bits 32`. An immediate writes that width only (`mov bx, 67` leaves the high half of `ebx`). In 64-bit code the error is `register 'ebx' is not available in bits 64`: `rbx` is the scratch and `rbp` is the frame. Outside `naked`, `register 'bp' is only available in a naked function`. They are not parameter registers. `si` and `di` stay available in 64-bit naked code, as before.
- A call from `bits 16` or `bits 32` uses the naked parameter registers. `bits 32` evaluates each argument into `eax`, pushes `eax`, and pops into `esi`, `edx`, `ecx`, `eax` (at most four). `bits 16` does the same with `ax`, `cx`, `dx`, and `si`. `push rax` / `pop rdi` is not an encoding in `bits 16`, and a 64-bit `ret` would pop a return address a 16-bit `call` did not push. Both functions are naked, so the callee already reads those registers. The callee must be the same bits mode: `a bits 16 call cannot target a 64-bit function`, or `a bits 16 call cannot target a bits 32 function`. More than four arguments is `a bits 32 call takes at most 4 register arguments`. The `call` is near. In `bits 16` the displacement is 16 bits (±32KiB); nasm reports it when the callee is farther, and a far call is not emitted because both functions share `CS`. An indirect call is `an indirect call is not available in bits 16`, because the name is what selects the mode. A 64-bit call is still `push rax` / `pop rdi`. The first argument lives in `ax` or `eax`, which the next expression overwrites.
- `syscall(n, a1, a2, a3, a4, a5, a6)` in `--freestanding`, `--flat`, or `--boot` loads `rax`, then `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9`, executes `syscall`, and leaves the result in `rax`. At most six arguments after the number: `syscall() takes at most 6 arguments`. `syscall()` with no arguments is still the one instruction, so existing code that set the registers itself is unchanged. `bits 16` and `bits 32` stay `syscall() is a 64-bit instruction`. Hosted `syscall` / `syscall6` are unchanged.
- `tests/boot/call16.lap` calls a `bits 16` function, uses `bx` and `bp`, jumps through `jmp word [land]`, and writes `COPIHUE` to COM1. QEMU checks that string. `tests/boot/call32.lap` does the same in protected mode (`eax` / `ebx` / `ebp` / `esi`, `jmp dword [land]`). `tests/freestanding/jmp_call.lap` is the instruction stream at `-O0` and `-O2`, including `syscall(1, 65, 1, 1)`. It is assembled, not booted.

### Changed
- Language version is 0.27.3.

## [0.27.2] - 2026-10-25

### Added
- Hosted 64-bit `-O2` stores a constant with an immediate `mov` when the value fits the element, including a constant index (`mov byte [mem], 0`).
- `i = 0; while (i < lim) { a[i] = C; i = i + 1; }` on a local `u8` or `i8` array becomes `rep stosb` from element 0 and sets `i = lim`, when `lim` is inside the length. If `lim` is not a proved constant, a runtime check keeps the original loop. A negative limit or one past the array is left alone.
- `s = S; i = I; while (i < N) { s = s + A*i + B; i = i + 1; }` is replaced by the closed sum only when `N`, `A`, `B`, `S`, and `I` are constants, every product `A*i` and every partial sum fits in `i64`, and the terms are all non-negative or all non-positive. A sum that can wrap keeps the loop. `tests/lang/o2_reduce.lap` checks that against `-O0`, including a negative trip count, mixed signs, `i * i`, overflow, and division by a negative, by zero, and by a power of two.

### Changed
- Language version is 0.27.2. `-O0`, `--freestanding`, `--flat`, and `--boot` generate the same code as 0.27.1. `bits 16` and `bits 32` do not take these paths. The `make bench` samples were taken on a compiler that printed 0.26.3, before 0.27.0 and 0.27.1 landed on main. On this machine the medians are `fold` 142.1 ms (141.8–142.7) to 1.6 ms (1.1–2.5), overlapping C (1.3–1.9) and Rust (1.3–2.1), and `sieve` 82.3 ms (82.1–82.5) to 69.6 ms (69.4–70.0). Against C (70.0–70.4) the ranges touch at 70.0, so `sieve` is not below C, and it stays above Rust (42.8–43.6). `arith`, `fib`, `matmul`, `chain`, `array`, and `sort` are the same binary as 0.26.2. `fib` is still 3.00× C. Inverting `while` was measured (2026-10-25 16:52:01–16:52:37 UTC) and not kept: `arith` went from 67.6 ms (66.9–71.6) to 73.4 ms (70.7–74.6). There is no inlining, no unrolling, and no rewrite of `fib`.

## [0.27.1] - 2026-10-25

### Added
- `label name;` is a code address inside a function. `label_addr(name)` is `lea rax, [name]` for that label, a function, or a data label. An unknown name is `unknown label 'nope'`. In `bits 16` or `bits 32` it is `label_addr() is a 64-bit address`. The address does not change `rsp`.
- `save_context(&ctx)` stores eight qwords: `rip`, `rsp`, `rbp`, `rbx`, `r12`, `r13`, `r14`, `r15`. `rip` is the next instruction. It returns 0. `restore_context(&ctx)` loads them and jumps there, so `save_context` then returns 1. The argument is a name or `&name`. Anything else is `save_context() takes the address of a context`. In `bits 16` or `bits 32` it is `save_context() is a 64-bit context switch`. The compiler emits the `rsp` and `rbp` moves, after the prologue, so a framed function's epilogue still matches. Callee-saved registers are restored so a value kept in `r12`–`r15` survives the other task. The address is a `lea` into `r11`, which is not part of the context. The builtin is `restore_context`, not `switch_to`, because a kernel may already have a function of that name. Writing `ctx.rip = label_addr(after)` before the restore resumes at that label with the saved frame.
- `interrupt fn pf(frame: *PfFrame)` takes a pointer to the CPU frame. Without an error code the fields are `rip`, `cs`, `rflags`, `rsp`, `ss` and the pointer is `[rbp+120]`. A struct whose first field is `error_code` points at `[rbp+128]`, and `rip` is the next qword. The epilogue reads the parameter again, so `frame = other` returns through another saved frame, and `frame.rip = x` / `frame.rsp = y` are what `iretq` pops. The fifteen saved registers sit 120 bytes below that first CPU qword (`r15` at the low address through `rax`). Each field must be 8 bytes at `0, 8, 16, ...`. `*i64` is `an interrupt frame parameter must point at a struct`. `interrupt fn timer()` and `interrupt fn fault(err: i64)` are unchanged.
- `tests/lang/context.lap` runs the switch under Linux at `-O0` and `-O2`. `tests/ctx` boots under QEMU: a `#PF` handler rewrites `rip` and `rsp`, an IRQ handler switches stacks, and `restore_context` returns. COM1 shows `COPIHUE`. `tests/freestanding/frame_handler.lap` is the instruction stream at `-O0` and `-O2`. It is assembled, not booted.

### Changed
- Language version is 0.27.1.

## [0.27.0] - 2026-10-25

### Added
- An ordinary `struct` lays fields out at their real width with natural alignment. `u8`/`i8` align to 1, `u16`/`i16` to 2, `u32`/`i32` to 4, and `u64`/`i64` and every pointer to 8. A nested struct uses its own alignment and size. The struct's alignment is the strictest of its fields, and its size is rounded up to that alignment. `packed struct` is unchanged for scalars: no padding, so an IDT gate stays 16 bytes and `rsp0` stays at offset 4. A TSS still has to be packed, because natural alignment would place `rsp0` at 8.
- A field `[T; N]` occupies `N * sizeof(T)` bytes, packed or not. `e.name[i]` and `e.blocks[i]` address that field. `T` may be an integer, a pointer, or a struct whose size is already known. A count that is not positive, an unknown element type, or a size that does not fit is `cannot lay out field 'name' of type '[Missing; 4]'` and the compiler exits with status 1. It does not emit a smaller struct and exit 0.
- `sizeof(T)` and `sizeof(value)` are the type's size in bytes. `offsetof(Struct, field)` is the field's offset. Both are compile-time integers (`mov rax, N`). `sizeof()` takes one argument. `offsetof(Struct, field)` takes a struct name and a field name. An unknown size is `sizeof() has no size for this type`.
- A load of a `u8`/`u16`/`u32` field zero-extends into `rax`. A load of an `i8`/`i16`/`i32` field sign-extends. A store writes only that field's width. Operators stay 64-bit; the narrow value is the one that was loaded, and the store truncates again. Scalar locals and globals still occupy 8-byte slots.
- `tests/lang/struct_width.lap` runs under Linux. A packed directory entry is 56 bytes (`name` at 0, `size` at 16, `nblocks` at 20, `blocks` at 24), the same shape without `packed` is also 56, and a `u8` then a `u32` then a `u16` then a `u64` is 24 bytes with those fields at 0, 4, 8, and 16. Writing `name[0]` does not change `size`. A `u8` field `200 + 100` stores 44. It is not booted. `tests/errors/struct_layout.lap` is the layout the compiler rejects.

### Changed
- Language version is 0.27.0.

## [0.26.2] - 2026-10-25

### Added
- Hosted 64-bit `-O2` places the arguments of a direct call in `rdi`, `rsi`, `rdx`, `rcx`, `r8`, and `r9` when every argument is a scalar the compiler can put in that register (at most six). An argument that is itself a call, an indirect call, a struct passed by value, `syscall` / `syscall6`, and the hosted builtins keep the push/pop sequence. A function that never addresses `rbp` and never pushes saves callee-saved registers with `push`/`pop`, so both calls in `fib` stay 16-byte aligned. One callee-saved temporary holds the left-hand side of `f(n - 1) + f(n - 2)` across the call.
- `if` and `while` jump on the flags when the condition is a comparison of two scalars or of one array element and a scalar, including `&&` and `||`. The comparison does not become 0 or 1. `&&` still skips the right-hand side. A comparison used as a value (`let x = a < b`) still materializes 0 or 1. `if carry` is unchanged.
- The value loaded by that comparison is reused by the next use of the same element. `a[j + 1] = a[j]` stores through `base+(j+1)*scale` and does not load `a[j]` again. A `while (i < lim)` that starts from a non-negative constant, only increments `i` by one at the end, and contains no call and no address-of, skips the local bounds check when every array it indexes with `i` has at least `lim` elements. A shorter array still traps. The inner product `s = s + a[i * n + k] * b[k * n + j]`, with `k` from 0 to `n`, walks the two rows with running pointers when `i` and `j` are in `[0, n)`. Otherwise the original loop runs. This does not delete loops.
- `tests/lang/o2_flow.lap` checks that this matches `-O0`: register arguments, a call used as an argument, `||`, a value comparison, an insertion-sort shift that stores `a[0]` when `j` is `-1`, a proved bounds-free loop, the matmul running pointers, and a bounds trap when the limit is larger than the array.

### Changed
- Language version is 0.26.2. `-O0`, `--freestanding`, `--flat`, and `--boot` generate the same code as 0.26.1. `bits 16` and `bits 32` do not take these paths. On this machine `sort` went from 255.4 ms to 78.3 ms and `matmul` from 78.8 ms to 46.8 ms (medians, `make bench`). `fib` has fewer instructions on the `n >= 2` path and its median did not improve (389.7 ms to 394.3 ms, ranges overlap). `arith`, `chain`, and `fold` are the same binary as 0.25.1.

## [0.26.1] - 2026-10-25

### Added
- `lodsb()`, `stosb()`, `stosd()`, `stc()`, `clc()`, and `ud2()` emit that instruction and take no arguments. `rep_stosb()`, `rep_stosw()`, `rep_stosd()`, `rep_stosq()`, `rep_movsb()`, `rep_movsw()`, `rep_movsd()`, and `rep_movsq()` emit `rep` plus the string instruction. `rep_stosq()` and `rep_movsq()` in `bits 16` or `bits 32` are `rep_stosq() is not available in bits 16`. These instructions use `si`/`rsi`, `di`/`rdi`, `al`/`eax`, `cx`/`rcx`, and the direction flag as they stand. Copihue does not emit `cld`.
- `pushfd()` and `popfd()` emit `pushfd`/`popfd` in `bits 16` and `bits 32`, and `pushfq`/`popfq` in 64-bit code. A 32-bit push of EFLAGS is not an encoding in long mode.
- `push(ax)` pushes that register. `push(0x33)` pushes the immediate (`push 51`; expression literals are decimal). Any other argument is evaluated and pushed from the accumulator. `pop(rbx)` pops into that register. The register list is wider than the naked lvalues: the 16 general registers in the width the mode can encode, plus `ds`/`es`/`ss`/`fs`/`gs`. `push(rax)` in `bits 16` is `register 'rax' cannot be pushed in bits 16`. `pop(1)` is `pop() takes a register name`. Push and pop are allowed outside `naked`, because saving a register is not an assignment. Popping `rbp` in a function that has a frame discards the frame.
- `extern name;` declares a qword and does not emit storage. `data name { dq 42; }` is the bytes, in either order. A `let` of the same name is `extern 'x' is already defined`. `let saved_ksp` already exports the NASM label `saved_ksp`. `extern __syscall_rsp` and `extern __syscall_user_rsp` make the compiler emit those two quads, the same ones `set_syscall_stack` writes.
- `rdi`, `rsi`, and `rdx` are naked registers, the same names as the first three 64-bit parameters. `rdi = 65` overwrites that parameter. They exist so ring-3 code can fill `syscall()` with one instruction and no inserted moves.
- `syscall()` with `--freestanding`, `--flat`, or `--boot` emits the `syscall` instruction and nothing else. `rax` is the number, `rdi`/`rsi`/`rdx` are the arguments, and the result stays in `rax`. It takes no arguments: `syscall() takes no arguments with --freestanding; set rax, rdi, rsi and rdx first`. In `bits 16` or `bits 32` it is `syscall() is a 64-bit instruction`. The hosted `syscall(n, a1, ...)` helper is unchanged and still rejected in freestanding code.
- `tests/boot/stc_carry.lap` sets the carry flag and writes `COPIHUE` to COM1. QEMU checks that string. `tests/freestanding/insns.lap` is the instruction stream at `-O0` and `-O2`. It is assembled, not booted. `tests/lang/extern_data.lap` runs under Linux and prints 42.

### Changed
- Language version is 0.26.1.

## [0.26.0] - 2026-10-25

### Added
- Named registers are lvalues and rvalues inside a `naked` function, in every bit mode: `ds`/`es`/`ss`/`fs`/`gs`, `al`/`ah`/`dl`, `ax`/`sp`/`si`/`di`, `eax`/`ecx`/`edx`/`esp`, and `rax`/`rbx`/`rsp`/`rbp`. Outside a naked function the error is `register 'ax' is only available in a naked function`. A 64-bit name in `bits 16` or `bits 32` is `register 'rax' is not available in bits 16`. A local or a global of the same name is that variable. `cs` is not one of these names. An immediate writes that register only (`mov ah, 14`, `mov sp, 31744`), so the other bytes of `eax` survive. Any other right-hand side is computed in the accumulator first, which clears those bytes. A segment write still evaluates the right-hand side and then emits `mov ds, ax`, so `ds = 0` is unchanged. Reading a segment is `mov ax, ds` and then a zero-extend, because `movzx eax, ds` does not assemble.
- `jmp 0x8200` jumps to that address. The number is copied as written. `jmp other` jumps to a function or a data label in this assembly unit. `jmp(...)` is still a call. An unknown name is `unknown jump target 'name'`. There is no indirect jump.
- A naked function may take parameters. There is no frame and no prologue move: the name is the incoming register. 64-bit code, including a naked function with no `bits`, uses `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9` (at most 6), the same registers a Copihue call already fills. `bits 32` uses `eax`, `ecx`, `edx`, `esi` (at most 4). `bits 16` uses `ax`, `cx`, `dx`, `si` (at most 4). `ax` and `eax` are the accumulator, so a later expression overwrites that parameter. `bx`/`ebx`/`rbx` stay scratch and are not parameter registers. One parameter too many is `a naked function takes at most 6 register parameters`. A struct parameter is `a naked parameter must be an integer or a pointer`. `&param` is `naked parameter 'n' has no address`. A scalar or pointer local is still the 0.25.2 slot.
- `*(0x1000 as *u32) = 0x2003` and `*(rbp + 136) = x` store through an address that is not a bare name. `*p = v` is unchanged. The width is the cast pointee (`u8` is 1 byte, `u16` is 2, `u32` is 4, `u64` and `i64` are 8). With no cast the store is 4 bytes in `bits 16` and `bits 32`, and 8 bytes otherwise. A number, a naked register, or `reg+disp` / `reg-disp` is one memory operand. Anything else is computed, saved, and the value is stored through `ebx` or `rbx`. An 8-byte load or store in `bits 16` or `bits 32` is `an 8-byte store is not available in bits 16`. A literal that does not fit the pointee is the usual `expected u8, found integer literal 300`. An 8-bit or 16-bit load through this form zero-extends, and a 32-bit load is `mov eax`.
- `far_jump(selector)` reloads `CS` in 64-bit freestanding, flat, or boot code. It pushes the selector, the address of the next instruction, and `retfq`. Execution continues there. The two pushes are popped, so a frame above them stays. It clobbers `rax`. In `bits 16` or `bits 32` the error is `far_jump() is a 64-bit far return; use ljmp(selector, label) in bits 16 or 32`, because `ljmp` already emits `jmp sel:label` there. `retfq()` emits `retfq`, or `retf` in `bits 16` and `bits 32`. `jmp sel:label` is not an encoding in 64-bit mode.
- `tests/freestanding/naked_regs.lap` is the instruction stream at `-O0` and `-O2`. It is assembled, not booted. `tests/boot/jmp_sp.lap` sets the segments and `sp`, jumps to a label, and writes `COPIHUE` to COM1. QEMU checks that string. `jmp 0x8200` is assembled after the halt loop (`org 0x7C00`, a 16-bit near jump) and is not executed. `tests/lang/naked_param.lap` runs under Linux: a naked `inc` and `add` print 42, and a local named `ax` prints 7.

### Changed
- Language version is 0.26.0.

## [0.25.2] - 2026-10-25

### Added
- A scalar or pointer `let` inside a `naked` function is a labeled slot in the image (`mov [__loc_fn_N], eax` in `bits 16` / `bits 32`, `rax` in 64-bit code). It is not a stack frame: a prologue at the first instruction would use the BIOS stack, and `ss = 0` would then address the slot in the wrong segment. The slot is `dd 0` in 16/32-bit code and `dq 0` in 64-bit code, so it stays in a `--boot` sector. An array or a struct local is still `a naked function cannot have local variables`.
- `ds = expr`, and the same form for `es`, `ss`, `fs`, and `gs`, emits `mov <seg>, ax`. The value is the low 16 bits of the accumulator. A local or global with that name is still a variable. `cs` is not writable this way. Reading a segment register is not an expression.
- `tests/boot/seg_local.lap` sets `ds` and `es`, stores a local, and writes `COPIHUE` to COM1. QEMU checks that string. `tests/freestanding/naked_local.lap` is the instruction stream, including a `u8` truncation in `bits 32`. It is assembled, not booted.

### Changed
- `while` / `if` in `bits 16` and the other boot builtins shipped in 0.24.1. This release is the two items that release left out.
- Language version is 0.25.2.

## [0.25.1] - 2026-10-25

### Added
- Hosted `-O2` evaluates `+ - & | ^ *` and an immediate shift directly in the destination register when the operands are scalars, immediates, or scalar memory. `*` by a power of two is still `shl`. `/` and `%` stay on the `rax`/`rdx` path. A narrow integer still truncates in `rax` before the value is stored. `bits 16` and `bits 32` keep the `eax`/`ebx` stream.
- Two more scalar locals may live in caller-saved `r10` and `r11` when no call falls inside the live range. Names that are live across a call still use only callee-saved `r12`–`r15`; a further one stays in its stack slot. `let volatile`, address-taken names, arrays, pointers, structs, a name declared twice, and any function that contains `asm()` stay on the stack.
- A local or global array whose element is 1, 2, 4, or 8 bytes is loaded and stored with `base+index*scale` in one instruction. Pointer indexing is unchanged.
- When a hosted `-O2` function starts with `if (param <op> leaf) { return param; }` and `leaf` is an immediate or another register parameter, the compare uses the incoming register and returns before the frame is built. The base case does not spill a callee-saved register.
- `tests/lang/o2_alu.lap` checks that this matches `-O0`, including six scalars, a value live across a call, a fifth parameter that does not fit in `r12`–`r15`, scaled array loads and stores, `a[j+1]`, short-circuit `&&`, and a `let volatile` local.

### Changed
- Language version is 0.25.1. `-O0`, `--freestanding`, `--flat`, and `--boot` generate the same code as 0.25.0.

## [0.25.0] - 2026-10-25

### Added
- `copihue` accepts more than one source file. The files share one namespace and one assembly unit, in the order given. This replaces concatenating `.lap` files with `cat`.
- A top-level `import "path.lap";` compiles that file at that point. The path is relative to the directory of the file that contains the import. An absolute path is used as written. Importing a file that is already loaded is a no-op, including a second `import` of the same path. `import` is only recognized at the top level and only when the next token is a string, so a function may still be named `import`.
- A second definition of the same top-level function, global, struct, or `data` name is `function 'f' is already defined at file:line:col` (the word is `global`, `struct`, or `data` for those declarations). The message names the earlier file and line, and the caret is on the new definition. A forward declaration (`fn f();`) and a later definition are one function, in either order, including across files.
- A cycle is `import of 'path' is a cycle`. A missing file is `cannot open import 'path'` from an import, or `copihue: error: cannot open 'path'` from the command line. The path compared is the string, not a canonical real path: `a.lap` and `./a.lap` are not the same file.
- `tests/lang/multi_file.lap` imports a sibling and prints 42. The suite also compiles two files from the command line and rejects a duplicate name across them.

### Changed
- Language version is 0.25.0.

## [0.24.1] - 2026-10-25

### Added
- In a `bits 16` or `bits 32` function, integer expressions and `if` / `while` use `eax` (and `ebx` / `ecx` / `edx`), not `rax`. `mov rax, 1` is what nasm rejects in those modes. `while (1)` and `if (inb(port) & mask)` assemble. A 64-bit function is unchanged.
- `read_cr0` / `read_cr2` / `read_cr3` / `read_cr4` and `write_cr0` / `write_cr3` / `write_cr4` emit `mov eax, crN` and `mov crN, eax` in `bits 16` and `bits 32`. There is no `mov ax, cr0` encoding. In `bits 64` they still emit `mov rax, crN`.
- `inb` / `inw` / `inl` / `outb` / `outw` / `outl` load the port with `edx` in those modes. `outb(0x92, inb(0x92) | 2)` no longer emits `mov rax, 146`.
- `rdmsr` in `bits 16` / `bits 32` leaves the low half in `eax` and the high half in `edx`. It does not emit `shl rdx, 32`. `wrmsr` writes `edx:eax`. `wrmsr(m, rdmsr(m) | 0x100)` keeps the high half. A value that is not an `rdmsr` writes `edx = 0`.
- `lgdt(&name)`, `lidt(&name)`, and `invlpg(&name)` accept a `data` label (and a label inside a `data` block). The old error was `use of undeclared variable 'gdtr'`. In `bits 16` / `bits 32` the instruction is `lgdt [name]`, not `lgdt [rax]`.
- A `db` / `dw` / `dd` / `dq` operand may be a label plus or minus an integer: `dq page + 3`, `dq page + 0x83`. NASM evaluates the address. A lone label (`dd gdt`) is unchanged.
- `if carry { } else { }` branches on CF (`jnc`). `int_carry()` returns 1 when CF is set and 0 otherwise (`setc`). Both work after `int(0x13)` or `asm("stc")`, in every mode.
- `cpuid(leaf)` executes `cpuid` and returns `eax`. `cpuid_edx(leaf)` returns `edx`, which is where the long-mode bit lives (`1 << 29` after leaf `0x80000001`). `cpuid()` with no argument is `cpuid(leaf) takes 1 argument`.
- `tests/boot/realmode_builtins.lap` enters protected mode with `lgdt(&gdtr)` and `write_cr0` and writes `COPIHUE` to COM1. QEMU checks that string. `tests/freestanding/bits32_regs.lap` is the instruction stream for the MSR and `cpuid` forms; it is assembled, not booted.

### Changed
- Language version is 0.24.1.

## [0.24.0] - 2026-10-25

### Added
- Assignments, direct-call arguments, and `return` check integer and pointer types. The message is `expected T, found U`, `argument N of 'f': expected T, found U`, or `return value: expected T, found U`, with the source line and a caret. The argument count was already checked (`function 'f' takes N argument(s), not M`).
- `as` is an explicit conversion and is not a keyword. `&e as *u8` is `(&e) as *u8`. `1 + 2 as u8` is `1 + (2 as u8)`. A cast to a pointer or to `i64`/`u64` emits no instruction. A cast to 8, 16, or 32 bits truncates (unsigned zero-extend, signed sign-extend): `300 as u8` is 44 and `200 as i8` is -56.
- A literal that does not fit an 8, 16, or 32-bit destination is `expected u8, found integer literal 300` (and the same form for the other widths). A non-literal store into that width truncates. A narrow parameter is truncated when it is received.

### Changed
- An integer literal still converts to any pointer, so `let vga: *u16 = 0xB8000` keeps compiling. An integer variable does not. `*i8` and `*u8` match; `*u8` and `*Point` do not. `mut` and `volatile` are ignored when comparing pointers. `malloc`, string literals, and `bss_start()` / `bss_end()` keep the types existing kernels use. `syscall` / `syscall6` arguments stay raw register values.
- Arithmetic operators are still 64-bit. The truncation is at the store, at a narrow parameter, and at `as`. Scalar slots are still 8 bytes; the value is masked or sign-extended before the qword store, or before it is written to `r12`–`r15` when hosted `-O2` keeps that local in a register. Packed, volatile, pointer, and array stores already write the real width.
- Indirect calls, comparisons, and array or struct return types are not checked.
- Language version is 0.24.0.

## [0.23.1] - 2026-10-25

### Added
- Hosted `-O2` keeps up to four scalar locals or parameters in `r12`–`r15` and evaluates a simple right-hand side in `rbx` instead of `push`/`pop`. Arrays, pointers, structs, address-taken names, `let volatile` locals, a name declared twice, and any function that contains `asm()` stay on the stack. `-O0`, `--freestanding`, `--flat`, and `--boot` generate the same code as 0.23.0.
- `tests/lang/o2_regs.lap` checks that this matches `-O0`, including a call, recursion, `&local`, sibling blocks, `malloc`, and a volatile global.

### Changed
- `malloc` holds the requested size in `rbx` so it does not clobber a local kept in `r12`.
- Language version is 0.23.1.

## [0.23.0] - 2026-10-25

### Added
- `ltr(selector)` loads the task register (`ltr ax`). The argument is the selector value, not the address of the TSS. In hosted mode the error is `ltr() is only available with --freestanding, --flat, or --boot`.
- `iretq()` and `sysretq()` emit the instructions a kernel uses to enter or leave ring 3 on a frame it built itself. NASM has no `sysretq` mnemonic; the text is `o64 sysret` (REX.W SYSRET, encoding `48 0F 07`), which loads RIP from `rcx` and RFLAGS from `r11` and enters ring 3.
- A 64-bit TSS is a `packed struct` of 104 bytes: `rsp0` at offset 4, `ist1` at 36, and the I/O map base at 102. `tss.rsp0 = sp` (or `p->rsp0 = sp`) is the context-switch write. An ordinary struct would put every field in an 8-byte slot and would not match the CPU.
- `tests/flat/user_syscall.lap` programs `EFER.SCE`, `STAR`, `LSTAR`, and `SFMASK` with `rdmsr` / `wrmsr`, then `ltr`.
- `tests/usermode` is a floppy image QEMU boots. The sector loads a flat kernel at `0x8000`. The kernel enters long mode, loads a TSS, programs the syscall MSRs, `iretq`s to ring 3, and the user code does `syscall` and `int 0x80`. COM1 shows `USER`. The second interrupt runs only after `tss.rsp0` is changed, on that new stack.

### Fixed
- A `syscall fn` with 7 parameters popped its return value into `rax` before `sysret`. The spacer and the seventh argument are dropped with `add rsp, 16`, so ring 3 receives the value the body returned.

### Changed
- `interrupt fn` still ends in `iretq`. That returns to ring 3 when the CPU frame's `CS.RPL` is 3; the epilogue does not build the frame. `syscall fn` still ends in `o64 sysret`, which is the 64-bit return to ring 3. The `syscall` instruction does not switch stacks through the TSS; `set_syscall_stack` still does. `TSS.rsp0` is what the CPU loads on an interrupt from ring 3.
- Language version is 0.23.0.

## [0.22.2] - 2026-10-25

### Fixed
- `let` is scoped to its block. A name is visible from its declaration to the end of that block, a nested block may shadow it, and declaring the same name twice in one block is `redeclaration of 'name' in the same block`. Two `let b` in sibling blocks no longer share a lookup: the use reads the `b` that is in scope, not an earlier one from another branch. `{ ... }` is a statement. `for (let i = ...)` scopes `i` to the loop. Parameters share the function body's scope.

### Added
- Freestanding and flat tests that `fn f() -> *u8` compiles. The form has been accepted since 0.22.1.

### Changed
- Language version is 0.22.2.

## [0.22.1] - 2026-10-25

### Added
- A function return type may be any type, not a single token. `-> *u8`, `-> *mut u8`, `-> *volatile mut u8`, `-> **u8`, `-> [u8; 4]`, and `-> [*u8; 2]` parse. A missing type is `expected a type, found '...'`. The type is not checked, and the value still comes back in `rax`.

### Changed
- Language version is 0.22.1.

## [0.22.0] - 2026-10-25

### Added
- `naked fn` emits no prologue and no `leave`. `return` and falling off the end are a bare `ret`. A naked function takes no parameters and cannot have locals. It cannot be `interrupt` or `syscall`.
- `bits 16`, `bits 32`, and `bits 64`, in either order with `naked`, select the NASM mode for that function. `bits 16` and `bits 32` must be naked, because the frame uses 64-bit registers. The compiler emits `bits 64` afterwards. A function without `bits` emits no mode directive.
- `data name { ... }` emits labeled raw bytes in the text stream: `db`, `dw`, `dd`, `dq`, `times`, `align` (zero fill), and labels. A string expands to its bytes, so `;` inside the text is data. `asm()` still splits on `;`.
- `ljmp(selector, label)` emits `jmp sel:name`. `int(n)` emits `int N` from one integer literal (`int(0x10)` is `int 16`, encoding `CD 10`). Both are a compile error in hosted mode.
- `--boot` writes a 512-byte sector at `org 0x7C00`, padded with `times 510-($-$$) db 0` and signed `dw 0xAA55`. `_start` must be `bits 16 naked fn`. `--flat` and `--boot` cannot be combined.
- `tests/boot/boot_sector.lap`: a sector written only in Copihue. It prints with BIOS teletype and jumps to 32-bit protected mode, which writes `COPIHUE` to COM1. The hexdump is golden at `-O0` and `-O2`. CI boots it with QEMU when `qemu-system-x86_64` is installed.

### Changed
- Language version is 0.22.0.

## [0.21.0] - 2026-10-25

### Added
- Local arrays can be indexed in `--freestanding` and `--flat`. The hosted bounds trap (`syscall` + `exit`) is omitted there. Hosted programs keep the check. `&arr[i]` follows the same rule.
- Struct assignment `a = b` copies exactly the struct's bytes for ordinary and packed structs: locals, globals, array elements, and nested fields. A packed copy does not overwrite the next element. A mismatched struct type is a compile error.
- `*volatile T`, `*volatile mut T`, `*mut volatile T`, and `volatile name: T` fields. Each read and write uses the access width, is marked `  ; volatile`, and is not folded or lifted out of a loop at `-O2`. `let volatile` on a struct or an array is a compile error.
- `tests/flat/min_kernel.lap` exercises a local array, a packed struct copy, a `*volatile u32` store, and `read_cr2()`.

### Fixed
- Nested field access (`r.b.x` on an ordinary struct) uses the field's real offset. It used to address the first 8 bytes of the nested value, so `area` of a `Rect` was wrong. The callee's copy is still independent of the caller.

### Changed
- Language version is 0.21.0.

## [0.20.1] - 2026-10-25

### Added
- Freestanding and flat builtins `read_cr0()`, `read_cr2()`, `read_cr3()`, and `read_cr4()` return the register as an `i64`. `write_cr0(v)`, `write_cr3(v)`, and `write_cr4(v)` store that value and leave it in `rax`. In hosted mode each one is a compile error.
- `invlpg` accepts a variable address. The freestanding suite assembles `invlpg [rax]` from a computed local.

### Fixed
- `tests/flat/min_kernel.lap` installs IDT gates with code selector `0x18`, the 64-bit code segment of the AtomicOS bootloader GDT. Selector `0x08` is that loader's 32-bit code segment and hangs. The image assumes that GDT and is not a generic bootable kernel.

### Changed
- Language version is 0.20.1.

## [0.20.0] - 2026-10-25

### Added
- `packed struct`: fields use their real width with no padding, so an x86-64 IDT gate is 16 bytes. Global arrays of structs can be indexed and their fields assigned without clobbering a neighbor.
- Function addresses (`&name`) and indirect calls through a local, a global, or an array element.
- `interrupt fn` (0 arguments, or 1 for the CPU error code; saves the general registers; returns with `iretq`) and `syscall fn` (Linux register convention in, up to 7 arguments, stack switch, `sysret`). `set_syscall_stack(top)` publishes the kernel stack those entries switch to.
- `let volatile` globals. A volatile global is loaded and stored on every use and is not folded or lifted out of a loop.
- Functions may take up to 16 parameters. Arguments past the sixth use the System V stack slots.
- Freestanding instructions `lidt`, `lgdt`, `invlpg`, `rdmsr`, `wrmsr`, and `swapgs`.
- `tests/flat/min_kernel.lap`: a `--flat` image with boot, serial output, a bump allocator, a packed IDT, an interrupt handler, and a syscall entry.
- Diagnostics for an unknown function, a direct call with the wrong argument count, an assignment to an undeclared name, and an interrupt function with the wrong shape.

### Changed
- Language version is 0.20.0.

## [0.19.0] - 2026-10-22

### Changed
- Source files now use the `.lap` extension (from *Lapageria rosea*, the copihue). All tests and examples were renamed.

### Deprecated
- `.cph` (v0.18.x) and `.ch` (Chronos) are still accepted, with a warning asking you to rename the file to `.lap`.

## [0.18.2] - 2026-10-22

### Fixed
- `ptr->field` through a struct pointer parameter generated no code (reads returned garbage, writes were dropped). It now loads and stores the field, and an unknown field is a compile error.

## [0.18.1] - 2026-10-22

### Changed (breaking)
- The compiler command is now `copihue` (source `compiler/bootstrap-c/copihue.c`). `make install` installs `copihue`.

### Added
- `--flat ADDR`: build a raw bare-metal binary (`nasm -f bin`, `org ADDR`) that starts at `fn _start`, with no assembly entry stub. New built-ins `bss_start()` and `bss_end()` (flat mode only). This lets the AtomicOS kernel be written entirely in Copihue.
- Tests for flat binaries and for the new error messages.

---

## [0.18.0] - 2026-10-22

First release under the new name **Copihue** (formerly Chronos).

### Changed (breaking)
- Renamed the language to Copihue. The compiler command is `copihue` (`compiler/bootstrap-c/copihue.c`) and the source extension is `.cph`. `.ch` is still accepted with a deprecation warning.
- The default output executable is `a.out` (was `chronos_program`). Use `-o` to choose a name.
- Indexing a variable that is not an array or a pointer is now a compile error. It used to silently index the stack. Declare such parameters as pointers, e.g. `buf: *i8`.

### Added
- CLI: `-o`, `-S`, `--freestanding`, `--version`, `--help`, `-v`. The compiler is quiet by default and exits non-zero on any error.
- Diagnostics as `file:line:col: error: ...` with the source line and a caret: syntax errors, undeclared variables, unknown struct fields, bad escapes, unexpected characters, `break` outside a loop.
- `else if`, `break` and `continue`, and `for` steps written as `i++` or `i += n`.
- Bitwise operators `& | ^ ~ << >>` with Rust-like precedence; hex, binary and `_`-separated literals; character literals; string escape sequences.
- `u16`/`u32`/`u64` element types for arrays and pointers.
- Freestanding mode for kernels: `inb/inw/inl`, `outb/outw/outl`, `cli`, `sti`, `hlt`, `pause`, `asm("...")`.
- `make`, `make test`, `make install`; language reference in `docs/REFERENCE.md`.
- Regression suite: every test and example is compiled at `-O0` and `-O2` and compared to golden output, plus error-message, freestanding and HTTP server/client tests, all running in CI.

### Fixed
- Structs passed by value (callee crashed or assembled with undefined symbols); nested struct sizes and nested struct literals.
- Nested calls in argument lists no longer clobber earlier arguments, including in `write(fd, s, strlen(s))`.
- Stack frames are sized from the actual locals (was a fixed 1 KB, so large locals overwrote the return address), and `println` no longer overwrites locals.
- Signed `/` and `%` round toward zero, including the `-O2` shift and mask fast paths (`-7 / 2` was `-4`).
- `print`/`println` of a buffer printed nothing.
- `open()` with no mode created files that the owner could not read (`0644` was emitted as decimal).
- Examples: wrong `sin_port` byte order in the HTTP server and clients (the server listened on port 16415 instead of 8000), pointer parameters, a `test_loop` that looped forever.
- `break` used to be silently ignored.

---

## [0.17.0] - 2025-10-27

### Añadido
- **Compilador optimizador**:
  - Constant folding (-O1): Evaluación en compile-time
  - Strength reduction (-O2): Optimización de potencias de 2
  - Flags de optimización: -O0, -O1, -O2
- Documentación completa de optimizaciones
- Suite de benchmarks
- Demo interactivo de optimizaciones

### Mejorado
- Reducción de código generado: 20%
- Speedup: 3-40x en operaciones optimizadas
- Documentación reorganizada y consolidada

### Resultados
- 100% correctitud verificada
- 100% seguridad mantenida
- 100% determinismo preservado

---

## [0.16.0] - 2025-10-27

### Añadido
- Documentación reorganizada
- Guías de optimización manual
- Ejemplos de benchmarks

### Mejorado
- Estructura de carpetas
- Standard library organizada

---

## [0.15.0] - 2025-10-27

### Añadido
- **Operador módulo** (`%`)
- **Operadores lógicos** (`&&`, `||`, `!`)
- **Arrays locales tipados**: `let arr: [i32; 10]`
- **Operadores de incremento**: `++`, `--`
- **Operadores compuestos**: `+=`, `-=`, `*=`, `/=`, `%=`

### Mejorado
- Sintaxis más expresiva
- Código 40% más conciso
- Ejemplos actualizados

---

## [0.14.0] - 2025-10-26

### Añadido
- Reorganización de proyecto
- Standard library expandida
- Documentación mejorada

### Mejorado
- Estructura de carpetas clara
- Separación docs/examples/stdlib

---

## [0.13.0] - 2025-10-26

### Añadido
- **File I/O**: `open()`, `close()`, `read()`, `write()`
- Proof of concept de self-hosting
- Ejemplos de procesamiento de archivos

---

## [0.12.0] - 2025-10-25

### Añadido
- **Inicialización de arrays**: `{1, 2, 3}`
- **Inicialización de strings**: literales directos
- Ejemplos de uso

---

## [0.11.0] - 2025-10-25

### Añadido
- **Array types**: `[i32; 100]`
- **Pointer types** (básico)
- Verificación de tipos en arrays

---

## [0.10.0] - 2025-10-25

### Mejorado
- Refactorización del codegen
- Reducción de complejidad: 289 → 65 líneas
- Código más mantenible

---

## Formato

- **Añadido**: Nuevas características
- **Cambiado**: Cambios en funcionalidad existente
- **Deprecado**: Características que serán eliminadas
- **Eliminado**: Características eliminadas
- **Corregido**: Bugs corregidos
- **Mejorado**: Mejoras de performance o calidad
- **Seguridad**: Correcciones de seguridad

---

**Nota**: Versionado sigue [Semantic Versioning](https://semver.org/)
