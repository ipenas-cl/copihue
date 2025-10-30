# Chronos Compiler v0.0.1
# Written in pure x86-64 assembly (AT&T syntax)
#
# A template-based deterministic programming language compiler
# written entirely in assembly to maximize determinism and control.

.text
.global _start

_start:
    # Stack layout on entry:
    # (%rsp)     = argc
    # 8(%rsp)    = argv[0] (program name)
    # 16(%rsp)   = argv[1] (input file)

    # Check argc
    movq (%rsp), %rdi       # argc
    cmpq $2, %rdi
    jl .usage_error         # Need at least 2 args (program + input)

    # Get input filename (argv[1])
    movq 16(%rsp), %rdi     # argv[1]
    movq %rdi, %r15         # Save for later

    # Print status
    call print_compiling_msg

    # Open and read input file
    movq %r15, %rdi         # Restore filename
    call open_file          # fd in %rax
    testq %rax, %rax
    js .open_error          # negative = error
    movq %rax, %r12         # Save fd

    # Read entire file
    movq %r12, %rdi         # fd
    call read_file          # buffer in %rax, size in %rdx
    movq %rax, %r13         # Save buffer
    movq %rdx, %r14         # Save size

    # Close input file
    movq %r12, %rdi
    call close_file

    # Check if buffer is valid (not NULL or negative)
    testq %r13, %r13
    jle .open_error

    # Debug: file read successfully
    call print_debug_read

    # Parse template
    movq %r13, %rdi         # buffer
    movq %r14, %rsi         # size
    call parse_template     # AST in %rax
    movq %rax, %r15         # Save AST

    # Debug: parsed successfully
    call print_debug_parse

    # Check if AST is valid
    testq %r15, %r15
    jz .open_error

    # Generate assembly code
    movq %r15, %rdi         # AST
    call generate_code      # output buffer in %rax
    movq %rax, %rbx         # Save output

    # Debug: codegen successful
    call print_debug_codegen

    # Write output file
    leaq output_filename(%rip), %rdi
    movq %rbx, %rsi
    call write_output

    # Print success
    call print_success_msg

    # Exit with success
    movq $60, %rax          # syscall: exit
    xorq %rdi, %rdi         # status: 0
    syscall

.usage_error:
    # Print usage message
    movq $1, %rax           # syscall: write
    movq $2, %rdi           # stderr
    leaq usage_msg(%rip), %rsi
    movq $usage_msg_len, %rdx
    syscall

    # Exit with error
    movq $60, %rax
    movq $1, %rdi
    syscall

.open_error:
    # Print error message
    movq $1, %rax
    movq $2, %rdi
    leaq open_error_msg(%rip), %rsi
    movq $open_error_len, %rdx
    syscall

    movq $60, %rax
    movq $1, %rdi
    syscall

# Helper functions

print_compiling_msg:
    pushq %rbp
    movq %rsp, %rbp

    movq $1, %rax
    movq $1, %rdi
    leaq compiling_msg(%rip), %rsi
    movq $compiling_msg_len, %rdx
    syscall

    popq %rbp
    ret

print_success_msg:
    pushq %rbp
    movq %rsp, %rbp

    movq $1, %rax
    movq $1, %rdi
    leaq success_msg(%rip), %rsi
    movq $success_msg_len, %rdx
    syscall

    popq %rbp
    ret

print_debug_read:
    pushq %rbp
    movq %rsp, %rbp

    movq $1, %rax
    movq $1, %rdi
    leaq debug_read_msg(%rip), %rsi
    movq $debug_read_len, %rdx
    syscall

    popq %rbp
    ret

print_debug_parse:
    pushq %rbp
    movq %rsp, %rbp

    movq $1, %rax
    movq $1, %rdi
    leaq debug_parse_msg(%rip), %rsi
    movq $debug_parse_len, %rdx
    syscall

    popq %rbp
    ret

print_debug_codegen:
    pushq %rbp
    movq %rsp, %rbp

    movq $1, %rax
    movq $1, %rdi
    leaq debug_codegen_msg(%rip), %rsi
    movq $debug_codegen_len, %rdx
    syscall

    popq %rbp
    ret

.data
usage_msg:
    .ascii "Chronos Compiler v0.0.1\n"
    .ascii "Usage: chronos <input.chronos>\n"
usage_msg_len = . - usage_msg

open_error_msg:
    .ascii "Error: Cannot open input file\n"
open_error_len = . - open_error_msg

compiling_msg:
    .ascii "Compiling...\n"
compiling_msg_len = . - compiling_msg

success_msg:
    .ascii "✓ Generated: output.s\n"
success_msg_len = . - success_msg

output_filename:
    .ascii "output.s\0"

debug_read_msg:
    .ascii "[DEBUG] File read OK\n"
debug_read_len = . - debug_read_msg

debug_parse_msg:
    .ascii "[DEBUG] Parse OK\n"
debug_parse_len = . - debug_parse_msg

debug_codegen_msg:
    .ascii "[DEBUG] Codegen OK\n"
debug_codegen_len = . - debug_codegen_msg
