# Chronos v1.0 - Plan de Auto-Contenido 100%

**Objetivo**: Compilador completamente escrito en Chronos, sin dependencias externas.

**Estado Actual**: v0.17 - Self-hosting pero depende de NASM + LD
**Meta**: v1.0 - 100% Chronos (sin C, sin NASM, sin LD)

---

## 🎯 Visión General

```
ACTUAL (v0.17):                    META (v1.0):
═══════════════                    ═══════════

source.ch                          source.ch
    ↓                                  ↓
[chronos_v10 - C] ✗                [compiler.ch - Chronos] ✅
    ↓                                  ↓
output.asm                         output.asm
    ↓                                  ↓
[nasm - C] ✗                       [assembler.ch - Chronos] ✅
    ↓                                  ↓
output.o                           output.o
    ↓                                  ↓
[ld - C] ✗                         [linker.ch - Chronos] ✅
    ↓                                  ↓
executable                         executable

❌ 3 dependencias externas         ✅ 0 dependencias externas
```

---

## 📋 Componentes a Desarrollar

### Fase 1: Ensamblador en Chronos (2-3 semanas)

**Archivo**: `compiler/chronos/assembler.ch`

**Funcionalidad**:
- Parsear assembly x86-64
- Convertir instrucciones a bytes de código máquina
- Generar formato objeto ELF64
- Manejar símbolos y relocation

**Instrucciones Mínimas Necesarias**:
```asm
; Movimientos
mov reg, imm64          ; 48 B8+r [imm64]
mov reg, reg            ; 48 89 /r
mov [reg+offset], reg   ; 48 89 /r [disp]
movzx reg, byte [reg]   ; 48 0F B6 /r

; Aritmética
add reg, reg            ; 48 01 /r
sub reg, reg            ; 48 29 /r
imul reg, reg           ; 48 0F AF /r
div reg                 ; 48 F7 /6

; Stack
push reg                ; 50+r
pop reg                 ; 58+r

; Control de flujo
call offset             ; E8 [rel32]
ret                     ; C3
jmp offset              ; E9 [rel32]

; Syscalls
syscall                 ; 0F 05

; Otros
xor reg, reg            ; 48 31 /r
leave                   ; C9
```

**Estructura**:
```chronos
struct Instruction {
    mnemonic: [i8; 16],  // "mov", "add", etc.
    operands: [i8; 64],  // "rax, 42"
    bytes: [u8; 15],     // Código máquina
    length: i64          // Longitud en bytes
}

struct Symbol {
    name: [i8; 64],
    offset: i64,
    section: i64
}

fn parse_asm(filename: *i8) -> i64;
fn encode_instruction(instr: *Instruction) -> i64;
fn generate_elf_object(output: *i8) -> i64;
```

**Estimación**: 800-1000 líneas

---

### Fase 2: Linker en Chronos (2-3 semanas)

**Archivo**: `compiler/chronos/linker.ch`

**Funcionalidad**:
- Leer archivos objeto ELF64
- Resolver símbolos entre múltiples objetos
- Aplicar relocations
- Generar ejecutable ELF64 final

**Estructura ELF que necesitamos**:
```
ELF Header (64 bytes)
Program Headers (múltiples de 56 bytes)
.text section (código)
.data section (datos inicializados)
.bss section (datos no inicializados)
Section Headers
Symbol Table
String Table
```

**Estructura**:
```chronos
struct ElfHeader {
    magic: [u8; 4],      // 0x7F 'E' 'L' 'F'
    class: u8,           // 2 = 64-bit
    data: u8,            // 1 = little-endian
    version: u8,         // 1
    // ... más campos
}

struct ProgramHeader {
    type_: u32,
    flags: u32,
    offset: u64,
    vaddr: u64,
    paddr: u64,
    filesz: u64,
    memsz: u64,
    align: u64
}

fn read_object_file(filename: *i8) -> i64;
fn resolve_symbols(objects: *i64, count: i64) -> i64;
fn apply_relocations(sections: *i64) -> i64;
fn generate_executable(output: *i8) -> i64;
```

**Estimación**: 600-800 líneas

---

### Fase 3: Generador de ELF en Chronos (1 semana)

**Archivo**: `compiler/chronos/elf.ch`

**Funcionalidad**:
- Biblioteca para crear/escribir archivos ELF64
- Usado por assembler y linker

**Funciones**:
```chronos
fn elf_create() -> i64;
fn elf_add_section(elf: *i64, name: *i8, data: *u8, size: i64) -> i64;
fn elf_add_symbol(elf: *i64, name: *i8, value: i64) -> i64;
fn elf_write_object(elf: *i64, filename: *i8) -> i64;
fn elf_write_executable(elf: *i64, filename: *i8) -> i64;
```

**Estimación**: 400-600 líneas

---

### Fase 4: Mejorar Compiler v3 (1 semana)

**Mejoras necesarias**:
- Generar assembly más completo
- Soporte para múltiples funciones
- Mejor manejo de símbolos
- Generar información de debug (opcional)

**Estimación**: +200 líneas

---

### Fase 5: Runtime Mínimo en Chronos (1 semana)

**Archivo**: `runtime/runtime.ch`

**Funcionalidad**:
- Funciones básicas: malloc, free, print, etc.
- Pre-compilado a objeto (.o)
- Enlazado automáticamente con programas

**Estimación**: 300-400 líneas

---

## 🗓️ Timeline Estimado

### Sprint 1 (Semanas 1-3): Ensamblador
- Semana 1: Parser de assembly básico
- Semana 2: Encoder de instrucciones x86-64
- Semana 3: Generación de ELF objeto + testing

### Sprint 2 (Semanas 4-6): Linker
- Semana 4: Lector de objetos ELF
- Semana 5: Resolución de símbolos
- Semana 6: Generación de ejecutable + testing

### Sprint 3 (Semanas 7-8): Integración
- Semana 7: Runtime, mejoras al compiler
- Semana 8: Pipeline completo, testing end-to-end

### Sprint 4 (Semana 9): Bootstrap Final
- Compilar compiler.ch con sí mismo
- Ensamblar con assembler.ch
- Enlazar con linker.ch
- ¡V1.0 ALCANZADO!

**Total**: 9 semanas (~2 meses)

---

## 🎯 Hitos del Proyecto

### Milestone 1: "Hello World desde Chronos"
```bash
./compiler.ch hello.ch -o hello.asm
./assembler.ch hello.asm -o hello.o
./linker.ch hello.o -o hello
./hello  # ¡Funciona!
```

### Milestone 2: "Compiler Self-Hosting Total"
```bash
# Compilar el compiler
./compiler.ch compiler.ch -o compiler.asm
./assembler.ch compiler.asm -o compiler.o
./linker.ch compiler.o runtime.o -o compiler_new

# Compilar el assembler
./compiler_new assembler.ch -o assembler.asm
./assembler.ch assembler.asm -o assembler.o
./linker.ch assembler.o runtime.o -o assembler_new

# Compilar el linker
./compiler_new linker.ch -o linker.asm
./assembler_new linker.asm -o linker.o
./linker.ch linker.o runtime.o -o linker_new

# ¡Ciclo completo!
```

### Milestone 3: "Bootstrap Sin C"
```bash
# Última vez que usamos el bootstrap C
./chronos_v10 compiler.ch  # Genera compiler

# A partir de aquí, solo Chronos
./compiler source.ch -o source.asm
./assembler source.asm -o source.o
./linker source.o -o program
./program

# ¡CHRONOS 100% AUTO-CONTENIDO!
```

---

## 📦 Estructura de Archivos v1.0

```
Chronos/
├── compiler/
│   ├── bootstrap-c/
│   │   └── chronos_v10.c      # Solo para bootstrap inicial
│   └── chronos/
│       ├── lexer.ch            # ✅ Existente
│       ├── parser.ch           # ✅ Existente
│       ├── ast.ch              # ✅ Existente
│       ├── codegen.ch          # ✅ Existente
│       ├── compiler_v3.ch      # ✅ Existente - mejorar
│       ├── assembler.ch        # 🆕 A crear
│       ├── linker.ch           # 🆕 A crear
│       └── elf.ch              # 🆕 A crear
├── runtime/
│   ├── runtime.ch              # 🆕 A crear
│   └── runtime.o               # Pre-compilado
├── tools/
│   ├── chronos-compile         # Script wrapper
│   ├── chronos-asm             # Script wrapper
│   └── chronos-link            # Script wrapper
└── v1.0/
    ├── compiler                # Binario final
    ├── assembler               # Binario final
    └── linker                  # Binario final
```

---

## 🔧 Detalles Técnicos Clave

### Encoding x86-64

**REX Prefix**: `0x48` para operandos de 64-bit
**ModR/M byte**: Codifica operandos registro/memoria

Ejemplo: `mov rax, 42`
```
48 C7 C0 2A 00 00 00
│  │  │  └─ Immediate value (42 = 0x2A)
│  │  └─ ModR/M (C0 = register direct, rax)
│  └─ Opcode (C7 = MOV r/m64, imm32)
└─ REX.W prefix (64-bit operand)
```

### Formato ELF64

**Magic Number**: `7F 45 4C 46` (.ELF)
**Entry Point**: Dirección de `_start`
**Load Address**: `0x400000` (típico para ejecutables)

---

## 🧪 Testing Strategy

### Unit Tests
- Test individual de cada instrucción ensamblada
- Test de generación de cada sección ELF
- Test de resolución de símbolos

### Integration Tests
- Ensamblar programas simples
- Enlazar múltiples objetos
- Ejecutar y verificar resultados

### End-to-End Tests
- Pipeline completo: .ch → executable
- Self-compilation tests
- Bootstrap verification

---

## 🚀 Orden de Implementación

### Paso 1: Assembler Básico (EMPIEZA AQUÍ)
```chronos
// assembler_simple.ch
// Solo soporta las instrucciones más básicas:
// - mov rax, N
// - syscall
// - ret
```

### Paso 2: Linker Mínimo
```chronos
// linker_simple.ch
// Solo enlaza un archivo objeto simple
// Genera ejecutable ELF básico
```

### Paso 3: Expandir Gradualmente
- Más instrucciones en assembler
- Múltiples objetos en linker
- Símbolos externos
- Relocations

### Paso 4: Bootstrap Completo
- Compilar todo con Chronos
- Eliminar dependencia de C
- ¡V1.0!

---

## 📊 Métricas de Éxito

- [ ] Assembler: 100+ instrucciones soportadas
- [ ] Linker: Enlaza múltiples objetos correctamente
- [ ] Pipeline: .ch → executable sin herramientas externas
- [ ] Self-hosting: Compiler compila compiler sin C
- [ ] Performance: Tiempo razonable (<5 segundos para programas pequeños)
- [ ] Tests: 100% de tests pasando

---

## 🎉 Beneficios de v1.0

1. **Independencia Total**: Sin dependencias externas
2. **Control Completo**: Entendemos cada byte generado
3. **Portabilidad**: Fácil portar a otras plataformas
4. **Educativo**: Aprende cómo funciona todo el stack
5. **Orgullo**: ¡Compilador 100% hecho en casa!

---

## 🔥 ¡EMPECEMOS!

**Próximo paso inmediato**: Crear `assembler_simple.ch`
- Parsear líneas de assembly
- Encodear instrucciones básicas
- Generar archivo objeto ELF simple

**Estimado para primer prototipo**: 2-3 días
**Archivo**: `compiler/chronos/assembler_simple.ch`

---

**Última actualización**: 2025-10-29
**Estado**: ¡PLAN COMPLETO - LISTO PARA IMPLEMENTAR! 🚀
