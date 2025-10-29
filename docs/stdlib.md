# Chronos Built-in Functions Reference

**Version**: v0.17
**Last Updated**: 2025-10-27

---

## Overview

Chronos provides a set of **built-in functions** that are implemented directly in the compiler via system calls. There is currently **no external standard library** - all functionality is provided as compiler built-ins.

**Important**: Unlike languages with external standard libraries (like C's libc), Chronos built-in functions are:
- Compiled directly into your program
- Implemented via direct syscalls (no libc)
- Zero external dependencies
- Always available without imports

---

## Output Functions

### `println(str: string)`

Print a string literal followed by a newline.

```chronos
fn main() -> i32 {
    println("Hello, Chronos!");
    return 0;
}
```

**Notes**:
- Only works with string literals (`"text"`)
- Automatically adds newline
- Uses `write` syscall (fd 1 = stdout)

---

### `print(str: string)`

Print a string literal without a newline.

```chronos
fn main() -> i32 {
    print("Hello, ");
    print("World!");
    println("");  // Add newline
    return 0;
}
```

**Notes**:
- Only works with string literals
- No newline added
- Useful for formatted output

---

### `print_int(n: i32)`

Print an integer value.

```chronos
fn main() -> i32 {
    let x = 42;
    print("The answer is: ");
    print_int(x);
    println("");
    return 0;
}
```

**Output**: `The answer is: 42`

**Notes**:
- Converts integer to decimal string
- No newline added
- Works with all integer types (i8, i16, i32, i64)

---

## File I/O Functions

These functions provide low-level file I/O via direct syscalls.

### `open(path: string, flags: i32) -> i32`

Open a file and return a file descriptor.

```chronos
fn main() -> i32 {
    let fd = open("input.txt", 0);  // 0 = O_RDONLY
    if (fd < 0) {
        println("Failed to open file");
        return 1;
    }
    close(fd);
    return 0;
}
```

**Common flags**:
- `0` - O_RDONLY (read-only)
- `1` - O_WRONLY (write-only)
- `2` - O_RDWR (read-write)
- `64` - O_CREAT (create if doesn't exist)
- `512` - O_TRUNC (truncate to zero length)

**Returns**: File descriptor (>= 0) on success, -1 on error

---

### `close(fd: i32) -> i32`

Close a file descriptor.

```chronos
fn main() -> i32 {
    let fd = open("file.txt", 0);
    // ... use file ...
    close(fd);
    return 0;
}
```

**Returns**: 0 on success, -1 on error

---

### `read(fd: i32, buffer: *i8, count: i32) -> i32`

Read bytes from a file descriptor into a buffer.

```chronos
fn main() -> i32 {
    let fd = open("input.txt", 0);
    let buffer: [i8; 1024];
    let bytes_read = read(fd, buffer, 1024);

    print("Read ");
    print_int(bytes_read);
    println(" bytes");

    close(fd);
    return 0;
}
```

**Parameters**:
- `fd` - File descriptor from `open()`
- `buffer` - Array to read into
- `count` - Maximum bytes to read

**Returns**: Number of bytes read, 0 on EOF, -1 on error

---

### `write(fd: i32, buffer: *i8, count: i32) -> i32`

Write bytes from a buffer to a file descriptor.

```chronos
fn main() -> i32 {
    let fd = open("output.txt", 577);  // O_WRONLY | O_CREAT | O_TRUNC
    let text = "Hello, file!";
    let bytes_written = write(fd, text, 12);

    close(fd);
    return 0;
}
```

**Parameters**:
- `fd` - File descriptor from `open()`
- `buffer` - Data to write
- `count` - Number of bytes to write

**Returns**: Number of bytes written, -1 on error

---

## System Functions

### `exit(code: i32)`

Terminate the program with an exit code.

```chronos
fn main() -> i32 {
    println("Exiting...");
    exit(0);  // Never returns
}
```

**Note**: The `return` statement in `main()` automatically calls `exit()`.

---

## String Operations

Chronos has limited built-in string operations. Most string handling is done manually.

### String Literal Indexing

You can index into string literals:

```chronos
fn main() -> i32 {
    let first_char = "Hello"[0];  // Gets 'H' (72 in ASCII)
    print_int(first_char);
    println("");
    return 0;
}
```

**Limitations**:
- Only works with string literals, not string variables
- Returns the ASCII value as i32

---

## Complete Example

Here's a complete example using multiple built-in functions:

```chronos
fn main() -> i32 {
    // Output
    println("=== Chronos Built-in Functions Demo ===");
    println("");

    // Integer output
    print("The answer is: ");
    print_int(42);
    println("");

    // File I/O
    println("Creating file...");
    let fd = open("test.txt", 577);  // O_WRONLY | O_CREAT | O_TRUNC

    if (fd < 0) {
        println("Failed to create file");
        return 1;
    }

    // Write to file
    let message = "Hello from Chronos!";
    let bytes = write(fd, message, 19);

    print("Wrote ");
    print_int(bytes);
    println(" bytes");

    close(fd);

    // Read from file
    println("Reading file...");
    let fd2 = open("test.txt", 0);

    if (fd2 < 0) {
        println("Failed to open file");
        return 1;
    }

    let buffer: [i8; 100];
    let read_bytes = read(fd2, buffer, 100);

    print("Read ");
    print_int(read_bytes);
    println(" bytes");

    close(fd2);

    println("");
    println("Done!");

    return 0;
}
```

---

## Memory Management Functions

**NEW in v0.18** - Dynamic memory allocation is now available!

### `malloc(size: i32) -> *i8`

Allocate memory dynamically on the heap.

```chronos
fn main() -> i32 {
    // Allocate 100 bytes
    let ptr = malloc(100);

    if (ptr == 0) {
        println("malloc failed");
        return 1;
    }

    // Use the memory...

    free(ptr);
    return 0;
}
```

**Implementation**: Uses `mmap` syscall with `MAP_ANONYMOUS | MAP_PRIVATE`

**Returns**:
- Pointer to allocated memory (> 0) on success
- 0 on failure

**Notes**:
- Memory is zero-initialized by the kernel
- Minimum allocation size is enforced by mmap (usually 4KB)
- Suitable for dynamic data structures

---

### `free(ptr: *i8) -> i32`

Free previously allocated memory.

```chronos
fn main() -> i32 {
    let data = malloc(1024);

    // Use data...

    free(data);  // Release memory
    return 0;
}
```

**Implementation**: Currently a placeholder (bump allocator)

**Returns**: 0 (always succeeds)

**Notes**:
- In current implementation, memory is not actually freed
- Sufficient for single-pass compilation
- Will be improved with proper free list in v0.19+

---

### Example: Dynamic Struct Allocation

```chronos
struct Token {
    type: i32,
    line: i32
}

fn main() -> i32 {
    // Allocate array of 10 tokens
    let token_size = 16;  // 2 fields * 8 bytes
    let capacity = 10;
    let tokens = malloc(capacity * token_size);

    if (tokens == 0) {
        println("Allocation failed");
        return 1;
    }

    println("Token array allocated!");

    // Use tokens...

    free(tokens);
    return 0;
}
```

---

## System Call Reference

Built-in functions map directly to Linux syscalls:

| Function | Syscall | Number (x86-64) |
|----------|---------|-----------------|
| `read(fd, buf, count)` | sys_read | 0 |
| `write(fd, buf, count)` | sys_write | 1 |
| `open(path, flags)` | sys_open | 2 |
| `close(fd)` | sys_close | 3 |
| `malloc(size)` | sys_mmap | 9 |
| `free(ptr)` | (placeholder) | - |
| `exit(code)` | sys_exit | 60 |

---

## Limitations

### What's NOT Available

Chronos currently does NOT have:

- ❌ `strlen()` - Calculate string length manually
- ❌ `strcmp()` - Compare strings manually
- ❌ `strcpy()` - Copy strings manually
- ✅ `malloc()`/`free()` - **NOW AVAILABLE!** (v0.18+)
- ❌ `printf()` - Use `print()` + `print_int()` instead
- ❌ Network functions - Implement via direct syscalls
- ❌ Math functions - Implement manually
- ❌ Date/time functions - Use syscalls directly

### Manual String Length

```chronos
fn strlen(s: string) -> i32 {
    let len = 0;
    while (s[len] != 0) {
        len++;
    }
    return len;
}
```

### Manual String Comparison

```chronos
fn strcmp(s1: string, s2: string) -> i32 {
    let i = 0;
    while (s1[i] != 0 && s2[i] != 0) {
        if (s1[i] != s2[i]) {
            return s1[i] - s2[i];
        }
        i++;
    }
    return s1[i] - s2[i];
}
```

---

## Future Standard Library

A full standard library is planned for future versions:

### v0.18+ (Planned)

- String manipulation functions
- Memory management utilities
- Common data structures

### v0.19+ (Planned)

- Networking abstractions (TCP/HTTP)
- JSON parsing and generation
- File path utilities
- Error handling utilities

### v1.0+ (Planned)

- Collections (vectors, maps)
- Async I/O
- Regular expressions
- Compression utilities

---

## See Also

- **Language Syntax**: [syntax.md](syntax.md)
- **Compiler Documentation**: [compiler.md](compiler.md)
- **Examples**: `examples/` directory

---

**Note**: The lack of a standard library is intentional for the bootstrap phase. Chronos focuses on providing a solid language foundation first, with stdlib development coming in later versions.
