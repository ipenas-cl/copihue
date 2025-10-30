# Chronos Language Specification v2.0
**Real-Time Deterministic Systems Language**

**Fecha:** 29 de octubre de 2025
**Estado:** DRAFT - Language Design
**Filosofía:** Security First, Determinism Always, Performance By Design

---

## 0. PRINCIPIOS FUNDAMENTALES

### 0.1 Core Principles

1. **No Undefined Behavior** - Cada operación tiene comportamiento definido
2. **No Implementation-Defined Behavior** - Comportamiento consistente entre compiladores
3. **Determinismo Total** - Mismo input → mismo output, siempre
4. **Memory Safety** - Sin dangling pointers, sin data races
5. **Explicit Everything** - No conversiones implícitas, no coerciones ocultas
6. **Zero-Cost Abstractions** - Abstracciones sin overhead en runtime
7. **Compile-Time Guarantees** - Máximo checking en compile-time

### 0.2 Non-Goals

- ❌ Compatibilidad con C (diseño limpio > compatibilidad)
- ❌ Garbage collection (determinismo > conveniencia)
- ❌ Dynamic dispatch en RT paths (predecibilidad > flexibilidad)
- ❌ Reflection en runtime (WCET > metaprogramming)

---

## 1. SINTAXIS BASE

### 1.1 Comentarios

```chronos
// Comentario de línea

/* Comentario
   de bloque */

/// Comentario de documentación
/// Se adjunta al siguiente item
```

### 1.2 Identificadores

```chronos
// Válidos
let variable: i64;
let my_variable: i64;
let _private: i64;
let value123: i64;

// Inválidos
let 123value: i64;    // ERROR: no puede empezar con número
let my-variable: i64;  // ERROR: guión no permitido
let if: i64;          // ERROR: keyword reservada
```

**Keywords Reservadas:**
```
fn let mut const if else while for loop break continue return
match enum struct union type alias impl trait where
unsafe extern pub mod use as
true false
i8 i16 i32 i64 i128 u8 u16 u32 u64 u128
f32 f64 bool char str
ref move copy clone
async await yield
static atomic volatile
```

---

## 2. SISTEMA DE TIPOS

### 2.1 Tipos Primitivos

#### 2.1.1 Integers (Signed)

```chronos
let a: i8 = 127;           // -128 to 127
let b: i16 = 32767;        // -32768 to 32767
let c: i32 = 2147483647;   // -2^31 to 2^31-1
let d: i64 = 9223372036854775807;  // -2^63 to 2^63-1
let e: i128 = 0;           // -2^127 to 2^127-1

// Operaciones con overflow checking por defecto
let result: i32 = a.checked_add(b);  // Returns Option<i32>
let wrapped: i32 = a.wrapping_add(b); // Wrap around
let saturated: i32 = a.saturating_add(b); // Saturate at max
```

#### 2.1.2 Integers (Unsigned)

```chronos
let a: u8 = 255;          // 0 to 255
let b: u16 = 65535;       // 0 to 65535
let c: u32 = 4294967295;  // 0 to 2^32-1
let d: u64 = 0;           // 0 to 2^64-1
let e: u128 = 0;          // 0 to 2^128-1
```

#### 2.1.3 Floating Point

```chronos
// IEEE 754 con rounding mode explícito
let a: f32 = 3.14;
let b: f64 = 2.71828;

// Rounding modes
let rounded: f64 = (3.7).round_nearest();    // Nearest
let down: f64 = (3.7).round_toward_zero();   // Toward zero
let up: f64 = (3.7).round_toward_infinity(); // Away from zero

// ⚠️ WARNING: Floating point NO recomendado en RT paths
// Use fixed-point en su lugar
```

#### 2.1.4 Fixed-Point (RT-Safe)

```chronos
// Fixed-point decimal con precisión exacta
let price: Fix<32, 2> = Fix::from_str("19.99"); // 32-bit, 2 decimales
let quantity: Fix<16, 4> = Fix::from_int(10);   // 16-bit, 4 decimales

let total = price * quantity; // No pérdida de precisión
```

#### 2.1.5 Boolean

```chronos
let flag: bool = true;
let condition: bool = false;

// NO hay conversión automática a integer
let x: i32 = flag;  // ERROR: no implicit conversion
let y: i32 = if flag { 1 } else { 0 }; // OK: explicit
```

#### 2.1.6 Character

```chronos
// Unicode scalar value (UTF-8)
let c: char = 'a';
let emoji: char = '😀';
let escape: char = '\n';

// Encoding explícito
let ascii: Ascii = Ascii('A');      // ASCII solo
let utf8: Utf8Char = Utf8Char('ñ'); // UTF-8 explícito
```

#### 2.1.7 Unit y Never

```chronos
// Unit type (equivalente a void)
fn do_something() -> () {
    println("Done");
}

// Never type (función que nunca retorna)
fn panic(msg: &str) -> ! {
    // Diverge
    loop {}
}
```

### 2.2 Tipos Compuestos

#### 2.2.1 Arrays

```chronos
// Array de tamaño fijo (stack)
let arr: [i32; 5] = [1, 2, 3, 4, 5];
let zeros: [i64; 100] = [0; 100]; // Inicializar con valor

// Acceso bounds-checked
let first = arr[0];           // OK
let invalid = arr[10];        // PANIC: index out of bounds

// Acceso sin checking (unsafe)
unsafe {
    let fast = arr.get_unchecked(0); // Sin bounds check
}

// Tamaño conocido en compile-time
const LEN: usize = arr.len(); // Compile-time constant
```

#### 2.2.2 Slices

```chronos
// Vista a secuencia contigua
let slice: &[i32] = &arr[1..4]; // Elementos 1, 2, 3

// Fat pointer: (ptr, length)
// sizeof(&[T]) = 16 bytes en 64-bit
```

#### 2.2.3 Tuples

```chronos
// Tuplas heterogéneas
let pair: (i32, f64) = (42, 3.14);
let triple: (bool, char, &str) = (true, 'a', "hello");

// Destructuring
let (x, y) = pair;

// Acceso por índice
let first = pair.0;
let second = pair.1;
```

#### 2.2.4 Structs

```chronos
// Struct con campos nombrados
struct Point {
    x: i32,
    y: i32,
}

// Construcción
let p = Point { x: 10, y: 20 };

// Acceso
let x_val = p.x;

// Memory layout explícito
#[repr(C)]
struct Sensor {
    id: u16,
    value: i32,
}

// Packed (sin padding)
#[repr(packed)]
struct Message {
    header: u8,
    data: u32,
}

// Alignment explícito
#[repr(align(64))]  // Cache line alignment
struct CacheAligned {
    data: [u8; 64],
}
```

#### 2.2.5 Enums

```chronos
// Enum simple
enum Color {
    Red,
    Green,
    Blue,
}

// Enum con datos (tagged union)
enum Result<T, E> {
    Ok(T),
    Err(E),
}

// Enum con discriminant explícito
#[repr(u8)]
enum Status {
    Idle = 0,
    Running = 1,
    Stopped = 2,
}

// Pattern matching exhaustivo (obligatorio)
fn handle_result(r: Result<i32, &str>) -> i32 {
    match r {
        Result::Ok(val) => val,
        Result::Err(msg) => {
            println("Error: {}", msg);
            return -1;
        }
    }
}
```

#### 2.2.6 Unions

```chronos
// Union (unsafe, para FFI)
union Value {
    int_val: i32,
    float_val: f32,
}

// Acceso requiere unsafe
let v = Value { int_val: 42 };
unsafe {
    let f = v.float_val; // Puede ser UB si no fue inicializado
}
```

### 2.3 Punteros y Referencias

#### 2.3.1 Referencias Seguras

```chronos
// Immutable reference (shared)
let x: i32 = 42;
let r: &i32 = &x;

// Mutable reference (exclusive)
let mut y: i32 = 10;
let m: &mut i32 = &mut y;
*m = 20; // OK

// Reglas del borrow checker:
// 1. Una mutable reference O múltiples immutable references
// 2. Referencias no pueden outlive el owner
// 3. No dangling pointers

let r1: &i32 = &x;
let r2: &i32 = &x;  // OK: múltiples &
let m1: &mut y = &mut y;
let m2: &mut y = &mut y; // ERROR: solo una &mut
```

#### 2.3.2 Raw Pointers (Unsafe)

```chronos
// Raw pointers (para FFI, MMIO)
let x: i32 = 42;
let ptr: *const i32 = &x as *const i32;  // Immutable raw pointer
let mut_ptr: *mut i32 = &mut y as *mut i32; // Mutable raw pointer

// Dereference requiere unsafe
unsafe {
    let val = *ptr;
    *mut_ptr = 100;
}

// Arithmetic
let next = unsafe { ptr.offset(1) };
```

### 2.4 Lifetimes

```chronos
// Lifetime annotations
fn longest<'a>(x: &'a str, y: &'a str) -> &'a str {
    if x.len() > y.len() { x } else { y }
}

// Lifetime en struct
struct Parser<'a> {
    source: &'a str,
    pos: usize,
}

// Lifetime elision rules
fn first_word(s: &str) -> &str { ... }  // 'a elided
// Equivalente a:
fn first_word<'a>(s: &'a str) -> &'a str { ... }
```

### 2.5 Type Aliases

```chronos
// Type alias simple
type Kilometers = i32;

// Alias genérico
type Result<T> = Result<T, Error>;

// Opaque type (esconde implementación)
type Handle = opaque;
```

---

## 3. OWNERSHIP Y BORROWING

### 3.1 Move Semantics

```chronos
// Por defecto: MOVE
let s1: String = String::from("hello");
let s2 = s1;  // s1 se mueve a s2
// println(s1); // ERROR: value moved

// Tipos que implementan Copy: pueden copiarse
let x: i32 = 42;
let y = x;  // COPY (i32 implementa Copy)
println(x); // OK: x todavía válido
```

### 3.2 Copy vs Clone

```chronos
// Copy trait: copia bit-a-bit (barato)
#[derive(Copy, Clone)]
struct Point {
    x: i32,
    y: i32,
}

// Clone trait: copia explícita (puede ser caro)
struct Buffer {
    data: Vec<u8>,
}

impl Clone for Buffer {
    fn clone(&self) -> Self {
        Buffer { data: self.data.clone() }
    }
}

let b1 = Buffer::new();
let b2 = b1.clone(); // Explicit clone
```

### 3.3 Borrowing Rules

```chronos
fn main() {
    let mut vec = vec![1, 2, 3];

    // OK: immutable borrow
    let r1 = &vec;
    let r2 = &vec;
    println("{} {}", r1[0], r2[0]);

    // OK: r1 y r2 fuera de scope
    let m = &mut vec;
    m.push(4);

    // ERROR: no se puede tener & y &mut simultáneamente
    // let x = &vec;
    // let y = &mut vec; // ERROR
}
```

---

## 4. DETERMINISMO

### 4.1 Orden de Evaluación

```chronos
// Orden de evaluación: LEFT-TO-RIGHT, SIEMPRE
let result = f() + g() + h();
// Orden garantizado: f(), luego g(), luego h()

// Operador de secuencia explícito
let x = (init(), compute(), finalize()); // Orden garantizado
```

### 4.2 Short-Circuit Evaluation

```chronos
// && y || evalúan short-circuit (garantizado)
if expensive_check() && cheap_check() {
    // expensive_check() se evalúa primero
    // Si false, cheap_check() NO se evalúa
}

// Non-short-circuit variants
if expensive_check() & cheap_check() {
    // AMBOS se evalúan siempre
}
```

### 4.3 Arithmetic Determinístico

```chronos
// Por defecto: checked arithmetic (panic on overflow)
let a: i32 = 1000000;
let b: i32 = 1000000;
let c = a * b; // PANIC: overflow

// Variantes explícitas
let wrapped = a.wrapping_mul(b);    // Wrap around
let saturated = a.saturating_mul(b); // Saturate at i32::MAX
let checked = a.checked_mul(b);      // Returns Option<i32>

// Configuración global
#[overflow(wrap)]  // Todos los ops wrappean
fn legacy_code() {
    let c = a * b; // No panic, wrap around
}
```

---

## 5. FUNCIONES

### 5.1 Declaración de Funciones

```chronos
// Función básica
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

// Expression-based (sin return)
fn add(a: i32, b: i32) -> i32 {
    a + b  // Última expresión es el return
}

// Sin return type (retorna unit)
fn print_hello() {
    println("Hello");
}

// Never type (diverge)
fn panic_now() -> ! {
    loop {}
}
```

### 5.2 Parámetros

```chronos
// By value (move)
fn consume(s: String) {
    println(s);
} // s dropped aquí

// By immutable reference
fn borrow(s: &String) {
    println(s);
}

// By mutable reference
fn modify(s: &mut String) {
    s.push_str("!");
}

// Default arguments NO soportados (explicitness)
```

### 5.3 Generics

```chronos
// Función genérica
fn swap<T>(a: &mut T, b: &mut T) {
    let temp = *a;
    *a = *b;
    *b = temp;
}

// Trait bounds
fn print_all<T: Display>(items: &[T]) {
    for item in items {
        println(item);
    }
}

// Multiple bounds
fn process<T: Clone + Debug>(value: T) { ... }

// Where clause (para legibilidad)
fn complex<T, U>(t: T, u: U) -> i32
where
    T: Display + Clone,
    U: Debug,
{
    ...
}
```

### 5.4 Closures

```chronos
// Closure syntax
let add_one = |x: i32| x + 1;
let result = add_one(5); // 6

// Capture environment
let y = 10;
let add_y = |x| x + y; // Captura y

// Capture modes
let s = String::from("hello");
let print = || println(s);        // Borrow
let consume = move || println(s); // Move

// Closure types (traits)
// Fn: borrow immutable
// FnMut: borrow mutable
// FnOnce: consume (move)
```

---

## 6. CONTROL DE FLUJO

### 6.1 If/Else

```chronos
// If expression
let number = if condition { 5 } else { 6 };

// If statement
if x > 0 {
    println("positive");
} else if x < 0 {
    println("negative");
} else {
    println("zero");
}

// Pattern matching en if
if let Some(value) = optional {
    println(value);
}
```

### 6.2 Match

```chronos
// Match expression (exhaustivo obligatorio)
let result = match value {
    0 => "zero",
    1 | 2 => "one or two",
    3..=9 => "three to nine",
    _ => "other",
};

// Match con guards
match number {
    n if n < 0 => println("negative"),
    n if n > 0 => println("positive"),
    _ => println("zero"),
}

// Destructuring
match point {
    Point { x: 0, y: 0 } => println("origin"),
    Point { x, y: 0 } => println("on x-axis at {}", x),
    Point { x: 0, y } => println("on y-axis at {}", y),
    Point { x, y } => println("({}, {})", x, y),
}
```

### 6.3 Loops

```chronos
// Loop infinito (RT path: bounds annotation requerida)
#[max_iterations(1000)]
loop {
    if condition { break; }
}

// While loop
while condition {
    // ...
}

// For loop (range)
for i in 0..10 {
    println(i);
}

// For loop (iterator)
for item in collection {
    println(item);
}

// Labels para break/continue
'outer: loop {
    'inner: loop {
        break 'outer; // Break outer loop
    }
}
```

---

## 7. MÓDULOS Y VISIBILITY

### 7.1 Módulos

```chronos
// Definir módulo
mod network {
    pub fn connect() { ... }

    mod internal {
        pub fn helper() { ... }
    }
}

// Usar módulo
use network::connect;
use network::internal::helper;

// Re-export
pub use network::connect;
```

### 7.2 Visibility

```chronos
pub fn public_function() { ... }        // Público
fn private_function() { ... }           // Privado (default)
pub(crate) fn crate_public() { ... }   // Público en crate
pub(super) fn parent_public() { ... }  // Público en parent module
```

---

## 8. TRAITS (Interfaces)

### 8.1 Definir Traits

```chronos
trait Drawable {
    fn draw(&self);

    // Default implementation
    fn draw_twice(&self) {
        self.draw();
        self.draw();
    }
}

// Implement trait
impl Drawable for Circle {
    fn draw(&self) {
        println("Drawing circle");
    }
}
```

### 8.2 Trait Bounds

```chronos
// Trait como parámetro
fn draw_all<T: Drawable>(items: &[T]) {
    for item in items {
        item.draw();
    }
}

// Multiple traits
fn process<T: Clone + Debug>(value: T) { ... }

// Trait objects (dynamic dispatch)
fn draw_shape(shape: &dyn Drawable) {
    shape.draw();
}
```

---

## 9. ERROR HANDLING

### 9.1 Result Type

```chronos
enum Result<T, E> {
    Ok(T),
    Err(E),
}

// Función que puede fallar
fn divide(a: i32, b: i32) -> Result<i32, &str> {
    if b == 0 {
        return Err("division by zero");
    }
    Ok(a / b)
}

// Uso
match divide(10, 2) {
    Ok(result) => println("Result: {}", result),
    Err(e) => println("Error: {}", e),
}
```

### 9.2 Option Type

```chronos
enum Option<T> {
    Some(T),
    None,
}

// Función que puede no retornar valor
fn find(haystack: &[i32], needle: i32) -> Option<usize> {
    for (i, &item) in haystack.iter().enumerate() {
        if item == needle {
            return Some(i);
        }
    }
    None
}
```

### 9.3 Propagación de Errores

```chronos
// Operator ? para propagación
fn read_config() -> Result<Config, Error> {
    let file = open_file("config.txt")?;  // Early return on error
    let contents = read_contents(file)?;
    parse_config(contents)?
}
```

---

## 10. TIEMPO REAL

### 10.1 Annotations

```chronos
// WCET annotation
#[wcet(100us)]
fn process_sensor() {
    // Compiler verifica que WCET <= 100us
}

// Task annotation
#[task(priority = 10, period = 10ms, deadline = 8ms)]
fn periodic_task() {
    loop {
        process_sensor();
        wait_period();
    }
}

// Prohibir recursión
#[no_recursion]
fn real_time_path() {
    // Recursión aquí es compile error
}
```

### 10.2 Time Types

```chronos
use std::time::{Duration, Instant, Deadline};

let start = Instant::now();
let timeout = Duration::from_millis(100);
let deadline = start + timeout;

while Instant::now() < deadline {
    if try_operation() {
        break;
    }
}
```

---

## 11. UNSAFE

### 11.1 Unsafe Blocks

```chronos
// Unsafe operations
unsafe {
    let ptr = 0x40000000 as *mut u32;
    *ptr = 0xDEADBEEF; // Write to MMIO register
}

// Unsafe functions
unsafe fn dangerous() {
    // ...
}

// Caller must use unsafe
unsafe {
    dangerous();
}
```

### 11.2 Unsafe Trait

```chronos
unsafe trait Zeroable {
    // Safe para inicializar con zeros
}

unsafe impl Zeroable for u32 {
    // Garantiza que u32 puede ser all-zeros
}
```

---

## 12. MACROS

### 12.1 Declarative Macros

```chronos
macro_rules! vec {
    ( $( $x:expr ),* ) => {
        {
            let mut temp_vec = Vec::new();
            $(
                temp_vec.push($x);
            )*
            temp_vec
        }
    };
}

let v = vec![1, 2, 3];
```

### 12.2 Procedural Macros

```chronos
// Derive macro
#[derive(Debug, Clone, Copy)]
struct Point { x: i32, y: i32 }

// Attribute macro
#[route(GET, "/")]
fn index() { ... }

// Function-like macro
sql!("SELECT * FROM users WHERE id = ?", user_id);
```

---

## 13. CONCURRENCIA

### 13.1 Threads

```chronos
use std::thread;

let handle = thread::spawn(|| {
    println("Hello from thread");
});

handle.join().unwrap();
```

### 13.2 Channels

```chronos
use std::sync::mpsc;

let (tx, rx) = mpsc::channel();

thread::spawn(move || {
    tx.send(42).unwrap();
});

let received = rx.recv().unwrap();
```

### 13.3 Atomics

```chronos
use std::sync::atomic::{AtomicI64, Ordering};

let counter = AtomicI64::new(0);
counter.fetch_add(1, Ordering::SeqCst);
```

---

## 14. EJEMPLO COMPLETO

```chronos
// Real-time sensor processing system

/// Sensor data structure
#[repr(C, align(4))]
struct SensorData {
    id: u16,
    value: i32,
    timestamp: u64,
}

/// Process sensor data with WCET guarantee
#[wcet(50us)]
#[no_recursion]
fn process_sensor(data: &SensorData) -> Result<i32, &str> {
    // Validate input
    if data.id > 255 {
        return Err("invalid sensor id");
    }

    // Apply calibration (deterministic)
    let calibrated = data.value.saturating_mul(10);

    // Filter (no floating point in RT path)
    let filtered = apply_filter(calibrated);

    Ok(filtered)
}

/// Real-time task (10ms period, 8ms deadline)
#[task(priority = 10, period = 10ms, deadline = 8ms)]
fn sensor_task() -> ! {
    let mut buffer: [SensorData; 16] = [SensorData::default(); 16];

    loop {
        // Read sensor (MMIO)
        let data = unsafe { read_sensor_register() };

        // Process
        match process_sensor(&data) {
            Ok(result) => {
                // Send to output
                send_to_actuator(result);
            }
            Err(e) => {
                log_error(e);
            }
        }

        // Wait for next period
        wait_period();
    }
}

fn main() -> ! {
    // Initialize system
    init_hardware();
    init_scheduler();

    // Spawn RT task
    spawn_rt_task(sensor_task);

    // Start scheduler
    start_scheduler();
}
```

---

## 15. PRÓXIMOS PASOS

### Fase 1: Lexer/Parser (1 mes)
- Implementar lexer completo
- Parser recursive descent
- AST generation

### Fase 2: Type Checker (2 meses)
- Type inference
- Borrow checker
- Lifetime analysis

### Fase 3: Codegen (2 meses)
- Direct assembly generation
- Zero-cost abstractions
- Optimization passes

### Fase 4: RT Analysis (2 meses)
- WCET analysis
- Schedulability analysis
- Memory usage analysis

**Total:** ~7-8 meses para compiler básico funcional

---

**Firmado:** Chronos RT Language Design Team
**Versión:** 2.0.0-draft
**Estado:** Listo para implementación
