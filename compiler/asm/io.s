# io.s - File I/O functions
# Part of Chronos Compiler

.text

# open_file(filename: *char) -> fd: i64
# Opens a file for reading
# Args: %rdi = filename
# Returns: %rax = file descriptor (or negative on error)
.global open_file
open_file:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx

    # Save filename
    movq %rdi, %rbx

    # Setup openat syscall
    movq $257, %rax         # syscall: openat
    movq $-100, %rdi        # dirfd: AT_FDCWD (-100)
    movq %rbx, %rsi         # filename
    movq $0, %rdx           # flags: O_RDONLY (0)
    xorq %r10, %r10         # mode: 0 (not creating)
    syscall

    # %rax now contains fd or error

    popq %rbx
    popq %rbp
    ret

# close_file(fd: i64)
# Closes a file descriptor
# Args: %rdi = fd
.global close_file
close_file:
    pushq %rbp
    movq %rsp, %rbp

    movq $3, %rax           # syscall: close
    # %rdi already has fd
    syscall

    popq %rbp
    ret

# read_file(fd: i64) -> (buffer: *u8, size: u64)
# Reads entire file into memory
# Args: %rdi = fd
# Returns: %rax = buffer pointer, %rdx = size
.global read_file
read_file:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r12
    pushq %r13

    # Save fd
    movq %rdi, %r12

    # Get file size using fstat
    movq %r12, %rdi         # fd
    leaq stat_buf(%rip), %rsi
    movq $5, %rax           # syscall: fstat
    syscall

    # File size is at offset 48 in stat struct (st_size)
    movq stat_buf+48(%rip), %r13    # file size

    # Allocate buffer using mmap
    xorq %rdi, %rdi         # addr: NULL (kernel chooses)
    movq %r13, %rsi         # length: file size
    movq $3, %rdx           # prot: PROT_READ | PROT_WRITE
    movq $34, %r10          # flags: MAP_PRIVATE | MAP_ANONYMOUS
    movq $-1, %r8           # fd: -1
    xorq %r9, %r9           # offset: 0
    movq $9, %rax           # syscall: mmap
    syscall

    movq %rax, %rbx         # Save buffer address

    # Read file into buffer
    movq $0, %rax           # syscall: read
    movq %r12, %rdi         # fd
    movq %rbx, %rsi         # buffer
    movq %r13, %rdx         # count (file size)
    syscall

    # Return buffer and size
    movq %rbx, %rax         # buffer in %rax
    movq %r13, %rdx         # size in %rdx

    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret

# write_output(filename: *char, buffer: *char)
# Writes output buffer to file
# Args: %rdi = filename, %rsi = buffer (null-terminated)
.global write_output
write_output:
    pushq %rbp
    movq %rsp, %rbp
    pushq %rbx
    pushq %r12
    pushq %r13
    pushq %r14

    # Save args
    movq %rdi, %r14         # filename
    movq %rsi, %r12         # buffer

    # Calculate buffer length (find null terminator)
    movq %r12, %rdi
    call strlen
    movq %rax, %rbx         # Save length

    # Open file for writing
    movq $257, %rax         # syscall: openat
    movq $-100, %rdi        # dirfd: AT_FDCWD
    movq %r14, %rsi         # filename
    movq $577, %rdx         # flags: O_WRONLY | O_CREAT | O_TRUNC (0x241)
    movq $0644, %r10        # mode: rw-r--r--
    syscall

    movq %rax, %r13         # Save fd

    # Write buffer
    movq $1, %rax           # syscall: write
    movq %r13, %rdi         # fd
    movq %r12, %rsi         # buffer
    movq %rbx, %rdx         # count
    syscall

    # Close file
    movq $3, %rax           # syscall: close
    movq %r13, %rdi
    syscall

    popq %r14
    popq %r13
    popq %r12
    popq %rbx
    popq %rbp
    ret

# strlen(str: *char) -> length: u64
# Calculate string length
# Args: %rdi = string
# Returns: %rax = length
.global strlen
strlen:
    pushq %rbp
    movq %rsp, %rbp

    xorq %rax, %rax         # counter = 0
.strlen_loop:
    movzbq (%rdi, %rax), %rcx   # Load byte
    testb %cl, %cl               # Check if zero
    jz .strlen_done
    incq %rax
    jmp .strlen_loop

.strlen_done:
    popq %rbp
    ret

.bss
stat_buf:
    .skip 144               # sizeof(struct stat) = 144 bytes

output_filename_buf:
    .skip 256
