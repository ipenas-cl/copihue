.section __TEXT,__text,regular,pure_instructions
.globl _start

_start:
    // Banner message
    mov x8, #4                    // write syscall (0x2000004 & 0xFF)
    mov x0, #1                    // stdout
    adr x1, banner                // message
    mov x2, #100                  // approximate length
    svc #0x80                     // system call with high bits

    // Exit
    mov x8, #1                    // exit syscall (0x2000001 & 0xFF)  
    mov x0, #0                    // exit code
    svc #0x80                     // system call with high bits

.section __TEXT,__cstring,cstring_literals
banner:
    .ascii "╔═════╦═════╦═════╗\n"
    .ascii "║ 🛡️  ║ ⚖️  ║ ⚡  ║  TEMPO Bootstrap ARM64\n"
    .ascii "║  C  ║  E  ║  G  ║\n"
    .ascii "╚═════╩═════╩═════╝\n"
    .ascii "\n❌ ARM64 Bootstrap necesita implementación completa\n"
    .ascii "   Por ahora usa la versión x86-64 con Rosetta 2\n\0"
