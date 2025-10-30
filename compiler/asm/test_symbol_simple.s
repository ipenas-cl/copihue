# Test simple para verificar writes a memoria con RIP-relative
.data
counter:
    .quad 0

.text
.global _start

_start:
    # Leer counter
    movq counter(%rip), %rax

    # Imprimir valor inicial
    movq $1, %rax
    movq $1, %rdi
    leaq msg1(%rip), %rsi
    movq $17, %rdx
    syscall

    # Incrementar counter
    movq counter(%rip), %rax
    incq %rax
    movq %rax, counter(%rip)

    # Leer de nuevo
    movq counter(%rip), %rbx

    # Imprimir valor final
    movq $1, %rax
    movq $1, %rdi
    leaq msg2(%rip), %rsi
    movq $15, %rdx
    syscall

    # Exit
    movq $60, %rax
    xorq %rdi, %rdi
    syscall

msg1:
    .ascii "Counter start: 0\n"
msg2:
    .ascii "Counter end: 1\n"
