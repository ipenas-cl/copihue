# Compiler Improvement Plan - 100% Chronos

**Fecha:** 29 de Octubre, 2025
**Objetivo:** Mejorar compiler_main.ch para que compile typechecker.ch
**Filosofía:** Sin atajos, sin C, sin híbridos - Solo Chronos puro

---

## 🎯 Objetivo Final

**Compilar esto:**
```chronos
struct TypeInfo {
    id: i64,
    name: [i8; 32],
    kind: i64,
    size: i64
}

struct TypeTable {
    types: [TypeInfo; 128],
    count: i64
}

fn main() -> i64 {
    let table: TypeTable;
    table.count = 0;

    let type_info: *TypeInfo = table.types[0];
    type_info.id = 5;

    return table.count;
}
```

**Estado actual:** ❌ Ninguna de estas features funciona
**Estado objetivo:** ✅ Todo compila y ejecuta correctamente

---

## 📋 Features Necesarias (En Orden)

### ✅ Ya Tenemos (v0.17)
1. Funciones básicas con tipos
2. Return statements
3. Expresiones aritméticas (+, -, *, /)
4. Números literales
5. Variables simples (let x: i64)

### 🚧 Necesitamos Agregar

#### Fase 1: Struct Basics (3-4 días)
1. **Struct definition parsing**
   - Reconocer: `struct Name { field1: type1, field2: type2 }`
   - Almacenar información de fields
   - Calcular offsets y tamaño total

2. **Struct variable declarations**
   - Reconocer: `let obj: StructName;`
   - Allocar espacio en stack
   - Inicializar a cero

3. **Field access READ**
   - Reconocer: `obj.field`
   - Calcular offset del field
   - Generar código de lectura

4. **Field access WRITE**
   - Reconocer: `obj.field = value;`
   - Calcular offset del field
   - Generar código de escritura

#### Fase 2: Pointers to Structs (2-3 días)
5. **Pointer declarations**
   - Reconocer: `let ptr: *TypeName;`
   - Type tracking correcto

6. **Pointer field access**
   - Reconocer: `ptr.field` (equivalente a ptr->field en C)
   - Dereferenciar + offset
   - Generar código correcto

7. **Address-of operator**
   - Reconocer: `&variable`
   - Generar dirección

#### Fase 3: Arrays (2-3 días)
8. **Array declarations**
   - Reconocer: `let arr: [TypeName; N];`
   - Calcular tamaño total

9. **Array indexing**
   - Reconocer: `arr[index]`
   - Calcular offset (index * element_size)
   - Generar código de acceso

10. **Arrays of structs**
    - Combinar arrays + structs
    - `let types: [TypeInfo; 128];`
    - `types[i].field`

#### Fase 4: Comparisons & Logic (1-2 días)
11. **Comparison operators**
    - ==, !=, <, >, <=, >=
    - Generar cmp + conditional jumps

12. **Logical operators**
    - &&, ||, !
    - Short-circuit evaluation

13. **If statements**
    - if (condition) { } else { }
    - Generar labels y jumps

#### Fase 5: Loops (1-2 días)
14. **While loops**
    - while (condition) { }
    - Generar labels y jumps

15. **Break/continue** (opcional)

---

## 🔧 Implementación Detallada

### Fase 1: Struct Basics

#### Feature 1: Struct Definition Parsing

**Input:**
```chronos
struct Point {
    x: i64,
    y: i64
}
```

**Necesitamos:**
```chronos
// Nueva estructura para almacenar field info
struct FieldInfo {
    name: [i8; 32],
    type_name: [i8; 32],
    offset: i64,
    size: i64
}

// Nueva estructura para struct definition
struct StructDef {
    name: [i8; 32],
    fields: [FieldInfo; 16],
    field_count: i64,
    total_size: i64
}

// Global table de structs
let struct_defs: [StructDef; 32];
let struct_count: i64 = 0;

// Parser function
fn parse_struct_definition(source: *i8, pos: i64) -> i64 {
    // 1. Skip "struct"
    // 2. Parse name
    // 3. Skip "{"
    // 4. Loop: parse field name, type, comma
    // 5. Calculate offsets
    // 6. Store in struct_defs
    // 7. Return new position
}
```

**Testing:**
```chronos
// test_struct_parse.ch
struct Point { x: i64, y: i64 }

fn main() -> i64 {
    return 0;  // Just test parsing
}
```

**Compile:**
```bash
./bootstrap-c/chronos_v10 compiler_v2.ch
./chronos_program test_struct_parse.ch
# Should succeed without errors
```

---

#### Feature 2: Field Access READ

**Input:**
```chronos
struct Point { x: i64, y: i64 }

fn main() -> i64 {
    let p: Point;
    p.x = 10;
    return p.x;  // Should return 10
}
```

**Necesitamos:**

**1. Parser reconoce dot notation:**
```chronos
fn parse_field_access(source: *i8, pos: i64) -> i64 {
    // obj.field
    // 1. Parse variable name "obj"
    // 2. Check for '.'
    // 3. Parse field name "field"
    // 4. Lookup struct definition
    // 5. Get field offset
    // 6. Return AST node with offset
}
```

**2. Code generator:**
```chronos
fn gen_field_access_read(var_name: *i8, field_offset: i64) -> i64 {
    // Assuming variable is on stack at [rbp-N]
    emit("    mov rax, [rbp-");
    emit_num(var_stack_offset + field_offset);
    emit("]\n");
}
```

**Generated assembly:**
```asm
; let p: Point;  (Point is 16 bytes)
sub rsp, 16

; p.x = 10
mov rax, 10
mov [rbp-16], rax    ; p.x at offset 0

; return p.x
mov rax, [rbp-16]    ; Read p.x
```

---

#### Feature 3: Pointer Field Access

**Input:**
```chronos
struct Point { x: i64, y: i64 }

fn main() -> i64 {
    let p: Point;
    let ptr: *Point = &p;
    ptr.x = 42;
    return p.x;  // Should return 42
}
```

**Necesitamos:**

**1. Address-of operator:**
```chronos
fn gen_address_of(var_name: *i8) -> i64 {
    // &p
    emit("    lea rax, [rbp-");
    emit_num(var_stack_offset);
    emit("]\n");
}
```

**2. Pointer field access:**
```chronos
fn gen_pointer_field_access(ptr_var: *i8, field_offset: i64) -> i64 {
    // ptr.field  (ptr is pointer, need to dereference)

    // 1. Load pointer value into rax
    emit("    mov rax, [rbp-");
    emit_num(ptr_stack_offset);
    emit("]\n");

    // 2. Access field at offset
    if (field_offset > 0) {
        emit("    add rax, ");
        emit_num(field_offset);
        emit("\n");
    }

    // 3. Dereference
    emit("    mov rax, [rax]\n");
}
```

---

#### Feature 4: Arrays of Structs

**Input:**
```chronos
struct TypeInfo {
    id: i64,
    name: [i8; 32],
    size: i64
}

fn main() -> i64 {
    let types: [TypeInfo; 8];
    types[0].id = 5;
    return types[0].id;
}
```

**Necesitamos:**

**1. Array declaration parsing:**
```chronos
fn parse_array_type(source: *i8, pos: i64) -> i64 {
    // [TypeInfo; 8]
    // 1. Skip '['
    // 2. Parse element type
    // 3. Skip ';'
    // 4. Parse count
    // 5. Skip ']'
    // 6. Calculate total size = element_size * count
}
```

**2. Array indexing:**
```chronos
fn gen_array_index(array_var: *i8, index: i64, element_size: i64) -> i64 {
    // types[0]

    // Calculate offset = index * element_size
    let offset: i64 = index * element_size;

    emit("    lea rax, [rbp-");
    emit_num(array_stack_offset + offset);
    emit("]\n");
}
```

**3. Combined array + field access:**
```chronos
fn gen_array_element_field(
    array_var: *i8,
    index: i64,
    element_size: i64,
    field_offset: i64
) -> i64 {
    // types[0].id

    let total_offset: i64 = (index * element_size) + field_offset;

    emit("    mov rax, [rbp-");
    emit_num(array_stack_offset + total_offset);
    emit("]\n");
}
```

---

## 📊 Timeline Realista

### Fase 1: Struct Basics (3-4 días)
- Día 1: Struct definition parsing
- Día 2: Field access READ
- Día 3: Field access WRITE
- Día 4: Testing y fixes

### Fase 2: Pointers (2-3 días)
- Día 5: Pointer declarations + address-of
- Día 6: Pointer field access
- Día 7: Testing y fixes

### Fase 3: Arrays (2-3 días)
- Día 8: Array declarations
- Día 9: Array indexing + arrays of structs
- Día 10: Testing y fixes

### Fase 4: Comparisons (1-2 días)
- Día 11: Comparison operators
- Día 12: If statements

### Fase 5: Loops (1 día)
- Día 13: While loops

### Fase 6: Integration (2 días)
- Día 14: Compilar typechecker.ch
- Día 15: Fixes finales

**Total: ~15 días (3 semanas)**

---

## 🎯 Hitos Verificables

### Milestone 1: Struct Basics (Día 4)
```chronos
struct Point { x: i64, y: i64 }

fn main() -> i64 {
    let p: Point;
    p.x = 10;
    p.y = 20;
    return p.x + p.y;  // Should return 30
}
```

✅ Compila
✅ Ejecuta
✅ Retorna 30

### Milestone 2: Pointers (Día 7)
```chronos
struct Point { x: i64, y: i64 }

fn main() -> i64 {
    let p: Point;
    let ptr: *Point = &p;
    ptr.x = 42;
    return p.x;  // Should return 42
}
```

✅ Compila
✅ Ejecuta
✅ Retorna 42

### Milestone 3: Arrays (Día 10)
```chronos
struct TypeInfo { id: i64 }

fn main() -> i64 {
    let types: [TypeInfo; 4];
    types[0].id = 10;
    types[1].id = 20;
    types[2].id = 30;
    return types[1].id;  // Should return 20
}
```

✅ Compila
✅ Ejecuta
✅ Retorna 20

### Milestone 4: Comparisons (Día 12)
```chronos
fn main() -> i64 {
    let x: i64 = 10;
    if (x == 10) {
        return 42;
    } else {
        return 0;
    }
}
```

✅ Compila
✅ Ejecuta
✅ Retorna 42

### Milestone 5: Loops (Día 13)
```chronos
fn main() -> i64 {
    let sum: i64 = 0;
    let i: i64 = 0;
    while (i < 10) {
        sum = sum + i;
        i = i + 1;
    }
    return sum;  // Should return 45 (0+1+2+...+9)
}
```

✅ Compila
✅ Ejecuta
✅ Retorna 45

### Milestone 6: TypeChecker Compiles (Día 15)
```bash
./compiler_v2 compiler/chronos/typechecker.ch
```

✅ Compila sin errores
✅ typechecker ejecutable funciona
✅ Tests de typechecker pasan

---

## 🚀 Estrategia de Implementación

### Principio: Incremental + Testeable

**Cada feature:**
1. Escribir diseño (15 min)
2. Implementar parser (1-2 horas)
3. Implementar codegen (1-2 horas)
4. Escribir test (30 min)
5. Compilar test (5 min)
6. Ejecutar test (5 min)
7. Fix bugs (30 min - 2 horas)
8. **Commit** ✅

**Beneficios:**
- Progreso visible cada día
- Nada se rompe
- Fácil debuggear
- Siempre tenemos algo funcionando

---

## 🎯 Empezamos Ahora

### Tarea Inmediata: Feature 1 - Struct Definition Parsing

**Archivo:** `compiler/chronos/compiler_v2.ch` (copiar de compiler_main.ch)

**Cambios:**
1. Agregar estructuras FieldInfo y StructDef
2. Agregar global struct_defs array
3. Implementar parse_struct_definition()
4. Integrar en main() para reconocer structs antes de funciones

**Test:**
```chronos
// test1_struct_def.ch
struct Point { x: i64, y: i64 }

fn main() -> i64 {
    return 0;
}
```

**Objetivo:** Que compile sin error

**Tiempo estimado:** 2-3 horas

---

## 📝 Notas Importantes

### Por Qué Este Orden

1. **Structs primero** - Base de todo
2. **Pointers después** - Necesitan structs
3. **Arrays después** - Necesitan structs
4. **Comparisons** - Simples, sin dependencies
5. **Loops** - Usan comparisons

### Por Qué Esto Funciona

- Cada feature es **independiente**
- Podemos **testear inmediatamente**
- **No bloqueamos** otras cosas
- Progreso **visible y medible**

### Qué NO Necesitamos Aún

- ❌ Generics (v1.1)
- ❌ Traits (v1.2)
- ❌ Closures (v1.2)
- ❌ Macros (v1.3)
- ❌ Concurrency (v1.3)

**Enfoque:** Features esenciales primero.

---

## ✅ Conclusión

**Este plan es:**
- ✅ 100% Chronos (sin C, sin híbridos)
- ✅ Incremental (feature por feature)
- ✅ Testeable (milestone cada 3-4 días)
- ✅ Alcanzable (15 días de trabajo enfocado)
- ✅ Determinístico (especificado completamente)

**Resultado final:**
- Compiler que compila typechecker.ch
- Base sólida para assembler/linker
- Lenguaje más capaz
- Orgullo de haberlo hecho bien ✅

---

**Filosofía:** "Si Chronos no lo tiene, lo agregamos al lenguaje."

**Motto:** 100% Chronos. Sin atajos. Sin híbridos. El mejor lenguaje determinístico.

---

**Próximo paso:** Implementar Feature 1 - Struct Definition Parsing

**¿Empezamos?** 🚀
