# Chronos - Especificación Completa del Lenguaje Determinístico
**Versión:** 0.0.1
**Fecha:** 29 de octubre de 2025
**Estado:** Diseño completo basado en requisitos production-grade

---

## Filosofía

Chronos es un lenguaje de programación determinístico diseñado para:
- ✅ **Determinismo total** - Sin undefined behavior, sin race conditions
- ✅ **Tiempo real** - WCET garantizado, scheduling predecible
- ✅ **Memory safety** - Ownership, borrowing, sin leaks
- ✅ **Production-grade** - Para aviación, salud, finanzas, web

---

## 1. NÚCLEO DETERMINÍSTICO FUNDAMENTAL

### 1.1 Semántica del Lenguaje

#### Evaluación de Expresiones

```chronos
# Orden de evaluación SIEMPRE left-to-right (explícito)
Evaluation Order: left-to-right

# Ejemplo
let result = compute_a() + compute_b() * compute_c()
# Orden garantizado:
# 1. compute_a()
# 2. compute_b()
# 3. compute_c()
# 4. multiply
# 5. add

# Short-circuit evaluation (SIEMPRE)
if condition_a() and condition_b():
    # condition_b() NO se evalúa si condition_a() es false

# Eager vs Lazy EXPLÍCITO
let value = eager(expensive_computation())    # Evalúa inmediatamente
let value = lazy(expensive_computation())     # Evalúa cuando se usa

# NO undefined behavior
# Todas las operaciones tienen comportamiento especificado:
let x = 10 / 0          # ERROR en compile-time o panic en runtime
let y = array[100]      # Bounds check SIEMPRE (o compile error)
let z = ptr.deref()     # Null check SIEMPRE
```

#### Sistema de Tipos

```chronos
# Strong static typing sin coerciones implícitas
let x: i32 = 42
let y: i64 = x          # ERROR: no auto-conversion
let y: i64 = x as i64   # OK: conversión explícita

# Type inference sin ambigüedades
let x = 42              # Tipo inferido: i32 (default)
let x = 42i64           # Tipo inferido: i64 (sufijo explícito)
let x = [1, 2, 3]       # Tipo: [i32; 3]

# Nominal vs Structural typing (híbrido)
struct Point { x: i32, y: i32 }
struct Vector { x: i32, y: i32 }

let p: Point = Point { x: 1, y: 2 }
let v: Vector = p       # ERROR: tipos nominalmente diferentes

# Structural typing para traits
trait HasXY {
    x: i32
    y: i32
}

fn use_xy(obj: impl HasXY) {
    # Acepta cualquier tipo con campos x, y
}

# Subtyping explícito
struct Animal { name: String }
struct Dog extends Animal { breed: String }

let dog = Dog { name: "Rex", breed: "Labrador" }
let animal: Animal = dog    # OK: Dog es subtipo de Animal

# Variance annotations
trait Container<T> {
    covariant: T            # Covariant
    contravariant: in T     # Contravariant
    invariant: inout T      # Invariant
}

# Higher-kinded types
trait Functor<F<_>> {
    fn map<A, B>(fa: F<A>, f: A -> B) -> F<B>
}

# Dependent types (compile-time constraints)
fn create_array<const N: usize>(value: i32) -> [i32; N]
    requires N > 0
{
    [value; N]
}

# Refinement types (pre/post conditions)
fn divide(a: i32, b: i32) -> i32
    requires b != 0
    ensures result * b == a
{
    a / b
}

# Linear types (uso único)
struct FileHandle uses Linear {
    fd: i32
}

fn open(path: String) -> FileHandle { ... }
fn close(handle: FileHandle) { ... }  # Consume handle

let file = open("data.txt")
close(file)
# close(file)  # ERROR: file ya fue consumido

# Affine types (uso opcional único)
struct Transaction uses Affine {
    id: i64
}

# Session types (protocols)
protocol FileProtocol {
    Open -> Opened
    Opened -> (Read -> Opened | Write -> Opened | Close -> Closed)
    Closed -> End
}

# Effect types (side effects tracking)
fn pure_function(x: i32) -> i32 effect None {
    x * 2
}

fn impure_function(x: i32) -> i32 effect IO + Alloc {
    println("x = {}", x)
    let buffer = allocate(100)
    x * 2
}
```

#### Tipos Primitivos

```chronos
# Enteros con width explícito
i8, u8      # 8-bit signed/unsigned
i16, u16    # 16-bit
i32, u32    # 32-bit
i64, u64    # 64-bit
i128, u128  # 128-bit

# Floating point IEEE 754 con rounding modes
f32, f64
let x: f32 = 3.14 round nearest_even
let y: f64 = 1.0 / 3.0 round toward_zero

# Fixed-point decimal con precisión configurable
decimal<10, 2>  # 10 dígitos, 2 decimales (ej: 12345678.90)
let price: decimal<10, 2> = 99.99

# Arithmetic modes (checked por defecto)
let x: i32 = 100
let y = x + 50          # Checked arithmetic (panic on overflow)
let z = x +% 50         # Wrapping arithmetic (modulo)
let w = x +& 50         # Saturating arithmetic (clamp)

# Boolean sin conversión automática
let b: bool = true
let i: i32 = b          # ERROR: no conversión bool -> int
if i != 0 { }           # ERROR: no conversión int -> bool
if i.to_bool() { }      # OK: conversión explícita

# Character con encoding explícito
char_utf8    # UTF-8 encoded character (1-4 bytes)
char_utf16   # UTF-16 encoded
char_utf32   # UTF-32 encoded
char_ascii   # ASCII only (7-bit)

let c1: char_utf8 = 'a'
let c2: char_utf8 = '€'
let c3: char_ascii = '€'  # ERROR: fuera de rango ASCII

# Unit type (void)
()

fn do_nothing() -> () {
    # No return value
}

# Never type (divergencia)
!

fn panic() -> ! {
    abort()
}

fn infinite_loop() -> ! {
    loop { }
}
```

### 1.2 Modelo de Memoria

#### Memory Model

```chronos
# Sequential consistency por defecto
let x = 0
let y = 0

# Thread 1
x = 1
let r1 = y

# Thread 2
y = 1
let r2 = x

# Garantizado: r1 == 1 OR r2 == 1 (o ambos)
# Nunca: r1 == 0 AND r2 == 0 (violación de seq. consistency)

# Atomics con memory ordering explícito
use sync::atomic::*

let counter: AtomicI32 = AtomicI32::new(0)

# Orderings disponibles:
counter.load(Ordering::Relaxed)      # Sin sincronización
counter.load(Ordering::Acquire)      # Acquire para loads
counter.store(42, Ordering::Release) # Release para stores
counter.swap(10, Ordering::AcqRel)   # Acquire + Release
counter.load(Ordering::SeqCst)       # Sequential consistency

# Happens-before relationship
let x = 0
let y = 0

# Thread 1
x.store(1, Release)  # (A)

# Thread 2
if y.load(Acquire) == 1 {  # (B)
    assert(x.load(Relaxed) == 1)  # OK: (A) happens-before (B)
}

# Memory fences
fence(Ordering::Acquire)   # Compiler fence
fence(Ordering::Release)   # CPU fence
```

#### Layout de Memoria

```chronos
# Struct packing configurable
#[repr(C)]              # C-compatible layout
struct Point {
    x: i32,
    y: i32
}

#[repr(packed)]         # No padding
struct Packed {
    a: u8,
    b: u32  # Sin padding entre a y b
}

#[repr(align(64))]      # Cache line aligned
struct CacheAligned {
    data: [u8; 64]
}

# Field ordering definido (NO reordering)
struct Ordered {
    field1: i32,  # Offset 0
    field2: i64,  # Offset 8 (not 4, padding added)
    field3: i32   # Offset 16
}

# Tagged unions con discriminant explícito
enum Result<T, E> {
    #[discriminant = 0]
    Ok(T),
    #[discriminant = 1]
    Err(E)
}

# Zero-sized types sin overhead
struct ZeroSized { }
let x = ZeroSized { }  # sizeof(x) == 0, no allocation

# Fat pointers con layout conocido
&[i32]      # { ptr: *i32, len: usize }
&str        # { ptr: *u8, len: usize }
&dyn Trait  # { ptr: *data, vtable: *vtable }
```

#### Aliasing

```chronos
# Mutable/immutable aliasing prohibido
let mut x = 10
let r1 = &x        # Immutable borrow
let r2 = &mut x    # ERROR: ya hay borrow inmutable activo

# Interior mutability controlado
use sync::Cell

struct Counter {
    value: Cell<i32>  # Interior mutability
}

let counter = Counter { value: Cell::new(0) }
let r1 = &counter
let r2 = &counter
r1.value.set(10)   # OK: Cell permite mutación interior
r2.value.set(20)   # OK

# Pointer provenance tracking
let ptr1 = allocate(100)
let ptr2 = allocate(100)
let offset = ptr2 - ptr1  # OK: aritmética de punteros
let bad = ptr1 + offset   # ERROR: provenance violation
```

### 1.3 Control de Flujo Determinístico

```chronos
# Pattern matching exhaustivo OBLIGATORIO
enum Color {
    Red,
    Green,
    Blue
}

fn describe(color: Color) -> String {
    match color {
        Color::Red => "rojo",
        Color::Green => "verde"
        # ERROR: falta Color::Blue
    }
}

# Correcto:
fn describe(color: Color) -> String {
    match color {
        Color::Red => "rojo",
        Color::Green => "verde",
        Color::Blue => "azul"
    }
}

# Switch/match con fallthrough EXPLÍCITO
match value {
    1 => action1(),
    2 => {
        action2()
        fallthrough  # Explícito
    }
    3 => action3(),
    _ => default()
}

# Loops con bounds checking
for i in 0..10 {
    array[i]  # Bounds check en compile-time (rango conocido)
}

let limit = read_input()
for i in 0..limit {
    array[i]  # Bounds check en runtime (rango dinámico)
}

# Loop unrolling hints
#[unroll(4)]
for i in 0..100 {
    process(i)
}

# Branch prediction hints
if likely(condition) {
    hot_path()
} else {
    cold_path()
}

# Recursión prohibida en RT
#[realtime]
fn sensor_read() -> i32 {
    if condition {
        sensor_read()  # ERROR: recursión en función RT
    }
}

# Recursión acotada
#[max_recursion_depth(10)]
fn fibonacci(n: i32) -> i32 {
    if n <= 1 { return n }
    fibonacci(n-1) + fibonacci(n-2)
}

# Tail recursion garantizada
#[tail_call_optimize]
fn factorial_tail(n: i32, acc: i32) -> i32 {
    if n == 0 { return acc }
    factorial_tail(n-1, n * acc)  # Tail call optimized
}
```

---

## 2. SISTEMA DE TIEMPO REAL

### 2.1 Scheduling

```chronos
# Scheduling policies
RTTask: sensor_task
    Scheduling: FixedPriorityPreemptive  # FPPS
    Priority: 255                         # Highest
    Period: 10 milliseconds
    Deadline: 8 milliseconds

RTTask: compute_task
    Scheduling: EarliestDeadlineFirst     # EDF
    Period: 50 milliseconds
    Deadline: 40 milliseconds

# Priority inversion prevention
Mutex: resource_lock
    Protocol: PriorityCeilingProtocol
    Ceiling: 250                          # Highest priority using this mutex

# Schedulability analysis
System: flight_control
    Tasks: [sensor_task, compute_task, actuator_task]

    Analysis:
        Method: ResponseTimeAnalysis        # RTA
        UtilizationBound: Liu-Layland       # U ≤ n(2^(1/n) - 1)
        Result: Schedulable                 # ✅

# Multi-core scheduling
System: distributed_control
    Cores: 4
    Scheduling: PartitionedRMS             # Rate Monotonic per core
    Migration: disabled                    # No task migration
```

### 2.2 Timing Primitives

```chronos
# Clocks obligatorios
use time::*

let start: Instant = Instant::monotonic_now()
sleep(Duration::from_millis(10))
let elapsed: Duration = start.elapsed()

# Time types con unidades explícitas
let duration: Duration = 100.milliseconds()
let period: Period = Period::from_hz(100)  # 10ms period
let deadline: Deadline = Instant::now() + 50.microseconds()

# Timeout en blocking operations
let result: Result<Data, TimeoutError> =
    channel.receive(timeout: 1.second())

# Jitter tracking
RTTask: periodic_task
    Period: 10.milliseconds()
    Jitter: max 100.microseconds()         # Máximo jitter permitido

    Implementation:
        let start = Instant::now()
        do_work()
        let jitter = start.elapsed() - expected_duration
        assert(jitter < 100.microseconds())
```

### 2.3 WCET (Worst Case Execution Time)

```chronos
# WCET annotations
#[wcet(50.microseconds())]
fn sensor_read() -> i32 {
    read_adc_channel(0)
}

# Static analysis
#[wcet_analysis(static)]
fn control_law(input: i32) -> i32 {
    # Compiler analiza todos los paths
    # Garantiza WCET ≤ annotation
}

# Path analysis
fn compute(x: i32) -> i32 {
    if x > 100 {
        expensive_computation()  # WCET: 200us
    } else {
        cheap_computation()      # WCET: 10us
    }
    # WCET total: max(200us, 10us) = 200us
}

# Loop bounds annotations
#[wcet(1.millisecond())]
fn process_array(data: &[i32]) {
    #[loop_bound(max: 100)]
    for item in data {
        process_item(item)  # WCET: 10us
    }
    # WCET total: 100 * 10us = 1ms ✅
}

# No optimizaciones no determinísticas
#[realtime]
#[no_speculative_execution]
#[cache_locked(lines: 16)]
fn critical_section() {
    # Código con timing predecible
}
```

---

## 3. CONCURRENCIA Y PARALELISMO

### 3.1 Threading Model

```chronos
use threading::*

# OS threads
let handle = Thread::spawn({
    stack_size: 2.megabytes(),
    priority: 10,
    cpu_affinity: Core::CPU0,
    name: "worker-1"
}, || {
    do_work()
})

handle.join(timeout: 5.seconds())

# Green threads (M:N)
let runtime = GreenRuntime::new({
    os_threads: 4,
    max_green_threads: 10000
})

runtime.spawn(|| {
    lightweight_task()
})

# Thread pool
let pool = ThreadPool::new({
    workers: 8,
    queue_size: 1000,
    sizing: Fixed  # No dynamic growth
})

pool.execute(|| task())
```

### 3.2 Sincronización

```chronos
# Mutexes con priority inheritance
let mutex: Mutex<Data> = Mutex::new(data, {
    protocol: PriorityInheritance
})

let guard = mutex.lock()
# Use data
drop(guard)  # Unlock

# Timed mutex
let result = mutex.try_lock_for(100.milliseconds())
match result {
    Ok(guard) => use_data(guard),
    Err(Timeout) => handle_timeout()
}

# Semaphores
let sem = Semaphore::new({
    initial: 5,
    max: 10
})

sem.acquire()
critical_section()
sem.release()

# Barriers
let barrier = Barrier::new(num_threads: 4)

# Thread 1, 2, 3, 4
barrier.wait()  # Todos esperan aquí

# Read-Write Locks
let rwlock: RwLock<Data> = RwLock::new(data, {
    policy: WriterPreferred
})

let read_guard = rwlock.read()
let write_guard = rwlock.write()
```

### 3.3 Atomics y Lock-Free

```chronos
# Atomic types
use sync::atomic::*

let counter: AtomicI64 = AtomicI64::new(0)
counter.fetch_add(1, Ordering::SeqCst)

let flag: AtomicBool = AtomicBool::new(false)
flag.store(true, Ordering::Release)

# Compare-exchange
let old = 10
let new = 20
match counter.compare_exchange(old, new, Ordering::SeqCst, Ordering::Relaxed) {
    Ok(prev) => println("Swapped: {}", prev),
    Err(current) => println("Failed, current: {}", current)
}

# Lock-free queue
let queue: SPSCQueue<Message> = SPSCQueue::new(capacity: 1000)

# Producer
queue.push(message)

# Consumer
if let Some(msg) = queue.pop() {
    process(msg)
}

# Epoch-based reclamation (para memory management sin GC)
use sync::epoch::*

let guard = epoch::pin()
let node = guard.defer_destroy(old_node)
```

### 3.4 Message Passing

```chronos
# Bounded channels
let (tx, rx) = channel::bounded<Message>(capacity: 100)

# Sender
tx.send(message, timeout: 1.second())

# Receiver
let msg = rx.receive(timeout: 500.milliseconds())

# Select entre múltiples channels
select! {
    msg = rx1.receive() => handle1(msg),
    msg = rx2.receive() => handle2(msg),
    timeout(1.second()) => handle_timeout()
}

# Actor model
Actor: worker
    Mailbox: bounded(100)
    Supervision: one_for_one

    Messages:
        Process(data: Data)
        Stop

    Implementation:
        on Process(data):
            result = compute(data)
            sender.reply(result)

        on Stop:
            cleanup()
            exit()
```

### 3.5 Async/Await RT

```chronos
# RT async runtime
let runtime = RTRuntime::new({
    executor: SingleThreaded,
    task_queue: bounded(1000),
    priorities: true
})

# Async task con priority
#[async]
#[priority(100)]
async fn handle_request(req: Request) -> Response {
    let data = await read_database(req.id)
    let processed = await process_data(data)
    Response::new(processed)
}

# Timeout en futures
let result = timeout(1.second(), expensive_future()).await
match result {
    Ok(value) => use_value(value),
    Err(Timeout) => handle_timeout()
}

# Concurrent execution con join
let (a, b, c) = join!(
    fetch_a(),
    fetch_b(),
    fetch_c()
).await
```

---

## 4. MEMORIA

### 4.1 Allocators

```chronos
# Global allocator configurable
#[global_allocator]
static ALLOCATOR: TLSF = TLSF::new()

# RT allocator (determinístico)
let pool: FixedBlockAllocator = FixedBlockAllocator::new({
    block_size: 64.bytes(),
    num_blocks: 1000,
    alignment: 8
})

let ptr = pool.allocate()
pool.deallocate(ptr)

# Arena allocator (bump allocator)
let arena = Arena::new(capacity: 1.megabytes())

fn process_request(request: Request) {
    let scope = arena.scope()  # Todo se libera al salir
    let data = scope.allocate(100)
    # ... use data ...
}  # Scope freed automáticamente
```

### 4.2 Memory Pools

```chronos
# Typed object pool
let pool: ObjectPool<Connection> = ObjectPool::new({
    capacity: 100,
    initializer: || Connection::new()
})

let conn = pool.acquire()
use_connection(conn)
pool.release(conn)  # O automático con RAII

# Cache-aligned pool (para evitar false sharing)
#[cache_aligned]
let pool: Pool<CacheLineData> = Pool::new(100)
```

### 4.3 Stack Management

```chronos
# Stack size configurable
Thread::spawn({
    stack_size: 4.megabytes(),
    stack_guard: true  # Guard pages
}, || {
    recursive_function()
})

# Stack overflow detection
#[stack_limit(1.megabytes())]
fn deep_recursion() {
    # Panic si excede 1MB
}

# Prohibir VLAs y alloca en RT
#[realtime]
fn rt_function(n: usize) {
    let array = [0; n]  # ERROR: VLA en función RT
}
```

### 4.4 Ownership y Borrowing

```chronos
# Move semantics por defecto
let s1 = String::from("hello")
let s2 = s1  # s1 moved to s2
# println(s1)  # ERROR: s1 ya no es válido

# Copy trait explícito
#[derive(Copy)]
struct Point { x: i32, y: i32 }

let p1 = Point { x: 1, y: 2 }
let p2 = p1  # p1 copied (not moved)
println(p1)  # OK

# Borrowing
fn borrow(s: &String) {
    println(s)
}

let s = String::from("hello")
borrow(&s)  # Borrow
println(s)  # OK, aún válido

# Mutable borrow
fn modify(s: &mut String) {
    s.push_str(" world")
}

let mut s = String::from("hello")
modify(&mut s)

# Lifetimes
fn longest<'a>(s1: &'a str, s2: &'a str) -> &'a str {
    if s1.len() > s2.len() { s1 } else { s2 }
}

# Smart pointers
let boxed: Box<Data> = Box::new(data)           # Heap allocation
let rc: Rc<Data> = Rc::new(data)                # Reference counting
let arc: Arc<Data> = Arc::new(data)             # Atomic RC
```

---

## 5. HARDWARE Y BAJO NIVEL

### 5.1 Hardware Abstraction

```chronos
# Memory-mapped I/O type-safe
#[repr(C)]
#[mmio(0x4000_0000)]
struct UART {
    #[offset(0x00), read_write]
    data: u32,

    #[offset(0x04), read_only]
    status: u32,

    #[offset(0x08), write_only]
    control: u32,

    #[offset(0x0C), reserved]
    _reserved: u32
}

# Acceso volatile garantizado
let uart = UART::at(0x4000_0000)
uart.data.write(0x41)  # Volatile write
let status = uart.status.read()  # Volatile read

# Bit field manipulation type-safe
bitfield StatusRegister: u32 {
    [0]     ready: bool,
    [1]     error: bool,
    [2:4]   mode: u3,
    [5:31]  reserved: u27
}

let status = StatusRegister::from(uart.status.read())
if status.ready() {
    transmit()
}

# Interrupt handlers (ISR-safe)
#[interrupt(vector: 16, priority: 10)]
#[isr_safe]  # No allocations, bounded WCET
fn timer_interrupt() {
    flag.store(true, Ordering::Release)
}

# DMA transfers
DMATransfer: sensor_to_memory
    Source: MMIO(0x5000_0000)
    Destination: Memory(buffer)
    Size: 1024 bytes
    Mode: PeripheralToMemory
    Priority: High

    On Complete:
        signal_semaphore()
```

---

## Próximos Pasos

Este es el diseño completo del lenguaje. El compiler actual (en `/home/lychguard/Chronos/compiler/asm/`) implementa:
- ✅ Template parsing básico
- ✅ Code generation a x86-64
- ✅ Print statements

Para soportar TODAS estas características, necesitamos expandir:
1. **Parser** - Reconocer toda la sintaxis
2. **Type checker** - Validar tipos, ownership, lifetimes
3. **WCET analyzer** - Análisis estático de timing
4. **Runtime** - Scheduling, memory management, concurrency
5. **Standard library** - Implementar todos los primitivos

¿Comenzamos expandiendo el compiler step-by-step?
