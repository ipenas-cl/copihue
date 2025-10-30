.text
.global _start

_start:
    # Allocate stack space
    subq $16, %rsp

    # Initialize variable
    movq $2, %rax
    movq %rax, -4(%rsp)
    # Initialize variable
    movq $3, %rax
    movq %rax, -8(%rsp)
    # Initialize variable
    movq $4, %rax
    movq %rax, -12(%rsp)
    # Initialize variable
    movq -4(%rsp), %rax
    pushq %rax
    movq -8(%rsp), %rax
    movq %rax, %rcx
    popq %rax
    addq %rcx, %rax
    pushq %rax
    movq -12(%rsp), %rax
    movq %rax, %rcx
    popq %rax
    imulq %rcx, %rax
    movq %rax, -16(%rsp)
    # Print
    movq $1, %rax
    movq $1, %rdi
    leaq .Lstr_0(%rip), %rsi
    movq $27, %rdx
    syscall

    # Deallocate stack space
    addq $16, %rsp


    # Exit
    movq $60, %rax
    xorq %rdi, %rdi
    syscall

.data
.Lstr_0:
    .ascii "Ultimate: (2 + 3) * 4 = 20\n"
