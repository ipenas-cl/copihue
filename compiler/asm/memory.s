# memory.s - Memory management
# Part of Chronos Compiler
# Simple bump allocator for compiler data structures

.text

# allocate_buffer(size: u64) -> ptr: *u8
# Allocates memory from the heap
# Args: %rdi = size in bytes
# Returns: %rax = pointer to allocated memory
.global allocate_buffer
allocate_buffer:
    pushq %rbp
    movq %rsp, %rbp

    # Get current heap pointer
    movq heap_current(%rip), %rax

    # Advance heap pointer
    addq %rdi, %rax
    movq %rax, heap_current(%rip)

    # Return previous heap pointer (now allocated)
    movq heap_current(%rip), %rax
    subq %rdi, %rax

    popq %rbp
    ret

# allocate_ast() -> ast: *AST
# Allocates an AST structure
# Returns: %rax = pointer to AST
.global allocate_ast
allocate_ast:
    pushq %rbp
    movq %rsp, %rbp

    # AST structure:
    # 0: type (8 bytes)
    # 8: name pointer (8 bytes)
    # 16: action_count (8 bytes)
    # 24: actions array (up to 64 pointers = 512 bytes)
    # Total: 536 bytes

    movq $536, %rdi
    call allocate_buffer

    popq %rbp
    ret

.bss
# 1MB heap for compiler data
.align 8
heap_space:
    .skip 1048576

.data
# Heap starts after this address
heap_start_addr:
    .quad heap_space

heap_current:
    .quad heap_space
