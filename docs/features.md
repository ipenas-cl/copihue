# Chronos Language Features

**Version**: v0.17 (Optimizing Compiler)
**Status**: Self-Hosting Complete
**Author**: Ignacio Peña

---

## Currently Supported Features

### ✅ Data Types

- **Integers**: `i8`, `i16`, `i32`, `i64` (signed integers)
- **Pointers**: `*i32` (pointer to i32) - basic support
- **Arrays**: `[i32; 10]` (fixed-size arrays)
- **Strings**: String literals (compile-time)

**Planned for v0.18+**:
- **Structs**: User-defined structures with fields (not yet implemented)

### ✅ Variables and Declarations

```chronos
let x = 42;                  // Type inference
let y: i32 = 100;            // Explicit type
let arr: [i32; 5];           // Array declaration
let ptr: *i32 = &x;          // Pointer declaration
```

### ✅ Functions

```chronos
fn add(a: i32, b: i32) -> i32 {
    return a + b;
}

fn main() -> i32 {
    return add(10, 20);
}
```

- Function parameters
- Return values
- Recursion support
- Call expressions

### ✅ Control Flow

**If Statements:**
```chronos
if (x > 0) {
    println("positive");
} else {
    println("non-positive");
}
```

**While Loops:**
```chronos
let i = 0;
while (i < 10) {
    print_int(i);
    i = i + 1;
}
```

### ✅ Operators

**Arithmetic:**
- `+` (addition)
- `-` (subtraction)
- `*` (multiplication)
- `/` (division)
- `%` (modulo)

**Comparison:**
- `==` (equal)
- `!=` (not equal)
- `<` (less than)
- `>` (greater than)
- `<=` (less than or equal)
- `>=` (greater than or equal)

**Assignment:**
- `=` (assignment)

**Pointer:**
- `&` (address-of)
- `*` (dereference)

### ✅ Arrays

```chronos
let arr: [i32; 10];
arr[0] = 42;
arr[5] = arr[0] * 2;
let value = arr[5];
```

- Fixed-size arrays
- Array indexing
- Array element assignment

### ✅ Pointers

```chronos
let x = 100;
let ptr: *i32 = &x;    // Get address
*ptr = 200;            // Modify through pointer
let y = *ptr;          // Read through pointer
```

- Address-of operator (`&`)
- Dereference operator (`*`)
- Pointer arithmetic (basic)

### ❌ Structs (Planned for v0.18+)

**Status**: Parser recognizes struct definitions but field access is not yet implemented.

```chronos
// This syntax is planned but not yet working:
struct Point {
    x: i32,
    y: i32
}

fn main() -> i32 {
    let p: Point;  // ⚠️ Parse error
    p.x = 10;      // ⚠️ Not yet supported
    p.y = 20;
    return p.x + p.y;
}
```

**Planned features**:
- Struct definitions
- Local struct variables
- Field access and assignment
- Struct parameters and returns

See tests/test_struct*.ch for planned syntax examples.

### ✅ Built-in Functions

**Output:**
- `println(str)` - Print string with newline
- `print(str)` - Print string without newline
- `print_int(i32)` - Print integer

**String Operations:**
- `strcmp(s1, s2) -> i32` - Compare strings
- `strcpy(dest, src)` - Copy string
- `strlen(s) -> i32` - Get string length

---

## Compilation Process

### 1. Compile Source to Assembly

```bash
./compiler/bootstrap-c/chronos_v10 program.ch
```

This generates `output.asm` (NASM syntax)

### 2. Assemble to Object File

```bash
nasm -f elf64 output.asm -o output.o
```

### 3. Link to Executable

```bash
ld output.o -o program
```

### 4. Run

```bash
./program
echo $?  # Check exit code
```

---

## Example Programs

See `examples/` directory:
- `fizzbuzz.ch` - FizzBuzz implementation
- `fibonacci.ch` - Fibonacci sequence
- `primes.ch` - Prime number generator
- `bubble_sort.ch` - Sorting algorithm
- `binary_search.ch` - Search algorithm
- `gcd.ch` - Greatest common divisor

See `tests/basic/` directory for more examples.

---

## Limitations

### ❌ Not Yet Supported

- **For loops** (use while loops)
- **Dynamic memory allocation** (malloc/free)
- **Floating point** (only integers)
- **Enums**
- **Unions**
- **Type aliases**
- **Generics**
- **Traits/Interfaces**
- **Modules/Imports** (single file compilation only)
- **Standard library** (minimal built-ins only)

---

## Output Format

Chronos compiles to **x86-64 assembly** (NASM syntax) with:
- Direct syscalls (no libc dependency)
- Stack-based allocation
- System V AMD64 calling convention
- ELF64 format (Linux)

---

## Performance

- **Binary size**: 50-70% smaller than equivalent C programs
- **Compile speed**: ~50ms for small programs
- **Runtime speed**: Equivalent to C (same assembly)

See `benchmarks/BENCHMARK_REPORT.md` for detailed comparisons.

---

## Testing

Run the automated test suite:

```bash
./scripts/run_tests.sh
```

Current status: **26/28 tests passing (93%)**

---

**Author**: Ignacio Peña
**License**: MIT
**Repository**: https://github.com/ipenas-cl/Chronos
