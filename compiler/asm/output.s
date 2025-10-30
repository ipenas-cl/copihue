.text
.global _start

_start:
    # Allocate stack space
    subq $16, %rsp

    # Initialize variable
    movq $123456789, %rax
    movq %rax, -4(%rsp)
    # Print variable
    movq -4(%rsp), %rax
    # TODO: Convert %rax to string and print

    call int_to_string_runtime
    # Print integer result
    movq $1, %rax
    movq $1, %rdi
    leaq int_buffer(%rip), %rsi
    movq int_length(%rip), %rdx
    syscall

    # Deallocate stack space
    addq $16, %rsp


    # Exit
    movq $60, %rax
    xorq %rdi, %rdi
    syscall


# int_to_string_runtime: converts %rax to decimal string
# Input: %rax = integer
# Output: int_buffer contains string, int_length contains length
int_to_string_runtime:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %rcx
    pushq %rdx
    pushq %r8
    pushq %r9

    movq %rax, %r8          # Save number
    xorq %r9, %r9           # digit_count = 0
    leaq int_buffer(%rip), %rbx

    # Check if zero
    testq %r8, %r8
    jnz .its_check_negative
    movb $'0', (%rbx)
    incq %r9
    jmp .its_done

.its_check_negative:
    # Check if negative
    testq %r8, %r8
    jns .its_positive
    movb $'-', (%rbx)
    incq %rbx
    incq %r9
    negq %r8                # Make positive

.its_positive:
    # Extract digits (they come out reversed)
    movq %rbx, %rcx         # Save start of digits
    xorq %rdx, %rdx         # temp digit count

.its_digit_loop:
    testq %r8, %r8
    jz .its_reverse
    xorq %rdx, %rdx
    movq %r8, %rax
    movq $10, %r8
    divq %r8                # %rax = quot, %rdx = rem
    movq %rax, %r8          # number = quot
    addb $'0', %dl          # Convert to ASCII
    movb %dl, (%rbx)
    incq %rbx
    incq %r9
    jmp .its_digit_loop

.its_reverse:
    # Reverse digits in place
    movq %rcx, %rax         # left = start
    leaq -1(%rbx), %rdx     # right = end - 1

.its_reverse_loop:
    cmpq %rax, %rdx
    jle .its_add_newline
    movb (%rax), %r8b
    movb (%rdx), %cl
    movb %cl, (%rax)
    movb %r8b, (%rdx)
    incq %rax
    decq %rdx
    jmp .its_reverse_loop

.its_add_newline:
    movb $'\n', (%rbx)
    incq %r9

.its_done:
    movq %r9, int_length(%rip)
    popq %r9
    popq %r8
    popq %rdx
    popq %rcx
    popq %rbx
    popq %rbp
    ret

.data
.bss
int_buffer:
    .skip 32

int_length:
    .quad 0

