# Symbol Table for Chronos Compiler
# Manages variable definitions with types and stack offsets

.section .data
    .align 8

# Variables críticas en .data (inicializadas)
symbol_count:
    .quad 0

current_stack_offset:
    .quad 0

.section .bss
    .align 8

# Symbol entry structure (64 bytes each):
#   Offset 0-31:  name (32 bytes, null-terminated)
#   Offset 32-35: type (4 bytes: 0=i32, 1=i64, 2=bool)
#   Offset 36-39: padding (4 bytes, for 8-byte alignment)
#   Offset 40-47: stack_offset (8 bytes)
#   Offset 48-55: initial_value (8 bytes)
#   Offset 56-63: expr_ast_pointer (8 bytes)
.global symbol_table
symbol_table:
    .skip 6400      # Space for 100 symbols (64 bytes each)

.section .data
# Debug strings
dbg_init_called:
    .ascii "[SYMTAB] Init called\n"
dbg_saving_offset:
    .ascii "[SYMTAB] Saving offset\n"
dbg_verify_write:
    .ascii "[SYMTAB] Verifying write\n"
dbg_readback_ok:
    .ascii "[SYMTAB] Readback OK\n"
dbg_reading_size:
    .ascii "[SYMTAB] Reading size\n"
dbg_offset_value:
    .ascii "[SYMTAB] Value: "
dbg_nonzero:
    .ascii "NON-ZERO!\n"
dbg_zero:
    .ascii "ZERO\n"
dbg_before_store:
    .ascii "[SYMTAB] Before storing in sym\n"
dbg_stored_nonzero:
    .ascii "[SYMTAB] Stored: NON-ZERO\n"
dbg_stored_zero:
    .ascii "[SYMTAB] Stored: ZERO\n"

.section .text
.global symbol_table_init
.global symbol_table_add
.global symbol_table_add_expr
.global symbol_table_lookup
.global symbol_table_get_stack_size
.global symbol_table_get_count

# Initialize symbol table
symbol_table_init:
    pushq %rbp
    movq %rsp, %rbp

    # Reset count and stack offset
    movq $0, symbol_count(%rip)
    movq $0, current_stack_offset(%rip)

    movq %rbp, %rsp
    popq %rbp
    ret

# Add a symbol to the table
# Parameters:
#   %rdi = pointer to name string
#   %rsi = type (0=i32, 1=i64, 2=bool)
#   %rdx = initial value
# Returns:
#   %rax = stack offset for this variable
symbol_table_add:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14

    # Save parameters
    movq %rdi, %r12     # name
    movq %rsi, %r13     # type
    movq %rdx, %r14     # initial value

    # Calculate entry address: symbol_table + (symbol_count * 64)
    movq symbol_count(%rip), %rax
    movq $64, %rbx
    mulq %rbx
    leaq symbol_table(%rip), %rbx
    addq %rbx, %rax
    movq %rax, %rbx     # %rbx = entry address

    # Copy name (max 31 chars + null terminator)
    movq %r12, %rdi     # source
    movq %rbx, %rsi     # destination
    movq $32, %rcx
    call copy_string_bounded

    # Set type (at offset 32)
    movl %r13d, 32(%rbx)

    # Calculate and set stack offset based on type
    movq current_stack_offset(%rip), %rax

    # Determine size based on type
    cmpq $0, %r13       # i32
    je .type_i32
    cmpq $1, %r13       # i64
    je .type_i64
    cmpq $2, %r13       # bool
    je .type_bool
    jmp .type_done

.type_i32:
    addq $4, %rax       # i32 is 4 bytes
    jmp .type_done

.type_i64:
    addq $8, %rax       # i64 is 8 bytes
    jmp .type_done

.type_bool:
    addq $1, %rax       # bool is 1 byte
    # Align to 4 bytes
    addq $3, %rax
    andq $-4, %rax

.type_done:
    # Save new stack offset in global variable
    movq %rax, current_stack_offset(%rip)

    # Store stack offset in symbol entry (at offset 40)
    movq %rax, 40(%rbx)

    # DEBUG: Verify what we just stored (without clobbering %rax)
    movq 40(%rbx), %r15
    # If %r15 != %rax, we have a problem

    # Store initial value (at offset 48)
    movq %r14, 48(%rbx)

    # Increment symbol count
    movq symbol_count(%rip), %rcx
    incq %rcx
    movq %rcx, symbol_count(%rip)

    # Return stack offset
    # (Already in %rax)

    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    movq %rbp, %rsp
    popq %rbp
    ret

# Add a symbol to the table with an expression AST
# Parameters:
#   %rdi = pointer to name string
#   %rsi = type (0=i32, 1=i64, 2=bool)
#   %rdx = expression AST pointer
# Returns:
#   %rax = stack offset for this variable
symbol_table_add_expr:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14
    pushq %r15

    # Save parameters
    movq %rdi, %r12     # name
    movq %rsi, %r13     # type
    movq %rdx, %r15     # expression AST

    # Calculate entry address: symbol_table + (symbol_count * 64)
    movq symbol_count(%rip), %rax
    movq $64, %rbx
    mulq %rbx
    leaq symbol_table(%rip), %rbx
    addq %rbx, %rax
    movq %rax, %rbx     # %rbx = entry address

    # Copy name (max 31 chars + null terminator)
    movq %r12, %rdi     # source
    movq %rbx, %rsi     # destination
    movq $32, %rcx
    call copy_string_bounded

    # Set type (at offset 32)
    movl %r13d, 32(%rbx)

    # Calculate and set stack offset based on type
    movq current_stack_offset(%rip), %rax

    # Determine size based on type
    cmpq $0, %r13       # i32
    je .type_i32_expr
    cmpq $1, %r13       # i64
    je .type_i64_expr
    cmpq $2, %r13       # bool
    je .type_bool_expr
    jmp .type_done_expr

.type_i32_expr:
    addq $4, %rax       # i32 is 4 bytes
    jmp .type_done_expr

.type_i64_expr:
    addq $8, %rax       # i64 is 8 bytes
    jmp .type_done_expr

.type_bool_expr:
    addq $1, %rax       # bool is 1 byte
    # Align to 4 bytes
    addq $3, %rax
    andq $-4, %rax

.type_done_expr:
    # Save new stack offset in global variable
    movq %rax, current_stack_offset(%rip)

    # Store stack offset in symbol entry (at offset 40)
    movq %rax, 40(%rbx)

    # Store expression AST pointer (at offset 56 - in the padding area)
    movq %r15, 56(%rbx)

    # Store initial value as 0 (will be calculated from expression)
    movq $0, 48(%rbx)

    # Increment symbol count
    movq symbol_count(%rip), %rcx
    incq %rcx
    movq %rcx, symbol_count(%rip)

    # Return stack offset
    # (Already in %rax)

    popq %r15
    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    movq %rbp, %rsp
    popq %rbp
    ret

# Look up a symbol by name
# Parameters:
#   %rdi = pointer to name string
# Returns:
#   %rax = pointer to symbol entry, or 0 if not found
symbol_table_lookup:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r12
    pushq %r13

    movq %rdi, %r12     # Search name

    # Iterate through all symbols
    movq $0, %r13       # index

.lookup_loop:
    movq symbol_count(%rip), %rax
    cmpq %rax, %r13
    jge .lookup_not_found

    # Calculate entry address
    movq %r13, %rax
    movq $64, %rbx
    mulq %rbx
    leaq symbol_table(%rip), %rbx
    addq %rbx, %rax
    movq %rax, %rbx     # %rbx = entry address

    # Compare names
    movq %r12, %rdi     # search name
    movq %rbx, %rsi     # entry name
    call string_equal

    cmpq $1, %rax
    je .lookup_found

    incq %r13
    jmp .lookup_loop

.lookup_found:
    movq %rbx, %rax     # Return entry address
    jmp .lookup_done

.lookup_not_found:
    movq $0, %rax

.lookup_done:
    popq %r13
    popq %r12
    popq %rbx
    movq %rbp, %rsp
    popq %rbp
    ret

# Get total stack size needed for all variables
# Returns:
#   %rax = stack size in bytes (aligned to 16 bytes)
symbol_table_get_stack_size:
    pushq %rbp
    movq %rsp, %rbp

    movq current_stack_offset(%rip), %rax

    # Align to 16 bytes (System V ABI requirement)
    addq $15, %rax
    andq $-16, %rax

    movq %rbp, %rsp
    popq %rbp
    ret

# Get the number of symbols in the table
# Returns:
#   %rax = symbol count
symbol_table_get_count:
    pushq %rbp
    movq %rsp, %rbp

    movq symbol_count(%rip), %rax

    movq %rbp, %rsp
    popq %rbp
    ret

# Helper: Copy string with length limit
# Parameters:
#   %rdi = source
#   %rsi = destination
#   %rcx = max length (including null terminator)
copy_string_bounded:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx

    movq %rcx, %rbx     # max length
    xorq %rcx, %rcx     # current position

.copy_loop:
    cmpq %rbx, %rcx
    jge .copy_terminate

    movzbq (%rdi, %rcx), %rax
    movb %al, (%rsi, %rcx)

    cmpb $0, %al
    je .copy_done

    incq %rcx
    jmp .copy_loop

.copy_terminate:
    decq %rcx
    movb $0, (%rsi, %rcx)

.copy_done:
    popq %rbx
    movq %rbp, %rsp
    popq %rbp
    ret

# Helper: Compare two null-terminated strings
# Parameters:
#   %rdi = string 1
#   %rsi = string 2
# Returns:
#   %rax = 1 if equal, 0 if not equal
string_equal:
    pushq %rbp
    movq %rsp, %rbp

    xorq %rcx, %rcx     # position

.compare_loop:
    movzbq (%rdi, %rcx), %rax
    movzbq (%rsi, %rcx), %rdx

    cmpb %al, %dl
    jne .not_equal

    cmpb $0, %al
    je .are_equal

    incq %rcx
    jmp .compare_loop

.are_equal:
    movq $1, %rax
    jmp .compare_done

.not_equal:
    movq $0, %rax

.compare_done:
    movq %rbp, %rsp
    popq %rbp
    ret


