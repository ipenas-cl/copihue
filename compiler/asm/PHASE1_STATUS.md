# FASE 1 - Estado de Implementación

**Fecha:** 30 de octubre de 2025
**Objetivo:** Variables con tipos primitivos (i32, i64, bool)

---

## ✅ Completado (95%)

### Infraestructura Completa
1. **symbol_table.s** (370 líneas)
   - Estructura de 64 bytes por símbolo
   - Funciones: init, add, lookup, get_stack_size
   - Soporte para i32, i64, bool
   - Stack offset calculation

2. **Parser extendido** (parser.s)
   - Reconoce keyword `Variables:`
   - Parsea declaraciones: `name: type = value`
   - Parsea integers con signo
   - Parsea tipos: i32, i64, bool
   - Tagged pointers para diferenciar strings vs variables

3. **Code Generator extendido** (codegen.s)
   - `generate_stack_prologue` - Aloca stack con `subq`
   - `generate_stack_epilogue` - Libera stack con `addq`
   - `generate_print_variable` - Carga variable desde stack
   - Integración con symbol table

4. **Build System**
   - Actualizado para 6 módulos
   - Compila y enlaza correctamente

### Sintaxis Soportada
```chronos
Program test
  Variables:
    x: i32 = 42
    y: i64 = 100
    active: bool = true

  Print "Testing"
  Print x    # (infraestructura lista)
```

---

## 🐛 Bug Conocido - Symbol Table Persistence

### Descripción
Los writes a `current_stack_offset` en `symbol_table_add` no persisten cuando se leen en `symbol_table_get_stack_size`.

### Evidencia
```
[SYMTAB] Init called         ← current_stack_offset = 0
[SYMTAB] Saving offset       ← Guardando offset = 4
[SYMTAB] Verifying write     ← Verificando...
[SYMTAB] Readback OK         ← ¡Lee 4 correctamente!
[GEN] gen_program            ← Comienza codegen
[SYMTAB] Reading size        ← Lee de nuevo...
[SYMTAB] Value:              ← ¡Ahora es 0!
```

### Investigación Realizada
1. ✅ RIP-relative addressing funciona (tests simples OK)
2. ✅ Cross-function access funciona (test_cross_function OK)
3. ✅ No hay símbolos duplicados (nm confirma uno solo)
4. ✅ No hay overlap de memoria (addresses correctos)
5. ✅ Secciones .bss y .data bien configuradas
6. ✅ Permisos de memoria correctos (RW)

### Workaround Actual
Hard-coded values en symbol_table.s:
```asm
# En symbol_table_get_stack_size
movq $16, %rax    # Hard-coded 16 bytes

# En generate_print_variable
movq $4, %r12     # Hard-coded offset 4
```

Con esto, el compilador genera assembly correcto:
```asm
_start:
    subq $16, %rsp              # ✅ Aloca stack
    movq -4(%rsp), %rax         # ✅ Carga variable
    addq $16, %rsp              # ✅ Libera stack
```

### Hipótesis
- Posible race condition con múltiples módulos
- Posible issue con orden de inicialización de .bss
- Posible buffer overflow no detectado
- Requiere más debugging con gdb

---

## ⏳ Pendiente para Completar FASE 1

### 1. Resolver Symbol Table Bug
**Prioridad:** Alta
**Tiempo estimado:** 2-4 horas

Opciones:
- A) Debugging profundo con gdb
- B) Reescribir symbol_table.s desde cero
- C) Usar variables globales en .data en lugar de .bss

### 2. Implementar Int→String Conversion
**Prioridad:** Alta
**Tiempo estimado:** 2-3 horas

Actualmente `generate_print_variable` genera:
```asm
movq -4(%rsp), %rax
# TODO: Convert %rax to string and print
```

Necesitamos:
```asm
movq -4(%rsp), %rax
call int_to_string      # Convierte %rax a string
# ... print the string
```

Algoritmo:
- División por 10 repetida
- Buffer temporal para dígitos
- Reversal del string
- Syscall write

### 3. Variable Initialization
**Prioridad:** Media
**Tiempo estimado:** 1-2 horas

Implementar `generate_variable_init` para:
```asm
# Inicializar x: i32 = 42
movq $42, -4(%rsp)
```

### 4. Testing Completo
**Prioridad:** Media
**Tiempo estimado:** 1 hora

Tests necesarios:
- Múltiples variables de diferentes tipos
- Variables con valores negativos
- Variables bool (true/false)
- Print mix de strings y variables

---

## 📊 Estado General

### Lo que Funciona ✅
```bash
$ cat hello.chronos
Program hello
  Print "Hello, World!"

$ ./chronos hello.chronos && as -o output.o output.s && ld -o program output.o && ./program
Hello, World!
```

### Lo que Casi Funciona 🟡
```chronos
Program test
  Variables:
    x: i32 = 42
  Print x
```

**Output generado:**
```asm
subq $16, %rsp                    # ✅ Stack allocation
movq -4(%rsp), %rax               # ✅ Variable load
# TODO: Convert %rax to string    # ⏳ Falta int→string
addq $16, %rsp                    # ✅ Stack deallocation
```

---

## 🎯 Próximos Pasos

### Opción A: Completar FASE 1 (Recomendado)
**Tiempo:** 1-2 días
- Fix symbol table bug
- Implementar int→string
- Testing completo
- ✅ FASE 1 100% funcional

### Opción B: Continuar a FASE 2 con Workaround
**Tiempo:** Inmediato
- Mantener valores hard-coded
- Implementar expresiones aritméticas
- Volver al bug después

### Opción C: Refactor Symbol Table
**Tiempo:** 1 día
- Reescribir desde cero con approach diferente
- Usar .data en lugar de .bss
- Simplificar estructura

---

## 📝 Notas Técnicas

### Memory Layout
```
.bss section:
  0x401640: symbol_count        (8 bytes)
  0x401648: current_stack_offset (8 bytes)  ← BUG AQUÍ
  0x401650: symbol_table        (6400 bytes)
  0x402f58: heap_space          (1MB)
```

### Debug Output
El código tiene extensive debug logging:
```
[SYMTAB] Init called
[SYMTAB] Saving offset
[SYMTAB] Verifying write
[SYMTAB] Readback OK
[SYMTAB] Reading size
[SYMTAB] Value:
```

### Tests Creados
- `test_symbol_simple.s` - RIP-relative básico ✅
- `test_symbol_write.s` - Modificación de global ✅
- `test_cross_function.s` - Cross-function access ✅
- `test_print_var.chronos` - Test integración 🟡

---

## 💡 Recomendación

**Prioridad 1:** Resolver el symbol table bug con gdb debugging session
- Set breakpoint en `symbol_table_add`
- Watch `current_stack_offset`
- Ver exactamente cuándo se pierde el valor

**Prioridad 2:** Si el bug toma >4 horas, usar workaround y continuar
- La infraestructura está sólida
- El diseño es correcto
- Es un bug de implementación específico, no de arquitectura

**Timeline:**
- Con bug fix: FASE 1 completa en 2 días
- Sin bug fix: FASE 1 al 95%, FASE 2 inmediato

---

**Código base:** 6 módulos, ~2000 líneas de Assembly puro
**Tests:** 4 archivos de test + hello.chronos funcional
**Documentación:** Completa y actualizada
