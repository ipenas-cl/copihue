# Chronos - Análisis de Determinismo

**Fecha:** 29 de Octubre, 2025
**Versión Actual:** v0.17
**Meta:** v1.0 - Lenguaje 100% determinístico

---

## Estado Actual vs Meta

### ✅ Lo que TENEMOS (v0.17)

1. **Tipos primitivos básicos**
   - i8, i16, i32, i64 (signed integers)
   - u8, u32, u64 (unsigned integers)
   - Tamaños explícitos ✅

2. **Sintaxis declarativa**
   - Funciones con tipos explícitos
   - Return statements
   - Expresiones aritméticas

3. **Sin undefined behavior obvio**
   - No hay punteros NULL sin check
   - No hay acceso fuera de bounds (en runtime)

### ❌ Lo que FALTA (crítico para v1.0)

#### 1.1 Semántica del Lenguaje

##### Evaluación de Expresiones: ❌ NO IMPLEMENTADO

**Estado actual:**
```chronos
// El parser reconoce esto:
return 10 + 5 * 2;

// Pero NO está especificado:
// - ¿Se evalúa de izquierda a derecha?
// - ¿Operator precedence está garantizado?
// - ¿Qué pasa con side effects?
```

**Problemas:**
- ❌ Orden de evaluación no especificado
- ❌ No hay short-circuit evaluation (no hay &&, ||)
- ❌ No hay sequence points definidos
- ❌ Side effects no controlados (no hay side effects aún porque no hay variables!)
- ❌ Evaluation strategy no declarada

##### Sistema de Tipos: ❌ CASI NO IMPLEMENTADO

**Estado actual:**
```chronos
fn main() -> i64 {
    return 42;  // El tipo es i64, pero...
}

// NO hay type checking real
// NO hay prevención de coerción
// NO hay type inference
```

**Problemas:**
- ❌ No hay strong typing enforcement
- ❌ No hay type checking en compile-time
- ❌ No hay prevención de coerción implícita
- ❌ No hay type inference
- ❌ No hay ownership/borrowing
- ❌ No hay effect types

##### Tipos Primitivos: ⚠️ PARCIALMENTE IMPLEMENTADO

**Estado actual:**
```chronos
// ✅ Tenemos width explícito
let x: i64 = 100;

// ❌ Pero NO tenemos:
// - Checked arithmetic (overflow detection)
// - Saturating arithmetic
// - Wrapping arithmetic explícito
// - Floating point con rounding modes
// - Fixed-point decimal
```

---

## Análisis Detallado

### 1. EVALUACIÓN DE EXPRESIONES

#### 1.1 Orden de Evaluación

**Estado:** ❌ NO ESPECIFICADO

**Problema en Chronos v0.17:**
```chronos
// ¿Qué se evalúa primero?
fn foo() -> i64 { println("foo"); return 1; }
fn bar() -> i64 { println("bar"); return 2; }

fn main() -> i64 {
    return foo() + bar();  // ¿"foo bar" o "bar foo"?
}
```

**Para v1.0 necesitamos:**
```chronos
// Especificación explícita en documentación:
// "Los operandos de operadores binarios se evalúan de IZQUIERDA a DERECHA"

fn main() -> i64 {
    return foo() + bar();  // GARANTIZADO: "foo bar"
}
```

#### 1.2 Short-Circuit Evaluation

**Estado:** ❌ NO IMPLEMENTADO (no hay && ni ||)

**Para v1.0:**
```chronos
// && debe ser short-circuit GARANTIZADO
fn main() -> i64 {
    if (x != 0 && 10 / x > 2) {  // NUNCA divide por cero
        return 1;
    }
    return 0;
}

// || debe ser short-circuit GARANTIZADO
fn main() -> i64 {
    if (x == 0 || 10 / x > 2) {  // NUNCA divide por cero si x == 0
        return 1;
    }
    return 0;
}
```

**Especificación requerida:**
- `&&` evalúa lado derecho SOLO si lado izquierdo es true
- `||` evalúa lado derecho SOLO si lado izquierdo es false
- Esto debe estar GARANTIZADO, no es optimización

#### 1.3 Sequence Points

**Estado:** ❌ NO DEFINIDO

**Problema potencial:**
```chronos
let x: i64 = 1;
let y: i64 = x + (x = 2);  // ¿Resultado? ¿3 o 4?
```

**Para v1.0:**
Definir sequence points claros:
1. Después de evaluar todos argumentos de función, antes de llamar
2. Después de `&&` lado izquierdo (si true)
3. Después de `||` lado izquierdo (si false)
4. Después de `,` en expresiones de secuencia
5. Antes de entrar/salir de función

#### 1.4 Side Effects

**Estado:** ⚠️ NO RELEVANTE AÚN (no hay variables mutables)

**Para v1.0:**
```chronos
// Prohibir side effects ambiguos
let x: i64 = 1;

// ❌ Error en compile-time:
let y: i64 = x + (x = 2);  // ERROR: modificación durante evaluación

// ✅ OK - side effects en sequence points:
x = 2;
let y: i64 = x + x;  // OK: y = 4
```

---

### 2. SISTEMA DE TIPOS

#### 2.1 Strong Static Typing

**Estado:** ❌ NO IMPLEMENTADO (parser acepta tipos, pero no verifica)

**Problema actual:**
```chronos
fn foo() -> i64 {
    return 42;
}

fn main() -> i32 {  // Dice i32
    return foo();    // Pero retorna i64 - NO HAY ERROR
}
```

**Para v1.0 necesitamos:**
```chronos
fn foo() -> i64 { return 42; }

fn main() -> i32 {
    return foo();  // ❌ ERROR: Cannot assign i64 to i32
                   //    Explicit cast required: return foo() as i32;
}
```

**Implementación requerida:**
1. Type table durante parsing
2. Type checking en cada expresión
3. Type checking en return statements
4. Type checking en asignaciones
5. Type checking en llamadas de función

#### 2.2 No Coerciones Implícitas

**Estado:** ❌ NO IMPLEMENTADO

**Para v1.0:**
```chronos
let x: i32 = 42;
let y: i64 = x;      // ❌ ERROR: No implicit conversion
let z: i64 = x as i64;  // ✅ OK: Explicit cast

let a: u32 = 100;
let b: i32 = a;      // ❌ ERROR: No implicit conversion u32 -> i32
let c: i32 = a as i32;  // ✅ OK: Explicit cast (puede ser unsafe!)

// Especialmente importante:
let ptr: *i64 = 0;   // ❌ ERROR: Cannot convert i64 to pointer
let ptr: *i64 = null;  // ✅ OK: Explicit null
```

#### 2.3 Type Inference

**Estado:** ❌ NO IMPLEMENTADO

**Para v1.0 (opcional pero recomendado):**
```chronos
// Opción 1: Sin type inference (más simple)
let x: i64 = 42;     // Tipo explícito requerido
let y: i64 = x + 1;  // Tipo explícito requerido

// Opción 2: Con type inference (más ergonómico)
let x := 42;         // Inferido como i64 (literal default)
let y := x + 1;      // Inferido como i64 (de x)
let z := 3.14;       // Inferido como f64 (literal default)

// Pero los parámetros y returns SIEMPRE explícitos:
fn foo(x: i64) -> i64 {  // NO se pueden inferir
    let y := x * 2;       // OK inferir aquí
    return y;
}
```

**Recomendación:** Implementar inference mínimo para v1.0, expandir después.

#### 2.4 Ownership & Borrowing (Rust-style)

**Estado:** ❌ NO IMPLEMENTADO

**Para v1.0:**
```chronos
// Sin ownership, podemos tener:
fn main() -> i64 {
    let x: *i64 = malloc(8) as *i64;
    x[0] = 42;

    let y: *i64 = x;  // y y x apuntan a lo mismo
    free(x);           // Liberamos memoria

    return y[0];       // ❌ USE AFTER FREE!
}

// Con ownership:
fn main() -> i64 {
    let x: *i64 = malloc(8) as *i64;
    x[0] = 42;

    let y: *i64 = x;  // x se MUEVE a y, x ya no es válido
    // free(x);        // ❌ ERROR: x fue moved
    free(y);           // ✅ OK

    // return y[0];    // ❌ ERROR: y fue moved por free
    return 42;
}
```

**Implementación requerida:**
1. Ownership tracking en compile-time
2. Move semantics
3. Borrow checking
4. Lifetime annotations

**Prioridad:** ALTA para seguridad

---

### 3. TIPOS PRIMITIVOS DETERMINÍSTICOS

#### 3.1 Checked Arithmetic (Default)

**Estado:** ❌ NO IMPLEMENTADO

**Problema actual:**
```chronos
fn main() -> i64 {
    let x: i8 = 127;
    let y: i8 = x + 1;  // ¿Qué pasa? ¿-128? ¿127? ¿panic?
    return y as i64;
}
```

**Para v1.0:**
```chronos
// Por defecto: CHECKED (panic en overflow)
fn main() -> i64 {
    let x: i8 = 127;
    let y: i8 = x + 1;  // ❌ PANIC en runtime: "arithmetic overflow"
    return y as i64;
}

// Opción: saturating arithmetic
fn main() -> i64 {
    let x: i8 = 127;
    let y: i8 = x.saturating_add(1);  // y = 127 (clamp)
    return y as i64;
}

// Opción: wrapping arithmetic (explícito!)
fn main() -> i64 {
    let x: i8 = 127;
    let y: i8 = x.wrapping_add(1);  // y = -128 (two's complement)
    return y as i64;
}

// Opción: checked retorna Option
fn main() -> i64 {
    let x: i8 = 127;
    match x.checked_add(1) {
        Some(y) => return y as i64,
        None => return -1,  // Manejo de overflow
    }
}
```

**Implementación requerida:**
1. Runtime checks en cada operación aritmética (por defecto)
2. Métodos `.checked_add()`, `.saturating_add()`, `.wrapping_add()`
3. Flags de compilación para cambiar default behavior

**Prioridad:** ALTA para determinismo

#### 3.2 Floating Point Determinístico

**Estado:** ❌ NO IMPLEMENTADO (no hay f32/f64 aún)

**Para v1.0:**
```chronos
// IEEE 754 con rounding mode EXPLÍCITO
fn main() -> f64 {
    // Rounding mode por defecto: RoundNearestTiesToEven
    let x: f64 = 0.1 + 0.2;  // Resultado determinístico

    // Rounding mode explícito:
    with_rounding_mode(RoundTowardZero) {
        let y: f64 = 1.0 / 3.0;  // Truncamiento
        return y;
    }
}

// NO undefined behavior en división por cero:
fn main() -> f64 {
    let x: f64 = 1.0 / 0.0;  // x = +Infinity (determinístico)
    let y: f64 = 0.0 / 0.0;  // y = NaN (determinístico)
    return x;
}
```

**Implementación requerida:**
1. f32, f64 types con IEEE 754 compliance
2. Rounding mode control
3. Manejo determinístico de Infinity/NaN
4. Funciones como `.is_nan()`, `.is_infinite()`

#### 3.3 Fixed-Point Decimal

**Estado:** ❌ NO IMPLEMENTADO

**Para v1.0 (opcional):**
```chronos
// Para operaciones financieras exactas
fn main() -> Decimal {
    let price: Decimal<2> = 19.99;  // 2 decimales
    let tax: Decimal<2> = 0.08;
    let total: Decimal<2> = price * (1.0 + tax);
    return total;  // Exacto: 21.59 (sin errores de float)
}
```

---

## Prioridades para v1.0

### CRÍTICO (Must Have)

1. **✅ Strong Static Typing**
   - Type checking en compile-time
   - No coerciones implícitas
   - Errores claros de tipo

2. **✅ Checked Arithmetic**
   - Overflow detection por defecto
   - Panic en overflow (determinístico)
   - Métodos explícitos para wrapping/saturating

3. **✅ Orden de Evaluación Especificado**
   - Left-to-right garantizado
   - Documentado claramente

4. **✅ Short-Circuit Evaluation**
   - `&&` y `||` con short-circuit
   - Garantizado, no optimización

5. **✅ No Undefined Behavior**
   - Null pointer checks
   - Bounds checking en arrays
   - Division by zero handling

### IMPORTANTE (Should Have)

6. **⚠️ Type Inference Básico**
   - Solo para variables locales
   - Funciones siempre explícitas

7. **⚠️ Ownership/Borrowing Básico**
   - Prevenir use-after-free
   - Prevenir double-free

8. **⚠️ Effect Types Básico**
   - Track I/O, memory allocation
   - Pure functions marcadas

### DESEABLE (Nice to Have)

9. **💡 Dependent Types**
   - Arrays con tamaño en tipo
   - Constraints en compile-time

10. **💡 Linear Types**
    - Recursos usados exactamente una vez
    - File handles, sockets

11. **💡 Session Types**
    - Protocols verificados en compile-time

---

## Plan de Implementación

### Fase 1: Type System (Semanas 1-2)

**Objetivo:** Strong static typing con checking

**Tareas:**
1. Implementar type table
2. Type checking en expresiones
3. Type checking en funciones
4. Error messages claros
5. Tests comprehensivos

**Archivo:** Agregar a `compiler_main.ch` o crear `typechecker.ch`

### Fase 2: Arithmetic Semantics (Semanas 3-4)

**Objetivo:** Checked arithmetic por defecto

**Tareas:**
1. Runtime checks para overflow
2. Implementar `.checked_add()` etc.
3. Wrapping/saturating variants
4. Division by zero handling
5. Tests de overflow

**Archivo:** Actualizar `codegen.ch` para generar checks

### Fase 3: Evaluation Order (Semana 5)

**Objetivo:** Garantizar left-to-right evaluation

**Tareas:**
1. Documentar orden de evaluación
2. Implementar short-circuit para && y ||
3. Tests de side effects
4. Validar con benchmarks

**Archivo:** Documentación + `compiler_main.ch`

### Fase 4: Ownership Básico (Semanas 6-8)

**Objetivo:** Prevenir use-after-free

**Tareas:**
1. Ownership tracking básico
2. Move semantics
3. Borrow checker simple
4. Error messages
5. Tests exhaustivos

**Archivo:** Crear `ownership.ch`

---

## Métricas de Éxito

### Determinismo Completo

- [ ] Programa ejecutado N veces da mismo resultado N veces
- [ ] No undefined behavior bajo ninguna circunstancia
- [ ] No implementation-defined behavior en paths críticos
- [ ] Overflow detectado al 100%
- [ ] Type errors atrapados al 100% en compile-time

### Seguridad

- [ ] No use-after-free posible
- [ ] No double-free posible
- [ ] No null pointer dereference sin panic
- [ ] No buffer overflow sin panic
- [ ] No data races (cuando tengamos concurrency)

### Ergonomía

- [ ] Error messages claros y útiles
- [ ] Type inference donde tiene sentido
- [ ] No boilerplate excesivo
- [ ] Performance aceptable (overhead < 10% vs unchecked)

---

## Comparación con Otros Lenguajes

### C/C++
- ❌ Undefined behavior en todo lado
- ❌ No ownership
- ❌ Integer overflow es UB
- ❌ Null pointer dereference es UB
- **Chronos mejora:** TODO es definido

### Rust
- ✅ Ownership + borrowing
- ✅ No undefined behavior
- ✅ Strong typing
- ✅ Checked arithmetic opcional
- **Chronos objetivo:** Igual o mejor

### Go
- ⚠️ Garbage collector (no determinismo temporal)
- ⚠️ Integer overflow es wrapping silencioso
- ✅ No undefined behavior
- **Chronos mejora:** Sin GC, overflow checked

### Zig
- ✅ Checked arithmetic por defecto
- ✅ Explicit control
- ✅ No undefined behavior
- ⚠️ No ownership (manual memory management)
- **Chronos objetivo:** Similar + ownership

---

## Especificación Formal (v1.0)

### Documento Requerido: `CHRONOS_SPEC.md`

Contenido:
1. **Lexical Structure**
   - Tokens, keywords, operators

2. **Syntax**
   - Grammar completa (BNF o EBNF)

3. **Semantics**
   - Evaluation order GARANTIZADO
   - Type rules COMPLETAS
   - Memory model ESPECIFICADO

4. **Type System**
   - Todas las reglas de tipos
   - Ownership rules
   - Borrowing rules

5. **Standard Library**
   - Todas las funciones con especificación formal
   - Pre/post condiciones
   - Complexity guarantees

6. **Platform Dependencies**
   - Qué depende de la plataforma
   - Qué es portable
   - Endianness, pointer size, etc.

---

## Conclusión

**Estado actual (v0.17):**
- ✅ Sintaxis básica
- ⚠️ Semántica parcial
- ❌ Determinismo NO garantizado

**Meta v1.0:**
- ✅ Sintaxis completa
- ✅ Semántica COMPLETAMENTE especificada
- ✅ Determinismo 100% GARANTIZADO
- ✅ No undefined behavior NUNCA
- ✅ Strong typing con ownership

**Esfuerzo estimado:**
- Type system: 2 semanas
- Checked arithmetic: 2 semanas
- Evaluation semantics: 1 semana
- Ownership básico: 3 semanas
- **Total: ~8 semanas adicionales**

**Prioridad:** CRÍTICA - Sin esto, Chronos no es realmente determinístico.

---

**Autor:** Análisis basado en requisitos de lenguajes determinísticos
**Fecha:** 29 de Octubre, 2025
**Próximo paso:** Crear `CHRONOS_SPEC.md` con especificación formal
