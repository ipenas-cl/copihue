# Test writing to a struct at offset 40

.bss
    .align 8
test_struct:
    .skip 64      # 64 byte struct

.data
msg_before:
    .ascii "Before write\n"
msg_after_zero:
    .ascii "After write: ZERO\n"
msg_after_nonzero:
    .ascii "After write: NON-ZERO\n"

.text
.global _start

_start:
    # Print before
    movq $1, %rax
    movq $1, %rdi
    leaq msg_before(%rip), %rsi
    movq $13, %rdx
    syscall

    # Get address of struct
    leaq test_struct(%rip), %rbx

    # Write 42 at offset 40
    movq $42, %rax
    movq %rax, 40(%rbx)

    # Read it back
    movq 40(%rbx), %rcx

    # Check
    testq %rcx, %rcx
    jz .is_zero

    # Non-zero
    movq $1, %rax
    movq $1, %rdi
    leaq msg_after_nonzero(%rip), %rsi
    movq $22, %rdx
    syscall
    jmp .done

.is_zero:
    movq $1, %rax
    movq $1, %rdi
    leaq msg_after_zero(%rip), %rsi
    movq $18, %rdx
    syscall

.done:
    # Exit
    movq $60, %rax
    movq $0, %rdi
    syscall
