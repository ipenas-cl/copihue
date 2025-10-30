# Test que replica el comportamiento de symbol_table_add

.data
current_offset:
    .quad 0

.text
.global _start

_start:
    # Simular symbol_table_add
    call add_variable_i32

    # Leer offset después de agregar
    call read_offset

    # Exit
    movq $60, %rax
    xorq %rdi, %rdi
    syscall

add_variable_i32:
    pushq %rbp
    movq %rsp, %rbp

    # Leer offset actual
    movq current_offset(%rip), %rax

    # Agregar 4 bytes (i32)
    addq $4, %rax

    # Guardar nuevo offset
    movq %rax, current_offset(%rip)

    movq %rbp, %rsp
    popq %rbp
    ret

read_offset:
    pushq %rbp
    movq %rsp, %rbp

    # Leer offset
    movq current_offset(%rip), %rax

    # Imprimir mensaje
    movq $1, %rax
    movq $1, %rdi
    leaq msg(%rip), %rsi
    movq $24, %rdx
    syscall

    movq %rbp, %rsp
    popq %rbp
    ret

msg:
    .ascii "Offset should be 4 now\n"
