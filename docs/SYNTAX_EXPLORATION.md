# Chronos Syntax Exploration - ¿Qué sintaxis sirve al determinismo?

**Fecha:** 29 de octubre de 2025
**Pregunta:** ¿C-like es realmente lo óptimo para un lenguaje determinista?

---

## El Problema con C-like

### Sintaxis que Oculta Problemas

```c
// C/C++ - Operadores confusos
x = y++;              // ¿Pre o post incremento?
a = b = c = 0;        // Orden de evaluación
if (x = 5) { }        // Bug común (= vs ==)
x+++y                 // ¿Qué es esto? x++ + y o x + ++y
result = a > b ? a : b;  // Ternario anidado = ilegible
```

### Precedencia Imposible de Recordar

```c
// ¿Cuál es el resultado? (sin mirar tabla de precedencia)
x = a + b << c & d | e ^ f && g || h ? i : j;

// Nadie sabe sin paréntesis
```

### Undefined Behavior Sintáctico

```c
// C - Todos tienen undefined behavior
i = i++ + ++i;        // UB
a[i] = i++;           // UB
f(i++, i++);          // UB
```

---

## Alternativas: Lenguajes Safety-Critical

### Ada/SPARK - Diseñado para Aviónica

```ada
-- Ada: Explícito y claro
procedure Swap (X, Y : in out Integer) is
   Temp : Integer;
begin
   Temp := X;
   X := Y;
   Y := Temp;
end Swap;

-- Rangos explícitos
type Day is range 1 .. 31;
type Month is (Jan, Feb, Mar, Apr, May, Jun, Jul, Aug, Sep, Oct, Nov, Dec);

-- Contratos (precondiciones/postcondiciones)
function Divide (A, B : Integer) return Integer
   with Pre  => B /= 0,
        Post => Divide'Result * B = A;
```

**Ventajas Ada:**
- ✅ Extremadamente explícito
- ✅ No ambigüedad sintáctica
- ✅ Usado en sistemas críticos (aviones, trenes, satélites)
- ✅ Contratos en el lenguaje
- ✅ Ranges y tipos fuertes

**Desventajas:**
- ❌ Verboso (demasiado?)
- ❌ Sintaxis antigua (1980s)
- ❌ No popular (curva de adopción)

### ML Family - Determinismo Funcional

```ocaml
(* OCaml/SML - Funcional, inmutable por defecto *)
let rec fib n =
  match n with
  | 0 -> 0
  | 1 -> 1
  | n -> fib (n-1) + fib (n-2)

(* Pattern matching exhaustivo *)
type result = Ok of int | Error of string

let divide a b =
  match b with
  | 0 -> Error "Division by zero"
  | b -> Ok (a / b)

(* Tipos algebraicos *)
type tree =
  | Leaf of int
  | Node of tree * int * tree
```

**Ventajas ML:**
- ✅ Inmutabilidad por defecto (determinismo natural)
- ✅ Pattern matching exhaustivo
- ✅ Type inference potente
- ✅ Sin side effects inesperados
- ✅ Sintaxis limpia

**Desventajas:**
- ❌ Poco familiar para programadores imperativos
- ❌ ¿Demasiado funcional para systems programming?

### Rust - Lo que ya sabemos

```rust
// Rust - Híbrido imperativo/funcional
fn divide(a: i32, b: i32) -> Result<i32, String> {
    if b == 0 {
        Err("Division by zero".to_string())
    } else {
        Ok(a / b)
    }
}

match divide(10, 2) {
    Ok(val) => println!("Result: {}", val),
    Err(e) => println!("Error: {}", e),
}
```

**Ventajas Rust:**
- ✅ Pattern matching
- ✅ Expresiones vs statements
- ✅ Result/Option obligatorios
- ✅ Sintaxis moderna

**Desventajas:**
- ❌ Lifetime annotations complejas
- ❌ Curva de aprendizaje empinada
- ❌ Aún C-like en operadores

---

## Propuesta: Sintaxis Determinista Nueva

### Principios de Diseño

1. **Explicititud > Brevedad**
   - No operadores crípticos
   - No precedencia confusa
   - Cada operación clara

2. **Imposibilidad de Errores Comunes**
   - No `=` vs `==` (diferentes sintaxis)
   - No side effects en expresiones
   - No orden de evaluación ambiguo

3. **Costo Visible**
   - Allocaciones explícitas
   - Loops con bounds claros
   - No operaciones ocultas

4. **Determinismo Visual**
   - Mismo código → misma ejecución (obvio al leerlo)
   - No undefined behavior posible
   - No race conditions en sintaxis

---

## Opción 1: Ada-Inspired (Extremadamente Explícito)

```chronos
-- Función con contrato
function divide(a: Integer, b: Integer) -> Integer
    requires b != 0
    ensures result * b == a
is
    return a / b;
end divide;

-- Variable con rango
type Day is range 1 to 31;
var today: Day := 15;

-- Pattern matching
type Result is
    | Ok(value: Integer)
    | Error(message: String);

function safe_divide(a: Integer, b: Integer) -> Result is
    if b == 0 then
        return Error("Division by zero");
    else
        return Ok(a / b);
    end if;
end safe_divide;

-- Main
procedure main is
    var result: Result := safe_divide(10, 2);
begin
    match result is
        when Ok(val) =>
            print("Result: ", val);
        when Error(msg) =>
            print("Error: ", msg);
    end match;
end main;
```

**Características:**
- `requires`/`ensures` para contratos
- `is...end` bloques claros
- `when` para pattern matching (no `case` ambiguo)
- `:=` para asignación, `==` para comparación (no confusión)
- `procedure` (sin retorno) vs `function` (con retorno)

---

## Opción 2: ML-Inspired (Funcional Limpio)

```chronos
(* Inmutable por defecto *)
let fib n =
    match n with
    | 0 -> 0
    | 1 -> 1
    | n -> fib(n-1) + fib(n-2)

(* Tipos algebraicos *)
type result =
    | Ok of int
    | Error of string

let divide a b =
    if b = 0 then
        Error "Division by zero"
    else
        Ok (a / b)

(* Main *)
let main () =
    let result = divide 10 2 in
    match result with
    | Ok val ->
        print "Result: " val
    | Error msg ->
        print "Error: " msg
```

**Características:**
- `let` para todo (inmutable por defecto)
- `=` para binding, no asignación
- Pattern matching nativo
- Aplicación de funciones sin paréntesis: `divide 10 2`
- Indentación significativa (como Python/Haskell)

---

## Opción 3: Híbrido Determinista (Nueva Propuesta)

**Filosofía:** Lo mejor de Ada (explicititud), ML (inmutabilidad), Rust (modernidad)

```chronos
// Función simple
fn divide(a: i32, b: i32) -> Result[i32, String]
    require b != 0  // Precondición
{
    return Ok(a / b)
}

// Variable inmutable por defecto
let x = 10              // Inmutable
let mut y = 20          // Mutable (explícito)

// Asignación explícita
set y = 30              // 'set' keyword (no confusión con ==)

// Comparison siempre ==, !=
if x == 10 {
    print("Equal")
}

// Pattern matching
match divide(10, 2) {
    Ok(val) => print("Result: {val}"),
    Error(msg) => print("Error: {msg}"),
}

// Loops con bounds explícitos
for i in 0..10 {        // Range (exclusive)
    print(i)
}

// No ++ operator (side effect oculto)
set i = i + 1           // Explícito

// Arrays con bounds
let arr: [i32; 5] = [1, 2, 3, 4, 5]
let item = arr[2]       // Bounds checked

// Structs
struct Point {
    x: i32,
    y: i32,
}

let p = Point { x: 10, y: 20 }
print(p.x)

// Ownership (simple)
let s1 = String::from("hello")
let s2 = s1             // Move
// print(s1)            // ERROR: moved

// Borrowing
fn print_string(s: &String) {
    print(s)
}

let s = String::from("hello")
print_string(&s)        // Borrow
print(s)                // Still valid
```

**Características Clave:**

1. **`set` keyword para mutación**
   ```chronos
   let mut x = 10
   set x = 20          // Explícito
   // x = 20           // ERROR: use 'set'
   ```

2. **No operadores de side-effect**
   ```chronos
   // NO EXISTE: ++, --, +=, -=, etc.
   set x = x + 1       // Siempre explícito
   ```

3. **Asignación vs Comparación sin ambigüedad**
   ```chronos
   let x = 10          // Binding
   set x = 20          // Mutation (solo si mut)
   if x == 10 { }      // Comparison
   ```

4. **Precedencia simplificada (paréntesis obligatorios en casos ambiguos)**
   ```chronos
   let x = a + b * c       // OK: * tiene mayor precedencia (obvio)
   let y = a & b | c       // ERROR: use paréntesis
   let z = (a & b) | c     // OK
   ```

5. **Bounds siempre explícitos**
   ```chronos
   for i in 0..10 { }      // Range [0, 10)
   for i in 0..=10 { }     // Range [0, 10] (inclusive)
   while i < 10 {          // Condición explícita
       set i = i + 1
   }
   ```

6. **No operador ternario (usar if-expression)**
   ```chronos
   // NO: let x = a > b ? a : b;

   // SÍ:
   let x = if a > b { a } else { b }
   ```

7. **Contratos opcionales (phase 2)**
   ```chronos
   fn divide(a: i32, b: i32) -> i32
       require b != 0
       ensure result * b == a
   {
       return a / b
   }
   ```

---

## Comparación de Sintaxis

### Ejemplo: Fibonacci

**C-style:**
```c
int fib(int n) {
    if (n <= 1) return n;
    return fib(n-1) + fib(n-2);
}
```

**Ada-style:**
```ada
function fib(n: Integer) return Integer is
begin
    if n <= 1 then
        return n;
    else
        return fib(n-1) + fib(n-2);
    end if;
end fib;
```

**ML-style:**
```ocaml
let rec fib n =
    match n with
    | 0 | 1 -> n
    | n -> fib(n-1) + fib(n-2)
```

**Chronos Híbrido:**
```chronos
fn fib(n: i32) -> i32 {
    match n {
        0 | 1 => n,
        n => fib(n-1) + fib(n-2),
    }
}
```

### Ejemplo: Estado Mutable

**C-style:**
```c
int x = 10;
x++;
x += 5;
```

**Ada-style:**
```ada
X : Integer := 10;
X := X + 1;
X := X + 5;
```

**ML-style:**
```ocaml
let x = ref 10 in
x := !x + 1;
x := !x + 5
```

**Chronos Híbrido:**
```chronos
let mut x = 10
set x = x + 1
set x = x + 5
```

---

## Propuesta Final: Chronos Syntax v0.0.1

### Reglas Fundamentales

1. **Inmutabilidad por defecto**
   - `let x = 10` (inmutable)
   - `let mut x = 10` (mutable)

2. **Mutación explícita**
   - `set x = value` (keyword obligatorio)
   - No `=` para mutación (evita confusión)

3. **Sin side-effects ocultos**
   - No `++`, `--`, `+=`, `-=`
   - Siempre `set x = x + 1`

4. **Comparación sin ambigüedad**
   - `==`, `!=` para comparación
   - `=` solo en `let` (binding)
   - `set` para mutación

5. **Expresiones vs Statements claros**
   - `if` es expresión: `let x = if cond { a } else { b }`
   - `match` es expresión
   - `loop` es statement

6. **Pattern matching exhaustivo**
   - Compiler garantiza todos los casos
   - No `default` genérico (cada caso explícito)

7. **Precedencia simplificada**
   - Operadores matemáticos: estándar (*, /, +, -)
   - Operadores lógicos: paréntesis obligatorios si ambiguo
   - `a && b || c` → ERROR: use `(a && b) || c`

8. **Bounds explícitos siempre**
   - `for i in 0..10` (range)
   - `while cond { }` (condition visible)
   - No `for(;;)` (infinite loop confuso)
   - Sí `loop { }` (infinite loop claro)

---

## Sintaxis Completa v0.0.1

```chronos
// ===== VARIABLES =====

let x = 10                    // Inmutable
let mut y = 20                // Mutable
set y = 30                    // Mutación (keyword obligatorio)

// ===== TIPOS =====

let a: i32 = 10
let b: i64 = 20
let c: bool = true
let s: String = "hello"

// ===== FUNCIONES =====

fn add(a: i32, b: i32) -> i32 {
    return a + b
}

fn multiply(a: i32, b: i32) -> i32 {
    a * b                     // Implicit return (last expression)
}

// ===== CONTROL FLOW =====

// If (expression)
let max = if a > b { a } else { b }

// While
let mut i = 0
while i < 10 {
    print(i)
    set i = i + 1
}

// For
for i in 0..10 {              // [0, 10) exclusive
    print(i)
}

for i in 0..=10 {             // [0, 10] inclusive
    print(i)
}

// Infinite loop
loop {
    print("Forever")
    break
}

// ===== STRUCTS =====

struct Point {
    x: i32,
    y: i32,
}

let p = Point { x: 10, y: 20 }
print(p.x)

// ===== ENUMS =====

enum Option[T] {
    Some(T),
    None,
}

enum Result[T, E] {
    Ok(T),
    Error(E),
}

// ===== PATTERN MATCHING =====

match result {
    Ok(val) => print("Success: {val}"),
    Error(msg) => print("Error: {msg}"),
}

match number {
    0 => print("Zero"),
    1..=10 => print("Small"),
    11..=100 => print("Medium"),
    _ => print("Large"),
}

// ===== OWNERSHIP =====

let s1 = String::from("hello")
let s2 = s1                   // Move
// print(s1)                  // ERROR: moved

fn borrow(s: &String) {
    print(s)
}

let s = String::from("hello")
borrow(&s)                    // Borrow
print(s)                      // Still valid

// ===== ARRAYS =====

let arr: [i32; 5] = [1, 2, 3, 4, 5]
let slice: &[i32] = &arr[1..3]

// ===== CONTRATOS (Optional Phase 2) =====

fn divide(a: i32, b: i32) -> i32
    require b != 0
    ensure result * b == a
{
    return a / b
}
```

---

## Ventajas de esta Sintaxis

### 1. Imposible confundir asignación con comparación
```chronos
let x = 10          // Binding
set x = 20          // Mutation
if x == 10 { }      // Comparison

// En C: if (x = 10) // Bug común
// En Chronos: ERROR, no compila
```

### 2. Side effects siempre visibles
```chronos
// C: confuso
if (x++ > 10) { }

// Chronos: explícito
if x > 10 {
    set x = x + 1
}
```

### 3. Mutabilidad clara
```chronos
let x = 10          // Inmutable, obvio
let mut y = 10      // Mutable, obvio
set y = 20          // Mutación, obvio
```

### 4. Precedencia sin confusión
```chronos
a + b * c           // OK: obvio
a && b || c         // ERROR: ¿(a && b) || c? o ¿a && (b || c)?
(a && b) || c       // OK: explícito
```

---

## Desventajas (y mitigaciones)

### 1. Más verbose que C
**C:**
```c
x++;
```
**Chronos:**
```chronos
set x = x + 1
```

**Mitigación:** Claridad > Brevedad. Determinismo requiere explicititud.

### 2. No familiar
**Mitigación:**
- Documentación excelente
- Error messages educativos
- Tutoriales interactivos
- Vale la pena el cambio

### 3. `set` keyword extraño
**Alternativa considerada:**
```chronos
let mut x = 10
x = 20              // Sin 'set'
```

**Problema:** Confusión con `let x = 10` (se ve similar)

**Decisión:** Mantener `set` para claridad visual

---

## Decisión Final

**Sintaxis Chronos v0.0.1:**
- Híbrido: Ada (explicititud) + ML (inmutabilidad) + Rust (modernidad)
- `set` keyword para mutación
- No operadores side-effect (++, --, +=)
- Pattern matching exhaustivo
- Contratos opcionales (require/ensure)
- Precedencia simplificada

**Filosofía:**
- Explicititud > Brevedad
- Claridad > Familiaridad
- Determinismo > Conveniencia

---

**¿Procedemos con esta sintaxis?**

**Alternativas a considerar:**
A) Esta sintaxis híbrida (recomendado)
B) Más Ada-style (extremadamente explícito)
C) Más ML-style (más funcional)
D) Mantener C-style (familiar pero problemático)
E) Otra propuesta
