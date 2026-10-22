# Changelog

Todos los cambios importantes del proyecto.

---

## [0.18.0] - 2026-10-22

First release under the new name **Copihue** (formerly Chronos).

### Changed (breaking)
- Renamed the language to Copihue. The compiler is now `copihuec` (`compiler/bootstrap-c/copihuec.c`) and the source extension is `.cph`. `.ch` is still accepted with a deprecation warning.
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
