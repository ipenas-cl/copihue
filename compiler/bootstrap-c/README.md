# Chronos Bootstrap Compiler

**Version**: v0.17 (Optimizing Compiler)
**Last Updated**: 2025-10-27

---

## Directory Contents

### Main Files

- **chronos_v10** - The Chronos compiler executable (64KB)
- **chronos_v10.c** - Compiler source code in C (85KB, ~2500 lines)
- **print_int.asm** - Assembly helper for integer printing
- **include/** - Header files (lexer.h)

### Archive

- **archive/old-versions/** - Historical compiler versions (v02-v10 backups)
- **archive/experimental/** - HTTP/TCP/API experimental programs
- **archive/tests/** - Test programs and utilities

---

## Usage

### Compile a Chronos Program

```bash
# From project root
./compiler/bootstrap-c/chronos_v10 -O2 program.ch

# Output
./chronos_program
```

### Optimization Levels

- `-O0` - No optimizations (debug)
- `-O1` - Constant folding
- `-O2` - All optimizations (production)

### Rebuild Compiler

```bash
cd compiler/bootstrap-c
gcc -o chronos_v10 chronos_v10.c
```

---

## Compiler Features

**Language**: C (bootstrap compiler)
**Target**: x86-64 assembly (NASM syntax)
**Output**: Native ELF64 executables

### Optimizations

1. **Constant Folding** (-O1+)
   - Compile-time evaluation: `10 + 20` → `30`

2. **Strength Reduction** (-O2)
   - Power-of-2 multiply: `x * 4` → `shl`
   - Power-of-2 divide: `x / 8` → `sar`
   - Power-of-2 modulo: `x % 16` → `and`

### Safety Guarantees

- ✅ Bounds checking (always active)
- ✅ Division by zero checks
- ✅ Type safety
- ✅ Deterministic compilation

---

## Architecture

```
Source (.ch)
    ↓
Lexer → Tokenization
    ↓
Parser → AST
    ↓
Type Checker
    ↓
Optimizer (if -O1/-O2)
    ↓
Code Generator → Assembly (.asm)
    ↓
Assembler (nasm) → Object (.o)
    ↓
Linker (ld) → Executable
```

---

## Archive Structure

### old-versions/

Historical compiler versions preserved for reference:
- `chronos_v02` through `chronos_v09` - Evolution of the compiler
- `chronos_v10_backup*` - Development snapshots during v0.17 work
- `chronos_v10_old*` - Previous stable versions
- `chronos`, `chronos_test`, `chronos_complete.c` - Early prototypes

### experimental/

Experimental networking programs written in C:
- `api_client` - HTTP API client
- `http_api_server` - HTTP API server
- `http_api_v2`, `http_api_opt` - HTTP server variations
- `http_server`, `http_test_client` - HTTP utilities
- `tcp_client`, `tcp_server` - TCP socket programs
- `post_client` - POST request client

### tests/

Test utilities:
- `json_test` - JSON testing
- `test_json_*` - Various JSON test programs
- `test_suite` - Compiler test suite
- `test_write_key` - Write testing

---

## Development History

The Chronos compiler has evolved through multiple versions:

| Version | Features Added |
|---------|----------------|
| v0.02-v0.09 | Bootstrap development |
| v0.10 | Self-hosting capable |
| v0.11 | Array and pointer types |
| v0.12 | File I/O, array initialization |
| v0.13 | Bug fixes |
| v0.14 | Project reorganization |
| v0.15 | Modulo, logical operators, local arrays |
| v0.16 | Increment/compound operators |
| **v0.17** | **Optimizing compiler** ⭐ |

---

## Build Requirements

### System Requirements

- **GCC** - To compile the compiler
- **NASM** - To assemble generated code
- **ld** - To link executables
- **Linux x86-64** - Primary platform

### Installation (Ubuntu/Debian)

```bash
sudo apt-get install gcc nasm binutils
```

### Installation (macOS)

```bash
brew install nasm
xcode-select --install  # For gcc/ld
```

---

## Performance

### Compilation Speed

- Small programs (< 100 lines): ~50-200ms
- Medium programs (< 1000 lines): ~200-1000ms
- Large programs (1000+ lines): ~1-5s

### Generated Code Quality

- **-O0**: Straightforward, debuggable
- **-O1**: 10-20% smaller
- **-O2**: 20% smaller, 3-40x faster operations

---

## Troubleshooting

### Compiler doesn't run

```bash
chmod +x compiler/bootstrap-c/chronos_v10
```

### Compilation fails

Check that nasm and ld are installed:
```bash
which nasm ld
```

### Assembly errors

Verify NASM version:
```bash
nasm --version  # Should be 2.x or higher
```

---

## More Information

- **Documentation**: `../../docs/compiler.md`
- **Syntax Reference**: `../../docs/syntax.md`
- **Examples**: `../../examples/`
- **Tests**: `../../tests/`

---

**Chronos v0.17** - Fast, Safe, Deterministic Compiler
