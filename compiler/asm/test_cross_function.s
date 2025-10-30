# Test cross-function variable access like our compiler does

.bss
    .align 8
shared_var:
    .skip 8

.data
msg1:
    .ascii "Write: Setting to 4\n"
msg2:
    .ascii "Read: Value is 0\n"
msg3:
    .ascii "Read: Value is NOT 0\n"

.text
.global _start

_start:
    # Call function that writes
    call write_function

    # Call function that reads
    call read_function

    # Exit
    movq $60, %rax
    movq $0, %rdi
    syscall

write_function:
    pushq %rbp
    movq %rsp, %rbp

    # Print message
    movq $1, %rax
    movq $1, %rdi
    leaq msg1(%rip), %rsi
    movq $21, %rdx
    syscall

    # Write to shared_var
    movq $4, %rax
    movq %rax, shared_var(%rip)

    # Read it back immediately
    movq shared_var(%rip), %rbx

    movq %rbp, %rsp
    popq %rbp
    ret

read_function:
    pushq %rbp
    movq %rsp, %rbp

    # Read shared_var
    movq shared_var(%rip), %rax

    # Check if it's 0
    testq %rax, %rax
    jz .is_zero

    # Not zero
    movq $1, %rax
    movq $1, %rdi
    leaq msg3(%rip), %rsi
    movq $23, %rdx
    syscall
    jmp .done

.is_zero:
    # Is zero
    movq $1, %rax
    movq $1, %rdi
    leaq msg2(%rip), %rsi
    movq $18, %rdx
    syscall

.done:
    movq %rbp, %rsp
    popq %rbp
    ret
