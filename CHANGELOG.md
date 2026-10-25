# Changelog

Todos los cambios importantes del proyecto.

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
