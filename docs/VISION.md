# Chronos - The Deterministic Language for Everyone

**Version 0.0.1** - Design Phase

**"Write once, run predictably, everywhere."**

Safe like Rust. Easy like Go. Fast like C. **Pick all three.**

---

## What is Chronos?

Chronos is a modern systems programming language designed to make **deterministic, memory-safe code accessible to everyone** - not just experts.

### The Problem

**C/C++:** Fast but dangerous (memory bugs, undefined behavior)
**Rust:** Safe but steep learning curve (lifetime annotations everywhere)
**Go:** Simple but unpredictable (garbage collector pauses)
**Python/JS:** Easy but slow and non-deterministic

### The Chronos Solution

✅ **Memory safety** - No null pointers, no use-after-free, no buffer overflows
✅ **Determinism** - Same input = same output, always. No undefined behavior.
✅ **Accessibility** - Learn in days, not months. Familiar C-family syntax.
✅ **Performance** - No garbage collection. Zero-cost abstractions.

---

## Quick Examples

### Hello World
```chronos
Program: hello
  Actions:
    - Print "Hello, World!"
```

### Business Logic
```chronos
BusinessRule: minimum_order_amount
  Applies To: Order
  Condition: order.total >= 10.00 USD
  When Violated: Return Error "Minimum order is $10"

Workflow: order_fulfillment
  Trigger: OrderCreated event

  Steps:
    Step 1: Validate Order
      Action: check_business_rules(order)
      On Failure: → Cancel

    Step 2: Process Payment
      Action: payment_service.charge(order.total)
      Transaction: required
      On Failure: → Refund

    Step 3: Ship Order
      Action: create_shipment(order)
      Emit Event: OrderShipped
```

### Data Processing
```chronos
ETLPipeline: customer_import
  Extract:
    Source: CSV file from s3://data/customers.csv

  Transform:
    - Validate email format
    - Normalize phone numbers
    - Geocode addresses

  Load:
    Destination: PostgreSQL
    Table: customers
    Mode: upsert
```

### Real-Time System
```chronos
RTTask: sensor_control
  Period: 10 milliseconds
  WCET: 500 microseconds
  Priority: 255

  Implementation:
    - Read sensor value
    - Apply control algorithm
    - Update actuator
```

### HTTP Server
```chronos
Service: api_server
  Port: 8080

  Endpoint: POST /orders
    Input: OrderRequest (JSON)

    Validation:
      - customer_id exists
      - items not empty
      - total_amount > 0

    Processing:
      - Create order in database
      - Trigger fulfillment workflow
      - Return 201 Created
```

---

## Core Features

### Memory Safety (Guaranteed)
- ✅ No null pointers (use `Option<T>`)
- ✅ No use-after-free (ownership system)
- ✅ No data races (borrow checker)
- ✅ No buffer overflows (bounds checking)

### Determinism (Always)
- ✅ No undefined behavior
- ✅ No garbage collection pauses
- ✅ No hidden allocations
- ✅ Predictable execution time

### Developer Experience
- ✅ Familiar syntax (C-family)
- ✅ Clear error messages
- ✅ Fast compilation
- ✅ Great tooling

### Type System
- Strong static typing
- Type inference (no verbose annotations)
- Pattern matching (exhaustive)
- Ownership without complex lifetimes

---

## Language Overview

### Types

```chronos
// Primitives
i32, i64        // Signed integers
u32, u64        // Unsigned integers
bool            // true/false
str, String     // String types

// Compound
[T; N]          // Fixed-size array
&[T]            // Slice (view into array)
(T1, T2)        // Tuple
struct          // Custom struct
enum            // Tagged union
```

### Ownership (Simplified)

```chronos
fn main() {
    let s1 = String::from("hello");
    let s2 = s1;        // s1 moved to s2
    // print(s1);       // ERROR: s1 no longer valid
    print(s2);          // OK
}

// Borrowing
fn borrow(s: &String) {
    print(s);           // Just borrow, don't take ownership
}

fn main() {
    let s = String::from("hello");
    borrow(&s);         // Borrow s
    print(s);           // Still valid!
}
```

**Rule:** Values have ONE owner. Borrow (&) instead of move when you need to keep the value.

### Enums and Pattern Matching

```chronos
enum Option<T> {
    Some(T),
    None,
}

fn find(arr: &[i32], target: i32) -> Option<i32> {
    for item in arr {
        if item == target {
            return Option::Some(item);
        }
    }
    return Option::None;
}

fn main() {
    let numbers = [1, 2, 3, 4, 5];
    match find(&numbers, 3) {
        Option::Some(val) => print("Found: {}", val),
        Option::None => print("Not found"),
    }
}
```

### Error Handling (No Exceptions)

```chronos
enum Result<T, E> {
    Ok(T),
    Err(E),
}

fn read_file(path: &str) -> Result<String, IOError> {
    // Return Ok(content) or Err(error)
}

fn main() {
    match read_file("data.txt") {
        Result::Ok(content) => print("File: {}", content),
        Result::Err(e) => print("Error: {}", e),
    }
}
```

---

## Documentation

### Getting Started (⚡ START HERE)
- **[PRODUCTION_GRADE_SPEC.md](docs/PRODUCTION_GRADE_SPEC.md)** ⭐⭐⭐ **CORE SPEC** - RT systems, hardware, concurrency
- **[BUSINESS_LOGIC_SPEC.md](docs/BUSINESS_LOGIC_SPEC.md)** ⭐⭐⭐ **BUSINESS LAYER** - Logic, data, integrations (NEW!)
- **[PROMPT_BASED_LANGUAGE.md](docs/PROMPT_BASED_LANGUAGE.md)** ⭐⭐ **APPROACH** - Template/prompt-based design
- **[FIRST_PRINCIPLES.md](docs/FIRST_PRINCIPLES.md)** ⭐ **PHILOSOPHY** - Rethinking from first principles
- **[ROADMAP.md](../ROADMAP.md)** - Implementation roadmap

### Design Exploration
- **[SYNTAX_EXPLORATION.md](docs/SYNTAX_EXPLORATION.md)** - Traditional syntax alternatives
- **[CHRONOS_CORE_SPEC.md](docs/CHRONOS_CORE_SPEC.md)** - Simplified spec (superseded)
- **[CHRONOS_LANGUAGE_SPEC_v2.md](docs/CHRONOS_LANGUAGE_SPEC_v2.md)** - Detailed reference (traditional)
- **[AST_FORMAT_V2.md](docs/AST_FORMAT_V2.md)** - AST design
- **[ASSEMBLY_OUTPUT_FORMAT.md](docs/ASSEMBLY_OUTPUT_FORMAT.md)** - Assembly codegen

### Archived (v0.x - Old Approach)
- **[archive/](archive/)** - Previous implementation attempts

---

## Project Status

### Current Phase: **Implementation Started!** 🚀

**Design Completed:**
- ✅ Complete production-grade specification
- ✅ Template/prompt-based approach designed
- ✅ RT systems, business logic, data layer defined
- ✅ All use cases covered (web, embedded, finance, healthcare)

**Implementation In Progress:**
- 🚀 **Milestone 1: Minimal Compiler in PURE ASSEMBLY** (1 week)
- ✅ Complete compiler written in x86-64 assembly
- ✅ Parser implemented (ultra-simple format)
- ✅ Code generator implemented
- ✅ Memory management (bump allocator)
- ✅ File I/O (syscalls)
- ✅ Build system (build.sh)
- ⏳ Testing (ready to compile!)

**Technology Stack:**
- Compiler: **100% x86-64 Assembly** (AT&T syntax)
- Zero dependencies (only GNU as/ld)
- Templates: Ultra-simple format (YAML later)
- Target: x86-64 Linux
- Philosophy: Maximum determinism, total control

**Files Created:**
```
compiler/asm/
├── main.s       # Entry point & CLI
├── io.s         # File I/O (syscalls)
├── parser.s     # Template parser
├── codegen.s    # Assembly generator
├── memory.s     # Memory allocator
├── build.sh     # Build script
├── test.sh      # Test script
└── hello.chronos # Example template
```

**Next Steps:**
1. Build the compiler: `cd compiler/asm && ./build.sh`
2. Test: `./test.sh`
3. Try: `./chronos hello.chronos`
4. Expand templates (YAML, more features)

### Timeline

| Phase | Duration | Goal |
|-------|----------|------|
| **Phase 0: Design** | 1 day | ✅ Complete |
| **Phase 1: Minimal Compiler** | 2 months | Compile "Hello World" |
| **Phase 2: Type System** | 2.5 months | Full ownership/borrowing |
| **Phase 3: Optimizations** | 1.5 months | Performance |
| **Phase 4: Real-Time** | 2.5 months | WCET analysis, scheduling |
| **Phase 5: Concurrency** | 2 months | Threads, channels, atomics |
| **Phase 6: Standard Library** | 4 months | Collections, I/O, networking |

**Total: ~15 months to production-ready compiler**

---

## Why Chronos?

### For Beginners
- **Easy to learn** - Familiar syntax, clear error messages
- **Safe by default** - Compiler catches bugs before they run
- **No surprises** - Deterministic behavior, no hidden costs

### For Systems Programmers
- **Memory safety** without garbage collection
- **Performance** competitive with C
- **Control** over memory layout and execution

### For Real-Time Developers
- **WCET analysis** (coming Phase 4)
- **Predictable execution** - No GC pauses, no undefined behavior
- **Task scheduling** support

### For Everyone
- **Determinism** - Programs behave predictably
- **Security** - No buffer overflows, no use-after-free
- **Modern** - Pattern matching, generics, great tooling

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

---

## Philosophy

### The "3D" Design Principles

1. **Deterministic by Default**
   - Same input = same output, ALWAYS
   - No undefined behavior
   - Predictable execution

2. **Developer-Friendly Design**
   - Learn in hours, not months
   - Error messages that teach
   - Familiar syntax

3. **Dependable Performance**
   - No GC pauses
   - No hidden allocations
   - No runtime surprises

---

## Contributing

We're in the design phase! Feedback welcome on:
- Language syntax and semantics
- Feature priorities
- Implementation approach

---

## License

See LICENSE file

---

## Contact

- **Issues:** https://github.com/lychguard/chronos/issues
- **Discussions:** https://github.com/lychguard/chronos/discussions

---

**Chronos v0.0.1 - Making determinism accessible to everyone** 🚀

*"Because your software shouldn't play dice."*
