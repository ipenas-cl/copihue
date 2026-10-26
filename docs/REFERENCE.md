# Copihue Language Reference (v0.40.1)

This document describes the language that `copihue` accepts today. Anything not listed here is not supported.
Planned features are in the README roadmap.

## Source files and compilation

- Source files use the `.lap` extension (from *Lapageria rosea*, the copihue). `.cph` (v0.18.x) and `.ch` (the pre-rename Chronos extension) are still accepted with a deprecation warning.
- A program is a list of `struct` definitions, `enum` definitions, global `let` declarations, functions, and (with `--freestanding`, `--flat`, or `--boot`) `section` blocks. Hosted programs start at `fn main() -> i32`, whose return value becomes the process exit code (modulo 256).
- `copihue file.lap [more.lap ...] [-o out]` writes one `output.asm` (NASM), assembles it with `nasm -f elf64`, and links it with `ld` into a static ELF64 executable (default `a.out`). There is no libc. Every file on that command is the same object. A file with no `module` shares one namespace with the other files that have none, and those declarations are visible across those files. A file that starts with `module` is its own module (see Modules). `-c` stops after the object (see Relocatable objects).
- `import "path.lap";` at the top level compiles that file there. The path is relative to the importing file's directory. An absolute path is kept as written. A file already loaded under the same path string is skipped. `import of 'path' is a cycle` reports a loop in a file with no `module`. A file that declared `module` reports `import of 'path' (module a) is a cycle`, and the diagnostic prefix is `module:file:line:col`. `cannot open import 'path'` reports a missing file. `import` is not a keyword: it is recognized only at the top level when the next token is a string. Importing a file with no `module` still places its names in the flat namespace. Importing a file with `module` loads that module; it does not copy the names (see Modules).
- Defining the same top-level function, global, struct, or `data` name twice is `function 'name' is already defined at file:line:col` (or `global`, `struct`, `data`). The earlier site is the one named in the message. `fn name();` followed by a body, or the reverse, is one function. Two forward declarations are allowed. The path is the string the compiler was given, not a canonical real path.
- `-S` writes assembly only. `-c` writes one ELF64 relocatable object and does not link (see Relocatable objects). `--freestanding` writes assembly for bare metal (see below); with `-c` it writes the object instead, and with `-c -S` it writes that object's assembly. `-O0`/`-O1`/`-O2` select the optimization level. `--flat ADDR` builds a raw bare-metal binary that starts at `fn _start` (see below). `--flat ADDR --elf` writes an ELF64 executable of that same image instead of the raw binary. `--boot` builds a 512-byte sector at `0x7C00` (see below). `--flat` and `--boot` cannot be combined, and neither can be combined with `-c`. `--elf` requires `--flat` and cannot be combined with `--boot`. `--checked` asks `--freestanding`, `--flat`, and `--boot` to bounds-check `[]T` and a local array. A hosted program checks both without `--checked`.
- `-O1` folds constant arithmetic. Hosted `-O2` also rewrites `*`, `/`, and `%` by a power of two. A binary operation whose operands are registers, immediates, or scalar memory is evaluated directly into the destination register (`add r15, r12`, `imul r12, r12, 13`) instead of copying through `rax` and `rbx`. `/` and `%` still use `rax`/`rdx`. The four hottest scalar or pointer locals that are live across a call stay in callee-saved `r12`–`r15` (saved in the frame, restored on every `ret`). Up to two more (`r10`, `r11`, caller-saved) hold scalars or pointers whose live range contains no call; a fifth name that is live across a call stays in its stack slot. A local or global array whose element size is 1, 2, 4, or 8 is addressed as `base+index*scale` in one instruction. A local stays on the stack if its address is taken, it is `volatile`, it is an array, or it is a struct, the name is declared more than once, or the function contains `asm()`. A pointer uses one of those six registers when it wins a slot. Pointer indexing is unchanged. A field load or store through a pointer already in a register is one memory operand. A numeric constant of that width is an immediate store (`mov qword [r12], 0`, `mov byte [r12+8], 1`). `sum = sum + a[i].field` and `sum = sum - a[i].field` add or subtract that load into `sum` when `sum` is already in a callee-saved register and the field is a scalar. If the function starts with `if (param <op> leaf) { return param; }` and `leaf` is an immediate or another register parameter, that compare runs on the incoming register and the return happens before the frame is built. A direct call to another function in the same program places up to six arguments in `rdi`, `rsi`, `rdx`, `rcx`, `r8`, and `r9` when each argument is a scalar the compiler can put in that register. An argument that is itself a call, an indirect call, a struct passed by value, `syscall`, and the hosted builtins keep the push/pop sequence. A function that never addresses `rbp` and never pushes saves its callee-saved registers with `push`/`pop`. One callee-saved temporary holds the left-hand side of an expression such as `f(n - 1) + f(n - 2)` across the call. `if` and `while` jump on the flags when the condition is a comparison of two scalars or of one array element and a scalar, including `&&` and `||`. The comparison does not become 0 or 1. `&&` still skips the right-hand side. A comparison used as a value still materializes 0 or 1. `if carry` is unchanged. The load that comparison performed is reused by the next use of that same element, including `a[j + 1] = a[j]`. A `while (i < lim)` whose previous statement sets `i` to a non-negative constant, whose body only increments `i` by one at the end, and which contains no call and no address-of, skips the local bounds check when every array it indexes with `i` has at least `lim` elements. A shorter array still traps. The inner product `s = s + a[i * n + k] * b[k * n + j]`, with `k` running from 0 to `n`, walks the two rows with running pointers when `i` and `j` are inside `[0, n)`. Otherwise the original loop runs, bounds checks included. Inside `i = C; while (i < N)`, with `C >= 0`, `N` a sole positive constant, and `i` incremented only at the end of the body, `j = i - 1; while (j >= 0 && a[j] > key) { a[j + 1] = a[j]; j = j - 1; } a[j + 1] = key` on a local `i64` array of at least `N` elements is a rotated loop and does not compare `j` with the length. `j` starts at `i - 1` and only decreases by one after `j >= 0`, so it stays at least -1 and `a[j + 1]` is in range. `key < a[j]` is the same condition. `a[j] > key && j >= 0` is not matched, because that reads `a[-1]`. A `>=` comparison, an extra statement, a narrow or spilled name, or a failed proof of `i` keeps the check. A store of a constant that fits the element width is an immediate `mov`, including a constant index. `i = 0; while (i < lim) { a[i] = C; i = i + 1; }` on a local `u8` or `i8` array, with `lim` inside the length, becomes `rep stosb` from element 0 and sets `i = lim`. If `lim` is not a proved constant, a runtime check keeps the original loop as the slow path. A negative limit, or one past the array, is not rewritten. `s = S; i = I; while (i < N) { s = s + A*i + B; i = i + 1; }` is replaced by the closed sum only when `N`, `A`, `B`, `S`, and `I` are constants, every product `A*i` and every partial sum fits in `i64`, and every term is non-negative or every term is non-positive. A sum that can wrap, a mixed-sign term, or `i * i` keeps the loop. A multiply of a register by 3, 5, or 9 is `lea` (`lea rax, [r10+r10*2]`). `*4` and `*8` stay shifts. `*6` stays `imul`. A straight-line run of two or more `x = (x + y) & imm`, with `imm` from 0 through 2147483647 and with neither `x` nor `y` assigned earlier in the run, emits the adds and then the masks. A later statement that reads an earlier destination keeps the original order. The inner product above is not unrolled: two steps per branch when `n` is even was measured slower. A bottom-tested loop that multiplies from memory was also slower, so the test stays at the top of the loop. A `while` is not inverted. A `while` whose body is straight-line statements around one `if (x == k) { ... } else { ... }`, with no nested `if`, `while`, `match`, `break`, `continue`, or call, jumps to the equal arm. The not-equal arm falls through, and each arm jumps back to the test. If that body indexes exactly one non-volatile global array, `lea r8, [name]` runs before the loop and the element is `[r8+index*scale]` instead of a disp32 index. That loop label is `align 32`, so the back edge starts a 32-byte chunk. Two global arrays, a call, `asm()`, `-O0`, `--freestanding`, `--flat`, `--boot`, and `bits 16` / `bits 32` keep the ordinary layout. A different `while` shape has one `if (x == k)` whose equal arm is cold: it contains a nested `if`, `while`, `match`, `break`, `continue`, `return`, or a call. The other statements in the body are straight and do not call, and the tail after the `if` is not a `let`. The equal arm is moved past the back edge (`je`). The tail falls through and jumps back once. An `else`, when present, is straight and stays on that path. `align 32` is not applied to this loop. When the equal arm always `return`s or `break`s, the compare is one field of `arr[i]`, and the tail loads another field of that same element into a register before it writes `i`, the element address is computed once and both loads use it. A size that is not a power of two stays `imul`. The equal arm is not reached with that address. A `let` in the tail, a volatile array, and an operand that is not a leaf stay on the ordinary layout, as do `-O0`, `--freestanding`, `--flat`, `--boot`, and `bits 16` / `bits 32`. This is not an inversion of `while`, and it is not an `align` on every function or every loop. The one exception is hosted `-O2` and a body that is only `cpu_pause()`: the true case jumps to a `pause` emitted after `ret`, and the false case falls through, so a spin does not take a branch on the way out. Any other body, including a `for` increment, stays top-tested. A hosted `-O2` loop that contains a call, whose index is `i = i + K` or `i = i - K` with `K` a positive constant and `i` an `i64` or `u64` (or an untyped local), rewrites each struct array indexed only by that `i` as a pointer that advances by `sizeof(element) * K`, when that size is not a power of two. `sum = sum + p->field` through that pointer still adds into a callee-saved `sum`. A power of two stays `shl` or a SIB scale. A loop with no call is left as an index. A local array is rewritten only when `i` is initialized to a constant, the condition is `i < N`, and that range fits the declared length; the per-access trap is then redundant. A name whose address is taken, a narrow integer index, a volatile array, and a whole-element load or store stay indexed. There is no general inlining: a leaf inliner was measured and slowed the order book, so a call stays a call. There is no unrolling, and `fib` is not rewritten. A byte copy `i = 0; while (i < lim) { dst[i] = src[i]; i = i + 1; }` of local or global `u8`/`i8` arrays, with `lim` a proved constant inside both lengths and `i` not narrow, is `rep movsb` and then `i = lim`. The same array on both sides is left alone. A limit that is a parameter stays the scalar loop. `i = 0; while (i < lim) { a[i] = (a[i] + i + r) & 255; i = i + 1; }` with `lim` a proved multiple of 16 inside the length is an SSE2 `paddb` loop (`movdqu`). The index vector is the bytes 0..15, then 16 added each step, so it wraps the same way as `i & 255`. `r` contributes its low byte, including when `r` is negative. A length that is not a multiple of 16 stays scalar. `i = 0; while (i < lim) { sum = sum + a[i]; b[i] = (b[i] + imm) & 255; i = i + 1; }` with `lim` a proved multiple of 16, `a` a `u8` array, and `b` a byte array, is SSE2 `psadbw` of the bytes before the add and `paddb` of `imm`'s low byte. The sum is added to `sum` and `i` becomes `lim`. An `i8` sum stays scalar, because `psadbw` is unsigned. A byte store `a[i] = (a[i] + imm) & 255`, `a[i] = a[i] + imm`, or `dst[i] = src[i] + imm` that is not that loop is `add byte` when the arrays match, and `movzx` / `add al` / `mov byte` when they differ. `(e) as u8` around that sum is the same low byte. The two `movzx` that 0.30.0 inserts after a narrow add and a narrow mask are not emitted for that store. A power-of-two index stride that is not 1, 2, 4, or 8 is `shl` (`sal` is the same opcode); 2, 4, and 8 stay a SIB scale. `while (i >= 0) { sum = sum + g[i].f; i = g[i].h; }` and `while (i > -1)` with the same body, on a global array of structs whose size is a power of two and whose two fields are 8-byte non-volatile scalars, is one `shl`, one `lea` of `base+scaled`, an add from memory, and a load of the next index. A local array is not rewritten, so its bounds check stays. A size that is not a power of two stays `imul`. Multiplying by 10 stays `imul`. The `lea` of `(x + x*4)*2`, including `v = v * 10 + (c - 48)`, was measured on `parse` and the min–max overlapped the previous window, so it is not emitted. gcc `-O2` vectorizes the C `matmul` inner product with `pmuludq` and does not interchange `i, j, k`; that is not emitted. gcc `-O2` rewrites the C `fib` as an even/odd split with one recursive call; that is not emitted either. Reusing the one callee-saved local as the temporary across the second call (12 instructions, still two calls) was measured on `fib` and the min–max overlapped the previous window, so the 16-instruction prologue stays. A modulo by a constant that is not a power of two stays `idiv`. These rewrites do not delete an arbitrary loop. `--freestanding`, `--flat`, `--boot`, `-O0`, and `bits 16` / `bits 32` do not do this.
- On error the compiler prints `file:line:col: error: message`, followed by the source line and a caret, and exits with status 1. A file that has declared `module` prints `module:file:line:col: error: message`. The lexer runs before that declaration, so an unclosed literal has no module prefix.

## Modules

A file with no `module` declaration stays in the flat namespace. `module`, `pub`, and `::` are contextual, not keywords. The design note is `docs/DESIGN_MODULES.md`.

`module name;` is the first declaration of the file. An item before it is `module must be the first declaration`. A second `module` in the same file is `a file has one module`. Two files in one compilation that use the same module name are `module 'mem' is already defined at file:line:col`. One file is one module; the same name in a later `-c` invocation is a different object (see below).

`pub` before `fn`, `let`, `struct`, `data`, `extern`, `packed`, `interrupt`, `syscall`, `bits`, `naked`, or `align` marks that item public. Any other item in the module is private. `pub` outside a module is `pub is only valid inside a module`, and the item stays in the flat namespace.

Inside the module, an unqualified name sees that module's items, private and public, and the builtins (`print_int`, `sizeof`, and the rest). It does not see the flat namespace or another module. Outside, and from another module, `mem::alloc` names a public item of a module this compilation loaded, by `import` or because the file was on the command line. Loading the module makes it visible to the whole compilation. `import` does not re-export names. A private item is `item 'hidden' of module 'mem' is private`. An unknown module is `unknown module 'mem'`. A missing item is `module 'mem' has no item 'name'`. The same short name may exist in two modules and in the flat namespace.

The link symbol of `mem::alloc` is `mem__alloc`. A flat declaration keeps the source spelling (`add`, `main`, `_start`). `fn main` and `fn _start` are entry points only in the flat namespace. `pub fn main` inside a module is `mod__main` and is not the entry. Two declarations that produce the same link symbol are `link symbol 'mem__add' is already used`. `__syscall_rsp` and `__syscall_user_rsp` keep those spellings.

A module struct is registered under the link spelling (`mem__Cell`). `let x: Cell` inside `mem` and `let y: mem::Cell` outside name that record. `sizeof` and `offsetof` use the same name.

One `copihue` invocation, imports included, is still one assembly unit: one ELF, or one `-c` object, or one `--flat` image. `pub` is not an ELF visibility flag. A defined function or global, private or public, is a NASM `global` in a `-c` object and a global symbol in a `--flat --elf` executable, named `mod__item` or the flat name. `--freestanding`, `--flat`, and `--boot` accept `module`, `pub`, and `import` with these rules. `import` inside `section` is still rejected. `--flat --elf` is that same unit as one ELF64 executable (see Flat binaries).

`extern name;` and `extern fn` still mean that another object defines the symbol. Without `-c`, `extern fn` is still an error. In the flat namespace the symbol is the name as written, so `tests/link` is unchanged. Inside `module mem`, `extern fn alloc` is `mem__alloc` and `extern slot` is `mem__slot`. `pub` on that `extern` decides whether another file of the same compilation may write `mem::alloc`. It does not change the NASM `extern`. The caller object declares the same `module mem` and the same `extern`, and `ld` links the two objects. `fn main` for that program is a flat file in the caller's invocation.

`tests/lang/modules.lap` imports `a`, then `b`, then `c`, with no cycle. `a::part` is 42, `b::part` is 32, and `c::part` is 12, at `-O0` and `-O2`. `c` publishes `struct Cell` and `let extra`. `a` keeps `hidden` private and calls it from `part`. `tests/errors/module_cycle.lap` is the cycle. `tests/errors/module_private.lap` is the private item. `tests/freestanding/module.lap` assembles `mfs__answer` and `call mfs__answer` and is not booted. `tests/link/parts/mod_mem.lap` is one `-c` object (`T mem__add`, `D mem__bias`, no `_start`). `mod_mem_use.lap` and `mod_mem_main.lap` are the other (`U mem__add`, `U mem__bias`, `T mem__use_it`, `T main`, `T _start`). Linked with `ld` and no libc, the program prints 42 at `-O0` and `-O2`.

## Lexical structure

- Comments: `// to end of line`.
- Identifiers: `[A-Za-z_][A-Za-z0-9_]*`.
- Keywords: `fn let mut if else while for return struct`. `break` and `continue` are statements. `enum` and `match` are not keywords.
- `packed`, `interrupt`, `syscall`, `volatile`, `align`, `bits`, `naked`, `data`, `import`, `section`, `extern`, `module`, `pub`, `unsafe`, `enum`, and `match` are contextual words, not keywords. `in` is contextual in `for name in`. `..` is the range token in `for i in a..b`. `::` is the module qualifier (`mem::alloc`) and is not a keyword. `packed struct`, `interrupt fn`, `syscall fn`, `let volatile`, `*volatile`, `align(64) struct`, `align(64) field: T`, `align(64) let`, `bits 16 naked fn`, `data name { ... }`, `import "path.lap";`, `section "name" { ... }`, `extern name;`, `extern fn`, `module name;`, `pub fn`, `unsafe { ... }`, `enum Name { ... }`, and `match e { ... }` use them. A function or variable may still be named `syscall`, `volatile`, `align`, `data`, `import`, `section`, `extern`, `module`, `pub`, `unsafe`, `enum`, or `match`. `match(1)` with no `{` after the call is still a call. `=>` is `=` followed by `>`. `align(N)` as a statement is an error. `module` opens a module only when the next token is an identifier and the one after that is `;`. `unsafe` opens a block only when the next token is `{`.
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
| `[]T` | slice: a pointer and an `i64` length, 16 bytes |
| `enum Name` | tag `i64` at offset 0, payload from offset 8, size rounded up to 8 |
| `struct Name` | fields at their real width, naturally aligned, nested structs embedded |
| `packed struct Name` | fields at their real size, with no padding |
| `[T; N]` as a field | `N * sizeof(T)` bytes, packed or not |

`i8`, `u8`, `i16`, `u16`, `i32`, and `u32` arithmetic uses that width. The result is truncated after the operator: unsigned keeps the low bits, signed sign-extends. `i64` and `u64` are still computed in 64-bit registers. The element type also affects loads and stores through arrays, pointers, and struct fields:
loads from unsigned types zero-extend, signed narrow loads sign-extend, and stores truncate to the field's width. Scalar locals and globals always occupy 8 bytes. `sizeof` reports the type's width, not that slot. `sizeof` of a `[]T` is 16. `sizeof` of an `enum` is the value size (`Option` with `Some(i64)` is 16, a nullary enum is 8).

## Slices

`[]T` is the easy level: the compiler checks the index. The explicit level is `unsafe { ... }` or `s.ptr[i]`, and that is what kernel code uses. The design note is `docs/DESIGN_SLICES.md`. `[T; N]` is still the fixed array. A pointer index is still unchecked.

```copihue
let buf: [i64; 4] = [10, 20, 30, 40];
let s: []i64 = buf;       // pointer to the first element, length 4
let n: i64 = s.len;       // 4
let p: *i64 = s.ptr;      // the pointer, without the length
let t: []i64 = slice(p, n);
```

The pointer is at offset 0 and the length is at offset 8. `T` is an integer, a pointer, or a struct, the same types `[T; N]` allows. `[][]T` and `[][T; N]` are `a slice element must be an integer, a pointer, or a struct`. `[ []T; N ]` is `a slice cannot be an array element`. A struct field of slice type is `a slice cannot be a struct field`. `-> []T` is `a slice cannot be returned`. In `bits 16` and `bits 32`, `[]T` is `[]T is a 64-bit value`.

`s.ptr` is `*T` and `s.len` is `i64`. Both are readable. Assigning either is `slice field 'ptr' is read-only` or `slice field 'len' is read-only`.

`let s: []T = arr` converts a local or global `[T; N]` of the same element type. Another slice is copied (pointer and length). `let s: []T = slice(p, n)` builds one from a `*T` and a length. The pointer is not checked. A negative literal is `negative slice length`. In a hosted program, and with `--checked`, a negative length that is not a literal writes `negative slice length` and exits with code 1. `--freestanding`, `--flat`, and `--boot` without `--checked` store that length as it is. A different element type is `expected []i64, found [u8; 4]`. Any other initializer is `a slice is initialized from an array, a slice, or slice()`. `let s: []T;` with no initializer is a null pointer and length 0. `s = t` copies the 16 bytes. `slice()` on a global is `slice() initializes a local []T`.

A parameter `s: []T` is the address of those 16 bytes. The callee copies them, the same way a struct is passed by value. The caller passes a slice. The conversion from an array is the `let`. An array parameter is `an array parameter is a pointer or a slice; use *T or []T`.

`s[i]` compares `i` as an unsigned integer with the length. A negative index fails. On failure the program writes `index out of range` and a newline to file descriptor 2 (syscall 1) and exits with syscall 60 and code 1. There is no libc. That is the hosted default, at `-O0` and at `-O2`. `--freestanding`, `--flat`, and `--boot` do not insert it. `--checked` asks for it in those modes, and also turns on the local-array check. `s[i] = v` uses the same rule. When `T` is a struct, `s[i]` is the element's address, and `s[i].field` writes that field.

`unsafe { ... }` is a block. Inside it, `s[i]` and a local-array index do not trap. Types are still checked. `s.ptr[i]` is a pointer index and never checks, hosted or freestanding.

`for x in e { }` requires a slice or an array. Anything else is `for in expects a slice or an array`. It lowers to a `while` from 0 to the length, and the element load is not checked: the index is the loop index and it only grows by one. `for x in` of a struct element is `for in of a struct element; index the slice`. `for i in start..end { }` is a `while` from `start` up to but not including `end`. `i` is `i64`. `for (init; cond; step)` is unchanged. `continue` still runs the increment.

The check of `s[i]` is omitted when `start` is the literal `0`, `end` is `s.len`, and the body does not assign `i` or the slice. `a[i]` in `for i in 0..N` is omitted when `N` is a literal, the array has at least `N` elements, and the body does not assign `i`. Another index, a shorter array, or an assignment of `i` keeps the check. This holds at `-O0` and at `-O2`. It does not use the hosted `-O2` `while` proof.

There is no capacity, no `append`, no slice nested in a struct, no returning a slice, and no check of a pointer that is not a slice. A `vec` (below) is a different value: it has a capacity, and it is not a slice.

## Vectors and owning pointers

`vec` and `own` are contextual, not keywords. `vec` is a type only when the next token starts a type (`*`, `[`, or a name). `own` is a type only when the next token is `*`. A parameter or a local named `vec` stays a name. Both forms are hosted locals. `--freestanding`, `--flat`, `--boot`, `bits 16`, `bits 32`, `naked`, `interrupt fn`, and `syscall fn` reject them (`vec is hosted-only`, `an owning pointer is hosted-only`).

```copihue
let v: vec i64;
vec_push(v, 7);          // returns the new length, 1
let n = v.len;           // 1
let c = v.cap;           // 4 after the first growth
let p: *i64 = v.ptr;
let q: own *i64 = malloc(8);
*q = 1;
```

`let v: vec T` is 24 bytes: a pointer at offset 0, an `i64` length at 8, and an `i64` capacity at 16. It starts empty (a null pointer, length 0, capacity 0). An initializer is `a vec starts empty`. `T` is an integer (`i8`/`u8`/`i16`/`u16`/`i32`/`u32`/`i64`/`u64`), a pointer, or a struct the program defines. A name that is none of those is `a vec element must be an integer, a pointer, or a struct`. A slice, an array, another `vec`, or `own *T` is `a vec element cannot be a slice, an array, a vec, or an owning pointer`.

`vec_push(v, x)` appends one element and returns the new length as `i64`. The first growth allocates 4 elements. Later growth doubles the capacity, copies the old bytes with `rep movsb`, and `free`s the old buffer. Allocation is the existing `malloc` (`mmap`, no libc). If `mmap` fails, the program writes `out of memory` and a newline to file descriptor 2 and exits with code 1. `v` must be a local `vec` (`vec_push() takes a local vec`). The value is checked against `T`. A struct element is copied from an addressable value: a local, an index, or a field. The new slot is `ptr + len * sizeof(T)`, not a checked index. A struct literal has no address (`cannot copy this struct value`).

`v[i]` and `v[i] = x` use the slice rule: the index is compared as an unsigned integer with the length, and failure writes `index out of range` and exits 1. A struct element is the address of that element, so `v[i].field` reads the field. `v.ptr[i]` is a pointer index and is not checked. `v.ptr` is `*T`, `v.len` is `i64`, and `v.cap` is `i64`. Assigning any of them is `vec field '...' is read-only`. A slice has no `cap` (`slice has no field 'cap'`). Using the `vec` itself as a scalar is `a vec is not a scalar; read an element, .ptr, .len, or .cap`. `v = e` is `a vec cannot be assigned`. `&v` is `a vec has no address`.

`let s: []T = v` copies the pointer and the length into `s`. `s` does not own the buffer. The `vec` still frees it. The copy is the bytes at that `let`: a later `vec_push` that reallocates, or the drop of `v`, leaves `s` dangling. There is no lifetime check. A different element type is `expected []U, found vec T`. `for x in v` makes the same copy at the start of the loop, into a temporary that does not own the buffer. A `vec_push` inside the loop is not visible to that copy. `for x in` of a struct element is `for in of a struct element; index the slice`.

`let p: own *T = e` is a pointer local. The block that declares it calls `free` on it when the block ends. `return`, `break`, and `continue` do the same before they leave. `free` of a null pointer does nothing. A `let` with no initializer stores null, so its drop is that no-op. After a drop the slot is set to null, so a second drop of the same slot is also a no-op. The pointer stays on the stack: the register planner does not keep it in a register across the hidden `free`.

Copying one owning pointer into another is `an owning pointer cannot be copied`. `p = e` is `an owning pointer cannot be reassigned` (it does not free the old pointer and install a new one). `&p` is `an owning pointer has no address`. Passing `p` to a `*T` parameter borrows it: the slot is not cleared, the callee must not `free` it, and the owner still does. `let q: *T = p` is the same borrow. Storing a raw `*T`, or `malloc`, into a new `own *T` adopts it; only that owner frees it.

`fn take(p: own *T)` is an owning parameter. The callee frees it when `take` returns. The argument must be an owning local or a call that returns `own *T` (`an owning parameter takes an owning pointer`). After the pointer is in the argument register, the caller's slot is set to null, so the caller's drop is `free` of null. The same owner in two arguments of that call is `an owning pointer cannot be moved twice`, including a borrow beside the move. An indirect call does not move.

`fn give() -> own *T` returns an owning pointer. `return p`, when `p` is an owning local, loads it and then stores null, so `give`'s drop does not free it. The caller must move that result into `own *T`: `let q: own *T = give()`, or `take(give())`. `let q: *T = give()`, `q = give()`, a discarded `give();`, and passing `give()` to a `*T` parameter are `an owning pointer must be moved into 'own *T'`. `return p` from a function that is not `-> own *T` is still `an owning pointer cannot be returned`. `return malloc(8)` from `-> own *T` adopts the pointer. A global, a struct field, and `-> vec T` stay rejected (`an owning pointer must be a local`, `a vec must be a local`, `a vec cannot be returned`). `own` not followed by a pointer is `own applies to a pointer`.

## Enums

`enum` and `match` are contextual, not keywords. The design note is `docs/DESIGN_ENUMS.md`. An enum is a sum type: a tag and a payload, stored in a local, a parameter, a global, or a struct field. There is no heap and no libc. The same lowering works in hosted code, `--freestanding`, and `--flat`.

```copihue
enum Option { None, Some(i64) }
enum Pair { Empty, Both(i64, i32), Byte(u8) }

fn take(o: Option) -> i64 {
    return match o {
        None => 0,
        Some(n) => n
    };
}
```

`Option` is not a compiler type. The program writes the enum. A variant has zero or more fields. A field is an integer or a pointer (`*T`, including `*volatile T`). Anything else, including `own *T`, a slice, an array, a `vec`, or another enum, is `an enum field must be an integer or a pointer`. `None()` in the definition is `variant 'None' has no fields; omit the parentheses`. An empty enum is `an enum needs a variant`. A repeated name is `variant 'Some' is already defined`. More than 16 fields is `variant '...' has too many fields`. A payload past 4096 bytes is `variant '...' is too large`.

The tag is an `i64` at offset 0. Tags are the source order, starting at 0; a duplicate name is not given a tag. The payload starts at offset 8. Each field is aligned to its width. The value size is rounded up to 8. `sizeof(Option)` for `None` and `Some(i64)` is 16. Three nullary variants are 8. `Both(i64, i32)` puts the `i64` at 8 and the `i32` at 16, and that enum is 24 bytes when a later variant does not need more.

`Option::None` and `Option::Some(7)` write a local: the slot is zeroed, then the tag and the fields are stored. A narrow field is truncated. `let x = Option::Some(1)` and `let x: Option = Option::Some(1)` have that enum type. `let o: Option;` with no value is `an enum local needs a value`. `let volatile` on an enum is `volatile cannot apply to an enum`. `x = y` copies the bytes when both sides are the same enum. A different enum is `expected Option, found Color`. `Option::Some()` is `variant 'Some' takes 1 field, not 0`. `Option::None()` is `variant 'None' has no fields; omit the parentheses`. `Option::Nope` is `enum 'Option' has no variant 'Nope'`. A field of the wrong type is the usual check (`expected i64, found *u8`).

A parameter `o: Option` is passed by address and copied, the same way a struct is. The callee's assignment does not change the caller. Passing the same enum twice is another copy, not a move. `take(Option::Some(7))` fills a temporary of that size and passes its address. A global, a field, and a call that returns the enum do the same: the call writes a temporary, then that address is the argument. Using the enum as a scalar is `an enum is not a scalar; match it`. Indexing it is `cannot index 'o'; match it`.

`fn make(n: i64) -> Option` returns the enum through a hidden caller pointer in `rdi`, not through `rax:rdx`. `Color` is 8 bytes, `Option` is 16, and `Pair` is 24, so one ABI covers every size. The callee stores `rdi` in an anonymous frame slot and writes the bytes through that pointer. After the drops, `rax` holds the address. The caller reads the slot. Real parameters shift: the first five use `rsi`, `rdx`, `rcx`, `r8`, and `r9`, and the sixth is at `[rbp+16]`. `make(4)` therefore sees `4` in `rsi`. `let x = give()` sets the destination to `x`. `return give()` forwards the caller's hidden pointer. A discarded `give();` allocates a temporary. A different enum is `expected Option, found Color`. `return o` from `-> i64` is still `an enum cannot be returned`, and the caret is on `return`. An indirect call is `an indirect call cannot return an enum`. `bits 16`, `bits 32`, `naked`, `interrupt fn`, and `syscall fn` cannot return an enum. Hosted, `--freestanding`, and `--flat` 64-bit functions can.

`match e { Arm => expr, ... }` is an expression. The arm is an expression, or a block whose last statement is an expression. `e` is a local, a parameter, a global, or a field of enum type, including `p->field` when `p` points at a struct. `match Option::Some(1)`, `match give()`, and `match *p` of a pointer to the enum are `match needs a local enum`. `&o` is the address of the bytes; matching through that pointer is not supported. Each variant appears once (`variant 'Some' is already matched`). A variant of another enum is `variant 'Color::Red' is not in 'Option'`. One diagnostic names every missing variant, and the caret is on `match`: `match does not cover 'None', 'Other'`. The pattern's field count must match the variant (`variant 'Some' takes 1 field, not 0`). Bindings are new locals of that arm and may hide an outer name. Two bindings of the same name in one arm are `redeclaration of '...' in the same block`.

`_` covers every variant that is left and skips `ud2`. `Option::_` is `'_' is not a variant` and does not cover. `_(x)` and `_()` are `'_' has no fields` and do not cover. A second `_` is `'_' is already matched`. An arm after a well-formed `_` is `arm after '_' is unreachable`. A block arm whose last statement does not yield a value is `match arm block needs a result`, and the caret is on `{`.

At `-O0`, and in `--freestanding` and `--flat`, a local is still `cmp qword [rbp+off], tag` / `jne` to the next arm. Hosted `-O2` loads the tag once (`mov rbx, [slot]`) and each arm is `cmp rbx, imm` / `jne`. The arm body may clobber `rbx`; that path jumps to the end, so the next compare is reached only by `jne`. A global is `mov rbx, [name]`. A field through a pointer is one `mov rax, [rbp+slot]` and `mov rbx, [rax+off]`. The field is still spilled to a new slot and reloaded. Each field uses `movzx`, `movsx`, or `movsxd` when it is narrower. The arm leaves a scalar in `rax`. Without `_`, a tag that matches nothing is `ud2`. A program that covers every variant does not execute it. There is no `syscall`.

Hosted `-O2` does not put an enum, an enum parameter, or an enum return in a register. That function builds a normal `rbp` frame, including a function that returns an enum, because it has to save `rdi`. The short `push`/`pop` frame has no `rbp`, and copying the tag there would write the caller's frame. `match` is treated as a stack use, so it does not get that short frame either. A short-frame caller does not call a function that returns an enum.

Arm results are scalars. Integer arms use the widest width. A literal arm is `i64`, so a match of only literals is `i64` and does not depend on source order. `let x: i8 = match c { Red => 1, ... }` is `expected i8, found i64`. Pointer arms must be compatible; the more volatile side wins when one direction was already allowed. Mixing a pointer and an integer is `match arm: expected ..., found ...`. An arm whose value is an enum, a struct, or a slice is `an enum is not a scalar; match it` (or the same shape of message for that value).

`let g: Option;` is a zeroed BSS global (`resb` of the enum size). Tag 0 is `None` when `None` is the first variant. An initializer is `an enum global starts at zero; assign it in a function`. `volatile` on that global is `volatile cannot apply to an enum`. Assign it from a function, then `match g`. `struct Box { o: Option, n: i64 }` is a field: assign it, copy it, return it, and match `b.o` or `p->o`. An array element is still `an enum cannot be an array element`. A slice element is `a slice element cannot be an enum`. A `vec` element is `an enum cannot be a vec element`. An enum inside `module` is `an enum must be in the flat namespace`. `bits 16`, `bits 32`, and `naked` (including 64-bit `naked`) are `an enum is a 64-bit value`. There is no `|` pattern, no guard, and no named field (`Some { x: i64 }`). Reassignment copies the bytes; it does not free anything. An owning pointer cannot be an enum field, so a variant does not drop.

`tests/lang/enum_opt.lap` prints 7, 0, 4, 14, 9, 1, 16, 24, 8, 0, 5, 8, 0, 104, 2, 40, 7, 40, and 1 at `-O0` and `-O2`. The 2 and the following 40 are `take` of an enum that is not the first local: the call keeps its own frame, and the `i64` beside it stays 40. `tests/lang/enum_use.lap` returns an `Option` (one register argument, five, and six, with the sixth at `[rbp+16]`), copies through `id`, shows that a callee assignment leaves the caller's `Some(3)` in place, matches `_` and a block arm, matches a zero global and then `Some(11)`, and matches a struct field and `p->o`. It prints 4, 15, 10, 4, 3, 3, 3, 4, 0, 8, 2, 6, 7, 0, 0, 11, 12, 12, 12, 13, 12, 1, and 14 at `-O0` and `-O2`. `tests/freestanding/enum.lap` assembles with no `syscall` at both levels and is not booted. `tests/flat/enum.lap` is the same image at both levels and is not booted. Both still return `i64`; the hidden pointer is not in that stream.

## Generics

A function or a struct may take type parameters. The compiler emits one ordinary copy for each combination the program uses. There is no dictionary and no runtime lookup. The design note is `docs/DESIGN_GENERICS.md`. `vec` and `Option` are not rewritten as generic types.

```copihue
fn max<T>(a: T, b: T) -> T {
    if (a > b) { return a; }
    return b;
}

struct Par<A, B> { a: A, b: B }
```

A type argument is an integer, a pointer (`*T`, `*mut T`, `*volatile T`, or a nested pointer), or a struct, including an instantiated generic struct (`Box<Par<i32, u8>>`). An enum, a slice, an array, a `vec`, or `own *T` is `a type argument cannot be a slice, an array, a vec, or an owning pointer`, or `type argument 'Option' is not an integer, a pointer, or a struct`. An unknown name is `unknown type argument 'Foo'`. `mut` is not part of the type and is not part of the symbol: `*mut i32` and `*i32` are one copy. `*volatile i32` is another.

At most eight type parameters. More is `too many type parameters`. A repeated name is `type parameter 'T' is already defined`. A builtin integer used as a name is `type parameter 'i32' hides a builtin type`. An empty `<>` is `expected a type parameter`.

The symbol is stable. `max<i32>` is `max$1$i32`. `id<*u8>` is `id$1$p$u8`. `Par<i32, *u8>` is `Par$2$i32$p$u8`. `Box<Par<i32, u8>>` is `Box$1$Par$2$i32$u8`. A digit after `$` is a count, so a struct named `p` with one argument is `p$1$i32` and `*u8` is `p$u8`. `$` is not an identifier character, so a source name cannot collide with a copy. Inside a module the link spelling wraps that name: `mem::max<i32>` is `mem__max$1$i32`. The template is not emitted and is not a symbol. The copy is. With `-c`, the object that defines the body emits `global max$1$i32`, and `extern fn max<T>` emits `extern max$1$i32` for the copy the call uses. `pub` is still source visibility. Two objects that both emit the same body still fail at `ld`. There is no COMDAT and no weak symbol.

`>>` inside a type-argument list is two `>` characters, so `Box<Par<i32, u8>>` parses. A value `x >> 2` stays a shift. `a < b` stays a comparison. `<...>` is a type-argument list only when a `>` closes it and the next token is `(` (a call) or `{` (a struct literal).

`max(a, b)` infers each type parameter from the value arguments whose types mention it. A bare `T` binds the whole argument type, including a pointer. `*T` does not match `*volatile u8` (`cannot infer type argument 'T' for 'star' from '*volatile u8'`). Two arguments that disagree are `type argument 'T' is 'i32' and 'i64' in 'max'`. A literal is `i64`, so `max(1, 2)` is `max<i64>`. No information is `cannot infer type argument 'T' for 'make'`. `max<i32>(1, 2)` fixes the type and then uses the existing argument check; a literal that fits `i32` is allowed. The wrong count is `function 'max' takes 1 type argument, not 2`. A non-generic function or a builtin is `function 'print_int' takes 0 type arguments, not 1`.

A struct is named with its arguments: `let p: Par<i32, i64>` and `Par<i32, i64> { a: 4, b: 5 }`. Omitting them is `struct 'Par' needs type arguments`. The wrong count is `struct 'Par' takes 2 type arguments, not 1`. A literal `Par { ... }` does not infer the arguments. `sizeof(Par<i32, i64>)` and `offsetof` see the mangled name. A field of an instantiated struct is laid out before any user struct that contains it. `*Node<T>` inside `Node<T>` is a pointer and does not recurse. A parameter `s: []T` infers `T` from the slice element. `[]T` is still not a legal type argument.

The operation is checked when the copy is generated, not on the template. `+`, a comparison, or unary `-` or `~` whose operand is a struct, an enum, or a slice is `cannot apply '+' to 'Point' in 'add<Point>'`. Pointer arithmetic the compiler already accepts stays. An integer copy keeps that width's truncation, so the bytes match a function written by hand for the same type. The diagnostic points at the template. There are no traits.

The template is not code-generated. Each used combination is cloned, substituted, and emitted later. A second look at the same call finds the copy. A recursive call finds the copy already registered. The cap is 64 struct copies and 64 function copies (`too many monomorphized copies`). The copy keeps the template's `bits` and `naked`. `--freestanding` and `--flat` use the same code when the function does not need `malloc`. `_start` is still emitted first.

`tests/lang/generic.lap` prints 9, 9, 2, 3, 0, 4, 5, 2, 9, 6, 7, 8, 1, 1, 7, 5, 1, 44, 11, 8, 9, 5, 16, 8, and 4 at `-O0` and `-O2`. `tests/freestanding/generic.lap` assembles `call max$1$i32` with no `syscall` at both levels and is not booted. `tests/flat/generic.lap` is the same image at both levels and is not booted. `tests/link` also links `max$1$i32` (prints 9) and `mem__max$1$i32` (prints 7). The template is not a symbol.

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
fn fib(n) { return n; }             // n is i64, and so is the result
fn sum(a, b) { return a + b; }      // both parameters and the result are i64
```

- Functions take up to 16 parameters. The first six are passed in `rdi, rsi, rdx, rcx, r8, r9`. The seventh is at `[rbp+16]`, then one 8-byte slot each. A scalar return value comes back in `rax`. A function that returns an enum passes the destination in `rdi` and shifts the real arguments: five registers (`rsi` through `r9`), then `[rbp+16]`. `rax` then holds that address. See Enums.
- The type after `->` may be any type: a name (`i64`, `Point`), a pointer (`*u8`, `*mut u8`, `*volatile mut u8`, `**u8`), `own *T`, or an array (`[u8; 4]`, `[*u8; 2]`). `volatile` and `mut` may appear in either order after `*`. A missing piece is `expected a type, found '...'`. An integer or pointer `return` is checked against that type (see Type checking). An array or struct return type is parsed and is not checked. `-> []T` is rejected (`a slice cannot be returned`). `-> vec T` is rejected. `-> Enum` returns the bytes through a hidden pointer in `rdi` (see Enums). `return` of an enum from a non-enum function is `an enum cannot be returned`. `-> own *T` moves; see Vectors and owning pointers. A scalar still comes back in `rax`. The same form compiles in hosted, `--freestanding`, and `--flat` code, except `own *T`, which is hosted-only.
- A parameter with no `: T` is `i64`. A function with no `->` returns `i64`. An explicit annotation still wins. This is the same default as an untyped integer `let`, not a guess from later uses. `fn sumar(datos, n)` does not make `datos` a pointer because the body indexes it: `datos[0]` is `cannot index 'datos': it is not an array or a pointer`. A use of `datos[i]` or `*datos` does not fix `*u8` versus `*i64`: a narrower integer still type-checks in a wider destination, `i8` and `u8` are the same pointer width, and `*p` of a non-volatile pointer is a 64-bit load that does not reveal the pointee. A field through that parameter does not pick a struct either. Declare the pointer. `let x = e` still infers `i64` from an integer, including a narrow load, and the pointer type from a pointer. It does not infer a struct.
- A name is visible from its `let` to the end of the block that contains it. `{ ... }` is a block, and so is the body of `if`, `while`, and `for`. A nested block may declare the same name again (the inner one hides the outer one until the inner block ends). Declaring it twice in the same block is `redeclaration of 'name' in the same block`. Parameters belong to the function body, so `fn f(b: i64) { let b = 1; ... }` is that error. `for (let i = 0; ...)` scopes `i` to the loop. Using a name after its block, with no outer declaration, is `use of undeclared variable 'name'`.
- A struct parameter (`r: Rect`) is passed by value: the callee gets its own copy. A pointer parameter (`r: *Rect`) refers to the caller's struct; use `r->field`.
- Using an undeclared variable, or assigning to one, is a compile error.
- A direct call of an unknown function is a compile error, and the argument count must match the declaration (`function 'f' takes N argument(s), not M`). When the count matches, each argument is checked against the parameter type. A call through a local or a global is indirect and is not checked (see Function pointers).
- Indexing (`x[i]`) requires `x` to be an array, a slice, or a pointer. Indexing a scalar is a compile error. A slice index is checked in hosted code (see Slices).

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

### Alignment

`align(N)` before `struct`, `packed struct`, a field, or a global `let` sets that alignment. `N` is a power of two from 1 to 4096. Anything else is `align() takes a power of two from 1 to 4096`. `align(64) let` inside a function is `align() applies to a struct, a field, or a global`.

A field's `align(N)` replaces its natural alignment, including inside `packed struct`. The struct's alignment becomes the strictest of its fields and of `align(N)` on the struct itself. The size is rounded up to that alignment, so `sizeof` and `offsetof` count the padding. `struct Lines { align(64) a: u64, align(64) b: u64 }` is 128 bytes, with `b` at 64. `align(64) struct One { x: u64 }` is 64 bytes. A packed struct with no `align` stays contiguous.

A global is placed with NASM `align` when it says `align(N)` or when its type's alignment is stricter than 8. In `.bss` the pad is `resb`. Two `align(64)` globals do not share a 64-byte line: their addresses differ by at least 64 and each is 0 mod 64. A local stays an 8-byte slot. `tests/lang/align64.lap` prints the offsets, the sizes, the gap, and the low 6 bits of the addresses.

`a = b` copies one struct value onto another of the same type. The copy is exactly the struct's size: whole qwords, then a byte tail, so a packed record does not spill into the next field or array element. It works for locals, globals, array elements (`table[i] = other`), and nested fields (`box.point = p`). A different struct type is a compile error. Nested scalar access (`box.point.x`) addresses that field, not the first eight bytes of the parent.

Passing a struct to a function is still a copy into the callee's frame. There is no move, and the two objects are independent after the assignment.

### Volatile globals

```copihue
let volatile ticks: i64 = 0;
```

Every read and every write of a volatile global is a memory operand. The compiler does not fold it or lift it out of a loop, at `-O0` or `-O2`. Use this for a counter that an interrupt handler updates.

`*volatile T`, `*volatile mut T`, and `*mut volatile T` make accesses through that pointer volatile. `p[i] = v`, `*p = v`, and `*p` load or store the pointee's real width (`u8`/`u16`/`u32`/`u64`) and the assembly is marked `  ; volatile`. A struct field may be declared `volatile name: T`; reads and writes of that field use the same mark and the field's width, including `ptr->field`. `let volatile` on a struct or an array is a compile error: mark the fields, or point at the object with `*volatile T`.

`volatile` is part of the pointer type. `*volatile mut T` and `*mut volatile T` are both `*volatile T`. `mut` is not part of the type, so `*mut u8` and `*u8` still match. Assigning `*volatile T` to a non-volatile pointer is `expected *u8, found *volatile u8`. The other direction is allowed: `*u8` may initialize `*volatile u8`, and so may an array of `u8`. `*volatile i8` and `*volatile u8` are the same byte pointer. An integer literal still converts to any pointer, including `*volatile u32`. `as` is the explicit drop (`p as *u8`) and emits no instruction.

What is guaranteed: each source-level read or write of a volatile global, a `*volatile` pointee, or a volatile field emits a memory instruction of that width, and the compiler does not reorder those accesses with each other. `-O2` does not delete one, fold one, or lift one out of a loop. What is not guaranteed: `volatile` alone is not a C memory model. Use the atomic builtins below for a protocol. A non-volatile access next to a volatile one can still be simplified. `let volatile` on a local scalar keeps that local in its stack slot when hosted `-O2` puts other scalars in registers. At `-O0`, and with `--freestanding`, `--flat`, or `--boot`, every local is already on the stack. The address of a volatile scalar or a volatile field is `*volatile T`, so `fetch_add(&cell, 1)` type-checks when `cell` is `let volatile cell: u64`.

### Atomics

These builtins work in hosted code and in `--freestanding`, `--flat`, and 64-bit `--boot` code. They are rejected in `bits 16` and `bits 32` (`is a 64-bit operation`). The pointer must be `*volatile u32`, `*volatile i32`, `*volatile u64`, or `*volatile i64`. A non-volatile pointer is `fetch_add() takes a *volatile u32 or *volatile u64, found *u64`. A `*volatile u8` is the same error with `found *volatile u8`. The width is the pointee. A 32-bit operation zero-extends into `rax` (`mov eax`). The returned type is that pointee (`u32`, `i32`, or `i64` for a 64-bit pointee).

| Built-in | x86-64 | Returns |
|---|---|---|
| `atomic_load_acquire(p)` | `mov` | the value |
| `atomic_load_seq_cst(p)` | `mov` then `mfence` | the value |
| `atomic_store_release(p, v)` | `mov` | 0 |
| `atomic_store_seq_cst(p, v)` | `xchg` (locked by itself) | 0 |
| `fetch_add(p, v)` | `lock xadd`. A hosted `-O2` statement whose result is discarded is `lock add` | the old value |
| `atomic_add(p, v)` | `lock xadd`, then add the addend. A hosted `-O2` statement whose result is discarded is `lock add` | the new value |
| `swap(p, v)` | `xchg` | the old value |
| `compare_exchange(p, expected, desired)` | `lock cmpxchg` | the old value, whether or not it matched |
| `fence()` | `mfence` | 0 |
| `cpu_pause()` | `pause` | 0 |

On x86 a plain load already has acquire semantics and a plain store already has release semantics. `seq_cst` is the stronger one: the load is followed by `mfence`, and the store is `xchg`. `compare_exchange` does not return a boolean. The caller compares the old value with `expected`. `r10` and `r11` hold the pointer and the addend; they are caller-saved. `tests/lang/atomics.lap` runs the sequence on one thread at `-O0` and `-O2`. `tests/freestanding/atomics.lap` is the instruction stream, assembled and not booted.

Hosted `-O2` emits an atomic on `&name` or `&name.field` as one memory operand when `name` is a global or a stack slot, and not a pointer or an array. A field of a field accumulates the offsets. A pointer variable still loads the pointer into `r11`. An indexed element still computes the address. A load through any other address is `mov r11, rax` and then the `mov` from `[r11]`. A store or a read-modify-write of that other address still pushes it, because the value expression runs first. A statement `fetch_add` or `atomic_add` whose result is discarded is `lock add` (an immediate addend is immediate). When the caller uses the old or the new value, the instruction stays `lock xadd`. `-O0`, `--freestanding`, `--flat`, and `--boot` keep `lock xadd` and the push. A statement that is only a store, `fence()`, or `cpu_pause()` does not emit `xor eax, eax`. A nested call still returns 0. `&cells[i].field` is the address of that field, including a volatile field of an array element, so `atomic_store_release(&cells[i].seq, 7)` writes that cell. `tests/lang/field_atomic.lap` prints the load, the plain field, `sizeof` 64, and both `fetch_add` results. `tests/lang/o2_atomic.lap` prints 0, 6, 10, 2, 9 at `-O0` and `-O2`.

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

A naked function may take parameters. Nothing is copied into a slot. The name is the register the caller already used. 64-bit code, and a naked function with no `bits`, uses `rdi`, `rsi`, `rdx`, `rcx`, `r8`, `r9`, at most six. That is the same order a Copihue call already uses, so `naked fn inc(n)` can return `n + 1` and a hosted caller sees the result. `bits 32` uses `eax`, `ecx`, `edx`, `esi`, at most four. `bits 16` uses `ax`, `cx`, `dx`, `si`, at most four. `bx`, `ebx`, and `rbx` are not parameter registers. In 64-bit code `rbx` is scratch. In `bits 16` and `bits 32` an arithmetic expression restores `ebx` (see below). `bx` and `bp` (and `ebx`, `ebp`, `esi`, `edi`) are naked lvalues in `bits 16` and `bits 32`; a 64-bit function still cannot name them, because `rbx` is the scratch and `rbp` is the frame. `ax` and `eax` are also the accumulator, so a later expression overwrites a parameter that lives there. An `if` or `while` that only jumps on the flags does not, and neither does a store of a fitting literal. One parameter past the limit is `a naked function takes at most 6 register parameters` (4 in `bits 16` or `bits 32`). A struct parameter is `a naked parameter must be an integer or a pointer`. Taking the address is `naked parameter 'n' has no address`: there is no slot to point at.

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

`jmp rax` and `jmp [resume_rip]` are indirect, and only inside `naked`. NASM treats `jmp rax` as a register jump, not as a label, so outside `naked` the compiler reports `an indirect jmp is only available in a naked function` and does not emit the instruction. A framed function has no business discarding `rbp`. The register names are the same general registers `push` accepts (`ax` through `r15`, not the segment registers). The width has to match the mode: `jmp rax` in `bits 16` is `register 'rax' is not available in bits 16`. `jmp [name]` reads an address from a global, a `data` label, or a naked local slot. The size prefix is the mode's address width: `jmp qword [resume_rip]` in 64-bit code, `jmp dword [name]` in `bits 32`, `jmp word [name]` in `bits 16`. That is the form AtomicOS used as `asm("jmp qword [resume_rip]")` in `resume_shell`. A name that is not memory is `jmp [name] takes a variable or a data label`, and nothing is emitted. In 64-bit code `rbx` is the scratch of a binary operator, so a value left there does not survive `a + b`. In `bits 16` and `bits 32` that operator push/pops `ebx`, and a shift push/pops `ecx`, so a named `ebx` survives `a + b`. `bp` and `ebp` are safe only because `naked` has no frame.

An `if` or `while` condition in `bits 16` or `bits 32` does not keep that scratch. The compare is still `cmp eax, ebx`: `push ebx` runs first, and `pop ebx` runs after `cmp`. `pop` does not change the flags, so the jump is the same and a named `ebx` (the INT 15h continuation token) is intact when the body runs. `edx` is still saved across the right-hand operand. The same `push ebx` / `pop ebx` wraps every `+`, `-`, `&`, `|`, `^`, `*`, `/`, `%`, or shift, including one outside the condition: `ebx = 7; di = di + 24` leaves `ebx` at 7, not at 24. A shift also push/pops `ecx`, because `shl`, `shr`, and `sar` need `cl`, inside a condition and outside one. A `u32` `>>` in these modes is `shr`, and a `u32` `/` or `%` is `div` (it zeros `edx` for the divide, then pops the saved `edx`). A comparison used as a value (`let x = a == b`, or `si = di == 0`) push/pops `ebx` too. `pop` is after `cmp` and before `sete`, and it does not change the flags. A `let` whose initializer is one of those expressions, a compound assignment (`di += 4` is `di = di + 4`), and each argument of a call take that path, so they do not leave `ebx` modified. A `bits 16` or `bits 32` call itself uses `ax`/`cx`/`dx`/`si` or `eax`/`ecx`/`edx`/`esi`, not `ebx`. `idiv`, and `div` of a `u32`, `push edx` before the divide and `pop edx` after it. The remainder copies `edx` into `eax` before that pop. The zero-divisor path jumps to the same pop, so a named `edx` survives `ax / 7`, `ax % 7`, `ax / 0`, and `u / 2`. A store whose address is not a bare operand pushes `ebx` before the address and pops it after the store. `lgdt`, `lidt`, and `invlpg` of a pointer that is not a data label still use `[bx]` or `[ebx]`, and pop `ebx` afterward. A data label stays `lgdt [gdtr]` and does not touch `ebx`. 64-bit code is unchanged: `rbx` remains scratch, and a condition there does not write 0 or 1 into `rax`.

`*(0x1000 as *u32) = 0x2003` and `*(rbp + 136) = x` are stores whose address is not a bare name. `*p = v` for a named pointer stays the older store, including a `*volatile` pointee. The width is the cast's pointee: `u8`/`i8` one byte, `u16`/`i16` two, `u32`/`i32` four, `u64`/`i64` eight. With no cast the store is 4 bytes in `bits 16` and `bits 32`, and 8 bytes otherwise. A number, a usable naked register, or `reg+disp` / `reg-disp` is one NASM memory operand. An integer literal that fits that width is an immediate store and does not use the accumulator: `*(0x1000 as *u32) = 0x2003` is `mov dword [4096], 8195`, `*(rbp + 136) = 7` is `mov qword [rbp+136], 7`, and `*(4200200 as *u8) = 0` is `mov byte [4200200], 0`. Any other value is still computed in the accumulator and stored from it (`mov [rbp+136], rax`). An address that is not one of those operands is evaluated and pushed. In `bits 16` and `bits 32` `ebx` is pushed first and popped after the store, so a named `ebx` is unchanged. 64-bit code still stores through `rbx` and leaves that scratch as it is. A literal that does not fit the pointee is `expected u8, found integer literal 300`. An 8-byte load or store in `bits 16` or `bits 32` is `an 8-byte store is not available in bits 16` (or `load`). A load of one or two bytes zero-extends. A 32-bit load is `mov eax`, which clears the upper half of `rax`. A signed 8-bit or 16-bit cast is not sign-extended here: `bits 16` has no `movsx rax`, and the zero-extending form is the one that assembles in every mode. `rbp` is readable in a 64-bit naked function, so `rax = *(rbp + 128)` is `mov rax, [rbp+128]`.

`far_jump(0x18)` is the 64-bit far return. `jmp 24:label` is not an encoding in long mode. The builtin pushes the selector literal, `lea`s the address of the next instruction, pushes that address, and executes `retfq`. Execution continues at the label. The two pushes are popped, so a frame that was already on the stack is unchanged. `rax` is clobbered. The selector is one integer literal. In `bits 16` or `bits 32` the error is `far_jump() is a 64-bit far return; use ljmp(selector, label) in bits 16 or 32`. Hosted code is `far_jump() is only available with --freestanding, --flat, or --boot`. `retfq()` takes no arguments and emits `retfq`, or `retf` in `bits 16` and `bits 32`. `ljmp` is unchanged and still emits `jmp sel:label`, which NASM accepts in 16-bit and 32-bit code.

`bits 16` and `bits 32` require `naked`. The ordinary frame uses `rbp` and `rax`, which nasm rejects in 16-bit mode (`instruction not supported in 16-bit mode`). Inside those functions the compiler evaluates integers in `eax` / `ebx` / `ecx` / `edx`, so `while (1)`, `if (e)`, `inb`, `outb`, `read_cr0`, and `wrmsr` assemble. After a 16-bit or 32-bit function the compiler emits `bits 64`, so the next function is 64-bit again. `bits 64` emits that one directive before the function and no extra restore after it.

A call from `bits 16` or `bits 32` uses the same registers as a naked parameter, not `push rax` / `pop rdi`. `push rax` has no encoding in `bits 16`, and a 64-bit `ret` would pop a return address the 16-bit `call` did not push. Every function in these modes is naked, so the callee already reads `ax`, `cx`, `dx`, `si` or `eax`, `ecx`, `edx`, `esi`. The caller evaluates each argument into the accumulator, pushes that width (`push ax` or `push eax`), and pops into those registers in reverse. At most four arguments: `a bits 32 call takes at most 4 register arguments`. The callee has to be the same bits mode. `a bits 16 call cannot target a 64-bit function` and `a bits 16 call cannot target a bits 32 function` are the two mismatches. The `call` is near and relative. In `bits 16` the displacement is 16 bits, so the callee must sit within ±32KiB or nasm rejects the object; both functions share `CS`, and a far call is not emitted. An indirect call has no name to check, so `an indirect call is not available in bits 16`. The result is left in `ax` or `eax`, which is also the accumulator: the callee must copy the first argument out of `ax`/`eax` before the next expression if it still needs it. A 64-bit call is unchanged (`push rax` / `pop rdi`).

### Loose instructions, `extern`, and `syscall()`

`lodsb()`, `stosb()`, `stosd()`, `stc()`, `clc()`, `cld()`, `std()`, and `ud2()` emit that one instruction. `rep_stosb()`, `rep_stosw()`, `rep_stosd()`, `rep_stosq()`, `rep_movsb()`, `rep_movsw()`, `rep_movsd()`, and `rep_movsq()` emit `rep` and the string instruction (`rep stosd`, `rep movsb`). `rep_stosq()` and `rep_movsq()` in `bits 16` or `bits 32` are `rep_stosq() is not available in bits 16`. None of these take arguments. `lodsb` reads `ds:si` or `rsi` into `al` and steps the index. `stosb` writes `al` at `es:di` or `rdi`. `stosd` writes `eax`. `rep` repeats on `cx` or `rcx`. `cld()` clears DF and `std()` sets it. They take no arguments and assemble in `bits 16`, `bits 32`, and 64-bit code. `rep` still uses whatever DF is at the instruction.

`pushfd()` and `popfd()` are `pushfd`/`popfd` in `bits 16` and `bits 32`, and `pushfq`/`popfq` in 64-bit code.

`push(ax)` and `pop(rbx)` name the register. The names are `ax` `bx` `cx` `dx` `si` `di` `bp` `sp`, the `e` and `r` forms of those, `r8` through `r15`, and `ds` `es` `ss` `fs` `gs`. A 64-bit name in `bits 16` is `register 'rax' cannot be pushed in bits 16`. A 16-bit or 32-bit name in 64-bit code is rejected the same way: long mode has no `push eax`. Segments are accepted in every mode. `push(0x33)` is `push 51`. Any other expression is evaluated and then `push rax` (or `push eax`). That evaluation uses the accumulator. `pop(1)` is `pop() takes a register name`. Push and pop are not naked-only. Popping `rbp` in a function that has a frame drops the frame on the floor.

`extern __syscall_rsp;` is a qword with no storage of its own. Reading it is `mov rax, [__syscall_rsp]`. Naming `__syscall_rsp` or `__syscall_user_rsp` makes the compiler emit both quads, which is the same pair `set_syscall_stack` defines. Without `-c`, any other `extern` name needs a definition in this assembly file. `data answer { dq 42; }` is that definition, and the two declarations may come in either order. `let saved_ksp: i64 = 0` is the other form: the NASM label is the Copihue name (`saved_ksp: dq 0`). `extern` of a name that already has a `let` is `extern 'x' is already defined`. With `-c`, an `extern` name that this object does not define is a NASM `extern` (see Relocatable objects). `__syscall_rsp` and `__syscall_user_rsp` stay defined in the object that names them, so only one object in the link may name them.

`syscall()` in `--freestanding`, `--flat`, or `--boot` with no arguments is the one instruction `syscall`. The caller has already set `rax`, `rdi`, `rsi`, and `rdx`. `syscall(n, a1, a2, a3, a4, a5, a6)` loads those registers and then executes `syscall`: `rax` is the number, then `rdi`, `rsi`, `rdx`, `r10`, `r8`, `r9`. The result stays in `rax` until the next expression that writes the accumulator. Six arguments after the number is the maximum (`syscall() takes at most 6 arguments`). A bare `syscall()` stays one instruction, so a program that filled the registers itself does not gain extra moves. In `bits 16` or `bits 32` it is `syscall() is a 64-bit instruction`. Hosted `syscall(n, a1, a2, ...)` is the same register load plus `syscall`, and that helper is still the Linux path. It is not what `--freestanding` calls. `let r = syscall(8)` stores `rax` into `r` before the next statement. A later immediate store or `if` reloads `r`. A naked `rax = 8; syscall(); while (rax == 0)` keeps the result as well, because that `while` jumps on the flags instead of replacing `rax` with 0 or 1. In `bits 16` and `bits 32` the same condition also restores `ebx` after `cmp eax, ebx`, so `while (ebx != 0)` does not zero a named `ebx`. An arithmetic expression in those modes restores `ebx` as well, so `di = di + 24` does not replace the INT 15h continuation token.

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
a block `{ ... }`, `if (c) { } else if (d) { } else { }`, `if carry { } else { }`, `while (c) { }`, `for (init; cond; step) { }`, `for x in e { }`, `for i in start..end { }`, `unsafe { ... }`, `match e { ... }`, `break;`, `continue;`, `return e;`, and expression statements (calls). `match` is an expression; see Enums.
`if carry` has no parentheses. It branches on CF (`jnc` skips the body). `if (carry)` is still a call. CF must still be set: `println` and most arithmetic clear it before a later `int_carry()`.
An `if` or `while` whose condition is a comparison, `&&`, `||`, or `!` jumps on the flags. The same is true of the `while` a `for` becomes. The branch does not write 0 or 1 into the accumulator, so `while (rax == 0)` still holds the compared value when it ends, and the body of `if (rax == 0 - 1)` still sees that value in `rax`. A comparison, `&&`, `||`, or `!` used as a value (`let x = rax == 0`) still writes 0 or 1. On the fall-through path the flags are the flags of that `cmp`. `if carry` as the next statement reads those flags.
In a `for (init; cond; step)` loop, `step` can be any simple statement (`i++`, `i += 2`, `i = i * 2`). `continue` jumps to `step`. `for x in e` and `for i in start..end` lower to that same `while`, so `break` and `continue` still apply. The slice and range forms are described under Slices.

## Operators (highest precedence first)

| Operators | Notes |
|---|---|
| `x[i]` `s.f` `p->f` `f(...)` | postfix |
| `-x` `!x` `~x` `*p` `&x` | unary |
| `e as T` | cast. Looser than unary and postfix, tighter than `*` `/` `+`. `as` is not a keyword |
| `*` `/` `%` | `/` and `%` truncate toward zero. Signed, except a `u32` `/` or `%` in `bits 16` or `bits 32`, which is `div` |
| `+` `-` | the result is then truncated to the result type when that type is narrower than 64 bits |
| `<<` `>>` | the result type is the left operand. `>>` is arithmetic, except a `u32` `>>` in `bits 16` or `bits 32`, which is `shr` |
| `&` | |
| `^` | |
| `\|` | |
| `==` `!=` `<` `>` `<=` `>=` | result 0 or 1. Signed, except a `u32` compare in `bits 16` or `bits 32` (`jb` / `ja` / `setb` / `seta`). A zero-extended `u8` or `u16` still uses the signed condition |
| `&&` | short-circuit |
| `\|\|` | short-circuit |

As in Rust, bitwise operators bind tighter than comparisons: `x & 2 == 2` means `(x & 2) == 2`. `1 + 2 as u8` is `1 + (2 as u8)`, and `&e as *u8` is `(&e) as *u8`.

## Type checking

A direct assignment (`let`, `x = e`, `a[i] = e`, `s.f = e`, `p->f = e`), a direct call, and `return` check integer and pointer types. A mismatch is `file:line:col: error: expected T, found U`, with the source line and a caret. A call says `argument N of 'f': expected T, found U` (N starts at 1). A return says `return value: expected T, found U`. The compiler keeps going and ends with `1 error, compilation failed` or `N errors, compilation failed`.

`as` converts on purpose. It is allowed between any two types. A cast to a pointer or to `i64` / `u64` emits no instruction. A cast to `i8` / `u8` / `i16` / `u16` / `i32` / `u32` keeps the low bits: unsigned zero-extends (`300 as u8` is 44), signed sign-extends (`200 as i8` is -56). That explicit cast is not a range error.

These implicit conversions stay legal, because kernels and the existing programs rely on them:

- An integer literal, including `0` and `0xB8000`, converts to any pointer. `let vga: *u16 = 0xB8000` needs no `as`. The same form covers `0x8000`, `0x10000`, `0x200000`, and `0x400000`. A variable that is an integer does not: `let n = 1; let p: *u8 = n` is an error, and `let p: *u8 = n as *u8` is the fix.
- `*i8` and `*u8` are the same byte pointer. `*u8` and `*Point` are not, and neither are `*u8` and `**u8`, or `*i32` and `*i64`.
- `mut` is not part of the comparison. `volatile` is. `*mut u8` and `*u8` match. `*volatile u8` does not match `*u8`: dropping volatile is `expected *u8, found *volatile u8`. Assigning `*u8` to `*volatile u8` is allowed. `*volatile i8` and `*volatile u8` match.
- A narrower integer promotes to a wider one (`u8` passed to `putc(c: i64)`, `return` of a `u8` from `-> i64`).
- A pointer plus or minus an integer has that pointer's type and may also be used as `i64`. An array decays to a pointer to its element and may not be used as `i64` without `as`. `&function` is `i64`, not a pointer. `&struct` is `*Struct` and does not become `*u8` without `as`.
- A string literal is `*u8`. `malloc` returns an address that converts to any pointer and to an integer. `bss_start()` and `bss_end()` are `*u8`; `bss_end() - bss_start()` is `i64`.
- `read` / `write` buffers, `free`, `lidt`, `lgdt`, and `invlpg` accept an integer or any pointer. `&` of a struct is one of those addresses: `lgdt(&gdtr)` and `lidt(&idtr)` with `gdtr` and `idtr` of struct type need no `as`. `save_context(&ctx)` and `restore_context(&ctx)` take that same address of a struct. `syscall` and `syscall6` arguments are raw register values, so a pointer or an integer is accepted.

A literal that does not fit the destination is an error: `expected u8, found integer literal 300`. The ranges are `u8` 0..255, `i8` -128..127, `u16` 0..65535, `i16` -32768..32767, `u32` 0..4294967295, `i32` -2147483648..2147483647. `-1` does not fit an unsigned type. `0xFF` fits `u8` and does not fit `i8`. `i64` accepts any integer literal; a negative literal does not fit `u64`.

A wider integer does not narrow by itself. `let wide = 300; let z: u8 = wide` is `expected u8, found i64`. The conversion is `let z: u8 = wide as u8`, which stores 44 (the low bits; a signed cast sign-extends). The same check applies to a field, an array element, a store through a pointer, a direct call argument, and `return`. `as` binds tighter than `+`, `-`, `*`, `/`, `%`, `&`, `|`, `^`, and the shifts, so the whole right-hand side needs parentheses: `vga[i] = ((attr << 8) | c) as u16`. A narrower integer still promotes (`return` of a `u8` from `-> i64`, a `u8` passed to `putc(c: i64)`). The same width still matches, including a change of sign (`i64` and `u64`, `i8` and `u8`, `i32` and `u32`). A literal that fits is still implicit and stores no extra instruction. A narrow parameter is still truncated when it is received. A cast whose width is the destination memory slot is that store: the slot write does not add a second mask. A scalar local truncates through the cast, and the following store does not mask again.

A `u8`, `u16`, `u32`, `i8`, `i16`, or `i32` operator still truncates to that type after the instruction, so `let x: u8 = 200; x + 100` is 44 and `x + 100 < 50` is true. The width is the wider typed operand. The result is signed only when both operands are signed; if either is unsigned, the result is unsigned. A shift uses the left operand's type and does not widen to the count. An untyped integer literal adopts the other operand when it fits (`x + 100` with `x: u8`). A literal that does not fit stays 64-bit (`x + 300` is 500, not a wrap of 300). Two untyped literals stay `i64`, so `let x: u8 = 200 + 100` is `expected u8, found i64` at `-O0`, and a literal error at `-O1` and `-O2` after folding (`expected u8, found integer literal 300`). `i64` and `u64` operators are not truncated. In 64-bit code a narrow unsigned value is zero-extended, so `sar`, `idiv`, and a signed condition match the unsigned operation. In `bits 16` and `bits 32` a `u32` with bit 31 set does not: `>>` is `shr`, `/` and `%` are `div`, and `<` `>` `<=` `>=` use the unsigned condition. `u8` and `u16` stay on `sar`, `idiv`, and the signed condition because the high bits of `eax` are clear.

Scalar locals and globals still occupy 8-byte slots when they live in memory. Struct fields occupy their real width. Hosted `-O2` may keep a scalar local in `r12`–`r15`, or in `r10`/`r11` when no call is in its live range; the mask or sign-extend is applied before the value is written to that register or to the slot, and again after a narrow operator. That truncation uses the destination register's own low half, including a call argument in `rdi`, `rsi`, `rdx`, `rcx`, `r8`, or `r9` (`movsxd rdi, edi`, not `eax`). A struct field, a store through a pointer, and an array element write only the real width. A load of a `u8`/`u16`/`u32` field zero-extends. A load of an `i8`/`i16`/`i32` field sign-extends.

Not checked: indirect calls, comparisons as a type error, bitwise mixes of pointers, `mut` used as a permission, and array or struct return types. An untyped `let x = e` infers `i64` from an integer, including a narrow load, the pointer type from a pointer, and an address from `malloc`. It does not infer a struct onto that slot: write `let p: Point = ...` or a struct literal. An omitted parameter type and an omitted `->` are `i64`. That default is not inferred from a use: indexing an untyped parameter does not make it `*i64`. An expression the compiler cannot type is accepted.

## Built-ins (hosted mode)

| Built-in | Meaning |
|---|---|
| `print(s)`, `println(s)` | write a string literal or a NUL-terminated buffer to stdout (`println` adds `\n`) |
| `print_int(n)` | write a signed decimal integer |
| `strlen(s)`, `strcmp(a, b)`, `strcpy(dst, src)` | C-style string helpers |
| `malloc(n)`, `free(p)` | memory from `mmap`/`munmap`. `free` of a null pointer returns 0 |
| `open(path, flags[, mode])`, `read(fd, buf, n)`, `write(fd, buf, n)`, `close(fd)` | Linux syscalls; default mode is `0644` |
| `syscall6(nr, a1, ..., a6)` | any Linux syscall |
| `vec_push(v, x)` | append one element to a local `vec` and return the new length. See Vectors and owning pointers |
| `exit(code)` | terminate the process |
| `thread_spawn(fn, stack_top, tid)` | Linux `clone` (syscall 56). See Hosted threads |
| `thread_join(tid)` | wait until the kernel stores 0 in that word |
| `sched_setaffinity(cpu)` | syscall 203 for the current thread. `cpu >= 1024` returns -1 |
| `cpu_pause()` | `pause`, returns 0. Also available in freestanding code |

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
| `lidt(ptr)`, `lgdt(ptr)`, `invlpg(ptr)` | `lidt/lgdt/invlpg [rax]` in 64-bit code. `ptr` is any address expression, including a local variable. `&name` or `name` may be a `data` label (`lgdt(&gdtr)`). In `bits 16` / `bits 32` a label is `lgdt [gdtr]` and does not touch `ebx`; another address is `lgdt [bx]` or `lgdt [ebx]`, and `ebx` is popped afterward |
| `read_cr0()`, `read_cr2()`, `read_cr3()`, `read_cr4()` | `mov rax, crN` in 64-bit code. In `bits 16` and `bits 32`, `mov eax, crN`. There is no `mov ax, crN` |
| `write_cr0(v)`, `write_cr3(v)`, `write_cr4(v)` | `mov crN, rax` in 64-bit code, `mov crN, eax` in `bits 16` / `bits 32`. The written value stays in the accumulator |
| `rdmsr(msr)` | in 64-bit code, returns the 64-bit MSR (`shl rdx, 32` / `or rax, rdx`). In `bits 16` / `bits 32`, leaves the low half in `eax` and the high half in `edx` |
| `wrmsr(msr, value)` | in 64-bit code, writes a 64-bit value. In `bits 16` / `bits 32`, writes `edx:eax`. ORing a small integer into `rdmsr(m)` keeps the high half. `/` and `%` of that `rdmsr` do too, because the divide restores `edx` before `wrmsr` reads it. A value that never writes `edx` is written with `edx = 0` |
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
| `bss_start()`, `bss_end()` | address of the first / one past the last zero-initialized global (`--flat` only). `*u8`. The symbols are `__bss_start` and `__bss_end` |
| `section_start("name")`, `section_end("name")` | `i64` load address of the first byte, and one past the last byte. See Named sections |

### Hosted threads

`thread_spawn`, `thread_join`, and `sched_setaffinity` need the hosted runtime. In `--freestanding` they are `'thread_spawn' needs the hosted runtime (Linux syscalls) and is not available with --freestanding`.

`thread_spawn(fn, stack_top, tid)` is `clone` with flags `CLONE_VM | CLONE_FS | CLONE_FILES | CLONE_SIGHAND | CLONE_THREAD | CLONE_SYSVSEM | CLONE_PARENT_SETTID | CLONE_CHILD_CLEARTID` (`0x350F00`). `fn` is a function address (`&worker`). The child calls it with no arguments and then `exit` (syscall 60), which ends that thread. `stack_top` is the high end of a stack the caller owns; the builtin aligns it down to 16 bytes so the `call` enters `fn` with `rsp ≡ 8 (mod 16)`. `tid` is the address of a 32-bit word. The kernel writes the thread id there and, when the child exits, stores 0 and wakes a futex. The parent return value is that id, or a negative errno.

`thread_join(tid)` loads that word. While it is not 0 it calls `futex` (syscall 202, `FUTEX_WAIT`) with the value just read, then loads again. The pointer is kept in `rbx` because `syscall` overwrites `r11`. It returns 0.

`sched_setaffinity(cpu)` builds a 128-byte mask on the stack (1024 bits), sets bit `cpu`, and calls syscall 203 with pid 0. `cpu >= 1024` returns -1 and does not call the kernel. A successful call returns 0.

`cpu_pause()` is `pause`. It is the spin hint and does not require the hosted runtime. The older `pause()` builtin is the same instruction and does not define `rax`.

`tests/lang/threads.lap` starts two threads. Each calls `sched_setaffinity(0)` and `cpu_pause`, then `fetch_add`s a shared `u64` 1000000 times. The sum is 2000000. The stacks are two global `[u8; 65536]` arrays. There is no libc. `tests/lang/queues.lap` starts an SPMC ring (one producer, three consumers), an MPMC ring (two producers, two consumers), and a thread pool (main submits, three workers pop). Each phase pushes 1..4096. The sum is 8390656, the duplicate count is 0, and the pop count is 4096. A consumer that sees an empty ring after `done` is release-stored tries one more pop, so the last value is not lost.

`examples/lowlat/share.lap` is the same `fetch_add` on two counters. `align(64) struct` with both fields at their natural offset puts them on one 64-byte line (`sizeof` 64). A field `align(64)` puts the second counter at offset 64 (`sizeof` 128). Each thread updates only its own counter. `examples/lowlat/pin.lap` calls `sched_setaffinity` on the current thread: cpu 0 returns 0, cpu 1024 returns -1, and two workers pin themselves to cpu 0 and cpu 1. `examples/lowlat/watch.lap` is a second thread that release-stores a heartbeat and then stops. Main waits until that stop, then acquire-loads the heartbeat until it has stayed at the same value. The quiet window is a count of identical samples, so the printed value does not depend on the scheduler. The timed forms are `examples/lowlat/bench/` (`share`, `split`, `pin`, `same`, `free`, `watch`), measured by `examples/lowlat/bench/run.sh` into `examples/lowlat/RESULTS.md`. Those six are not pinned with `taskset`. The pool, the book, and the engine still are.

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

## Named sections

`section "name" { ... }` and `section "name" @ address { ... }` place the functions, `let` globals, and `data` blocks inside the braces into that NASM section. `section` is recognized only when the next token is a string, so `section(...)` is still a call. The block is top-level. `a section cannot contain another section`. `import` inside a section is `import is not allowed inside a section`.

A section requires `--freestanding`, `--flat`, or `--boot`. Hosted code is `section is only available with --freestanding, --flat, or --boot`. The name starts with a letter, `_`, or `.`, and then contains only letters, digits, `_`, and `.`. Anything else, including an empty name, is `section name 'boot rom' is not a NASM section name`. More than 32 sections is `too many sections`.

`@ address` is NASM `vstart`. Labels in the section resolve to that load address, and the bytes stay packed in the image. It is not `start=`. `start=` would insert a hole, so `@ 0x100000` would write a megabyte of zeros. The address is one non-negative integer literal: `section load address must be a non-negative integer`. `0` is allowed. A character literal is not. The first `@` fixes the address. Opening the same name again appends to it. A different address is `section '.a' already has load address 0x1000`. A later block with no `@` keeps the first address. A first block with no `@` and a later one with `@` sets it. The directive is `vstart=0x` followed by lowercase hex, so an address in the high half of the 64-bit space stays positive (`0xffff800000000000`).

`section_start("name")` and `section_end("name")` return `i64`: the load address of the first byte and one past the last byte. The size is the difference, so `section_start(".boot")` can be compared with `0x7C00`. The argument is a string literal. An unknown name is `unknown section '.nope'`. Without `--freestanding`, `--flat`, or `--boot` the error is `section_start() is only available with --freestanding, --flat, or --boot` (the same text for `section_end`). A missing or non-string argument is `section_start() takes a section name`. `.text`, `.data`, and `.bss` are known even when the source never wrote `section ".text"`. `bits 16` emits `mov ax, SYM`. `bits 32` emits `mov eax, SYM`. 64-bit code emits `mov rax, SYM`. When the section has a `vstart` above `0xFFFF` and the call is in `bits 16`, the error is `section '.high' does not fit in bits 16` and no instruction is emitted.

The symbols are `__text_start` / `__text_end`, `__data_start` / `__data_end`, and `__bss_start` / `__bss_end`. Any other name replaces each `.` with `_` and adds the prefix `__sec_`, so `.boot` is `__sec__boot_start`. Two different names that produce the same symbol are `section 'a_b' uses the same bounds symbol as 'a.b'`. `bss_start()` and `bss_end()` are not these builtins. They stay `*u8`, `--flat` only, and they load `__bss_start` and `__bss_end`.

A program that never mentions `section`, `section_start`, or `section_end` emits the same instructions and the same flat bytes as before. Bound labels are emitted only for a name the source asked about, or for a section the source declared. Declaring the section asks for both labels. `--flat` and `--boot` still emit `fn _start` first. If `_start` sits inside a named section, that section is opened before the function, so the entry is byte 0 of the section. A `let` or a `data` block written before `_start` in the source is still emitted after `_start`.

An initialized scalar stays `name: dq value` (an 8-byte slot). An uninitialized `let` in the section named `.bss` uses `resb`, `resw`, `resd`, or `resq`, and nasm omits `.bss` from a flat file (nobits). The same directive in any other section is progbits and occupies zeros. nasm may warn `uninitialized space declared in ... section: zeroing`. Only the section named `.bss` is left out of the file. NASM's bin format otherwise aligns a new section to 4 bytes. A named section is emitted with `align=1`, so its bytes stay packed against the previous section. The default `section .text`, `section .data`, and `section .bss` lines of a program that does not name a section are unchanged.

The boot pad `times 510-($-$$) db 0` and `dw 0xAA55` is emitted in the section that holds `_start`, after that section's end label. The measured span excludes the pad, and a one-section image is still 512 bytes ending in `55 aa`. A boot image that also puts bytes in another section can pad `_start`'s section and still grow past 512. That is the caller's layout, not a second image the compiler builds.

`tests/boot/section.lap` is one section, `.boot` at `0x7C00`. QEMU checks `COPIHUE`. The hexdump starts with `b8 00 7c`: `mov ax, __sec__boot_start` when that label is `0x7C00`. `tests/freestanding/sections.lap` is the same bounds, a `let`, and a `data` block, at `-O0` and `-O2`. It is assembled, not booted. `nasm -f elf64` on that assembly (a `--freestanding -S` file, not `-c`) keeps a `vstart` attribute as text and warns that it ignores it. `--flat` and `--boot` are `nasm -f bin`, which applies it. `-c` does not emit `vstart`.

`section_start(".text")` and `section_end(".text")` (and `.data` and `.bss`) make the load address and the span visible to a kernel that already builds with `--flat`. `bss_end() - bss_start()` is already the bss size at run time. A named `@` lets a sector or a flat piece say where its labels live without using only the CLI address. The kernel does not have to be rewritten. `@` is not a relocatable ELF. Separate compilation is `-c`.

## Relocatable objects

`copihue -c -o file.o file.lap` assembles one relocatable ELF64 object with `nasm -f elf64` and does not run `ld`. The default name is `output.o`. `-c -S` writes that object's assembly and does not assemble. `--freestanding -c` is the same object with no hosted runtime. `--freestanding` without `-c` still stops at assembly.

`-c` cannot be combined with `--flat` or `--boot`. Both of those are `nasm -f bin` images, and the error is `copihue: error: -c cannot be combined with --flat or --boot`. Several files on one `-c` command, and `import`, are still one object. A second object is a second invocation. `ld a.o b.o -o prog` links them. There is no libc. A module item's symbol is `mod__item` (see Modules). A flat item keeps its source name.

A defined function, a `let` global, and a `data` name are NASM `global`. Labels inside a `data` block stay local to the object. String literals stay local. With `-c`, `extern name;` that this object does not define is NASM `extern`, and the load is still `mov rax, [name]`. The other object defines it with `let` or `data`. `__syscall_rsp` and `__syscall_user_rsp` are not `extern`: the object that names them still defines both quads.

`extern fn add(a: i64, b: i64) -> i64;` is a function with no body. The call checks the argument count and the argument types against that declaration. The object contains `extern add` and no `add:`. `extern bits 32 naked fn putc(c: i64) -> i64;` records the bits mode, so a `bits 32` call is allowed and a `bits 16` call of that function is still `a bits 16 call cannot target a bits 32 function`. Without `-c` the error is `extern fn is only available with -c`. A brace body is `extern fn has no body`. A real `fn add` later in the same object defines `add`, the same way `fn add();` followed by a body is one function, and the object emits `global add` instead of `extern add`.

A hosted `-c` object emits `_start` (`call main`, `mov rdi, rax`, `mov rax, 60`, `syscall`) only when it defines a flat `fn main`. `fn main` inside a module is `mod__main` and does not emit `_start`. That object also defines `__print_int`, `__strcmp`, `__strcpy`, and `__strlen`, and marks them `global`. An object with no `main` has no `_start` and no copy of those helpers. If it calls `print_int`, `strcmp`, `strcpy`, `strlen`, `print`, or `println`, the helper is `extern`. Two objects that define `main` both define `_start`. Two objects that define the same function do too. `ld` reports the duplicate. `import` does not split them into two objects.

`section "name" { ... }` is allowed in a `--freestanding -c` object. The section directive has no `vstart`, so `nasm -f elf64` does not warn. `section "name" @ address` is `section '@' is a load address for --flat and --boot; -c does not emit vstart`. Compilation fails, and the object is not written. `--flat` and `--boot` still emit `vstart`, and `nasm -f bin` still applies it. `section_start` and `section_end` name symbols in this object. They are not a cross-object relocation by themselves.

A `bits 16` or `bits 32` call is still a near `call`. In `bits 16` the displacement is 16 bits. `tests/link` does not claim a far call. It does link a `bits 32` `extern fn`: the relocation is the ordinary PC-relative `call`, and both functions land in one binary.

`tests/link/parts/mod_add.lap` and `mod_main.lap` are compiled with `-c` at `-O0` and `-O2`, linked with `ld` and no libc, and the program prints `42`. The library object defines `add` and `bias` and does not define `_start`. Linking that object to itself fails. `tests/link/parts/gen_max.lap` defines `max<T>` and calls `max<i32>`, so the object has `T max$1$i32` and no symbol `max`. `gen_main.lap` is `extern fn max<T>` and calls `max<i32>`; that object has `U max$1$i32`. Linked, the program prints `9`. `gen_mem.lap` is the same shape inside `module mem` (`T mem__max$1$i32`). `gen_mem_use.lap` externs it and `gen_mem_main.lap` calls `mem::call_it`. Linked, the program prints `7`. Both pairs match at `-O2`. `tests/link/parts/mod_mem.lap` is a second pair: `module mem` defines `mem__add` and `mem__bias`. `mod_mem_use.lap` redeclares `module mem` with `extern` for those two names, and `mod_mem_main.lap` is the flat `fn main` in that same `-c` invocation. The linked program prints `42` at `-O0` and `-O2`. `tests/link/parts/sec_plain.lap` is `--freestanding -c` with `section ".extra"` and no `@`; the assembly has no `vstart`, and `nasm -f elf64` does not warn. `fs_entry.lap` and `fs_putc.lap` are `bits 32`. They are linked with `tests/link/flat.ld` at `0x8000` (`--oformat binary`) and booted from `tests/usermode/boot.lap`, which jumps to selector `0x08`. QEMU reads `COPIHUE` on COM1 at `-O0` and `-O2`. The entry loads the boot sector's data selector `0x10` before the first call, because that jump does not reload `SS`. The image has to fit in the 16 sectors that boot sector reads (8192 bytes).

Limits: one object per invocation. A file with no `module` is the flat namespace. A file with `module` uses `mod__item`, and `pub` is source visibility, not ELF visibility. There is no libc, no relocatable form of `--flat` or `--boot`, no `vstart` in a `-c` object, and `__syscall_rsp` is defined by every object that names it. `--flat --elf` is an executable, not this relocatable object. A `bits 16` call across objects is not a separate feature. Hosted helpers live in the object that defines `main`.

## Flat binaries

`copihue --flat ADDR -o out.bin file.lap` implies `--freestanding` and produces a raw binary (assembled with `nasm -f bin`, `org ADDR`) instead of assembly. Execution starts at the first byte, and the compiler always emits `fn _start` first, wherever it appears in the source; without `_start` compilation fails. `bss_start()` and `bss_end()` bound the zero-initialized globals so `_start` can clear them before calling anything else. `-S` writes the assembly instead of the binary. Several files on that command, and `import`, are one image. A `module` in one of those files uses `mod__item` the same way a hosted compilation does.

`copihue --flat ADDR --elf -o kernel.elf a.lap b.lap` is that same compilation and writes an ELF64 executable (`ET_EXEC`, machine x86-64) instead of the raw binary. `-o` names that file. `-S` still writes the assembly and does not write the ELF. The entry is the virtual address of `_start`. A defined function, `let`, or `data` name is a global symbol. `pub` does not change that: a private item is still in the symbol table. Named sections are ELF sections. `section "name" @ address` sets that section's virtual address (`vstart`). A section with no `@` keeps the address NASM assigns in the packed image. `.text` is executable. `.data` and `.bss` are writable. Any other name is readable, writable, and executable, because the section may hold code and a `let`. `.bss` is nobits. Each non-empty section is its own program header, aligned to 1, so a gap between virtual addresses is not filled with zeros. The bytes of each section are the bytes `--flat` without `--elf` writes into the raw image. NASM may insert a few alignment bytes between sections; those bytes stay in the file and are not a section. A loader maps the program headers. Loading the raw file at one base address matches a label whose address is that base plus its file offset, and it does not match a section whose `@` is somewhere else.

`--elf` without `--flat` is `copihue: error: --elf requires --flat`. `--elf` with `--boot` is `copihue: error: --elf cannot be combined with --boot`. `-c` with `--flat` is still rejected. Without `--elf`, the raw image is unchanged.

`tests/link/parts/flat_entry.lap`, `flat_console.lap`, and `flat_note.lap` are one `--flat 0x10000 --elf` image. `_start` is in `.text` at `0x10000` and is byte 0 of the raw image. `module console` places `console__putc` and `console__mark` (the qword 65) in `.console` at `0x12000`. `banner` is the seven bytes `COPIHUE` in `.note`. `console__hidden` is private and is still a symbol. `-O0` and `-O2` are the same raw image and the same ELF. The image is not booted. `.console` is not at its file offset plus `0x10000`, so loading the raw file as one blob would not run that section. The check reads the ELF headers.

`tests/flat/min_kernel.lap` is one such image, loaded at `0x10000`. Its IDT gates use code selector `0x18`, the 64-bit code segment of the AtomicOS bootloader GDT. Selector `0x08` on that loader is a 32-bit code segment; a gate that names it leaves long mode and the image hangs. The binary assumes that GDT is already loaded. It is not a generic bootable kernel. The image also indexes a local array, copies a packed struct, writes a `*volatile u32`, and reads CR2. Indexing a local array in `--freestanding`, `--flat`, or `--boot` does not insert the hosted bounds trap unless `--checked`. `unsafe { a[i] }` and a `for i in 0..N` that the array is long enough for still omit it. `bits 16` and `bits 32` do not insert it: the trap is a 64-bit `syscall`.

## Boot sectors

`copihue --boot -o boot.bin file.lap` implies `--freestanding` and `org 0x7C00`. The output file is the sector image (`nasm -f bin`), not assembly and not an ELF. `-S` writes the assembly instead.

The file starts with `org 0x7C00` and the body. There is no leading `bits 64` and no `.data` or `.bss` section in front of the code: either one would move the entry off the first byte or assemble the prologue in the wrong mode. `_start` is emitted first and must be `bits 16 naked fn`. A missing `_start` is `--boot needs a function named _start (the entry point)`. A `_start` that is not `bits 16 naked fn` is `--boot _start must be 'bits 16 naked fn' (real mode, no 64-bit frame)`. A `section` around `_start` is opened before the function. An empty default `.text` before that section adds no bytes, so the entry stays at the first byte of the file. The pad is then written back into `_start`'s section, after its end label.

`data` blocks stay in source order after `_start`, still in the text stream, so labels such as `dd gdt` resolve. The compiler then appends:

```
times 510-($-$$) db 0
dw 0xAA55
```

Those two lines are emitted after `bits 64`. `dw` does not depend on the bit mode. Leaving them under `bits 16`, next to a 64-bit prologue, is what makes nasm report `instruction not supported in 16-bit mode`. The code and the data must fit in 510 bytes; the image is then exactly 512 bytes and the last two are `55 aa`. An uninitialized `let` global is not copied into the sector. Put constants in `data`.

`tests/boot/boot_sector.lap` prints `Copihue` with BIOS teletype (`int(0x10)`, AH=`0x0E`) and far-jumps to a 32-bit stub that writes `COPIHUE` to port `0x3F8`. The sector installs its own GDT: selector `0x08` is 32-bit code and `0x10` is data, both base 0. That is not the AtomicOS loader GDT (`0x18` is 64-bit code there). The far jump is the 16-bit form `EA ip16 sel`; with `org 0x7C00` the IP is the label's low 16 bits, and the protected-mode segment has base 0, so execution continues at that linear address. `tests/boot/jmp_sp.lap` stays in real mode: it sets `ds`, `es`, `ss`, and `sp`, jumps to a label, and writes `COPIHUE`. `tests/boot/call16.lap` calls another `bits 16` function, sets `bp` and `bx`, and `jmp word [land]` continues at `hue`. `tests/boot/call32.lap` enters protected mode, calls a `bits 32` function through `eax`, and `jmp dword [land]` continues at `hue`. `jmp 0x8200` is assembled after the halt loop and is not taken. Those sectors do not enter long mode. A page table is larger than the free space in the sector; it has to be written at some other physical address. `tests/usermode` is the image that does: its sector loads a kernel at `0x8000`, and that kernel's `bits 32 naked fn _start` builds the page tables and far-jumps to 64-bit code.

The regression suite compares the hexdump at `-O0` and `-O2`. When `qemu-system-x86_64` is on `PATH`, it boots the image (`-display none -boot a`, floppy, raw) and requires `COPIHUE` on COM1. That check is skipped, not failed, when QEMU is absent. CI installs the `qemu-system-x86` package.

## Known limitations

- `i64` and `u64` arithmetic is 64-bit two's complement and wraps. `u8`, `u16`, `u32`, `i8`, `i16`, and `i32` truncate after the operator, as described under Type checking. Pointer indexing is not bounds-checked. A `[]T` index is, in hosted code; `s.ptr[i]` and `unsafe` are not. A `vec` index checks the length the same way. `vec T` is a hosted local, not a parameter, a return, a global, or a struct field. Its element is an integer, a pointer, or a struct. An enum is not a `vec` element. `let s: []T = v` and `for x in v` copy the pointer and the length and do not own the buffer. `own *T` is a hosted local, a hosted parameter, or a hosted return. It is not copied or reassigned. Returning it from `-> own *T`, or passing it to an `own *T` parameter, moves it and stores null. Passing it to `*T` does not. An untyped parameter is not inferred from an index or a dereference. A drop placed after an explicit `return` is still emitted and is not executed.
- An `enum` is a tag `i64` and a payload of integers and pointers. It can be returned (hidden pointer in `rdi`), passed by value, stored in a global or a struct field, and matched with `_` or a block arm. It is not an element of a slice, an array, or a `vec`. `match` of a call, a constructor, or `*p` of the enum still needs a local. There is no `|` pattern, no guard, and no named field. `bits 16`, `bits 32`, `naked`, and a `module` reject the value. A literal arm is `i64`. Without `_`, the corrupt-tag path is `ud2`. Hosted `-O2` compares the tag once from `rbx`. See Enums.
- A generic function or struct is one monomorphized copy per used combination of integer, pointer, or struct arguments. The copy is an ordinary function or struct. There are no traits: the operation is checked on the copy (`cannot apply '+' to 'Point' in 'add<Point>'`). A type argument cannot be an enum, a slice, an array, a `vec`, or `own *T`. Two `-c` objects that emit the same copy still collide at `ld`. `vec` and `Option` are not generic types. See Generics.
- Direct calls check the function name, the argument count, and argument types. Assignments and integer or pointer returns are checked. Indirect calls are not. `*i8` and `*u8` match. An integer literal converts to any pointer; a variable does not, without `as`.
- Struct fields use their real width. Ordinary structs add natural alignment; `packed struct` does not, unless `align(N)` says so. `align(N)` is a power of two from 1 to 4096 on a struct, a field, or a global. Locals are not aligned that way. An array field is `N * sizeof(T)` or a compile error. `sizeof` and `offsetof` are compile-time integers. A narrow integer operator truncates to that type; `i64` and `u64` stay 64-bit. Scalar locals and globals still occupy 8-byte slots. Struct assignment copies the whole value; it does not assign field by field in the source. Nested fields (`r.b.x`) use the real offset.
- Indexing a local array in hosted mode inserts a bounds check that exits the process. `--freestanding`, `--flat`, and `--boot` omit that check unless `--checked`. `unsafe { a[i] }` omits it. `for i in 0..N` omits it when the array has at least `N` elements, including under `--checked`. `bits 16` and `bits 32` do not insert it. Pointer indexing is never checked. A slice `s[i]` is checked in hosted code and writes `index out of range` before exiting with code 1. `--freestanding`, `--flat`, and `--boot` omit that check unless `--checked`. `unsafe { s[i] }` and `s.ptr[i]` omit it in every mode. `for x in s` and `for i in 0..s.len` omit it when the bound is the loop itself.
- Several files on one command, and `import`, are one assembly unit and one object. Modules, `pub`, and `mod__item` are described under Modules. `-c` writes one relocatable object for that unit; `ld` links objects and there is no libc. `bits 16` and `bits 32` exist only as naked functions. A scalar or pointer `let` there is a slot in the image; an array or a struct local is rejected. Named registers are readable and writable only inside `naked`. `bx`, `bp`, `ebx`, `ebp`, `esi`, and `edi` are naked lvalues in `bits 16` and `bits 32`, and rejected in 64-bit code. An `interrupt fn` cannot name `rbp`; it receives a pointer to the CPU frame instead. `save_context` / `restore_context` capture `rsp`, `rbp`, the callee-saved registers, and a resume address without naming those registers. Integer expressions in `bits 16` and `bits 32` use `eax`. A call in those modes uses the naked parameter registers (`ax`/`cx`/`dx`/`si` or `eax`/`ecx`/`edx`/`esi`), at most four, and only a callee of the same bits mode. `jmp reg` and `jmp [name]` are naked-only. The hosted suite assembles `iretq`, `sysret`, `ltr`, and the 64-bit control-register moves and does not execute them; it does execute `cpuid` and `int_carry`. The multi-file tests run as a Linux process. `tests/lang/context.lap` runs the framed switch as a Linux process. `tests/ctx` runs a `#PF` handler and an IRQ handler that rewrite the frame, under QEMU. The boot-sector tests execute real mode and protected mode under QEMU, including `lgdt(&gdtr)`, `write_cr0`, `if carry`, and `outb` on `tests/boot/realmode_builtins.lap`, `ds = 0` plus a local on `tests/boot/seg_local.lap`, and `sp` plus `jmp` on `tests/boot/jmp_sp.lap`. `tests/boot/call16.lap` and `tests/boot/call32.lap` call another function and jump through memory. `tests/boot/rax_keep.lap` executes the `ax` compare and the immediate store under QEMU. `tests/boot/edx_keep.lap` executes `idiv`, a `u32` `div`, a store through `bp+di`, and `lgdt` of a register under QEMU. `tests/freestanding/edx_keep.lap` assembles the remainder, a zero divisor, `lidt`, `invlpg`, and the `bits 32` forms, and is not booted. `tests/freestanding/rax_branch.lap` assembles `syscall`, `while (rax == 0)`, and `let r = syscall(8)`, and is not booted. `tests/freestanding/bits32_regs.lap` assembles `write_cr4`, `rdmsr` / `wrmsr`, and `cpuid` in `bits 32` and is not booted. `tests/freestanding/naked_local.lap` assembles the slot and the segment write and is not booted. `tests/freestanding/naked_regs.lap` assembles the named registers, the sized store, `jmp 0x8200`, and `far_jump` / `retfq`, and is not booted. `tests/freestanding/insns.lap` assembles `lodsb`, `rep stos`, `push`/`pop`, `syscall`, and a load of `__syscall_rsp`, and is not booted. `tests/freestanding/jmp_call.lap` assembles the indirect jump, the bits 16/32 call, and `syscall` with arguments, and is not booted. `tests/freestanding/sections.lap` assembles a named section, `vstart`, and `section_start` / `section_end`, and is not booted. `tests/boot/stc_carry.lap` executes `stc` and `if carry` under QEMU. `tests/boot/section.lap` executes one named section at `0x7C00` under QEMU. `tests/lang/naked_param.lap` runs the 64-bit register parameters. `tests/lang/extern_data.lap` runs a load of an `extern` qword. `tests/link` links two hosted `-c` objects and runs them, and links two freestanding `bits 32` objects that QEMU boots. `tests/usermode` executes long mode, `ltr`, the syscall MSRs, `iretq` to ring 3, `syscall`/`sysret`, and `iretq` from an interrupt, also under QEMU.
- `syscall fn` does not program `STAR`, `LSTAR`, `SFMASK`, or `EFER`, and it does not emit `swapgs`. The kernel does that with `wrmsr` and `asm`. The `syscall` stack is `set_syscall_stack`, not `TSS.rsp0`. `volatile` is per access, as described above. It is not a memory model by itself; the atomic builtins are the protocol, and they are 64-bit only. `thread_spawn` is a Linux `clone` of one function with a caller-provided stack. It is not a pool, and it is not available in freestanding code. `let volatile` on a local scalar keeps the stack slot at hosted `-O2`.
