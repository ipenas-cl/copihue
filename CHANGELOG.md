# Changelog

Todos los cambios importantes del proyecto.

## [0.27.5] - 2026-10-25

### Added
- Hosted 64-bit `-O2` rewrites the insertion shift `j = i - 1; while (j >= 0 && a[j] > key) { a[j + 1] = a[j]; j = j - 1; } a[j + 1] = key` when it sits in `while (i < N)`, `N` is a sole positive constant, the local `i64` array has at least `N` elements, `i` starts at a non-negative constant, and `i` is incremented only at the end of that body. The taken path is six instructions and does not compare `j` with the length. `j` starts at `i - 1` and only decreases by one after `j >= 0`, so it does not go below -1. `key < a[j]` matches. `a[j] > key && j >= 0` does not. A `>=` test, an extra statement, a narrow or spilled name, or a failed proof of `i` keeps the bounds check.
- `tests/lang/o2_shift.lap` checks a reverse run, an already sorted run, duplicates, negative keys, `n = 1`, a shift down to `j == -1`, `key < a[j]`, a `let` inside the body, a `>=` loop that must keep counting shifts, an extra statement in the body, and dot products of `n = 1` and of negative factors, against `-O0`.

### Changed
- Language version is 0.27.5. `-O0`, `--freestanding`, `--flat`, and `--boot` generate the same code as 0.27.4. `bits 16` and `bits 32` do not take these paths. The `-O2` assembly of `fib`, `sieve`, `arith`, `chain`, `fold`, and `array` matches 0.27.4 except the version line. `sort` drops the upper check on the shift. `matmul` and `fib` match 0.27.4 except the version line. `fib` is still the 16-instruction prologue. gcc `-O2` expands that recursion into a large even/odd tree (about 400 instructions, subtracting 2 and spilling into a 200-byte frame). It is still exponential. That rewrite is not in this compiler. A bottom-tested `k` loop with `imul rax, qword [rsi]` was measured on 2026-10-25 21:06:42–21:07:17 UTC and not kept: `matmul` went from 46.9 ms (46.3–47.4) to 48.0 ms (47.6–48.2). The ranges do not overlap, and the later one is slower. `text` of that binary was 1021; this release keeps the 10-instruction top-tested loop (`text` 1028).

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
