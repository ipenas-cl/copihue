# Chronos - The Deterministic Language for Everyone

**Version:** 0.0.1 (Design Phase)
**Tagline:** "Write once, run predictably, everywhere."

**Date:** October 29, 2025
**Philosophy:** Determinism should be effortless, not exhausting.

**Status:** ⚠️ Syntax under review - See SYNTAX_EXPLORATION.md for alternatives

---

## What Makes Chronos Different?

### The Problem with Current Languages

**C/C++:** Fast but dangerous. Memory bugs, undefined behavior, data races.
**Rust:** Safe but steep learning curve. Lifetime annotations everywhere.
**Go:** Simple but unpredictable. Garbage collector pauses, race conditions.
**Python/JS:** Easy but slow and non-deterministic.

### The Chronos Solution

**Safe like Rust, but simpler.**
**Fast like C, but deterministic.**
**Easy like Go, but predictable.**

---

## Core Principles (The "3D" Philosophy)

### 1. Deterministic by Default
- Same input = Same output, ALWAYS
- No undefined behavior, PERIOD
- No race conditions in safe code
- Execution time is predictable

### 2. Developer-Friendly Design
- Learn in hours, not months
- Error messages that teach
- Familiar syntax (C-family)
- Great tooling from day 1

### 3. Dependable Performance
- No garbage collection pauses
- No hidden allocations
- No runtime surprises
- What you write is what executes

---

## Language Overview

### Hello World

```chronos
fn main() {
    print("Hello, World!");
}
```

### Variables (Immutable by Default)

```chronos
fn main() {
    let x = 10;          // Immutable, type inferred
    // x = 20;           // ERROR: cannot mutate

    let mut y = 10;      // Mutable
    y = 20;              // OK

    let z: i32 = 30;     // Explicit type
}
```

### Functions

```chronos
fn add(a: i32, b: i32) -> i32 {
    return a + b;        // Explicit return
}

fn multiply(a: i32, b: i32) -> i32 {
    a * b                // Implicit return (last expression)
}
```

### Ownership (Simplified)

```chronos
fn main() {
    let s1 = String::from("hello");
    let s2 = s1;         // s1 moved to s2
    // print(s1);        // ERROR: s1 no longer valid
    print(s2);           // OK
}

fn use_string(s: String) {
    print(s);
}  // s dropped here

fn borrow_string(s: &String) {
    print(s);
}  // s not dropped, just borrowed

fn main() {
    let s = String::from("hello");
    borrow_string(&s);   // Borrow (s still valid)
    print(s);            // OK
    use_string(s);       // Move (s no longer valid)
    // print(s);         // ERROR
}
```

**Rule:** Values have ONE owner. When owner goes out of scope, value is dropped.
**Borrowing:** You can lend (&) but not give away ownership.

### Structs

```chronos
struct Point {
    x: i32,
    y: i32,
}

fn main() {
    let p = Point { x: 10, y: 20 };
    print("Point: ({}, {})", p.x, p.y);
}

struct Rectangle {
    top_left: Point,
    bottom_right: Point,
}

fn area(rect: &Rectangle) -> i32 {
    let width = rect.bottom_right.x - rect.top_left.x;
    let height = rect.bottom_right.y - rect.top_left.y;
    width * height
}
```

### Enums (Tagged Unions)

```chronos
enum Result {
    Ok(i32),
    Error(String),
}

fn divide(a: i32, b: i32) -> Result {
    if b == 0 {
        return Result::Error("Division by zero");
    }
    return Result::Ok(a / b);
}

fn main() {
    let result = divide(10, 2);
    match result {
        Result::Ok(value) => print("Result: {}", value),
        Result::Error(msg) => print("Error: {}", msg),
    }
}
```

### Pattern Matching

```chronos
enum Option {
    Some(i32),
    None,
}

fn find(arr: &[i32], target: i32) -> Option {
    for item in arr {
        if item == target {
            return Option::Some(item);
        }
    }
    return Option::None;
}

fn main() {
    let numbers = [1, 2, 3, 4, 5];
    let result = find(&numbers, 3);

    match result {
        Option::Some(val) => print("Found: {}", val),
        Option::None => print("Not found"),
    }

    // Or use if-let for single case
    if let Option::Some(val) = result {
        print("Found: {}", val);
    }
}
```

### Arrays and Slices

```chronos
fn main() {
    let arr: [i32; 5] = [1, 2, 3, 4, 5];  // Fixed-size array
    let slice: &[i32] = &arr[1..3];        // Slice (view into array)

    print("Array length: {}", arr.len());
    print("Slice: {:?}", slice);           // [2, 3]
}

fn sum(numbers: &[i32]) -> i32 {
    let mut total = 0;
    for num in numbers {
        total = total + num;
    }
    total
}
```

### Loops

```chronos
fn main() {
    // Infinite loop
    loop {
        print("Forever!");
        break;  // Exit loop
    }

    // While loop
    let mut i = 0;
    while i < 10 {
        print("{}", i);
        i = i + 1;
    }

    // For loop (range)
    for i in 0..10 {
        print("{}", i);
    }

    // For loop (array)
    let numbers = [1, 2, 3, 4, 5];
    for num in &numbers {
        print("{}", num);
    }
}
```

---

## Type System (Minimal but Complete)

### Primitive Types

```chronos
// Integers (signed)
i32     // 32-bit signed integer (default)
i64     // 64-bit signed integer

// Integers (unsigned)
u32     // 32-bit unsigned integer
u64     // 64-bit unsigned integer

// Boolean
bool    // true or false

// String
str     // String slice (immutable view)
String  // Owned string (heap-allocated)
```

**Note:** We start with just these. More types (i8, i16, f32, f64) can be added later.

### Compound Types

```chronos
[T; N]      // Array (fixed size N)
&[T]        // Slice (dynamic view)
(T1, T2)    // Tuple
struct      // Custom struct
enum        // Tagged union
```

### References

```chronos
&T          // Immutable reference (borrow)
&mut T      // Mutable reference (exclusive borrow)
```

**Rules:**
- Either ONE &mut T OR many &T (not both)
- References must always be valid (no dangling)

---

## Memory Safety (Guaranteed)

### No Null Pointers

```chronos
// Instead of null, use Option
enum Option<T> {
    Some(T),
    None,
}

fn find_user(id: i32) -> Option<User> {
    // Return Some(user) or None
}
```

### No Use-After-Free

```chronos
fn main() {
    let s = String::from("hello");
    let r = &s;
    drop(s);        // ERROR: cannot drop while borrowed
    print(r);
}
```

### No Data Races

```chronos
// Compiler enforces: no shared mutable state
// Either shared OR mutable, NOT both
```

### No Buffer Overflows

```chronos
fn main() {
    let arr = [1, 2, 3];
    let x = arr[5];     // PANIC: index out of bounds (detected at runtime)
}
```

---

## Error Handling (No Exceptions)

### Result Type

```chronos
enum Result<T, E> {
    Ok(T),
    Err(E),
}

fn read_file(path: &str) -> Result<String, IOError> {
    // Return Ok(content) or Err(error)
}

fn main() {
    let content = read_file("data.txt");
    match content {
        Result::Ok(data) => print("File: {}", data),
        Result::Err(e) => print("Error: {}", e),
    }
}
```

### The `?` Operator (Early Return)

```chronos
fn process() -> Result<i32, String> {
    let file = read_file("data.txt")?;  // If error, return early
    let num = parse_int(&file)?;
    return Result::Ok(num * 2);
}
```

---

## Determinism Features

### No Undefined Behavior

```chronos
// All operations have defined behavior
let x = 1000000;
let y = 1000000;
let z = x * y;      // Overflow: PANIC or wrap (explicit choice)
```

### Explicit Overflow Handling

```chronos
fn main() {
    let a: i32 = 100;
    let b: i32 = 200;

    // Default: panic on overflow (debug mode)
    let c = a * b;

    // Explicit wrapping
    let d = a.wrapping_mul(b);

    // Checked (returns Option)
    match a.checked_mul(b) {
        Option::Some(val) => print("Result: {}", val),
        Option::None => print("Overflow!"),
    }
}
```

### No Hidden Allocations

```chronos
// All allocations are explicit
let s = String::from("hello");   // Heap allocation (explicit)
let arr = [1, 2, 3];             // Stack allocation
```

### Predictable Execution

```chronos
// No garbage collector pauses
// No dynamic dispatch (unless explicit)
// No virtual function overhead (unless trait objects)
```

---

## Standard Library (Minimal)

### Core Types

```chronos
Option<T>       // Optional value
Result<T, E>    // Result with error
Vec<T>          // Growable array (heap)
String          // Owned string (heap)
```

### Collections

```chronos
Vec<T>          // Dynamic array
HashMap<K, V>   // Hash table (coming soon)
```

### I/O

```chronos
print()         // Print to stdout
read_line()     // Read from stdin
read_file()     // Read file
write_file()    // Write file
```

---

## Concurrency (Phase 2)

### Threads

```chronos
fn worker() {
    print("Worker thread");
}

fn main() {
    let thread = spawn(worker);
    thread.join();
}
```

### Channels (Message Passing)

```chronos
fn main() {
    let (sender, receiver) = channel();

    spawn(move || {
        sender.send(42);
    });

    let value = receiver.recv();
    print("Received: {}", value);
}
```

**Key:** No shared mutable state. Use message passing instead.

---

## Real-Time Extensions (Phase 3)

### WCET Annotations

```chronos
#[wcet(100us)]  // Worst-Case Execution Time
fn sensor_read() -> i32 {
    // Compiler verifies this completes in <= 100us
}
```

### Task Scheduling

```chronos
#[task(priority = 10, period = 10ms)]
fn control_loop() {
    // Real-time task
}
```

---

## Syntax Design Rationale

### Why C-Family Syntax?

**Familiar:** Most programmers know C, Java, JavaScript, Go
**Readable:** Clear and concise
**Proven:** Decades of use in industry

### Differences from Rust

**Simpler:** No explicit lifetimes in most cases (inferred)
**Clearer:** Less syntax noise
**Easier:** Shorter learning curve

### Differences from C

**Safer:** No undefined behavior
**Modern:** Pattern matching, enums, Option/Result
**Better:** No null pointers, no manual memory management

---

## Compiler Design

### Single-Pass Compilation

- Fast compile times
- Minimal memory usage
- Predictable build times

### Direct Assembly Generation

- No LLVM dependency
- Full control over output
- Deterministic code generation

### Zero-Cost Abstractions

- High-level features with no runtime overhead
- Enums compile to simple tags
- Pattern matching compiles to jumps
- Generics compile to specialized code (monomorphization)

---

## Tooling

### Compiler

```bash
chronos build main.ch        # Build executable
chronos run main.ch          # Build and run
chronos check main.ch        # Type check only (fast)
```

### Package Manager (Phase 2)

```bash
chronos new my_project       # Create new project
chronos add package          # Add dependency
chronos update               # Update dependencies
```

### Language Server (Phase 2)

- IDE integration
- Auto-completion
- Error checking
- Refactoring

---

## Example Programs

### Fibonacci (Recursive)

```chronos
fn fib(n: i32) -> i32 {
    if n <= 1 {
        return n;
    }
    return fib(n - 1) + fib(n - 2);
}

fn main() {
    print("fib(10) = {}", fib(10));
}
```

### Fibonacci (Iterative)

```chronos
fn fib(n: i32) -> i32 {
    if n <= 1 {
        return n;
    }

    let mut a = 0;
    let mut b = 1;
    let mut i = 2;

    while i <= n {
        let temp = a + b;
        a = b;
        b = temp;
        i = i + 1;
    }

    return b;
}
```

### Linked List

```chronos
enum List {
    Cons(i32, Box<List>),
    Nil,
}

fn sum(list: &List) -> i32 {
    match list {
        List::Cons(value, rest) => value + sum(rest),
        List::Nil => 0,
    }
}

fn main() {
    let list = List::Cons(1,
               Box::new(List::Cons(2,
               Box::new(List::Cons(3,
               Box::new(List::Nil))))));

    print("Sum: {}", sum(&list));
}
```

### Binary Search

```chronos
fn binary_search(arr: &[i32], target: i32) -> Option<i32> {
    let mut left = 0;
    let mut right = arr.len() - 1;

    while left <= right {
        let mid = (left + right) / 2;
        let val = arr[mid];

        if val == target {
            return Option::Some(mid);
        } else if val < target {
            left = mid + 1;
        } else {
            right = mid - 1;
        }
    }

    return Option::None;
}
```

---

## Feature Comparison

| Feature | C | Rust | Go | Chronos |
|---------|---|------|-----|---------|
| Memory Safety | ❌ | ✅ | Partial (GC) | ✅ |
| No GC | ✅ | ✅ | ❌ | ✅ |
| Deterministic | ❌ | ✅ | ❌ | ✅ |
| Easy to Learn | ✅ | ❌ | ✅ | ✅ |
| Fast Compile | ✅ | ❌ | ✅ | ✅ |
| Pattern Matching | ❌ | ✅ | Partial | ✅ |
| Null Safety | ❌ | ✅ | ❌ | ✅ |
| Concurrency | ❌ | ✅ | ✅ | ✅ (Phase 2) |

---

## Learning Path

### Day 1: Basics
- Variables, functions, types
- Control flow (if, while, for)
- Arrays and slices

### Day 2: Ownership
- Move semantics
- Borrowing
- Lifetimes (automatic)

### Day 3: Structs and Enums
- Custom types
- Pattern matching
- Option and Result

### Day 4: Error Handling
- Result type
- The `?` operator
- Panic vs Result

### Day 5: Collections
- Vec, String
- Iterators
- Common patterns

**After 1 week:** Ready to write real programs

---

## Implementation Plan

### Phase 1: Minimal Compiler (2 months)
- Lexer, Parser, Type checker
- Code generator (x86-64 assembly)
- Ownership checking (basic)
- Compile "Hello World"

### Phase 2: Complete Language (3 months)
- Full ownership system
- Pattern matching
- Generics (basic)
- Standard library (core)

### Phase 3: Performance (2 months)
- Optimizations
- Faster compilation
- Better code generation

### Phase 4: Real-Time (3 months)
- WCET analysis
- Task scheduling
- Determinism verification

### Phase 5: Concurrency (2 months)
- Threads
- Channels
- Atomics

**Total: 12 months to production-ready compiler**

---

## Why Chronos Will Succeed

### 1. Solves Real Problems
- Memory bugs cost billions annually
- Undefined behavior causes security vulnerabilities
- Non-determinism makes debugging hell

### 2. Easy to Learn
- Familiar syntax
- Great error messages
- Short learning curve (days, not months)

### 3. Proven Concepts
- Ownership (Rust proved this works)
- Pattern matching (loved by everyone)
- No GC (C/C++ developers want this)

### 4. Modern Tooling
- Fast compiler
- IDE integration
- Package manager

### 5. Clear Value Proposition
- "Safe like Rust, easy like Go, fast like C"
- Determinism by default
- No surprises

---

## Marketing Taglines

- **"Write once, run predictably, everywhere."**
- **"The language that doesn't surprise you."**
- **"Safe, fast, deterministic. Pick all three."**
- **"Memory safety without the complexity."**
- **"Because your software shouldn't play dice."**

---

## Next Steps

1. ✅ Finalize language specification
2. Implement minimal compiler (Phase 1)
3. Write tutorial and documentation
4. Build community
5. Release v1.0

---

**Chronos: Making determinism accessible to everyone.**

*No undefined behavior. No surprises. Just predictable, safe code.*
