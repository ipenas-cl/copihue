# Chronos Compiler - Assembly x86-64 Puro

**Versión:** 0.0.1
**Fecha:** 29 de octubre de 2025
**Approach:** Compiler escrito en Assembly x86-64 desde día 1

---

## ¿Por qué Assembly Puro?

### Ventajas Absolutas

1. **Máximo Determinismo**
   - Cada instrucción es explícita
   - No abstracciones ocultas
   - No runtime inesperado
   - Comportamiento 100% predecible

2. **Cero Dependencias**
   - No compiladores de otros lenguajes
   - No librerías externas
   - Solo el assembler (as) y linker (ld)
   - Luego ni siquiera esos (assembler propio)

3. **Control Total**
   - Cada byte en memoria bajo control
   - Cada syscall visible
   - Cada registro asignado manualmente
   - Performance óptimo

4. **Bootstrapping Real**
   - El compiler se puede compilar a sí mismo
   - Verdadero self-hosting
   - Verificación perfecta

5. **Alineado con Filosofía**
   - Un lenguaje determinista necesita compiler determinista
   - Predicar con el ejemplo
   - Demostrar que es posible

---

## Arquitectura del Compiler

### Pipeline Simplificado

```
Template File (.chronos)
    ↓
[Lexer] (assembly)
    ↓
Token Stream
    ↓
[Parser] (assembly)
    ↓
AST (en memoria)
    ↓
[Validator] (assembly)
    ↓
Validated AST
    ↓
[Code Generator] (assembly)
    ↓
Assembly Output (.s)
```

### Componentes en Assembly

```
compiler/asm/
├── main.s              # Entry point, CLI
├── lexer.s             # Tokenización
├── parser.s            # Parsing YAML-like
├── validator.s         # Validación de templates
├── codegen.s           # Generación de código
├── memory.s            # Gestión de memoria
├── io.s                # File I/O, stdout
├── string.s            # String utilities
└── syscalls.s          # Wrappers de syscalls
```

---

## Milestone 1: Ultra-Minimal (1 Semana)

**Objetivo:** Leer archivo, detectar template "Program", generar "Hello World"

### Input (hello.chronos)
```
Program hello
  Print "Hello, World!"
```

**Sintaxis ultra-simple (no YAML por ahora):**
- Una línea por declaración
- Keywords reconocibles
- String literals entre comillas

### Output (hello.s)
```asm
.global _start
_start:
    movq $1, %rax
    movq $1, %rdi
    leaq msg(%rip), %rsi
    movq $14, %rdx
    syscall
    movq $60, %rax
    xorq %rdi, %rdi
    syscall
.data
msg:
    .ascii "Hello, World!\n"
```

### Componentes Mínimos

**1. main.s - Entry Point**
```asm
# Chronos Compiler v0.0.1
# Written in pure x86-64 assembly

.text
.global _start

_start:
    # Check command line arguments
    # argc in (%rsp)
    # argv[0] in 8(%rsp)
    # argv[1] in 16(%rsp) - input file

    popq %rdi               # argc
    cmpq $2, %rdi
    jl .usage_error

    popq %rax               # argv[0] (skip)
    popq %rdi               # argv[1] - input filename

    # Open input file
    call open_file          # Returns fd in %rax
    movq %rax, %r12         # Save fd

    # Read entire file into buffer
    movq %r12, %rdi
    call read_file          # Returns buffer ptr in %rax, size in %rdx
    movq %rax, %r13         # Save buffer
    movq %rdx, %r14         # Save size

    # Parse template
    movq %r13, %rdi         # buffer
    movq %r14, %rsi         # size
    call parse_template     # Returns AST ptr in %rax
    movq %rax, %r15         # Save AST

    # Generate code
    movq %r15, %rdi         # AST
    call generate_code      # Returns output buffer in %rax
    movq %rax, %rbx         # Save output

    # Write output file
    leaq output_name(%rip), %rdi
    movq %rbx, %rsi
    call write_file

    # Exit success
    movq $60, %rax          # syscall: exit
    xorq %rdi, %rdi         # status: 0
    syscall

.usage_error:
    # Print usage and exit
    movq $1, %rax           # syscall: write
    movq $2, %rdi           # stderr
    leaq usage_msg(%rip), %rsi
    movq $usage_len, %rdx
    syscall

    movq $60, %rax          # syscall: exit
    movq $1, %rdi           # status: 1
    syscall

.data
usage_msg:
    .ascii "Usage: chronos <input.chronos>\n"
usage_len = . - usage_msg

output_name:
    .ascii "output.s\0"
```

**2. io.s - File I/O**
```asm
.text

# open_file(filename: *char) -> fd: i64
.global open_file
open_file:
    # rdi = filename
    movq $2, %rax           # syscall: open
    # rdi already has filename
    movq $0, %rsi           # flags: O_RDONLY
    xorq %rdx, %rdx         # mode: 0
    syscall

    # Check for error
    cmpq $0, %rax
    jl .open_error
    ret

.open_error:
    # Print error and exit
    movq $1, %rax
    movq $2, %rdi
    leaq open_err_msg(%rip), %rsi
    movq $open_err_len, %rdx
    syscall

    movq $60, %rax
    movq $1, %rdi
    syscall

# read_file(fd: i64) -> (buffer: *u8, size: u64)
.global read_file
read_file:
    pushq %rbp
    movq %rsp, %rbp

    # Save fd
    pushq %rdi              # fd on stack

    # Get file size with fstat
    movq %rdi, %rdi         # fd
    leaq stat_buf(%rip), %rsi
    movq $5, %rax           # syscall: fstat
    syscall

    # File size is at offset 48 in stat struct
    movq stat_buf+48(%rip), %r12  # file size

    # Allocate buffer (using mmap)
    xorq %rdi, %rdi         # addr: NULL (let kernel choose)
    movq %r12, %rsi         # length: file size
    movq $3, %rdx           # prot: PROT_READ | PROT_WRITE
    movq $34, %r10          # flags: MAP_PRIVATE | MAP_ANONYMOUS
    movq $-1, %r8           # fd: -1 (anonymous)
    xorq %r9, %r9           # offset: 0
    movq $9, %rax           # syscall: mmap
    syscall

    movq %rax, %r13         # Save buffer address

    # Read file into buffer
    popq %rdi               # fd
    movq $0, %rax           # syscall: read
    movq %r13, %rsi         # buffer
    movq %r12, %rdx         # count
    syscall

    # Return buffer and size
    movq %r13, %rax         # buffer
    movq %r12, %rdx         # size

    popq %rbp
    ret

# write_file(filename: *char, buffer: *u8, size: u64)
.global write_file
write_file:
    pushq %rbp
    movq %rsp, %rbp

    # Save args
    pushq %rsi              # buffer
    pushq %rdx              # size

    # Open file for writing
    # rdi already has filename
    movq $2, %rax           # syscall: open
    movq $577, %rsi         # flags: O_WRONLY | O_CREAT | O_TRUNC
    movq $0644, %rdx        # mode: rw-r--r--
    syscall

    movq %rax, %r12         # Save fd

    # Write buffer
    movq %r12, %rdi         # fd
    popq %rdx               # size
    popq %rsi               # buffer
    movq $1, %rax           # syscall: write
    syscall

    # Close file
    movq %r12, %rdi
    movq $3, %rax           # syscall: close
    syscall

    popq %rbp
    ret

.data
open_err_msg:
    .ascii "Error: Cannot open file\n"
open_err_len = . - open_err_msg

.bss
stat_buf:
    .skip 144               # sizeof(struct stat)
```

**3. lexer.s - Simple Tokenizer**
```asm
.text

# Tokens (ultra-simple)
TOKEN_PROGRAM = 1
TOKEN_PRINT = 2
TOKEN_STRING = 3
TOKEN_IDENTIFIER = 4
TOKEN_NEWLINE = 5
TOKEN_EOF = 6

# parse_template(buffer: *u8, size: u64) -> ast: *AST
.global parse_template
parse_template:
    pushq %rbp
    movq %rsp, %rbp

    # Save buffer and size
    movq %rdi, %r12         # buffer
    movq %rsi, %r13         # size
    xorq %r14, %r14         # current position

    # Allocate AST structure
    call allocate_ast       # Returns AST ptr in %rax
    movq %rax, %r15         # Save AST

.parse_loop:
    # Check if end of file
    cmpq %r14, %r13
    jge .parse_done

    # Get current character
    movzbq (%r12, %r14), %al

    # Skip whitespace
    cmpb $' ', %al
    je .skip_char
    cmpb $'\t', %al
    je .skip_char
    cmpb $'\n', %al
    je .skip_char

    # Check for "Program"
    leaq program_kw(%rip), %rsi
    movq %r12, %rdi
    addq %r14, %rdi
    call match_keyword
    cmpq $0, %rax
    jne .found_program

    # Check for "Print"
    leaq print_kw(%rip), %rsi
    movq %r12, %rdi
    addq %r14, %rdi
    call match_keyword
    cmpq $0, %rax
    jne .found_print

    # Unknown token
    jmp .parse_error

.skip_char:
    incq %r14
    jmp .parse_loop

.found_program:
    # Mark AST as Program type
    movq $1, (%r15)         # AST.type = PROGRAM
    addq %rax, %r14         # Skip "Program"

    # Read program name (next word)
    call skip_whitespace
    call read_identifier    # Returns name ptr in %rax
    movq %rax, 8(%r15)      # AST.name = name

    jmp .parse_loop

.found_print:
    # Add Print action to AST
    addq %rax, %r14         # Skip "Print"
    call skip_whitespace
    call read_string        # Returns string ptr in %rax

    # Store in AST actions
    movq 16(%r15), %rbx     # AST.action_count
    imulq $16, %rbx         # offset = count * 16
    addq $24, %rbx          # offset from AST base
    movq %rax, (%r15, %rbx) # Store action
    incq 16(%r15)           # Increment action_count

    jmp .parse_loop

.parse_done:
    movq %r15, %rax         # Return AST
    popq %rbp
    ret

.parse_error:
    # Print error and exit
    movq $1, %rax
    movq $2, %rdi
    leaq parse_err_msg(%rip), %rsi
    movq $parse_err_len, %rdx
    syscall

    movq $60, %rax
    movq $1, %rdi
    syscall

# match_keyword(text: *char, keyword: *char) -> matched_len: u64
match_keyword:
    pushq %rbp
    movq %rsp, %rbp

    xorq %rcx, %rcx         # counter
.match_loop:
    movzbq (%rsi, %rcx), %al    # keyword char
    cmpb $0, %al
    je .match_success

    movzbq (%rdi, %rcx), %bl    # text char
    cmpb %al, %bl
    jne .match_fail

    incq %rcx
    jmp .match_loop

.match_success:
    movq %rcx, %rax         # Return matched length
    popq %rbp
    ret

.match_fail:
    xorq %rax, %rax         # Return 0
    popq %rbp
    ret

# read_string() -> string: *char
# Reads a string literal between quotes
read_string:
    # Find opening quote
    movzbq (%r12, %r14), %al
    cmpb $'"', %al
    jne .string_error

    incq %r14               # Skip opening quote
    movq %r14, %rbx         # Save start position

    # Find closing quote
.string_loop:
    cmpq %r14, %r13
    jge .string_error

    movzbq (%r12, %r14), %al
    cmpb $'"', %al
    je .string_found

    incq %r14
    jmp .string_loop

.string_found:
    # Calculate string length
    movq %r14, %rcx
    subq %rbx, %rcx         # length = end - start

    # Allocate string buffer
    # (simplified: use static buffer for now)
    leaq string_buffer(%rip), %rax

    # Copy string
    movq %rbx, %rsi
    addq %r12, %rsi         # source
    movq %rax, %rdi         # dest
    movq %rcx, %rdx         # count
    call memcpy

    # Null-terminate
    movb $0, (%rax, %rcx)

    incq %r14               # Skip closing quote
    ret

.string_error:
    movq $1, %rax
    movq $2, %rdi
    leaq string_err_msg(%rip), %rsi
    movq $string_err_len, %rdx
    syscall
    movq $60, %rax
    movq $1, %rdi
    syscall

skip_whitespace:
    cmpq %r14, %r13
    jge .skip_done

    movzbq (%r12, %r14), %al
    cmpb $' ', %al
    je .skip_inc
    cmpb $'\t', %al
    je .skip_inc
    jmp .skip_done

.skip_inc:
    incq %r14
    jmp skip_whitespace

.skip_done:
    ret

# Simple memcpy
memcpy:
    pushq %rcx
.memcpy_loop:
    cmpq $0, %rdx
    je .memcpy_done

    movb (%rsi), %al
    movb %al, (%rdi)

    incq %rsi
    incq %rdi
    decq %rdx
    jmp .memcpy_loop

.memcpy_done:
    popq %rcx
    ret

.data
program_kw:
    .ascii "Program\0"
print_kw:
    .ascii "Print\0"

parse_err_msg:
    .ascii "Parse error\n"
parse_err_len = . - parse_err_msg

string_err_msg:
    .ascii "String parse error\n"
string_err_len = . - string_err_msg

.bss
string_buffer:
    .skip 1024
```

**4. codegen.s - Code Generator**
```asm
.text

# generate_code(ast: *AST) -> output: *char
.global generate_code
generate_code:
    pushq %rbp
    movq %rsp, %rbp

    # Save AST
    movq %rdi, %r12

    # Allocate output buffer
    movq $4096, %rdi        # 4KB output buffer
    call allocate_buffer
    movq %rax, %r13         # Save output buffer

    # Check AST type
    movq (%r12), %rax
    cmpq $1, %rax           # PROGRAM?
    je .gen_program

    # Unknown type
    jmp .gen_error

.gen_program:
    # Generate program code
    # 1. Emit header
    leaq gen_header(%rip), %rsi
    movq %r13, %rdi
    call append_string

    # 2. Emit _start
    leaq gen_start(%rip), %rsi
    call append_string

    # 3. Generate each action
    movq 16(%r12), %r14     # action_count
    xorq %r15, %r15         # current action index

.gen_actions:
    cmpq %r15, %r14
    jle .gen_footer

    # Get action string
    imulq $16, %r15
    addq $24, %r15          # offset
    movq (%r12, %r15), %rbx # action string

    # Generate Print syscall
    call generate_print

    incq %r15
    divq %r15, $16
    jmp .gen_actions

.gen_footer:
    # Emit exit syscall
    leaq gen_exit(%rip), %rsi
    call append_string

    # Return output buffer
    movq %r13, %rax
    popq %rbp
    ret

.gen_error:
    movq $1, %rax
    movq $2, %rdi
    leaq gen_err_msg(%rip), %rsi
    movq $gen_err_len, %rdx
    syscall
    movq $60, %rax
    movq $1, %rdi
    syscall

# generate_print(output_buf: *char, string: *char)
generate_print:
    pushq %rbp
    movq %rsp, %rbp

    # Generate label for string
    call gen_unique_label   # Returns label in %rax
    movq %rax, %r10

    # Emit write syscall code
    leaq print_tmpl1(%rip), %rsi
    call append_string

    # Emit label reference
    movq %r10, %rsi
    call append_string

    leaq print_tmpl2(%rip), %rsi
    call append_string

    # Calculate string length
    movq %rbx, %rdi
    call strlen
    movq %rax, %r11         # Save length

    # Emit length
    movq %r11, %rdi
    call int_to_string
    movq %rax, %rsi
    call append_string

    leaq print_tmpl3(%rip), %rsi
    call append_string

    # Later: emit .data section with label and string

    popq %rbp
    ret

.data
gen_header:
    .ascii ".text\n.global _start\n\n_start:\n"
    .byte 0

gen_start:
    .ascii "    # Chronos generated code\n\n"
    .byte 0

gen_exit:
    .ascii "\n    # Exit\n"
    .ascii "    movq $60, %rax\n"
    .ascii "    xorq %rdi, %rdi\n"
    .ascii "    syscall\n"
    .byte 0

print_tmpl1:
    .ascii "    # Print\n"
    .ascii "    movq $1, %rax\n"
    .ascii "    movq $1, %rdi\n"
    .ascii "    leaq "
    .byte 0

print_tmpl2:
    .ascii "(%rip), %rsi\n"
    .ascii "    movq $"
    .byte 0

print_tmpl3:
    .ascii ", %rdx\n"
    .ascii "    syscall\n\n"
    .byte 0

gen_err_msg:
    .ascii "Codegen error\n"
gen_err_len = . - gen_err_msg
```

**5. memory.s - Memory Management**
```asm
.text

# Simple bump allocator for AST/buffers

.bss
.align 8
heap_start:
    .skip 1048576           # 1MB heap

.data
heap_current:
    .quad heap_start

# allocate_buffer(size: u64) -> ptr: *u8
.global allocate_buffer
allocate_buffer:
    movq heap_current(%rip), %rax
    addq %rdi, heap_current(%rip)
    ret

# allocate_ast() -> ast: *AST
.global allocate_ast
allocate_ast:
    movq $1024, %rdi        # AST size
    call allocate_buffer
    ret
```

---

## Build Script

**build.sh:**
```bash
#!/bin/bash

# Assemble all modules
as -o main.o main.s
as -o io.o io.s
as -o lexer.o lexer.s
as -o codegen.o codegen.s
as -o memory.o memory.s

# Link
ld -o chronos main.o io.o lexer.o codegen.o memory.o

# Cleanup
rm -f *.o

echo "✓ Chronos compiler built"
```

---

## Test

**test.sh:**
```bash
#!/bin/bash

# Create test template
cat > test.chronos << 'EOF'
Program hello
  Print "Hello from Chronos!"
EOF

# Compile template
./chronos test.chronos

# Assemble generated code
as -o output.o output.s
ld -o hello output.o

# Run
./hello

# Cleanup
rm -f test.chronos output.s output.o
```

---

## Milestone 1 Entregables

Al final de 1 semana:

✅ **Compiler en Assembly puro**
- main.s (CLI)
- io.s (File I/O)
- lexer.s (Parsing ultra-simple)
- codegen.s (Generate assembly)
- memory.s (Heap management)

✅ **Puede compilar:**
```
Program hello
  Print "Hello, World!"
```

✅ **Genera assembly funcional**

✅ **Ejecuta correctamente**

---

## Próximos Pasos (Milestone 2)

- Parser YAML completo
- Más templates (Function, etc)
- Validator
- Better memory management
- Self-hosting (el compiler se compila a sí mismo)

---

**¿Comenzamos con main.s?**
