# Chronos - Roadmap de Expansión del Compiler
**Versión:** 0.0.1
**Compiler Actual:** Assembly puro (hello.chronos funciona)
**Objetivo:** Soportar TODAS las características determinísticas

---

## Estado Actual ✅

```bash
$ cat hello.chronos
Program hello
  Print "Hello, World!"

$ ./chronos hello.chronos
$ ./program
Hello, World!
```

**Compiler actual:**
- `/home/lychguard/Chronos/compiler/asm/main.s` - Entry point
- `/home/lychguard/Chronos/compiler/asm/parser.s` - Template parser
- `/home/lychguard/Chronos/compiler/asm/codegen.s` - x86-64 codegen
- `/home/lychguard/Chronos/compiler/asm/io.s` - File I/O
- `/home/lychguard/Chronos/compiler/asm/memory.s` - Bump allocator

---

## FASE 1: Tipos Primitivos (1 semana)

### Objetivo
Soportar variables con tipos explícitos.

### Sintaxis Target
```chronos
Program test_types
  Variables:
    x: i32 = 42
    y: i64 = 100
    z: bool = true

  Print x
  Print y
```

### Cambios en Compiler
1. **Parser** - Reconocer sección `Variables:`
2. **Symbol Table** - Almacenar nombre, tipo, offset de stack
3. **Codegen** - Alocar en stack, load/store variables

### Archivos a Modificar
- `parser.s` - Agregar parsing de variables
- `codegen.s` - Generar código de stack allocation
- Nuevo: `symbol_table.s` - Tracking de variables

### Tiempo Estimado: 3-5 días

---

## FASE 2: Expresiones Aritméticas (3 días)

### Objetivo
Soportar operaciones matemáticas.

### Sintaxis Target
```chronos
Program arithmetic
  Variables:
    a: i32 = 10
    b: i32 = 20
    result: i32 = a + b * 2

  Print result  # Debería imprimir 50
```

### Cambios en Compiler
1. **Parser** - Parsear expresiones con precedencia
2. **AST** - Representar árbol de expresiones
3. **Codegen** - Generar código para evaluación

### Archivos a Modificar
- `parser.s` - Expression parsing con precedence climbing
- Nuevo: `ast.s` - Build AST para expresiones
- `codegen.s` - Evaluar AST y generar assembly

### Tiempo Estimado: 2-3 días

---

## FASE 3: Control de Flujo (5 días)

### Objetivo
Soportar if/else, while, for.

### Sintaxis Target
```chronos
Program control_flow
  Variables:
    x: i32 = 10

  If x > 5:
    Print "x is large"
  Else:
    Print "x is small"

  While x > 0:
    Print x
    x = x - 1
```

### Cambios en Compiler
1. **Parser** - If, While, For statements
2. **Codegen** - Labels, jumps, comparisons

### Archivos a Modificar
- `parser.s` - Control flow parsing
- `codegen.s` - Branch code generation

### Tiempo Estimado: 4-5 días

---

## FASE 4: Funciones (1 semana)

### Objetivo
Soportar definición y llamada de funciones.

### Sintaxis Target
```chronos
Program functions

Function: add
  Parameters:
    a: i32
    b: i32
  Returns: i32
  Implementation:
    return a + b

Function: main
  Variables:
    result: i32 = add(10, 20)
  Print result
```

### Cambios en Compiler
1. **Parser** - Function definitions
2. **Codegen** - Calling convention (stack frames, ABI)

### Archivos a Modificar
- `parser.s` - Function parsing
- `codegen.s` - Call stack management
- Nuevo: `calling_convention.s` - System V ABI

### Tiempo Estimado: 5-7 días

---

## FASE 5: Structs y Ownership (2 semanas)

### Objetivo
Memory safety con ownership.

### Sintaxis Target
```chronos
Struct: Point
  Fields:
    x: i32
    y: i32

Function: create_point
  Returns: Point owns
  Implementation:
    return Point { x: 10, y: 20 }

Function: use_point
  Parameters:
    p: Point borrows
  Implementation:
    Print p.x
    Print p.y

Function: main
  Variables:
    p: Point = create_point()
  use_point(p)
  # p still valid here
```

### Cambios en Compiler
1. **Parser** - Struct definitions, ownership annotations
2. **Type Checker** - Borrow checker
3. **Codegen** - Struct layout, field access

### Archivos Nuevos
- `type_checker.s` - Ownership/borrow checking
- `struct_layout.s` - Memory layout calculation

### Tiempo Estimado: 10-14 días

---

## FASE 6: WCET Annotations (1 semana)

### Objetivo
Análisis de Worst Case Execution Time.

### Sintaxis Target
```chronos
Function: sensor_read
  WCET: 50 microseconds
  Returns: i32
  Implementation:
    # Read from hardware
    return read_adc()

RTTask: control_loop
  Period: 10 milliseconds
  Priority: 255
  Implementation:
    loop:
      value = sensor_read()
      actuate(value)
```

### Cambios en Compiler
1. **Parser** - WCET annotations
2. **Analyzer** - Static WCET analysis
3. **Codegen** - No optimizaciones no determinísticas

### Archivos Nuevos
- `wcet_analyzer.s` - WCET analysis
- `rt_codegen.s` - RT-safe code generation

### Tiempo Estimado: 5-7 días

---

## FASE 7: Concurrency Primitives (2 semanas)

### Objetivo
Threads, mutexes, channels.

### Sintaxis Target
```chronos
Mutex: shared_data
  Protocol: PriorityInheritance
  Type: i32

Thread: worker1
  Priority: 10
  Implementation:
    loop:
      lock(shared_data)
      shared_data += 1
      unlock(shared_data)

Thread: worker2
  Priority: 20
  Implementation:
    loop:
      lock(shared_data)
      shared_data += 2
      unlock(shared_data)
```

### Cambios en Compiler
1. **Runtime** - Necesitamos runtime library
2. **Codegen** - Llamadas a pthread, futex
3. **Linker** - Link con pthread

### Archivos Nuevos
- `runtime/threads.s` - Thread management
- `runtime/sync.s` - Mutex, semaphores
- `runtime/channels.s` - Message passing

### Tiempo Estimado: 10-14 días

---

## FASE 8: Memory Management Avanzado (2 semanas)

### Objetivo
Allocators, pools, arenas.

### Sintaxis Target
```chronos
Allocator: rt_pool
  Type: TLSF
  Size: 1 megabyte
  Alignment: 8

Function: process_data
  Allocator: rt_pool  # Usa este allocator
  Implementation:
    buffer = allocate(1024)
    # Use buffer
    deallocate(buffer)
```

### Archivos Nuevos
- `runtime/tlsf.s` - TLSF allocator
- `runtime/arena.s` - Arena allocator
- `runtime/pool.s` - Object pool

### Tiempo Estimado: 10-14 días

---

## FASE 9: Hardware Abstraction (1 semana)

### Objetivo
MMIO, interrupts, DMA.

### Sintaxis Target
```chronos
MMIO: UART
  Address: 0x40000000
  Registers:
    DATA: offset 0x00, read_write
    STATUS: offset 0x04, read_only
    CONTROL: offset 0x08, write_only

Interrupt: timer_irq
  Vector: 16
  Priority: 10
  Handler:
    flag = true
```

### Archivos Nuevos
- `hardware/mmio.s` - MMIO support
- `hardware/interrupts.s` - Interrupt handling
- `hardware/dma.s` - DMA transfers

### Tiempo Estimado: 5-7 días

---

## FASE 10: Self-Hosting (4 semanas)

### Objetivo
Compiler escrito EN Chronos, compila a sí mismo.

### Estrategia
1. Reescribir parser en Chronos
2. Reescribir codegen en Chronos
3. Bootstrap: Assembly compiler → Chronos compiler → Chronos compiler v2

### Archivos Target
- `compiler/chronos/parser.ch` - Parser en Chronos
- `compiler/chronos/codegen.ch` - Codegen en Chronos
- `compiler/chronos/main.ch` - Main en Chronos

### Tiempo Estimado: 20-30 días

---

## Timeline Total

| Fase | Duración  | Acumulado  | Características                    |
|------|-----------|------------|------------------------------------|
| 1    | 1 semana  | 1 semana   | Tipos primitivos                   |
| 2    | 3 días    | 1.5 semanas| Expresiones aritméticas           |
| 3    | 5 días    | 2.5 semanas| Control de flujo (if/while/for)   |
| 4    | 1 semana  | 3.5 semanas| Funciones                         |
| 5    | 2 semanas | 5.5 semanas| Structs + Ownership               |
| 6    | 1 semana  | 6.5 semanas| WCET annotations                  |
| 7    | 2 semanas | 8.5 semanas| Concurrency (threads, mutexes)    |
| 8    | 2 semanas | 10.5 sem.  | Allocators avanzados              |
| 9    | 1 semana  | 11.5 sem.  | Hardware (MMIO, interrupts)       |
| 10   | 4 semanas | 15.5 sem.  | Self-hosting                      |

**Total: ~4 meses de desarrollo**

---

## Estrategia de Testing

Cada fase incluye tests:

```bash
# FASE 1
$ cat tests/phase1_types.chronos
Program test_types
  Variables:
    x: i32 = 42
  Print x

$ ./chronos tests/phase1_types.chronos
$ ./program
42  # ✅ PASS

# FASE 2
$ cat tests/phase2_arithmetic.chronos
Program test_arithmetic
  Variables:
    result: i32 = 10 + 20 * 2
  Print result

$ ./chronos tests/phase2_arithmetic.chronos
$ ./program
50  # ✅ PASS

# ... y así sucesivamente para cada fase
```

---

## Decisión AHORA

¿Qué hacemos?

**Opción A: Comenzar FASE 1** (Recomendado)
- Empezar a expandir el compiler con tipos primitivos
- Progreso incremental y testeable
- 1 semana para tener variables funcionando

**Opción B: Diseñar más**
- Refinar specs de cada fase
- Crear más ejemplos
- Planear arquitectura interna

**Opción C: Prototipo en lenguaje de alto nivel**
- Implementar parser/typechecker en Python/Rust
- Validar diseño rápidamente
- Luego reescribir en Assembly

**Opción D: Otra idea**

¿Cuál prefieres?
