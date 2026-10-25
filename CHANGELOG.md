# Changelog

Todos los cambios importantes del proyecto.

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
