# Chronos Compiler - Pure Assembly Implementation
**Versión:** 0.0.1
**Estado:** FASE 1 en desarrollo
**Arquitectura:** x86-64 Linux (System V ABI)
**Sintaxis:** Template/Prompt-Based (YAML-like)

---

## Estado Actual

### ✅ Funcionando
```bash
$ cat hello.chronos
Program hello
  Print "Hello, World!"

$ ./chronos hello.chronos
✓ Generated: output.s

$ as -o output.o output.s && ld -o program output.o
$ ./program
Hello, World!
```

### 🔄 En Desarrollo (FASE 1)
Variables con tipos primitivos:
```chronos
Program test
  Variables:
    x: i32 = 42
    y: i64 = 100
  Print x
  Print y
```

**Estado:** Infraestructura completa (parser, symbol table, codegen), debugging en progreso.

---

## ¿Por qué Assembly Puro?

- **Determinismo Máximo** - Cada instrucción es explícita, sin comportamiento oculto
- **Cero Dependencias** - Sin runtime, sin librerías, solo syscalls
- **Control Total** - Cada registro, cada byte, completamente controlado
- **Self-Hosting** - El compilador eventualmente se compilará a sí mismo
- **Filosofía** - Un lenguaje determinista merece un compilador determinista

---

## Arquitectura del Compilador

### Módulos (6 archivos .s)

1. **main.s** - Entry point
   - CLI argument parsing
   - Orquesta el pipeline de compilación
   - Maneja errores

2. **io.s** - File I/O
   - `open_file` - Abre archivo usando syscall openat(257)
   - `read_file` - Lee contenido completo
   - `write_file` - Escribe assembly generado
   - `print_*` - Output de mensajes

3. **parser.s** - Template Parser
   - Reconoce keywords: `Program`, `Variables`, `Print`
   - Parsea string literals (entre `"`)
   - Parsea identificadores y tipos (`i32`, `i64`, `bool`)
   - Parsea integers con signo
   - Tagged pointers: diferencia strings de variables (low bit)
   - Construye AST (estructura de 536 bytes)

4. **symbol_table.s** - Symbol Table
   - Estructura: 64 bytes por símbolo (max 100)
   - Campos: name(32), type(4), padding(4), offset(8), value(8)
   - Funciones: init, add, lookup, get_stack_size

5. **codegen.s** - Code Generator
   - `generate_code` - Convierte AST → x86-64 assembly
   - `generate_stack_prologue/epilogue` - Manejo de stack frame
   - `generate_print_action` - Código para Print
   - `generate_string_data` - Sección .data

6. **memory.s** - Bump Allocator
   - Heap de 1MB para estructuras del compilador
   - `allocate_buffer` - Aloca memoria
   - `allocate_ast` - Aloca estructura AST

### Pipeline de Compilación

```
┌──────────────┐
│ hello.chronos│
└──────┬───────┘
       │
   ┌───▼───┐
   │  I/O  │  read_file() → buffer
   └───┬───┘
       │
   ┌───▼────┐
   │ Parser │  parse_template() → AST
   └───┬────┘      │
       │           └─→ Symbol Table (si hay Variables:)
   ┌───▼────┐
   │Codegen │  generate_code() → assembly string
   └───┬────┘
       │
   ┌───▼───┐
   │  I/O  │  write_file("output.s")
   └───────┘
```

---

## Sintaxis Chronos

Ver documentación completa en `/docs/CHRONOS_SYNTAX.md`.

### Básico
```chronos
Program nombre
  Print "mensaje"
```

### Con Variables (FASE 1)
```chronos
Program test
  Variables:
    x: i32 = 42
    y: i64 = 100
    active: bool = true

  Print "Testing"
  Print x
```

### Reglas
- Indentación: **2 espacios** (no tabs)
- Keywords con mayúscula: `Program`, `Variables`, `Print`
- Tipos en minúscula: `i32`, `i64`, `bool`
- Comentarios: `#`

---

## Build & Test

### Build
```bash
./build.sh
```

Compila los 6 módulos y los enlaza en `./chronos`.

### Test Manual
```bash
./chronos hello.chronos
as -o output.o output.s
ld -o program output.o
./program
```

### Ejemplo de Output Generado

**Input (hello.chronos):**
```chronos
Program hello
  Print "Hello, World!"
```

**Output (output.s):**
```asm
.text
.global _start

_start:
    # Print
    movq $1, %rax           # syscall: write
    movq $1, %rdi           # fd: stdout
    leaq .Lstr_0(%rip), %rsi
    movq $14, %rdx          # length
    syscall

    # Exit
    movq $60, %rax          # syscall: exit
    xorq %rdi, %rdi         # status: 0
    syscall

.data
.Lstr_0:
    .ascii "Hello, World!\n"
```

---

## Estructuras de Datos

### AST (536 bytes)
```
Offset 0-7:   type (1=PROGRAM)
Offset 8-15:  name pointer
Offset 16-23: action_count
Offset 24-535: actions[64] (array de pointers tagged)
```

### Tagged Pointers
Para diferenciar string literals de variable names:
- **String literal:** pointer directo (low bit = 0)
- **Variable name:** pointer OR 1 (low bit = 1)

### Symbol Entry (64 bytes)
```
Offset 0-31:  name (null-terminated, max 31 chars)
Offset 32-35: type (0=i32, 1=i64, 2=bool)
Offset 36-39: padding (alignment)
Offset 40-47: stack_offset (qword)
Offset 48-55: initial_value (qword)
Offset 56-63: padding
```

---

## Syscalls Usados

| Syscall | Número | Uso |
|---------|--------|-----|
| openat  | 257    | Abrir archivo de entrada |
| read    | 0      | Leer contenido del archivo |
| write   | 1      | Output (mensajes y archivo .s) |
| exit    | 60     | Terminar programa |

---

## Estado de Implementación

### ✅ FASE 0 - Hello World (Completado)
- [x] Parser básico (Program, Print)
- [x] Codegen para syscall write
- [x] String literals
- [x] Build system

### 🔄 FASE 1 - Tipos Primitivos (En Progreso)
- [x] Parser de `Variables:` section
- [x] Symbol table (estructura, add, lookup)
- [x] Tipos: i32, i64, bool
- [x] Stack allocation (prologue/epilogue)
- [x] Tagged pointers para diferenciar strings/variables
- [ ] Debug: persistencia de symbol table
- [ ] Conversión int→string para Print variables
- [ ] Variable initialization code

### ⏳ FASE 2 - Expresiones Aritméticas (Planeado)
```chronos
result: i32 = a + b * 2
```

### ⏳ FASE 3+ - Ver EXPANSION_ROADMAP.md

---

## Issues Conocidos

### 1. Symbol Table Memory Persistence
Los writes a `current_stack_offset` no se están persistiendo correctamente entre llamadas a funciones. Posiblemente relacionado con RIP-relative addressing o inicialización de .data/.bss.

**Workaround actual:** Valores hard-coded (stack_size=16, var_offset=4).

### 2. Print Variables - TODO
Falta implementar conversión de int→string para poder imprimir valores de variables. Actualmente genera:
```asm
movq -4(%rsp), %rax
# TODO: Convert %rax to string and print
```

---

## Debugging

### Debug Output
Muchos módulos tienen debug prints que se pueden ver en stderr:
```
[DEBUG] File read OK
[DEBUG] Parse OK
[GEN] entered
[GEN] gen_program
...
```

### Tools Útiles
```bash
# Ver secciones del binario
objdump -h ./chronos

# Ver símbolos exportados
nm ./chronos

# Disassembly del compilador
objdump -d ./chronos

# Hex dump de archivo .chronos
xxd hello.chronos

# Ver syscalls durante ejecución
strace ./chronos hello.chronos 2>&1 | grep -E "(open|read|write)"
```

---

## Convenciones

### Registers
- `%r12-%r15` - Callee-saved, usados para estado persistente
- `%r14` - Output position en codegen (global)
- `%rdi, %rsi, %rdx` - Parámetros de funciones (System V ABI)
- `%rax` - Retorno de funciones

### Stack Alignment
- 16 bytes (System V ABI requirement)

### Memory Layout
- Heap del compiler: 1MB bump allocator
- Symbol table: 6400 bytes (100 símbolos × 64 bytes)
- Output buffer: 16KB

---

## Roadmap

Ver `/EXPANSION_ROADMAP.md` para timeline completo (~4 meses, 10 fases).

**Timeline resumido:**
- FASE 1: Variables y tipos (1 semana) ← **AQUÍ ESTAMOS**
- FASE 2: Expresiones aritméticas (3 días)
- FASE 3: Control de flujo (5 días)
- FASE 4: Funciones (1 semana)
- FASE 5: Structs + Ownership (2 semanas)
- ...
- FASE 10: Self-hosting (4 semanas)

---

**Hecho con:** 100% x86-64 Assembly (AT&T syntax)
**Sin dependencias:** Solo Linux syscalls
**Sin runtime:** Ejecutable estático puro
**Completamente determinista:** Mismo input → Mismo output, siempre
