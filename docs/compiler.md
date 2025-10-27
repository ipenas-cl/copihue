# Chronos Compiler Documentation

**Version**: v0.17 (Optimizing Compiler)
**Last Updated**: 2025-10-27

---

## Overview

The Chronos compiler (`chronos_v10`) is a bootstrap C compiler that compiles Chronos source files (`.ch`) to native x86-64 executables via assembly generation.

**Location**: `compiler/bootstrap-c/chronos_v10`

---

## Basic Usage

### Compilation

```bash
# Basic compilation (no optimizations)
./compiler/bootstrap-c/chronos_v10 program.ch

# With optimizations (recommended)
./compiler/bootstrap-c/chronos_v10 -O2 program.ch

# Run the compiled program
./chronos_program
```

### Output Files

The compiler generates:
- `chronos_program` - The executable binary
- `output.asm` - Generated x86-64 assembly (intermediate)
- `output.o` - Object file (intermediate)

---

## Optimization Levels

Chronos v0.17 includes an optimizing compiler with three optimization levels:

### -O0: No Optimizations (Debug)

```bash
./compiler/bootstrap-c/chronos_v10 -O0 program.ch
```

- **Purpose**: Fastest compilation, easiest debugging
- **Output**: Straightforward assembly matching source structure
- **Use case**: Development and debugging

### -O1: Constant Folding

```bash
./compiler/bootstrap-c/chronos_v10 -O1 program.ch
```

- **Purpose**: Basic optimizations, still readable assembly
- **Optimizations**:
  - Constant folding: `10 + 20` → `30` at compile-time
  - Evaluates constant expressions during compilation
- **Result**: ~10-20% code size reduction
- **Use case**: Development with basic optimizations

### -O2: All Optimizations (Production)

```bash
./compiler/bootstrap-c/chronos_v10 -O2 program.ch
```

- **Purpose**: Maximum performance
- **Optimizations**:
  - Constant folding (from -O1)
  - Strength reduction:
    - `x * 2` → `shl rax, 1` (3-4x faster)
    - `x / 4` → `sar rax, 2` (20-40x faster)
    - `x % 8` → `and rax, 7` (20-40x faster)
- **Result**: 20% smaller code, 3-40x speedup on optimized operations
- **Use case**: Production builds

---

## Compilation Process

### 1. Lexical Analysis

Tokenizes the source code into:
- Keywords (`fn`, `let`, `if`, `while`, etc.)
- Identifiers (variable/function names)
- Literals (numbers, strings)
- Operators (`+`, `-`, `*`, `/`, `%`, `&&`, `||`, etc.)
- Delimiters (`{`, `}`, `(`, `)`, `;`, etc.)

### 2. Parsing

Builds an Abstract Syntax Tree (AST) representing:
- Function definitions
- Variable declarations
- Expressions
- Control flow structures

### 3. Type Checking

Verifies:
- Type compatibility in expressions
- Function signature matching
- Array bounds (static where possible)

### 4. Optimization (if enabled)

- **-O1**: Evaluates constant expressions
- **-O2**: Applies strength reduction for power-of-2 operations

### 5. Code Generation

Generates x86-64 assembly:
- Direct syscalls (no libc)
- Register-based computation
- Stack-based local variables
- Efficient array indexing

### 6. Assembly and Linking

Uses system tools:
- `nasm -f elf64 output.asm -o output.o`
- `ld -o chronos_program output.o`

---

## Language Features Supported

### Data Types

- `i8` (8-bit signed integer)
- `i16` (16-bit signed integer)
- `i32` (32-bit signed integer)
- `i64` (64-bit signed integer)
- `[T; N]` (fixed-size arrays)
- String literals

### Operators

**Arithmetic**: `+`, `-`, `*`, `/`, `%` (modulo)
**Comparison**: `==`, `!=`, `<`, `>`, `<=`, `>=`
**Logical**: `&&` (and), `||` (or), `!` (not)
**Increment/Decrement**: `++`, `--`
**Compound Assignment**: `+=`, `-=`, `*=`, `/=`, `%=`

### Control Flow

- `if (condition) { }`
- `if (condition) { } else { }`
- `while (condition) { }`
- `for (init; condition; increment) { }`

### Functions

```chronos
fn function_name(param1: i32, param2: i32) -> i32 {
    // body
    return result;
}
```

### Built-in Functions

**Output**:
- `println(str)` - Print string with newline
- `print(str)` - Print string without newline
- `print_int(i)` - Print integer

**File I/O** (via syscalls):
- `open(path, flags)` - Open file
- `close(fd)` - Close file
- `read(fd, buffer, size)` - Read from file
- `write(fd, buffer, size)` - Write to file

---

## Safety Guarantees

### Always Active (All Optimization Levels)

1. **Bounds Checking**: Array accesses are always bounds-checked
2. **Division by Zero**: All divisions check for zero divisor
3. **Type Safety**: Type mismatches caught at compile-time
4. **Determinism**: Same input always produces same output
5. **No Undefined Behavior**: All operations have well-defined semantics

**Important**: Optimizations never compromise safety. Bounds checking and division-by-zero checks remain active even at -O2.

---

## Performance

### Compilation Speed

- **Small programs** (< 100 lines): ~50-200ms
- **Medium programs** (< 1000 lines): ~200-1000ms
- **Large programs** (1000+ lines): ~1-5s

### Generated Binary Size

- **Hello World**: ~1-2KB
- **Typical program**: ~5-50KB
- **Complex program**: ~50-500KB

### Runtime Performance

- **Zero overhead abstractions**: No runtime or garbage collector
- **Direct syscalls**: No libc overhead
- **Optimized code**: -O2 produces near-optimal assembly for supported patterns

---

## Compiler Architecture

### Source Code

- **File**: `compiler/bootstrap-c/chronos_v10.c`
- **Language**: C (bootstrap compiler)
- **Lines**: ~2500 lines
- **Compilation**: `gcc -o chronos_v10 chronos_v10.c`

### Implementation Details

- **Parser**: Recursive descent
- **AST**: Tree-based representation
- **Codegen**: Direct assembly generation
- **Optimization**: AST transformation + codegen patterns

---

## Platform Support

### Currently Supported

- **Linux x86-64** - Primary platform
- **macOS x86-64** - Supported (may require adjustments)

### Output Format

- **Assembly**: x86-64 NASM syntax
- **Binary**: ELF64 (Linux) or Mach-O (macOS)
- **ABI**: System V AMD64 calling convention

---

## Limitations

### Not Yet Implemented

- ❌ Structs (planned for v0.18+)
- ❌ Enums
- ❌ Pattern matching
- ❌ Generics
- ❌ Modules/imports
- ❌ Macros
- ❌ Closures
- ❌ -O3 optimization level
- ❌ Link-time optimization
- ❌ Profile-guided optimization

### Known Issues

1. **String variables**: Only string literals can be indexed, not string variables
2. **Array initialization**: Limited support for complex initializers
3. **Error messages**: Could be more detailed in some cases

---

## Examples

### Hello World

```chronos
fn main() -> i32 {
    println("¡Hola, Chronos!");
    return 0;
}
```

### Optimizations Demo

```chronos
fn main() -> i32 {
    // Constant folding (-O1+)
    let a = 10 + 20;        // Becomes: mov rax, 30

    // Strength reduction (-O2)
    let b = a * 4;          // Becomes: shl rax, 2
    let c = b / 8;          // Becomes: sar rax, 3
    let d = c % 16;         // Becomes: and rax, 15

    return d;
}
```

---

## Troubleshooting

### "Command not found"

Make sure you're using the full path:
```bash
./compiler/bootstrap-c/chronos_v10 program.ch
```

### "Parse error"

Check syntax against [syntax.md](syntax.md). Common issues:
- Missing semicolons
- Unmatched braces
- Invalid type names

### "Permission denied"

Make the compiler executable:
```bash
chmod +x compiler/bootstrap-c/chronos_v10
```

### Generated program doesn't run

Make sure nasm and ld are installed:
```bash
# Ubuntu/Debian
sudo apt-get install nasm

# macOS
brew install nasm
```

---

## Future Roadmap

### v0.18 (Next Release)

- Peephole optimization
- Dead code elimination
- Better error messages

### v0.19+

- Loop optimizations
- Function inlining
- Structs and enums

### v1.0 (Long-term)

- Self-hosting (compiler written in Chronos)
- Pattern matching
- Module system
- Standard library

---

## More Information

- **Syntax Reference**: [syntax.md](syntax.md)
- **Optimization Guide**: [optimizations.md](optimizations.md)
- **Feature List**: [features.md](features.md)
- **Examples**: `examples/` directory

---

**Chronos v0.17** - Fast, Safe, Deterministic compiler with optimizations
