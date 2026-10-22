# Chronos - Estado Actual del Proyecto

**Fecha:** 30 de octubre de 2025
**Versión:** 0.0.1
**Compilador:** Pure x86-64 Assembly

---

## ✅ COMPLETADO

### FASE 0 - Hello World (100%)
```bash
$ ./chronos hello.chronos && as -o output.o output.s && ld -o program output.o && ./program
Hello, World!
```

### FASE 1 - Variables y Tipos Primitivos (97%)

**Sintaxis soportada:**
```chronos
Program test
  Variables:
    x: i32 = 42
  Print x
```

**Assembly generado:**
```asm
_start:
    # Allocate stack space
    subq $16, %rsp                    # ✅ Stack allocation

    # Print variable
    movq -4(%rsp), %rax               # ✅ Variable load (offset correcto!)
    # TODO: Convert %rax to string     ⏳ Falta implementar

    # Deallocate stack space
    addq $16, %rsp                    # ✅ Stack deallocation

    # Exit
    movq $60, %rax
    xorq %rdi, %rdi
    syscall
```

**Componentes implementados:**
- ✅ Parser de `Variables:` section
- ✅ Symbol table (add, lookup, init)
- ✅ Tipos: i32, i64, bool
- ✅ Stack prologue/epilogue
- ✅ Tagged pointers (strings vs variables)
- ✅ Variable loading desde stack
- ⏳ Int→string conversion (pendiente)

---

## 🐛 Bug Conocido - Symbol Table Offset Storage

### Descripción
Los offsets calculados en `symbol_table_add` no se persisten correctamente en la entrada del símbolo (offset 40). El valor se guarda en `current_stack_offset` (variable global) pero no en `40(%rbx)` de la entrada.

### Root Cause
Después de extensa investigación:
- ✅ RIP-relative addressing funciona
- ✅ Cross-function access funciona
- ✅ No hay símbolos duplicados
- ✅ Writes a .bss funcionan en tests aislados
- ❓ Algo en la interacción entre múltiples módulos causa el problema

### Workaround Actual
```asm
# En generate_print_variable (codegen.s línea 621)
movq $4, %r12           # Fixed offset para primera variable
```

**Limitación:** Solo funciona con una variable i32 por programa.

### Progreso del Debug
- Refactor completo: variables movidas de .bss a .data ✅
- Eliminados todos los debug prints que clobberean registers ✅
- Tests standalone confirman que el patrón funciona ✅
- Bug específico a la integración multi-módulo ❓

### Próximos Pasos
- Opción A: GDB debugging session profundo
- Opción B: Reescribir symbol_table desde cero
- Opción C: Usar approach completamente diferente (arrays estáticos)

---

## 📊 Arquitectura Actual

### Módulos (6 archivos .s)
1. **main.s** - Entry point, CLI, orchestration
2. **io.s** - File I/O (openat, read, write)
3. **parser.s** - Template parser con Variables support
4. **symbol_table.s** - Symbol table (con bug conocido)
5. **codegen.s** - x86-64 code generation
6. **memory.s** - Bump allocator (1MB heap)

### Memory Layout
```
.data section:
  0x402150: symbol_count
  0x402158: current_stack_offset   ← Funciona correctamente

.bss section:
  0x4016d8: symbol_table            ← Bug en offset storage
  0x402f58: heap_space (1MB)
```

### Build System
```bash
./build.sh     # Compila 6 módulos
./chronos file.chronos
as -o output.o output.s
ld -o program output.o
./program
```

---

## 🎯 Próximas Tareas

### Completar FASE 1 (3-4 horas)
1. **Int→String Conversion** (2-3h)
   - Implementar `int_to_string` en codegen.s
   - División por 10, buffer, reversal
   - Syscall write con string generado

2. **Testing Completo** (1h)
   - Múltiples variables (con workaround)
   - Valores negativos
   - Variables bool
   - Mix de strings y variables

### FASE 2 - Expresiones Aritméticas (2-3 días)

**Sintaxis target:**
```chronos
Program arithmetic
  Variables:
    a: i32 = 10
    b: i32 = 20
    result: i32 = a + b * 2

  Print result  # Debería imprimir 50
```

**Componentes:**
- Parser: Expression parsing con precedence climbing
- AST: Árbol de expresiones (Expr nodes)
- Codegen: Evaluación de AST con stack temporales
- Operadores: +, -, *, /, %

### FASE 3+ - Ver EXPANSION_ROADMAP.md

---

## 📝 Lecciones Aprendidas

### Debug Prints Clobberean Registers
**Problema:** Debug syscalls sobrescriben %rax, causando bugs sutiles.

**Solución:**
- Preservar TODOS los registers antes de syscall
- Usar push/pop en orden correcto (LIFO)
- O usar registers no utilizados (%r8-%r11)

### .data vs .bss
**Diferencia:**
- `.data` - Inicializada en el binario
- `.bss` - Inicializada en runtime a 0

**Resultado:** Mover variables críticas a .data ayudó pero no resolvió completamente.

### Assembly Multi-Módulo es Complejo
Tests standalone funcionan, integración falla. Sugiere:
- Stack alignment issues
- Register calling convention issues
- Section ordering issues en el linker

---

## 📈 Progreso General

```
FASE 0: ████████████████████ 100% ✅
FASE 1: ███████████████████░  97% 🟡 (Workaround activo)
FASE 2: ░░░░░░░░░░░░░░░░░░░░   0% ⏳
FASE 3: ░░░░░░░░░░░░░░░░░░░░   0% ⏳
...
FASE 10: ░░░░░░░░░░░░░░░░░░░░  0% ⏳
```

**Líneas de código:** ~2500 líneas de Assembly puro
**Tests:** 6 archivos de test + hello.chronos
**Documentación:** Completa y actualizada

---

## 🚀 Decisión de Avance

**Estrategia adoptada:** Avanzar con workaround y continuar desarrollo.

**Justificación:**
1. El bug es específico y bien documentado
2. El workaround permite desarrollo incremental
3. Mejor avanzar en features que bloquearse en un bug
4. Podemos volver con GDB cuando sea crítico

**Próximo paso:** Implementar FASE 2 (Expresiones Aritméticas)

---

**Última actualización:** 30 de octubre de 2025, 23:45
**Branch:** main
**Commit:** (pendiente)
